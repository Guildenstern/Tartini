/***************************************************************************
                          IIR_Filter.h  -  description
                             -------------------
    begin                : 2004
    copyright            : (C) 2004-2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#ifndef IIR_FILTER_H
#define IIR_FILTER_H

#include "Filter.h"
#include "array1d.h"

/** Infinite Impulse Response filter
  */
class IIR_Filter : public Filter
{
  Array<double> bufx, bufy; //tempery buffer storage
  Array<double> _a, _b; //The filter coefficient's
  Array<double> _x, _y; //The current filter state (last n states of input and output)

  void init(double *b, double *a, int n, int m=-1);
public:
  IIR_Filter() { }
  IIR_Filter(double *b, double *a, int n, int m=-1);
  ~IIR_Filter() { }
  void filter(const float *input, float *output, int n);
  void reset();
};

#endif
