// enable openmp optional
#include <typeinfo>

#include "msg.hpp"

#ifdef _OPENMP
#include <omp.h>
// #else
// #define omp_get_thread_num() 0
#endif

#ifdef USE_HDF5
#include "ioHDF5.h"
#endif

#include "defines.hpp"
#include "msg.hpp"
#include "cfg.hpp"
#include "lib.hpp"

#include <string>

#include "math/rand.hpp"
#include "math/maxwellJuttner.hpp"

#include "geometry.hpp"

#include "grid.hpp"

// #include "particles.hpp"

// #include "area.hpp"
#include "timeSim.hpp"

#include "specieP.hpp"
#include "beamP.hpp"

#include "fieldE.hpp"
#include "specieP.hpp"

#include "dataWriter.hpp"

// #include "pBunch.hpp"

// #include "probePlain.hpp"

using namespace std;

string parse_argv_get_config(int argc, char **argv)
{
  string filename;

  if (lib::cmd_option_exists(argv, argv+argc, "-h"))
  {
    cerr << "USAGE:" << endl << "  pdp3 [ --version | -f path/to/PicoPIC.json ]" << endl;
    exit(1);
  }

  if (lib::cmd_option_exists(argv, argv+argc, "--version"))
  {
    cerr << PACKAGE_NAME << " " << PACKAGE_VERSION << endl;
    exit(0);
  }

  if (lib::cmd_option_exists(argv, argv+argc, "-f"))
  {
    filename = lib::get_cmd_option(argv, argv + argc, "-f");
    if (filename.empty())
    {
      cerr << "ERROR: configuration path is not specified" << endl;
      exit(1);
    }
  }
  else
    filename = std::string(PACKAGE_NAME) + std::string(".json");

  return filename;
}

