#include "current.hpp"

Current::Current(Geometry *geom, TimeSim *t, vector<SpecieP *> species) : geometry(geom), time(t)
{
  current = Grid3D<double> (geometry->r_grid_amount+1, geometry->z_grid_amount+1);
  species_p = species;

  current.setall(0);
}

// Current::~Current()
// {
//   MSG("BBB ");
// }

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Current::simple_current_distribution(double radius_new,
                                          double longitude_new,
                                          double radius_old,
                                          double longitude_old,
                                          int i_n,
                                          int k_n,
                                          double p_charge)
{
  double dr = geometry->r_cell_size;
  double dz = geometry->z_cell_size;
  double wj = 0;
  double delta_t = time->step;

  // distance of particle moving //
  double delta_r = radius_new - radius_old;
  double delta_z = longitude_new - longitude_old;

  double some_shit_density;

  if ((abs(delta_r) < MNZL) || (abs(delta_z) < MNZL)) // MNZL see constant.h
    return;
  // if i cell is not equal 0
  if (i_n >= 1)
  {
    // equation y = k*x + b; //
    // finding k & b //
    double k = delta_r / delta_z;
    double b = radius_old;

    // calculate current jz in [i,k] cell
    some_shit_density = SOME_SHIT_DENSITY_R(p_charge, i_n * dr, dr, dz, delta_t);

    wj = some_shit_density
      * (dr * delta_z - k * delta_z * delta_z / 2. - delta_z * b + dr * dr / k
         * ((i_n + 0.5) * (i_n + 0.5) - 0.25) * log((k * delta_z + b) / b));
    // set new weighting current value
    current[2].inc(i_n, k_n, wj);

    some_shit_density = SOME_SHIT_DENSITY_R(p_charge, (i_n + 1) * dr, dr, dz, delta_t);

    // calculate current in [i+1,k] cell //
    wj = some_shit_density
      * (k * delta_z * delta_z / 2. + delta_z * b + delta_z * dr + dr * dr / k *
         (0.25-(i_n + 0.5) * (i_n + 0.5)) * log((k * delta_z + b) / b));
    // set new weighting current value
    current[2].inc(i_n+1, k_n, wj);

    ////////////////////////////////// /
    // calculate current jr in [i,k] cell //
    // equation y = k * x + b; //
    // finding k & b //
    k = -delta_z / delta_r;
    double r0 = (i_n + 0.5) * dr;
    double r1 =  radius_old;
    b = (k_n + 1.) * dz - longitude_old;

    // weighting jr in [i][k] cell
    some_shit_density = SOME_SHIT_DENSITY_Z(p_charge, r0, dr, dz, delta_t);

    wj = some_shit_density
      * (r0 * k * delta_r + k / 2. * delta_r * (radius_old + delta_r / 2.)
         + 0.5 * delta_r * (b - k * (2 * r0 + r1))
         + delta_r * (b-k * r1) * (4 * r0 * r0-dr * dr)
         / (8 * radius_old * (radius_old + delta_r)) +
         (k * (r0 * r0 / 2.-dr * dr / 8.))
         * log((radius_old + delta_r) / radius_old));
    current[0].inc(i_n,k_n, wj);

    b = longitude_old - k_n * dz;
    // weighting jr in [i][k+1] cell
    wj = some_shit_density
      * (-r0 * k * delta_r - k / 2. * delta_r * (radius_old + delta_r / 2.)
         + 0.5 * delta_r * (b + k * (2 * r0 + r1))
         + delta_r * (b + k * r1) * (4 * r0 * r0 - dr * dr)
         / (8 * radius_old * (radius_old + delta_r))
         - (k * (r0 * r0 / 2.-dr * dr / 8.))
         * log((radius_old + delta_r) / radius_old));
    current[0].inc(i_n, k_n+1, wj);
  }
  // if i cell is equal 0
  else
  {
    // equation y = k * x + b; //
    // finding k & b //
    double k = delta_r / delta_z;
    double b = radius_old;
    // calculate current jz in [i,k] cell //
    some_shit_density = p_charge
      / (2. * PI * dr / 4. * dr * dz
         * delta_t
         * dr);

    wj = some_shit_density
      * (dr * delta_z - k * delta_z * delta_z / 2. - delta_z * b );
    // set new weighting current value
    current[2].inc(i_n, k_n, wj);

    // calculate current in [i + 1,k] cell //
    some_shit_density = SOME_SHIT_DENSITY_R(p_charge, dr, dr, dz, delta_t);

    wj = some_shit_density
      * (k * delta_z * delta_z / 2. + delta_z * dr + delta_z * b);
    // set new weighting current value
    current[2].inc(i_n + 1,k_n, wj);

    ////////////////////////////////// /
    // calculate current jr in [i,k] cell //
    // equation y = k * x + b; //
    // finding k & b //
    k = -delta_z / delta_r;
    double r0 = (i_n + 0.5) * dr;
    double r1 = radius_old;
    b= (k_n + 1.) * dz - longitude_old;

    // weighting jr in [i][k] cell
    some_shit_density = SOME_SHIT_DENSITY_Z(p_charge, r0, dr, dz, delta_t);

    wj = some_shit_density
      * (r0 * k * delta_r + k / 2. * delta_r * (radius_old + delta_r / 2.)
         + 0.5 * delta_r * (b-k * (2 * r0 + r1))
         + delta_r * (b-k * r1) * (4 * r0 * r0-dr * dr)
         / (8 * radius_old * (radius_old + delta_r))
         + (k * (r0 * r0 / 2.-dr * dr / 8.))
         * log((radius_old + delta_r) / radius_old));
    current[0].inc(i_n,k_n, wj);

    b = longitude_old- k_n * dz;
    // weighting jr in [i][k + 1] cell
    // some_shit_density = SOME_SHIT_DENSITY_Z(charge_array[p_number], r0, dr, dz, delta_t);

    wj = some_shit_density
      * (-r0 * k * delta_r - k / 2. * delta_r * (radius_old + delta_r / 2.)
         + 0.5 * delta_r * (b + k * (2 * r0 + r1))
         + delta_r * (b + k * r1) * (4 * r0 * r0-dr * dr)
         / (8 * radius_old * (radius_old + delta_r))
         - (k * (r0 * r0 / 2.-dr * dr / 8.)) *
         log((radius_old + delta_r) / radius_old));
    current[0].inc(i_n, k_n + 1, wj);
  }
}

