/*
 * This file is part of the PiCoPiC distribution (https://github.com/cosmonaut-ok/PiCoPiC).
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

#ifndef _CURRENT_HPP_
#define _CURRENT_HPP_

#include <vector>

#include "defines.hpp"
#include "msg.hpp"

#include "algo/grid3d.hpp"
#include "timeSim.hpp"
#include "geometry.hpp"
#include "specieP.hpp"

using namespace std;

class SpecieP;

class Current
{
public:
  Geometry *geometry;
  TimeSim *time;
  vector<SpecieP *> species_p;
  Grid3D<double> current;
  
  Current() {};
  Current(Geometry *geom, TimeSim *t, vector<SpecieP *> species) : geometry(geom), time(t)
  {
    current = Grid3D<double> (geometry->cell_amount[0], geometry->cell_amount[1], 2);
    species_p = species;

    current = 0;
    current.overlay_set(0);
  };

  virtual void current_distribution() = 0;
};

#endif // end of _CURRENT_HPP_