void particles_runaway_collector (Grid<Area*> areas, Geometry *geometry_global)
{
  // ! collects particles, that runaways from their areas and moves it to
  // ! area, corresponding to their actual position
  // ! also, erase particles, that run out of simulation area
  unsigned int r_areas = areas.size_x();
  unsigned int z_areas = areas.size_x();
  int j_c = 0;
  int r_c = 0;
  for (unsigned int i=0; i < r_areas; i++)
    for (unsigned int j = 0; j < z_areas; j++)
    {
      Area *sim_area = areas.get(i, j);

      for (auto ps = sim_area->species_p.begin(); ps != sim_area->species_p.end(); ++ps)
      {
        (**ps).particles.erase(
          std::remove_if((**ps).particles.begin(), (**ps).particles.end(),
                         [&j_c, &r_c, &ps, &areas, &sim_area, &i, &j, &geometry_global](vector <double> * & o)
                         {
                           bool res = false;

                           // not unsigned, because it could be less, than zero
                           int r_cell = CELL_NUMBER(P_POS_R((*o)), sim_area->geometry.r_cell_size);
                           int z_cell = CELL_NUMBER(P_POS_Z((*o)), sim_area->geometry.z_cell_size);

                           int i_dst = (int)ceil(r_cell / sim_area->geometry.r_grid_amount);
                           int j_dst = (int)ceil(z_cell / sim_area->geometry.z_grid_amount);

                           if (r_cell < 0 || z_cell < 0)
                           {
                             LOG_ERR("Particle position is less, than 0. Position is: ["
                                     << P_POS_R((*o)) << ", "
                                     << P_POS_Z((*o)) << "]");
                           }

                           // remove out-of-simulation particles
                           if (r_cell >= geometry_global->r_grid_amount
                               || r_cell < 0
                               || z_cell >= geometry_global->z_grid_amount
                               || z_cell < 0
                             )
                           {
                             ++r_c;
                             res = true;
                           }

                           // move particles between cells
                           else if (i_dst != (int)i || j_dst != (int)j) // check that destination area is different, than source
                           {
                             ++j_c;

                             Area *dst_area = areas.get(i_dst, j_dst);
                             for (auto pd = dst_area->species_p.begin(); pd != dst_area->species_p.end(); ++pd)
                               if ((**pd).id == (**ps).id)
                                 (**pd).particles.push_back(o);

                             res = true;
                           }
                           return res;
                         }),
          (**ps).particles.end());
      }

      // update grid
      if (i < geometry_global->areas_by_r - 1)
      {
        Area *dst_area = areas.get(i+1, j);
        for (int v = 0; v < sim_area->geometry.z_grid_amount; ++v)
        {
          // set current
          dst_area->current->current[0].inc(0, v, sim_area->current->current[0].get(sim_area->geometry.r_grid_amount, v));
          dst_area->current->current[1].inc(0, v, sim_area->current->current[1].get(sim_area->geometry.r_grid_amount, v));
          dst_area->current->current[2].inc(0, v, sim_area->current->current[2].get(sim_area->geometry.r_grid_amount, v));

          // set eField
          dst_area->field_e->field[0].inc(0, v, sim_area->field_e->field[0].get(sim_area->geometry.r_grid_amount, v));
          dst_area->field_e->field[1].inc(0, v, sim_area->field_e->field[1].get(sim_area->geometry.r_grid_amount, v));
          dst_area->field_e->field[2].inc(0, v, sim_area->field_e->field[2].get(sim_area->geometry.r_grid_amount, v));

          // set hField
          dst_area->field_h->field[0].inc(0, v, sim_area->field_h->field[0].get(sim_area->geometry.r_grid_amount, v));
          dst_area->field_h->field[1].inc(0, v, sim_area->field_h->field[1].get(sim_area->geometry.r_grid_amount, v));
          dst_area->field_h->field[2].inc(0, v, sim_area->field_h->field[2].get(sim_area->geometry.r_grid_amount, v));
          dst_area->field_h->field_at_et[0].inc(0, v, sim_area->field_h->field_at_et[0].get(sim_area->geometry.r_grid_amount, v));
          dst_area->field_h->field_at_et[1].inc(0, v, sim_area->field_h->field_at_et[1].get(sim_area->geometry.r_grid_amount, v));
          dst_area->field_h->field_at_et[2].inc(0, v, sim_area->field_h->field_at_et[2].get(sim_area->geometry.r_grid_amount, v));
        }
      }

      if (j < geometry_global->areas_by_z - 1)
      {
        Area *dst_area = areas.get(i, j + 1);
        for (int v = 0; v < sim_area->geometry.r_grid_amount; ++v)
        {
          dst_area->current->current[0].inc(v, 0, sim_area->current->current[0].get(v, sim_area->geometry.z_grid_amount));
          dst_area->current->current[1].inc(v, 0, sim_area->current->current[1].get(v, sim_area->geometry.z_grid_amount));
          dst_area->current->current[2].inc(v, 0, sim_area->current->current[2].get(v, sim_area->geometry.z_grid_amount));

          // set eField
          dst_area->field_e->field[0].inc(v, 0, sim_area->field_e->field[0].get(v, sim_area->geometry.z_grid_amount));
          dst_area->field_e->field[1].inc(v, 0, sim_area->field_e->field[1].get(v, sim_area->geometry.z_grid_amount));
          dst_area->field_e->field[2].inc(v, 0, sim_area->field_e->field[2].get(v, sim_area->geometry.z_grid_amount));

          // set hField
          dst_area->field_h->field[0].inc(v, 0, sim_area->field_h->field[0].get(v, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field[1].inc(v, 0, sim_area->field_h->field[1].get(v, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field[2].inc(v, 0, sim_area->field_h->field[2].get(v, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field_at_et[0].inc(v, 0, sim_area->field_h->field_at_et[0].get(v, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field_at_et[1].inc(v, 0, sim_area->field_h->field_at_et[1].get(v, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field_at_et[2].inc(v, 0, sim_area->field_h->field_at_et[2].get(v, sim_area->geometry.z_grid_amount));
        }

        if (i < geometry_global->areas_by_r - 1 && j < geometry_global->areas_by_z - 1)
        {
          Area *dst_area = areas.get(i + 1, j + 1);

          dst_area->current->current[0].inc(0, 0, sim_area->current->current[0].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->current->current[1].inc(0, 0, sim_area->current->current[1].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->current->current[2].inc(0, 0, sim_area->current->current[2].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));

          // set eField
          dst_area->field_e->field[0].inc(0, 0, sim_area->field_e->field[0].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->field_e->field[1].inc(0, 0, sim_area->field_e->field[1].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->field_e->field[2].inc(0, 0, sim_area->field_e->field[2].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));

          // set hField
          dst_area->field_h->field[0].inc(0, 0, sim_area->field_h->field[0].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field[1].inc(0, 0, sim_area->field_h->field[1].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field[2].inc(0, 0, sim_area->field_h->field[2].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field_at_et[0].inc(0, 0, sim_area->field_h->field_at_et[0].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field_at_et[1].inc(0, 0, sim_area->field_h->field_at_et[1].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
          dst_area->field_h->field_at_et[2].inc(0, 0, sim_area->field_h->field_at_et[2].get(sim_area->geometry.r_grid_amount, sim_area->geometry.z_grid_amount));
        }
      }
    }
  LOG_DBG("Amount of particles to jump between areas: " << j_c << ", amount of particles to remove: " << r_c);
}

int main(int argc, char **argv)
{

#ifdef _OPENMP
#ifdef OPENMP_DYNAMIC_THREADS
  omp_set_dynamic(1); // Explicitly enable dynamic teams
  LOG_DBG("Number of Calculation Processors Changing Dynamically");
#else
  int cores = omp_get_num_procs();
  LOG_DBG("Number of Calculation Processors: " << cores);
  omp_set_dynamic(0); // Explicitly disable dynamic teams
  omp_set_num_threads(cores); // Use 4 threads for all consecutive parallel regions
#endif
#else
  LOG_DBG("There is no Openmp Here");
#endif

  ////!
  ////! Program begin
  ////!
  LOG_INFO("Initialization");

  // string cfgname;
  string cfgname = parse_argv_get_config(argc, argv);

  LOG_DBG("Reading Configuration File ``" << cfgname << "''");
  Cfg cfg = Cfg(cfgname.c_str());

  LOG_DBG("Checking if Configuration is Correct (TBD)");

  Geometry* geometry_global = cfg.geometry;

  TimeSim* sim_time_clock = cfg.time;

  LOG_DBG("Initializing Geometry, Particle Species and Simulation Areas");

  Grid<Area*> areas (geometry_global->areas_by_r, geometry_global->areas_by_z);

  LOG_DBG("Initializing Data Paths");

  vector<DataWriter> data_writers;

  for (auto i = cfg.probes.begin(); i != cfg.probes.end(); ++i)
  {
    int probe_size[4] = {i->r_start, i->z_start, i->r_end, i->z_end};

    DataWriter writer (cfg.output_data->data_root, i->component,
                       i->specie, i->shape, probe_size, i->schedule,
                       cfg.output_data->compress, cfg.output_data->compress_level,
                       geometry_global, sim_time_clock, areas);

    data_writers.push_back(writer);
  }

  unsigned int r_areas = geometry_global->areas_by_r;
  unsigned int z_areas = geometry_global->areas_by_z;

  unsigned int p_id_counter = 0;
  unsigned int b_id_counter = 1000;

  for (unsigned int i=0; i < r_areas; i++)
    for (unsigned int j = 0; j < z_areas; j++)
    {
      //! init geometry
      bool wall_r0 = false;
      bool wall_rr = false;
      bool wall_z0 = false;
      bool wall_zz = false;

      double pml_l_z0 = 0;
      double pml_l_zwall = 0;
      double pml_l_rwall = 0;

      // set walls to areas
      if (i == 0)
        wall_r0 = true;
      if (i == r_areas - 1)
        wall_rr = true;
      if (j == 0)
        wall_z0 = true;
      if (j == z_areas - 1)
        wall_zz = true;

      // set PML to areas
      if (geometry_global->r_size - geometry_global->r_size / r_areas * (i + 1)
          < geometry_global->pml_length[2])
        pml_l_rwall = geometry_global->pml_length[2];
      if (j * geometry_global->z_size / z_areas < geometry_global->pml_length[1])
        pml_l_z0 = geometry_global->pml_length[1];
      if (geometry_global->z_size - geometry_global->z_size / z_areas * (j + 1)
          < geometry_global->pml_length[3])
        pml_l_zwall = geometry_global->pml_length[3];

      unsigned int bot_r = (unsigned int)geometry_global->r_grid_amount * i / r_areas;
      unsigned int top_r = (unsigned int)geometry_global->r_grid_amount * (i + 1) / r_areas;

      unsigned int left_z = (unsigned int)geometry_global->z_grid_amount * j / z_areas;
      unsigned int right_z = (unsigned int)geometry_global->z_grid_amount * (j + 1) / z_areas;

      Geometry *geom_area = new Geometry (
        geometry_global->r_size / r_areas,
        geometry_global->z_size / z_areas,
        bot_r, top_r, left_z, right_z,
        pml_l_z0 * z_areas,    // multiplying is a workaround, because area
        pml_l_zwall * z_areas, // doesn't know abount whole simulation area size
        pml_l_rwall * r_areas, // aka geometry_global
        geometry_global->pml_sigma[0],
        geometry_global->pml_sigma[1],
        wall_r0,
        wall_rr,
        wall_z0,
        wall_zz
        );

      // WORKAROUND: // used just to set PML
      // for information about global geometry
      // WARNING! don't use it in local geometries!
      geom_area->areas_by_r = r_areas;
      geom_area->areas_by_z = z_areas;
      // /WORKAROUND

      // init particle species
      vector<SpecieP *> species_p;

      p_id_counter = 0; // counter for particle specie IDs
      b_id_counter = 1000; // counter for particle specie IDs

      for (auto k = cfg.particle_species.begin(); k != cfg.particle_species.end(); ++k)
      {
        unsigned int grid_cell_macro_amount = (int)(k->macro_amount / r_areas / z_areas);

        SpecieP *pps = new SpecieP (p_id_counter,
                                    k->name,
                                    k->charge, k->mass, grid_cell_macro_amount,
                                    k->left_density, k->right_density,
                                    k->temperature, geom_area, sim_time_clock);
        species_p.push_back(pps);

        ++p_id_counter;
      };

      // init particle beams
      for (auto bm = cfg.particle_beams.begin(); bm != cfg.particle_beams.end(); ++bm)
      {
        BeamP *beam = new BeamP (b_id_counter, ((string)"beam_").append(bm->name),
                                 bm->charge, bm->mass, bm->macro_amount,
                                 bm->start_time, bm->bunch_radius, bm->density,
                                 bm->bunches_amount, bm->bunch_length,
                                 bm->bunches_distance, bm->velocity,
                                 geom_area, sim_time_clock);

        species_p.push_back(beam);

        ++b_id_counter;
      }

      Area *sim_area = new Area(*geom_area, species_p, sim_time_clock);
      areas.set(i, j, sim_area);
    };

  LOG_INFO("Preparation to calculation");

#pragma omp parallel for
  for (unsigned int i=0; i < r_areas; i++)
    for (unsigned int j = 0; j < z_areas; j++)
    {
      Area *sim_area = areas.get(i, j);

      sim_area->distribute(); // spatial and velocity distribution
    }

  LOG_DBG("Initializing Boundary Conditions (TBD)");
  // TODO: this is legacy PDP3 code. Need to update it
//   void LoadInitParam::init_boundary ()
// {
//   //! initialize boundaries and boundary conditions

//   // Maxwell initial conditions
//   BoundaryMaxwellConditions maxwell_rad(efield); // TODO: WTF?
//   maxwell_rad.specify_initial_field(params->geom,
//                                     params->boundary_maxwell_e_phi_upper,
//                                     params->boundary_maxwell_e_phi_left,
//                                     params->boundary_maxwell_e_phi_right);

//   if (params->boundary_conditions == 0)
//   {
//     p_list->charge_weighting(c_rho_new);

//     // Seems: https://en.wikipedia.org/wiki/Dirichlet_distribution
//     PoissonDirichlet dirih(params->geom);
//     dirih.poisson_solve(efield, c_rho_new);
//   }
// }

  //! Main calculation loop
  LOG_INFO("Launching calculation");

  // need to set p_id_counter to zero, because bunches are injecting dynamically
  // and we need mark it
  p_id_counter = 0;

  while (sim_time_clock->current < sim_time_clock->end)
  {
    LOG_DBG("Processing areas at time: " << sim_time_clock->current);

#pragma omp parallel for
    for (unsigned int i=0; i < r_areas; i++)
      for (unsigned int j = 0; j < z_areas; j++)
      {
        Area *sim_area = areas.get(i, j);

        // ! 1. manage beam
        sim_area->manage_beam();

        // ! 2. Calculate magnetic field (H)
        sim_area->weight_field_h(); // +

        // ! 3. Calculate velocity
        sim_area->reset_current(); // +
        // TODO: sim_area->reset_charge(); // + for c_rho_old, c_rho_bunch
        sim_area->push_particles(); // +
        sim_area->dump_particle_positions_to_old(); // +
        sim_area->update_particles_coords_at_half(); // + +reflect
        sim_area->particles_back_position_to_rz(); // +
        sim_area->reflect(); // +
        // TODO: break and process area borders
      }

    // process borders
    LOG_DBG("Processing borders at time: " << cfg.time->current);
    particles_runaway_collector(areas, geometry_global);

#pragma omp parallel for
    for (unsigned int i=0; i < r_areas; i++)
      for (unsigned int j = 0; j < z_areas; j++)
      {
        Area *sim_area = areas.get(i, j);

        // current distribution
        sim_area->weight_current_azimuthal();
        sim_area->update_particles_coords_at_half(); // +
        sim_area->particles_back_position_to_rz(); // +
        sim_area->reflect(); // +
        // TODO: break and process area borders
      }

    // process borders
    LOG_DBG("Processing borders at time: " << cfg.time->current);
    particles_runaway_collector(areas, geometry_global);

#pragma omp parallel for
    for (unsigned int i=0; i < r_areas; i++)
      for (unsigned int j = 0; j < z_areas; j++)
      {
        Area *sim_area = areas.get(i, j);

        sim_area->weight_current();
        sim_area->particles_back_velocity_to_rz();

        // ! 5. Calculate electric field (E)
        sim_area->weight_field_e(); // +

        // ! 6. Continuity equation
        // sim_area->reset_charge(); // TODO: for c_rho_new
        // sim_area->weight_charge(); // TODO: r_who_new, c_bunch
      }

    // dump data
// #pragma omp parallel for
    for (unsigned int i=0; i < data_writers.size(); ++i)
      data_writers[i].go();

    sim_time_clock->current += sim_time_clock->step;
  }

  // this is only to finish pretty pringing of data_writers output
  if (! DEBUG)
  {
    MSG("+------------+-------------+-------------------------------------------------+-------+------------------+---------------------+");
  }

  LOG_INFO("SIMULATION COMPLETE");

  return 0;
}