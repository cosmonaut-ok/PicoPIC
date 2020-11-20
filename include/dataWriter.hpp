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

#ifndef _DATA_WRITER_HPP_
#define _DATA_WRITER_HPP_

#include <math.h> // ceil

#include "defines.hpp"
#include "msg.hpp"

#include "geometry.hpp"
#include "timeSim.hpp"
#include "domain.hpp"

#ifdef USE_HDF5
#include "outEngine/outEngineHDF5.hpp"
#else
#include "outEngine/outEnginePlain.hpp"
#endif // USE_HDF5


using namespace std;

class Domain;

class DataWriter
{
public:
  string path;
  string name;
  string component;
  string specie;
  unsigned int shape;
  unsigned int schedule;
  int size[4];
  bool compress;
  unsigned int compress_level;

  Geometry *geometry;
  TimeSim *time;
  Grid<Domain*> domains;

#ifdef USE_HDF5
  HighFive::File *hdf5_file;
  OutEngineHDF5 engine;
  void hdf5_init(string a_metadata);
#else
  OutEnginePlain engine;
#endif // USE_HDF5

private:
  Grid<double> out_data;
  vector<double> out_data_plain;

public:
  DataWriter(string a_path, string a_component,
             string a_specie, unsigned int a_shape,
             int *a_size, unsigned int a_schedule,
             bool a_compress, unsigned int a_compress_level,
             Geometry *a_geom, TimeSim *a_time,
             Grid<Domain *> a_domains, string a_metadata);

  void merge_domains(string component, string specie);
  void merge_particle_domains(string parameter, unsigned int component, string specie);
  void operator()();
};
#endif // end of _DATA_WRITER_HPP_
