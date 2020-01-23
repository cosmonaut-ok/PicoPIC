#include <gtest/gtest.h>
#include "geometry.hpp"
#include "geometry.cpp"


TEST(geometry, constructor)
{
  double radius = 0.6;
  double longitude = 1.2;

  int bot_ngr = 10;
  int top_ngr = 20;
  int left_ngz = 11;
  int right_ngz = 31;
  double pml_l_z0 = 0.01;
  double pml_l_zwall = 0.01;
  double pml_l_rwall = 0.01;
  double pml_sigma1 = 0.01;
  double pml_sigma2 = 0.01;
  bool wall_r0 = true;
  bool wall_rr = true;
  bool wall_z0 = true;
  bool wall_zz = true;

  Geometry geometry (radius, longitude,
                     bot_ngr, top_ngr, left_ngz, right_ngz,
                     pml_l_z0, pml_l_zwall, pml_l_rwall, pml_sigma1, pml_sigma2,
                     wall_r0, wall_rr, wall_z0, wall_zz);

  Geometry geometry_empty ();

  ASSERT_EQ(geometry.r_grid_amount, 10);
  ASSERT_EQ(geometry.z_grid_amount, 20);

  ASSERT_EQ(geometry.r_cell_size, 0.06);
  ASSERT_EQ(geometry.z_cell_size, 0.06);

  for (unsigned int i = 1; i < 3; ++i)
    ASSERT_EQ(geometry.pml_length[i], 0.01);

  for (unsigned int i = 1; i < 2; ++i)
    ASSERT_EQ(geometry.pml_sigma[i], 0.01);
}