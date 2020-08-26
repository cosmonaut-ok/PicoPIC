/*
 * This file is part of the PiCOPIC distribution (https://github.com/cosmonaut-ok/PiCOPIC).
 * Copyright (c) 2020 Alexander Vynnyk.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "collisionsSentokuM.hpp"
#include <iostream>
// say: TA77   - Takizuka, Abe; 1977; DOI: 10.1016/0021-9991(77)90099-7
//      TITU18 - SentokuM et al.; 2018; DOI: 10.1002/ctpp.201700121

using namespace std;

CollisionsSentokuM::CollisionsSentokuM (Geometry* _geometry, TimeSim *_time, vector <SpecieP *> _species_p) : Collisions ( _geometry, _time, _species_p)
{
  //! TA77: make dummies to place particles there
  // map_el2cell = Grid<vector< vector<double> * >> (geometry->r_grid_amount, geometry->z_grid_amount, 2);
  // map_ion2cell = Grid<vector< vector<double> * >> (geometry->r_grid_amount, geometry->z_grid_amount, 2);

}

void CollisionsSentokuM::collide_single(int i, int j, double m_real_a, double m_real_b,
                                vector<double> &pa, vector<double> &pb)
{
  // get required parameters
  double charge_a, mass_a, charge_b, mass_b;
  bool swap = false;

  double id = math::random::uniform();

  // TITU18: find weight ratio
  double w_ratio = 2; // P_MASS(pa) * m_real_b / (P_MASS(pb) * m_real_a);

  vector3d<double> v_a;
  vector3d<double> v_b;

  // a-particle should be lighter, than b-particle
  if (w_ratio <= 1)
  {
    v_a[0] = P_VEL_R(pa);
    v_a[1] = P_VEL_PHI(pa);
    v_a[2] = P_VEL_Z(pa);
    charge_a = P_CHARGE(pa);
    mass_a = P_MASS(pa);

    v_b[0] = P_VEL_R(pb);
    v_b[1] = P_VEL_PHI(pb);
    v_b[2] = P_VEL_Z(pb);
    charge_b = P_CHARGE(pb);
    mass_b = P_MASS(pb);
  }
  else
  {
    v_a[0] = P_VEL_R(pb);
    v_a[1] = P_VEL_PHI(pb);
    v_a[2] = P_VEL_Z(pb);
    charge_a = P_CHARGE(pb);
    mass_a = P_MASS(pb);

    v_b[0] = P_VEL_R(pa);
    v_b[1] = P_VEL_PHI(pa);
    v_b[2] = P_VEL_Z(pa);
    charge_b = P_CHARGE(pa);
    mass_b = P_MASS(pa);

    // swap particles when b-particle is lighter, than a-particle
    w_ratio = 1 / w_ratio;
    swap = true;
  }

  //// main calculation

  // get \gamma_a and \gamma_b --- LAB frame
  double gamma_a = phys::rel::lorenz_factor(v_a.length2());
  double gamma_b = phys::rel::lorenz_factor(v_b.length2());

  // get v of CM frame
  vector3d<double> v_cm, v_a_cm, v_b_cm;
  v_cm = v_a * gamma_a * mass_a + v_b * gamma_b * mass_b;
  v_cm /= gamma_a * mass_a + gamma_b * mass_b;
  double gamma_cm = phys::rel::lorenz_factor(v_cm.length2());

  // get v_a and v_b --- COM frame
  v_a_cm = v_cm * v_cm.dot(v_a) * (gamma_cm - 1) / v_cm.length2();
  v_a_cm += v_a;
  v_a_cm -= v_cm * gamma_cm;
  v_a_cm /= gamma_cm * (1 - v_cm.dot(v_a) / LIGHT_VEL_POW_2);
  double gamma_a_cm = phys::rel::lorenz_factor(v_a_cm.length2());

  v_b_cm = v_cm * v_cm.dot(v_b) * (gamma_cm - 1) / v_cm.length2();
  v_b_cm += v_b;
  v_b_cm -= v_cm * gamma_cm;
  v_b_cm /= gamma_cm * (1 - v_cm.dot(v_b) / LIGHT_VEL_POW_2);
  double gamma_b_cm = phys::rel::lorenz_factor(v_b_cm.length2());

  // get p_a_cm and p_b_cm --- COM frame
  vector3d<double> p_cm, p_a_cm, p_b_cm;
  p_a_cm = v_a_cm * mass_a * gamma_a_cm;
  p_b_cm = v_b_cm * mass_b * gamma_b_cm;
  p_cm = p_a_cm;

  // get v relative --- OPR frame
  vector3d<double> v_rel, p_rel;
  v_rel = v_a_cm - v_b_cm;
  v_rel /= 1 - v_a_cm.dot(v_b_cm) / LIGHT_VEL_POW_2;
  p_rel = p_b_cm - p_a_cm;

////////////////////////////////////////////////////////////////////////////////

  // get densities and electron temperature
  double temperature_el = get_el_temperature(i, j);
  double density_el = get_el_density(i, j);
  double density_lowest = min(get_el_density(i, j), get_ion_density(i, j));
  double density_highest = max(get_el_density(i, j), get_ion_density(i, j));

  // check if collision is possible
  if (p_rel.length2() == 0) return;
  if (v_rel.length2() < constant::MNZL) return;
  if (density_el <= 0) return;
  if (!isnormal(temperature_el)) return;

  // get debye length
  double debye = phys::plasma::debye_length(density_el, temperature_el);

  // get coulomb logarithm
  double L_coulomb = phys::plasma::coulomb_logarithm (mass_a, mass_b,
						      debye, v_rel.length());
  // TODO: figure out, why coulomb logarithm can acquire negative values
  if (L_coulomb <= 0) return;

  // get collision frequency
  double coll_freq = phys::plasma::collision_freqency (charge_a, charge_b,
                                                       density_lowest,
                                                       L_coulomb,
                                                       p_rel.length(),
                                                       v_rel.length());

  // TA77: calculate scattering angles Theta and Phi
  // TA77 sub: find delta
  //// find variance of delta
  double m_ab = mass_a * mass_b / (mass_a + mass_b);
  double variance_d = pow(charge_a, 2) * pow(charge_b, 2) * density_lowest
    * L_coulomb / (8 * constant::PI * pow(EPSILON0, 2) * m_ab
                   * p_cm.length2() * v_cm.length())
    * time->step;
  //// find standard deviation of delta
  double std_dev_d = lib::sq_rt(variance_d);
  //// find delta
  double delta = math::random::normal(std_dev_d);

  // TA77: find Theta angle
  double sin_theta = 2 * delta / (1 + pow(delta, 2));
  double cos_theta = 1 - 2 * pow(delta, 2) / (1 + pow(delta, 2));

  // TA77: find Phi angle
  double phi_angle = math::random::uniform_angle();
  double sin_phi = sin(phi_angle);
  double cos_phi = cos(phi_angle);

////////////////////////////////////////////////////////////////////////////////

  // get d_p --- COM frame
  vector3d<double> d_p;
  double p_cm_abs = p_cm.length();
  double p_cm_prp = lib::sq_rt(p_cm[0] * p_cm[0] + p_cm[1] * p_cm[1]);

  d_p[0] = p_cm[0] * p_cm[2] / p_cm_prp * sin_theta * cos_phi
    - p_cm[1] * p_cm_abs / p_cm_prp * sin_theta * sin_phi
    - p_cm[0] * ( 1 - cos_theta);
  d_p[1] = p_cm[1] * p_cm[2] / p_cm_prp * sin_theta * cos_phi
    + p_cm[0] * p_cm_abs / p_cm_prp * sin_theta * sin_phi
    - p_cm[0] * ( 1 - cos_theta);
  d_p[2] = -p_cm_prp * sin_theta * cos_phi - p_cm[2] * ( 1 - cos_theta );

  // get p_a_bar_cm and p_b_bar_cm --- COM frame
  vector3d<double> p_a_bar_cm, p_b_bar_cm;
  p_a_bar_cm = p_a_cm + d_p;
  p_b_bar_cm = p_b_cm - d_p;

  // get v_a_bar_cm and v_b_bar_cm --- COM frame
  vector3d<double> v_a_bar_cm, v_b_bar_cm;
  v_a_bar_cm = p_a_cm / gamma_a_cm / mass_a;
  v_b_bar_cm = p_b_cm / gamma_b_cm / mass_b;

  // get v_a_bar and v_b_bar --- LAB frame
  vector3d<double> v_a_bar, v_b_bar;

  v_a_bar = v_cm * v_cm.dot(v_a_bar_cm) * (gamma_cm - 1) / v_cm.length2();
  v_a_bar += v_a_bar_cm;
  v_a_bar += v_cm * gamma_cm; // FIXME: should it be a vector?
  v_a_bar /= gamma_cm * (1 + v_cm.dot(v_a_bar_cm) / LIGHT_VEL_POW_2);
  // double gamma_a_cm = phys::rel::lorenz_factor(v_a_cm.length2());

  v_b_bar = v_cm * v_cm.dot(v_b_bar_cm) * (gamma_cm - 1) / v_cm.length2();
  v_b_bar += v_b_bar_cm;
  v_b_bar += v_cm * gamma_cm; // FIXME: should it be a vector?
  v_b_bar /= gamma_cm * (1 + v_cm.dot(v_b_bar_cm) / LIGHT_VEL_POW_2);
  // double gamma_b_cm = phys::rel::lorenz_factor(v_b_cm.length2());

  // if (v_a.length() > 1e8 || v_b.length() > 1e8)
  // {
  //   LOG_S(ERROR) << v_a[0] << " " << v_a[1] << " " << v_a[2] << " " << v_a.length();
  //   LOG_S(ERROR) << v_b[0] << " " << v_b[1] << " " << v_b[2] << " " << v_b.length();
  //   LOG_S(ERROR) << sin_theta << " " << cos_theta << " " << sin_phi << " " << cos_phi;
  //   LOG_S(ERROR) << v_a_cm[0] << " " << v_a_cm[1] << " " << v_a_cm[2] << " " << v_a_cm.length();
  //   LOG_S(ERROR) << v_b_cm[0] << " " << v_b_cm[1] << " " << v_b_cm[2] << " " << v_b_cm.length();
  //   LOG_S(ERROR) << v_a_bar[0] << " " << v_a_bar[1] << " " << v_a_bar[2] << " " << v_a_bar.length();
  //   LOG_S(ERROR) << v_b_bar[0] << " " << v_b_bar[1] << " " << v_b_bar[2] << " " << v_b_bar.length();
  //   LOG_S(ERROR);
  // }
  //// end of main calculation

  // set new velocity components
  if (swap)
  {
    P_VEL_R(pa) = v_b_bar[0];
    P_VEL_PHI(pa) = v_b_bar[1];
    P_VEL_Z(pa) = v_b_bar[2];

    P_VEL_R(pb) = v_a_bar[0];
    P_VEL_PHI(pb) = v_a_bar[1];
    P_VEL_Z(pb) = v_a_bar[2];
  }
  else
  {
    P_VEL_R(pa) = v_a_bar[0];
    P_VEL_PHI(pa) = v_a_bar[1];
    P_VEL_Z(pa) = v_a_bar[2];

    P_VEL_R(pb) = v_b_bar[0];
    P_VEL_PHI(pb) = v_b_bar[1];
    P_VEL_Z(pb) = v_b_bar[2];
  }

  // if ( v_a_bar.length2() > LIGHT_VEL_POW_2 / 10 / 10
  //      || v_b_bar.length2() > LIGHT_VEL_POW_2 / 10 / 10)
  //   LOG_S(ERROR) << "OH SHI! " << v_a[0] << " " << v_a[1] << " " << v_a[2] << " => "
  //                << v_a_bar[0] << " " << v_a_bar[1] << " " << v_a_bar[2] << " -- "
  //                << charge_a << " " << mass_a << "; "
  //                << v_b[0] << " " << v_b[1] << " " << v_b[2] << " => "
  //                << v_b_bar[0] << " " << v_b_bar[1] << " " << v_b_bar[2] << " -- "
  //                << charge_b << " " << mass_b << "; "
  //                << "vels: " << lib::sq_rt(p_a.length2() + p_b.length2()) << " => "
  //                << lib::sq_rt(p_a_bar.length2() + p_b_bar.length2());
}

void CollisionsSentokuM::collide ()
{
  // pairing
  for (int i = 0; i < geometry->r_grid_amount; ++i)
    for (int j = 0; j < geometry->z_grid_amount; ++j)
    {
      unsigned int vec_size_ions = map_ion2cell(i, j).size();
      unsigned int vec_size_electrons = map_el2cell(i, j).size();

      // TA77: case 1a
      // ions
      if (vec_size_ions % 2 == 0)
        for (unsigned int k = 0; k < vec_size_ions; k = k + 2)
          collide_single(i, j, PROTON_MASS, PROTON_MASS,
                         (*map_ion2cell(i, j)[k]),
                         (*map_ion2cell(i, j)[k+1]));
      // electrons
      if (vec_size_electrons % 2 == 0)
        for (unsigned int k = 0; k < vec_size_electrons; k = k + 2)
          collide_single(i, j, EL_MASS, EL_MASS,
                         (*map_el2cell(i, j)[k]),
                         (*map_el2cell(i, j)[k+1]));

      // TA77: case 1b
      // ions
      if (vec_size_ions % 2 != 0)
      {
        if (vec_size_ions >= 3)
        {
          // first 3 collisions in special way
          collide_single(i, j, PROTON_MASS, PROTON_MASS,
                         (*map_ion2cell(i, j)[0]),
                         (*map_ion2cell(i, j)[1]));
          collide_single(i, j, PROTON_MASS, PROTON_MASS,
                         (*map_ion2cell(i, j)[1]),
                         (*map_ion2cell(i, j)[2]));
          collide_single(i, j, PROTON_MASS, PROTON_MASS,
                         (*map_ion2cell(i, j)[2]),
                         (*map_ion2cell(i, j)[0]));
        }
        if (vec_size_ions >= 5)
          for (unsigned int k = 3; k < vec_size_ions; k = k + 2)
            collide_single(i, j, PROTON_MASS, PROTON_MASS,
                           (*map_ion2cell(i, j)[k]),
                           (*map_ion2cell(i, j)[k+1]));
      }
      // TA77: electrons, case 1b
      // electrons
      if (vec_size_electrons % 2 != 0)
      {
        if (vec_size_electrons >= 3)
        {
          // first 3 collisions in special way
          collide_single(i, j, EL_MASS, EL_MASS,
                         (*map_el2cell(i, j)[0]),
                         (*map_el2cell(i, j)[1]));
          collide_single(i, j, EL_MASS, EL_MASS,
                         (*map_el2cell(i, j)[1]),
                         (*map_el2cell(i, j)[2]));
          collide_single(i, j, EL_MASS, EL_MASS,
                         (*map_el2cell(i, j)[2]),
                         (*map_el2cell(i, j)[0]));
        }
        if (vec_size_electrons >= 5)
          for (unsigned int k = 3; k < vec_size_electrons; k = k + 2)
            collide_single(i, j, EL_MASS, EL_MASS,
                           (*map_el2cell(i, j)[k]),
                           (*map_el2cell(i, j)[k+1]));
      }

      // TA77: case 2a. electrons-ions
      if (vec_size_ions == vec_size_electrons)
        for (unsigned int k = 0; k < vec_size_electrons; ++k)
          collide_single(i, j, EL_MASS, PROTON_MASS,
                         (*map_el2cell(i, j)[k]),
                         (*map_ion2cell(i, j)[k]));

      // TA77: case 2b. electrons-ions
      if (vec_size_ions > vec_size_electrons && vec_size_electrons > 0 && vec_size_ions > 0)
      {
        unsigned int c_i = floor( (float)vec_size_ions / (float)vec_size_electrons );
        double c_r = (float)vec_size_ions / (float)vec_size_electrons - c_i;

        int ions_1st_group = (c_i + 1) * c_r * vec_size_electrons;
        int els_1st_group = c_r * vec_size_electrons;

        int ions_2nd_group = c_i * (1 - c_r) * vec_size_electrons;
        // int els_2nd_group = (1 - c_r) * vec_size_electrons;

        // TA77: case 2b, 1st group, ions
        for (unsigned int fgi = 0; fgi < ions_1st_group; ++fgi)
        {
          unsigned int fge = floor(float(fgi) / float(c_i+1));
          collide_single(i, j, EL_MASS, PROTON_MASS,
                         (*map_el2cell(i, j)[fge]),
                         (*map_ion2cell(i, j)[fgi]));
        }
        // TA77: case 2b, 2nd group, ions
        for (unsigned int fgi = 0; fgi < ions_2nd_group; ++fgi)
        {
          unsigned int fge = floor(float(fgi) / float(c_i));
          collide_single(i, j, EL_MASS, PROTON_MASS,
                         (*map_el2cell(i, j)[fge+els_1st_group]),
                         (*map_ion2cell(i, j)[fgi+ions_1st_group]));
        }
      }
      // TA77: case 2b. electrons-ions
      if (vec_size_ions < vec_size_electrons && vec_size_electrons > 0 && vec_size_ions > 0)
      {
        double c_i = floor( (float)vec_size_electrons / (float)vec_size_ions );
        double c_r = (float)vec_size_electrons / (float)vec_size_ions - c_i;

        int els_1st_group = (c_i + 1) * c_r * vec_size_ions;
        int ions_1st_group = c_r * vec_size_ions;

        int els_2nd_group = c_i * (1 - c_r) * vec_size_ions;
        // int ions_2nd_group = (1 - c_r) * vec_size_ions;

        // TA77: case 2b, 1st group, electrons
        for (unsigned int fge = 0; fge < els_1st_group; ++fge)
        {
          unsigned int fgi = floor(float(fge) / float(c_i+1));
          // MSG("els 1st:  FGE " << fge << " FGI " << fgi << " VSI " << vec_size_ions << " VSE " << vec_size_electrons);
          collide_single(i, j, EL_MASS, PROTON_MASS,
                         (*map_el2cell(i, j)[fge]),
                         (*map_ion2cell(i, j)[fgi]));
        }
        // TA77: case 2b, 2nd group, electrons
        for (unsigned int fge = 0; fge < els_2nd_group; ++fge)
        {
          unsigned int fgi = floor(float(fge) / float(c_i));
          // MSG("els 2nd:  FGE " << fge << " FGI " << fgi << " VSI " << vec_size_ions << " VSE " << vec_size_electrons);
          collide_single(i, j, EL_MASS, PROTON_MASS,
                         (*map_el2cell(i, j)[fge+els_1st_group]),
                         (*map_ion2cell(i, j)[fgi+ions_1st_group]));
        }
      }
    }
}

// void CollisionsSentokuM::correct_velocities()
// {
//   // TITU18: correct velocities
//   for (int i = 0; i < geometry->r_grid_amount; ++i)
//     for (int j = 0; j < geometry->z_grid_amount; ++j)
//     {
//       unsigned int vec_size_ions = map_ion2cell(i, j).size();
//       unsigned int vec_size_electrons = map_el2cell(i, j).size();

//       // TITU18: calculate delta V
//       //// calculate summary moment components and total energy for t+delta_t
//       double moment_new_r_ion = 0, moment_new_phi_ion = 0, moment_new_z_ion = 0,
//         moment_new_r_el = 0, moment_new_phi_el = 0, moment_new_z_el = 0,
//         E_tot_ion_new = 0, E_tot_el_new = 0;

//       for (unsigned int k = 0; k < vec_size_ions; ++k)
//       {
//         double vr = P_VEL_R((*map_ion2cell(i, j)[k]));
//         double vphi = P_VEL_PHI((*map_ion2cell(i, j)[k]));
//         double vz = P_VEL_Z((*map_ion2cell(i, j)[k]));
//         double v_sq = vr*vr + vphi*vphi + vz*vz;

//         double mass = P_MASS((*map_ion2cell(i, j)[k]));
//         double weight = mass / PROTON_MASS;
//         double weighted_m = weight * mass;

//         moment_new_r_ion += weighted_m * vr;
//         moment_new_phi_ion += weighted_m * vphi;
//         moment_new_z_ion += weighted_m * vz;
//         E_tot_ion_new += weighted_m * v_sq / 2;
//       }

//       for (unsigned int k = 0; k < vec_size_electrons; ++k)
//       {
//         double vr = P_VEL_R((*map_el2cell(i, j)[k]));
//         double vphi = P_VEL_PHI((*map_el2cell(i, j)[k]));
//         double vz = P_VEL_Z((*map_el2cell(i, j)[k]));
//         double v_sq = vr*vr + vphi*vphi + vz*vz;

//         double mass = P_MASS((*map_el2cell(i, j)[k]));
//         double weight = mass / EL_MASS;
//         double weighted_m = weight * mass;

//         moment_new_r_el += weighted_m * vr;
//         moment_new_phi_el += weighted_m * vphi;
//         moment_new_z_el += weighted_m * vz;
//         E_tot_el_new += weighted_m * v_sq / 2;
//       }

//       //// calculate delta V components and delta E total
//       double delta_V_r_ion = (moment_new_r_ion - moment_tot_ion[0](i, j)) / mass_tot_ion(i, j);
//       double delta_V_phi_ion = (moment_new_phi_ion - moment_tot_ion[1](i, j)) / mass_tot_ion(i, j);
//       double delta_V_z_ion = (moment_new_z_ion - moment_tot_ion[2](i, j)) / mass_tot_ion(i, j);
//       double delta_V_r_el = (moment_new_r_el - moment_tot_el[0](i, j)) / mass_tot_el(i, j);
//       double delta_V_phi_el = (moment_new_phi_el - moment_tot_el[1](i, j)) / mass_tot_el(i, j);
//       double delta_V_z_el = (moment_new_z_el - moment_tot_el[2](i, j)) / mass_tot_el(i, j);

//       //// calculate V_0 components
//       double V_0_r_ion = moment_tot_ion[0](i, j) / mass_tot_ion(i, j);
//       double V_0_phi_ion = moment_tot_ion[1](i, j) / mass_tot_ion(i, j);
//       double V_0_z_ion = moment_tot_ion[2](i, j) / mass_tot_ion(i, j);
//       double V_0_r_el = moment_tot_el[0](i, j) / mass_tot_el(i, j);
//       double V_0_phi_el = moment_tot_el[1](i, j) / mass_tot_el(i, j);
//       double V_0_z_el = moment_tot_el[2](i, j) / mass_tot_el(i, j);
//       //// calculate delta E total
//       double delta_E_tot_ion = E_tot_ion_new - energy_tot_ion(i, j);
//       double delta_E_tot_el = E_tot_el_new - energy_tot_el(i, j);
//       //// link E total to local vars
//       double E_tot_ion = energy_tot_ion(i, j);
//       double E_tot_el = energy_tot_el(i, j);
//       double V_0_sq_ion = V_0_r_ion*V_0_r_ion + V_0_phi_ion*V_0_phi_ion + V_0_z_ion*V_0_z_ion;
//       double delta_V_sq_ion = delta_V_r_ion*delta_V_r_ion
//         + delta_V_phi_ion*delta_V_phi_ion
//         + delta_V_z_ion*delta_V_z_ion;
//       double V_0_sq_el = V_0_r_el*V_0_r_el + V_0_phi_el*V_0_phi_el + V_0_z_el*V_0_z_el;
//       double delta_V_sq_el = delta_V_r_el*delta_V_r_el
//         + delta_V_phi_el*delta_V_phi_el
//         + delta_V_z_el*delta_V_z_el;
//       /// calculate alpha
//       double alpha_ion =
//         ( E_tot_ion - mass_tot_ion(i, j) * V_0_sq_ion / 2 )
//         / ( E_tot_ion
//             + delta_E_tot_ion
//             - mass_tot_ion(i, j)
//             * ( pow(V_0_r_ion + delta_V_r_ion, 2)
//                 + pow(V_0_phi_ion + delta_V_phi_ion, 2)
//                 + pow(V_0_z_ion + delta_V_z_ion, 2)
//               )
//             / 2
//           );
//       double alpha_el =
//         ( E_tot_el - mass_tot_el(i, j) * V_0_sq_el / 2 )
//         / ( E_tot_el
//             + delta_E_tot_el
//             - mass_tot_el(i, j)
//             * ( pow(V_0_r_el + delta_V_r_el, 2)
//                 + pow(V_0_phi_el + delta_V_phi_el, 2)
//                 + pow(V_0_z_el + delta_V_z_el, 2)
//               )
//             / 2
//           );

//       // correct ion velocity
//       if (isnormal(alpha_ion)) // FIXME: sometimes E_tot_ion = mass_tot_ion(i, j) * V_0_sq_ion / 2
//       {
//         // FIXME: quick and dirty workadound
//         // caused negative values under squared root
//         // and zeros
//         if (alpha_ion < 0) alpha_ion = -alpha_ion;
//         if (alpha_ion == 0) alpha_ion = 1;
//         alpha_ion = lib::sq_rt(alpha_ion);

//         for (unsigned int k = 0; k < vec_size_ions; ++k)
//         {
//           double vr = P_VEL_R((*map_ion2cell(i, j)[k]));
//           double vphi = P_VEL_PHI((*map_ion2cell(i, j)[k]));
//           double vz = P_VEL_Z((*map_ion2cell(i, j)[k]));

//           double vr_corr = V_0_r_ion + alpha_ion * (vr - V_0_r_ion - delta_V_r_ion);
//           double vphi_corr = V_0_phi_ion + alpha_ion * (vphi - V_0_phi_ion - delta_V_phi_ion);
//           double vz_corr = V_0_z_ion + alpha_ion * (vz - V_0_z_ion - delta_V_z_ion);

//           P_VEL_R((*map_ion2cell(i, j)[k])) = vr_corr;
//           P_VEL_PHI((*map_ion2cell(i, j)[k])) = vphi_corr;
//           P_VEL_Z((*map_ion2cell(i, j)[k])) = vz_corr;
//         }
//       }

//       // correct electron velocity
//       if (isnormal(alpha_el)) // FIXME: sometimes E_tot_el = mass_tot_el(i, j) * V_0_sq_el / 2
//       {
//         // FIXME: quick and dirty workadound
//         // caused negative values under squared root
//         // and zeros
//         if (alpha_el < 0) alpha_el = -alpha_el;
//         if (alpha_el == 0) alpha_el = 1;
//         alpha_el = lib::sq_rt(alpha_el);

//         for (unsigned int k = 0; k < vec_size_electrons; ++k)
//         {
//           double vr = P_VEL_R((*map_el2cell(i, j)[k]));
//           double vphi = P_VEL_PHI((*map_el2cell(i, j)[k]));
//           double vz = P_VEL_Z((*map_el2cell(i, j)[k]));

//           double vr_corr = V_0_r_el + alpha_el * (vr - V_0_r_el - delta_V_r_el);
//           double vphi_corr = V_0_phi_el + alpha_el * (vphi - V_0_phi_el - delta_V_phi_el);
//           double vz_corr = V_0_z_el + alpha_el * (vz - V_0_z_el - delta_V_z_el);

//           P_VEL_R((*map_el2cell(i, j)[k])) = vr_corr;
//           P_VEL_PHI((*map_el2cell(i, j)[k])) = vphi_corr;
//           P_VEL_Z((*map_el2cell(i, j)[k])) = vz_corr;
//         }
//       }
//     }

// }

void CollisionsSentokuM::run ()
{
  clear();
  sort_to_cells();
  random_sort();
  collect_weighted_params_tot_grid();
  collide();
  // correct_velocities();
}