void Current::current_distribution()
{
  double dr = geometry->r_cell_size;
  double dz = geometry->z_cell_size;

  // for (unsigned int i = 0;i < number;i++)
  for (auto ps = species_p.begin(); ps != species_p.end(); ++ps)
    for (auto i = (**ps).particles.begin(); i != (**ps).particles.end(); ++i)
      if (P_ALIVE((**i)) == 1)
      {
        // finding number new and old cells
        int i_n = CELL_NUMBER(P_POS_R((**i)), dr);
        int k_n = CELL_NUMBER(P_POS_Z((**i)), dz);
        int i_o = CELL_NUMBER(P_POS_OLD_R((**i)), dr);
        int k_o = CELL_NUMBER(P_POS_OLD_Z((**i)), dz);
        // TODO: workaround: sometimes it gives -1.
        // Just get 0 cell if it happence
        if (i_n < 0) i_n = 0;
        if (k_n < 0) k_n = 0;
        if (i_o < 0) i_o = 0;
        if (k_o < 0) k_o = 0;

        if (P_POS_OLD_R((**i)) == (i_o + 1) * dr) i_o = i_n;
        if (P_POS_OLD_Z((**i)) == (k_o + 1) * dz) k_o = k_n;
        if (P_POS_R((**i)) == (i_n + 1) * dr) i_n = i_o;
        if (P_POS_Z((**i)) == (k_n + 1) * dz) k_n = k_o;

        int res_cell = abs(i_n - i_o) + abs(k_n - k_o);

        if ((abs(P_POS_R(i) - P_POS_OLD_R(i)) < MNZL)
            || (abs(P_POS_Z(i) - P_POS_OLD_Z(i)) < MNZL))
          strict_motion_weighting (P_POS_R((**i)), P_POS_Z((**i)),
                                   P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)),
                                   P_CHARGE((**i)));
        else
        {
          switch (res_cell)
          {
            // 1) charge in four nodes
          case 0: simple_current_distribution (P_POS_R((**i)), P_POS_Z((**i)),
                                               P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)),
                                               i_n, k_n, P_CHARGE((**i)));
            break;
            // 2) charge in 7 nodes
          case 1:
          {
            // charge in 7 nodes. Moving on r-axis (i_new != i_old)
            if ((i_n != i_o) && (k_n == k_o))
            {
              // moving to center from outer to innter cell
              if (P_POS_OLD_R((**i)) > (i_n + 1) * dr)
              {
                double a = (P_POS_OLD_R((**i)) - P_POS_R((**i))) / (P_POS_OLD_Z((**i)) - P_POS_Z((**i)));
                double r_boundary = (i_n + 1) * dr;
                double delta_r = r_boundary - P_POS_R((**i));
                double z_boundary = P_POS_Z((**i)) + delta_r / a;

                simple_current_distribution(r_boundary, z_boundary,
                                            P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n + 1, k_n, P_CHARGE((**i)));
                simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)),
                                            r_boundary, z_boundary, i_n, k_n, P_CHARGE((**i)));
              }
              // moving to wall
              else
              {
                double a = (P_POS_R((**i)) - P_POS_OLD_R((**i))) / (P_POS_Z((**i)) - P_POS_OLD_Z((**i)));
                double r_boundary = (i_n) * dr;
                double delta_r = r_boundary - P_POS_OLD_R((**i));
                double z_boundary = P_POS_OLD_Z((**i)) + delta_r / a;

                simple_current_distribution(r_boundary, z_boundary, P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n-1, k_n, P_CHARGE((**i)));
                simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r_boundary, z_boundary, i_n, k_n, P_CHARGE((**i)));
              }
            }
            // charge in seven cells. Moving on z-axis (k_new != k_old)
            else if ((i_n == i_o) && (k_n != k_o))
            {
              // moving forward from N to N + 1 cell
              if (P_POS_OLD_Z((**i)) < k_n * dz)
              {
                double z_boundary = k_n * dz;
                double delta_z = z_boundary - P_POS_OLD_Z((**i));
                double a = (P_POS_R((**i)) - P_POS_OLD_R((**i))) / (P_POS_Z((**i)) - P_POS_OLD_Z((**i)));
                double r_boundary = P_POS_OLD_R((**i)) + a * delta_z;
                simple_current_distribution(r_boundary, z_boundary ,P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n, k_n-1, P_CHARGE((**i)));
                simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r_boundary, z_boundary, i_n, k_n, P_CHARGE((**i)));
              }
              // moving backward
              else
              {
                double z_boundary = (k_n + 1) * dz;
                double delta_z = z_boundary - P_POS_Z((**i));
                double a = (P_POS_OLD_R((**i)) - P_POS_R((**i))) / (P_POS_OLD_Z((**i)) - P_POS_Z((**i)));
                double r_boundary = P_POS_R((**i)) + a * delta_z;
                simple_current_distribution(r_boundary, z_boundary, P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n, k_n + 1, P_CHARGE((**i)));
                simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r_boundary, z_boundary, i_n, k_n, P_CHARGE((**i)));
              }
            }
          }
          break;
          // 3) charge in 10 nodes
          case 2:
          {
            // moving forward
            if (i_o < i_n)
            {
              // case, when particle move from [i-1][k-1] -> [i][k] cell
              if(k_o < k_n)
              {
                double a = (P_POS_R((**i)) - P_POS_OLD_R((**i))) / (P_POS_Z((**i)) - P_POS_OLD_Z((**i)));
                double r1 = i_n * dr;
                double delta_z1 = (r1 - P_POS_OLD_R((**i))) / a;
                double z1 = P_POS_OLD_Z((**i)) + delta_z1;
                double z2 = k_n * dz;
                double delta_r2 = (z2 - P_POS_OLD_Z((**i))) * a;
                double r2 = P_POS_OLD_R((**i)) + delta_r2;
                if (z1 < k_n * dz)
                {
                  simple_current_distribution(r1, z1 ,P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n-1, k_n-1, P_CHARGE((**i)));
                  simple_current_distribution(r2, z2, r1, z1, i_n, k_n-1, P_CHARGE((**i)));
                  simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r2, z2, i_n, k_n, P_CHARGE((**i)));
                }
                else if (z1>k_n * dz)
                {
                  simple_current_distribution(r2, z2 ,P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n-1, k_n-1, P_CHARGE((**i)));
                  simple_current_distribution(r1, z1, r2, z2,i_n-1, k_n, P_CHARGE((**i)));
                  simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r1, z1, i_n, k_n, P_CHARGE((**i)));
                }
              }
              // case, when particle move from [i-1][k + 1] -> [i][k] cell
              else
              {
                double a = (P_POS_R((**i)) - P_POS_OLD_R((**i))) / (P_POS_Z((**i)) - P_POS_OLD_Z((**i)));
                double r1 = i_n * dr;
                double delta_z1 = (r1 - P_POS_OLD_R((**i))) / a;
                double z1 = P_POS_OLD_Z((**i)) + delta_z1;

                double z2 = (k_n + 1) * dz;
                double delta_r2 = -(P_POS_OLD_Z((**i))-z2) * a;
                double r2 = P_POS_OLD_R((**i)) + delta_r2;
                if (z1>(k_n + 1) * dz)
                {
                  simple_current_distribution(r1, z1 ,P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n-1, k_n + 1, P_CHARGE((**i)));
                  simple_current_distribution(r2, z2, r1, z1, i_n, k_n + 1,P_CHARGE((**i)));
                  simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r2, z2, i_n, k_n, P_CHARGE((**i)));
                }
                else if (z1<(k_n + 1) * dz)
                {
                  simple_current_distribution(r2, z2 ,P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n-1, k_n + 1, P_CHARGE((**i)));
                  simple_current_distribution(r1, z1, r2, z2,i_n-1, k_n, P_CHARGE((**i)));
                  simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r1, z1, i_n, k_n, P_CHARGE((**i)));
                }
              }
            }
            // case, when particle move from [i + 1] cell to [i] cell
            else if (i_o > i_n)
            {
              // case, when particle move from [i + 1][k-1] -> [i][k] cell
              if(k_o<k_n)
              {
                double a = (P_POS_R((**i)) - P_POS_OLD_R((**i))) / (P_POS_Z((**i)) - P_POS_OLD_Z((**i)));
                double r1 = (i_n + 1) * dr;
                double delta_z1 = -(P_POS_OLD_R((**i))-r1) / a;
                double z1 = P_POS_OLD_Z((**i)) + delta_z1;

                double z2 = k_n * dz;
                double delta_r2 = -(z2-P_POS_OLD_Z((**i))) * a;
                double r2 = P_POS_OLD_R((**i))- delta_r2;

                if (z1<(k_n) * dz)
                {
                  simple_current_distribution(r1, z1 ,P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n + 1, k_n-1, P_CHARGE((**i)));
                  simple_current_distribution(r2, z2, r1, z1, i_n, k_n-1, P_CHARGE((**i)));
                  simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r2, z2, i_n, k_n, P_CHARGE((**i)));
                }
                else if (z1>(k_n) * dz)
                {
                  simple_current_distribution(r2, z2 ,P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n + 1, k_n-1, P_CHARGE((**i)));
                  simple_current_distribution(r1, z1, r2, z2,i_n + 1, k_n, P_CHARGE((**i)));
                  simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r1, z1, i_n, k_n, P_CHARGE((**i)));
                }

              }
              // case, when particle move from [i + 1][k + 1] -> [i][k] cell
              else if (k_o>k_n)
              {
                double a = (P_POS_OLD_R((**i))-P_POS_R((**i))) / (P_POS_OLD_Z((**i))-P_POS_Z((**i)));
                double r1 = (i_n + 1) * dr;
                double delta_z1 = (r1-P_POS_R((**i))) / a;
                double z1 = P_POS_Z((**i)) + delta_z1;

                double z2 = (k_n + 1) * dz;
                double delta_r2 = (z2-P_POS_Z((**i))) * a;
                double r2 = P_POS_R((**i)) + delta_r2;

                if (z1>(k_n + 1) * dz)
                {
                  simple_current_distribution(r1, z1, P_POS_OLD_R((**i)),P_POS_OLD_Z((**i)), i_n + 1, k_n + 1, P_CHARGE((**i)));
                  simple_current_distribution(r2, z2, r1, z1, i_n, k_n + 1, P_CHARGE((**i)));
                  simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r2, z2, i_n, k_n, P_CHARGE((**i)));
                }
                else if (z1<(k_n + 1) * dz)
                {
                  simple_current_distribution(r2, z2, P_POS_OLD_R((**i)), P_POS_OLD_Z((**i)), i_n + 1, k_n + 1, P_CHARGE((**i)));
                  simple_current_distribution(r1, z1, r2, z2,i_n + 1, k_n, P_CHARGE((**i)));
                  simple_current_distribution(P_POS_R((**i)), P_POS_Z((**i)), r1, z1, i_n, k_n, P_CHARGE((**i)));
                }
              }
            }
          }
          break;
          }
        }
      }
}

void Current::azimuthal_current_distribution()
{
  double dr = geometry->r_cell_size;
  double dz = geometry->z_cell_size;

  for (auto ps = species_p.begin(); ps != species_p.end(); ++ps)
    for (auto i = (**ps).particles.begin(); i != (**ps).particles.end(); ++i)
      if (P_ALIVE((**i)))
      {
        int r_i = 0;  // number of particle i cell
        int z_k = 0;  // number of particle k cell

        double r1, r2, r3; // temp variables for calculation
        double dz1, dz2;  // temp var.: width of k and k + 1 cell

        double ro_v = 0; // charge density Q / V, V - volume of particle
        double v_1 = 0; // volume of [i][k] cell
        double v_2 = 0; // volume of [i + 1][k] cell
        // double ro_v_2=0; // charge density in i + 1 cell

        double rho = 0; // charge density in cell
        double curr; // j_phi in cell
        // double * * temp = this_j->get_j_phi();

        // finding number of i and k cell. example: dr = 0.5; r = 0.4; i =0
        r_i = CELL_NUMBER(P_POS_R((**i)), dr);
        z_k = CELL_NUMBER(P_POS_Z((**i)), dz);
        // TODO: workaround: sometimes it gives -1.
        // Just get 0 cell if it happence
        if (r_i < 0) r_i = 0;
        if (z_k < 0) z_k = 0;

        // in first cell other alg. of ro_v calc
        if(P_POS_R((**i)) > dr)
        {
          r1 = P_POS_R((**i)) - 0.5 * dr;
          r2 = (r_i + 0.5) * dr;
          r3 = P_POS_R((**i)) + 0.5 * dr;
          ro_v = P_CHARGE((**i)) / (2. * PI * dz * dr * P_POS_R((**i)));
          v_1 = CELL_VOLUME(r_i, dr, dz);
          v_2 = CELL_VOLUME(r_i + 1, dr, dz);
          dz1 = (z_k + 0.5) * dz - (P_POS_Z((**i)) - 0.5 * dz);
          dz2 = (P_POS_Z((**i)) + 0.5 * dz) - (z_k + 0.5) * dz;

          // weighting in j[i][k] cell
          rho = ro_v * CYL_RNG_VOL(dz1, r1, r2) / v_1;
          curr = rho * P_VEL_PHI((**i));
          current[1].inc(r_i, z_k, curr);

          // weighting in j[i + 1][k] cell
          rho = ro_v * CYL_RNG_VOL(dz1, r2, r3) / v_2;
          curr = rho * P_VEL_PHI((**i));
          current[1].inc(r_i + 1,z_k, curr);

          // weighting in j[i][k + 1] cell
          rho = ro_v * CYL_RNG_VOL(dz2, r1, r2) / v_1;
          curr = rho * P_VEL_PHI((**i));
          current[1].inc(r_i, z_k + 1, curr);

          // weighting in j[i + 1][k + 1] cell
          rho = ro_v * CYL_RNG_VOL(dz2, r2, r3) / v_2;
          curr = rho * P_VEL_PHI((**i));
          current[1].inc(r_i + 1, z_k + 1, curr);

        }
        else
        {
          r1 = P_POS_R((**i)) - 0.5 * dr;
          r2 = (r_i + 0.5) * dr;
          r3 = P_POS_R((**i)) + 0.5 * dr;
          dz1 = (z_k + 0.5) * dz - (P_POS_Z((**i)) - 0.5 * dz);
          dz2 = (P_POS_Z((**i)) + 0.5 * dz) - (z_k + 0.5) * dz;
          ro_v = P_CHARGE((**i)) / (2. * PI * dz * dr * P_POS_R((**i)));
          v_1 = CYL_VOL(dz, dr);
          v_2 = CELL_VOLUME(r_i + 1, dr, dz);

          // weighting in j[i][k] cell
          rho = ro_v * CYL_RNG_VOL(dz1, r1, r2) / v_1;
          curr = rho * P_VEL_PHI((**i));
          current[1].inc(r_i, z_k, curr);

          // weighting in j[i + 1][k] cell
          rho = ro_v * CYL_RNG_VOL(dz1, r2, r3) / v_2;
          curr = rho * P_VEL_PHI((**i));
          current[1].inc(r_i + 1,z_k, curr);

          // weighting in j[i][k + 1] cell
          rho = ro_v * CYL_RNG_VOL(dz2, r1, r2) / v_1;
          curr = rho * P_VEL_PHI((**i));
          current[1].inc(r_i, z_k + 1, curr);

          // weighting in j[i + 1][k + 1] cell
          rho = ro_v * CYL_RNG_VOL(dz2, r2, r3) / v_2;
          curr = rho * P_VEL_PHI((**i));
          current[1].inc(r_i + 1, z_k + 1, curr);
        }
      }
}

void Current::strict_motion_weighting(double radius_new,
                                      double longitude_new,
                                      double radius_old,
                                      double longitude_old,
                                      double p_charge)
{
  double dr = geometry->r_cell_size;
  double dz = geometry->z_cell_size;

  // defining number of cell
  int i_n = CELL_NUMBER(radius_new, dr);
  int k_n = CELL_NUMBER(longitude_new, dz);
  int i_o = CELL_NUMBER(radius_old, dr);;
  int k_o = CELL_NUMBER(longitude_old, dz);;
  // TODO: workaround: sometimes it gives -1.
  // Just get 0 cell if it happence
  if (i_n < 0) i_n = 0;
  if (k_n < 0) k_n = 0;
  if (i_o < 0) i_o = 0;
  if (k_o < 0) k_o = 0;

  if ((abs(radius_new - radius_old) < MNZL)
      && (abs(longitude_new - longitude_old) < MNZL))
    return;

  // stirct axis motion
  if (abs(radius_new - radius_old) < MNZL)
  {
    double delta_z = 0.;
    double value_part = 2. * PI * radius_new * dr * dz;
    double wj, wj_lower, wj_upper;
    double r1 = radius_new - 0.5 * dr;
    double r2 = (i_n + 0.5) * dr;
    double r3 = radius_new + 0.5 * dr;
    double delta_t = time->step;

    if (i_n == 0)
      wj_lower = p_charge
        / (delta_t * PI * dr * dr / 4.)
        * PI * (r2 * r2 - r1 * r1) / value_part;
    else
      wj_lower = p_charge
        / (delta_t * 2. * PI * i_n * dr * dr)
        * PI * (r2 * r2-r1 * r1) / value_part;
    wj_upper = p_charge
      / (delta_t * 2 * PI * (i_n + 1) * dr * dr)
      * PI * (r3 * r3 - r2 * r2) / value_part;

    current[0].inc(i_n, k_n, 0.);
    current[0].inc(i_n, k_n + 1,0.);
    int res_k = k_n - k_o;

    switch(res_k)
    {
    case 0:
    {
      delta_z = longitude_new - longitude_old;
      wj = wj_lower * delta_z;
      current[2].inc(i_n, k_n, wj);
      wj = wj_upper * delta_z;
      current[2].inc(i_n+1, k_n, wj);
    }
    break;

    case 1:
    {
      delta_z = k_n * dz - longitude_old;
      wj = wj_lower * delta_z;
      current[2].inc(i_n, k_n-1, wj);
      wj = wj_upper * delta_z;
      current[2].inc(i_n+1, k_n-1, wj);

      delta_z = longitude_new - k_n * dz;
      wj = wj_lower * delta_z;
      current[2].inc(i_n, k_n, wj);
      wj = wj_upper * delta_z;
      current[2].inc(i_n+1, k_n, wj);
    }
    break;

    case -1:
    {
      delta_z = (k_n + 1) * dz - longitude_old;
      wj = wj_lower * delta_z;
      current[2].inc(i_n, k_n+1, wj);
      wj = wj_upper * delta_z;
      current[2].inc(i_n+1, k_n+1, wj);

      delta_z = longitude_new - (k_n+1) * dz;
      wj = wj_lower * delta_z;
      current[2].inc(i_n, k_n, wj);
      wj = wj_upper * delta_z;
      current[2].inc(i_n+1, k_n, wj);
    }
    break;
    }
  }

  // stirct radial motion
  else if (abs(longitude_new-longitude_old)<MNZL)
  {
    double wj, delta_r, left_delta_z, right_delta_z, res_j, r0, some_shit_density;
    int res_i = i_n - i_o;
    double delta_t = time->step;

    switch(res_i)
    {
    case 0:
    {
      delta_r = radius_new - radius_old;
      left_delta_z = (k_n + 1) * dz-longitude_new;
      right_delta_z = longitude_new - k_n * dz;
      r0 = (i_n + 0.5) * dr;
      some_shit_density = SOME_SHIT_DENSITY_STRICT(p_charge, r0, dr, dz, delta_t);

      wj = some_shit_density
        * (delta_r - r0 * r0 / (radius_old + delta_r)
           + r0 * r0 / radius_old
           + dr * dr / (4. * (radius_old + delta_r))
           - dr * dr
           / (4. * radius_old));

      res_j = wj * left_delta_z;
      current[0].inc(i_n,k_n,res_j);
      res_j = wj * right_delta_z;
      current[0].inc(i_n,k_n + 1,res_j);
    }
    break;
    case 1:
    {
      delta_r = (i_n) * dr - radius_old;
      left_delta_z = (k_n + 1) * dz-longitude_new;
      right_delta_z = longitude_new - k_n * dz;
      r0 = (i_n-0.5) * dr;
      some_shit_density = SOME_SHIT_DENSITY_STRICT(p_charge, r0, dr, dz, delta_t);

      wj = some_shit_density
        * (delta_r - r0 * r0 / (radius_old + delta_r)
           + r0 * r0 / radius_old
           + dr * dr / (4. * (radius_old + delta_r))
           - dr * dr / (4. * radius_old));

      res_j = wj * left_delta_z;
      current[0].inc(i_n-1,k_n,res_j);
      res_j = wj * right_delta_z;
      current[0].inc(i_n-1,k_n + 1,res_j);

      delta_r = radius_new - i_n * dr;
      r0 = (i_n + 0.5) * dr;
      some_shit_density = SOME_SHIT_DENSITY_STRICT(p_charge, r0, dr, dz, delta_t);

      wj = some_shit_density
        * (delta_r - r0 * r0 / (i_n * dr + delta_r)
           + r0 * r0 / i_n * dr
           + dr * dr / (4. * (i_n * dr + delta_r))
           - dr * dr / (4. * i_n * dr));

      res_j = wj * left_delta_z;
      current[0].inc(i_n,k_n,res_j);
      res_j = wj * right_delta_z;
      current[0].inc(i_n,k_n + 1,res_j);
    }
    break;
    case -1:
    {
      delta_r = (i_n + 1) * dr - radius_old ;
      left_delta_z = (k_n + 1) * dz-longitude_new;
      right_delta_z = longitude_new - k_n * dz;
      r0 = (i_n + 1.5) * dr;
      some_shit_density = SOME_SHIT_DENSITY_STRICT(p_charge, r0, dr, dz, delta_t);

      wj = some_shit_density
        * (delta_r - r0 * r0 / (radius_old + delta_r)
           + r0 * r0 / radius_old
           + dr * dr / (4. * (radius_old + delta_r))
           - dr * dr / (4. * radius_old));

      res_j = wj * left_delta_z;
      current[0].inc(i_n+1, k_n, res_j);
      res_j = wj * right_delta_z;
      current[0].inc(i_n+1, k_n+1, res_j);

      delta_r = radius_new - (i_n + 1) * dr;
      r0 = (i_n + 0.5) * dr;
      some_shit_density = SOME_SHIT_DENSITY_STRICT(p_charge, r0, dr, dz, delta_t);

      wj = some_shit_density
        * (delta_r - r0 * r0 / ((i_n + 1) * dr + delta_r)
           + r0 * r0 / (i_n + 1) * dr
           + dr * dr / (4. * ((i_n + 1) * dr + delta_r))
           - dr * dr / (4. * (i_n + 1) * dr));

      res_j = wj * left_delta_z;
      current[0].inc(i_n, k_n, res_j);
      res_j = wj * right_delta_z;
      current[0].inc(i_n, k_n+1, res_j);
    }
    break;
    }
  }
}
