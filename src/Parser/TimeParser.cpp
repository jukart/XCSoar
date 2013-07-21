/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2013 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "TimeParser.hpp"
#include "Time/BrokenDateTime.hpp"

#include <cstdlib>


static bool
ParseNext(unsigned &val, const char **str, char delim) {
  char *endptr;
  val = strtoul(*str, &endptr, 10);
  if (*endptr != delim)
    return false;
  *str = endptr + 1;
  return true;
};

bool
ParseISO8601(const char *str, BrokenDateTime &stamp) {
  unsigned val;
  if (!ParseNext(val, &str, '-'))
    return false;
  stamp.year = val;
  if (!ParseNext(val, &str, '-'))
    return false;
  stamp.month = val;
  if (!ParseNext(val, &str, 'T'))
    return false;
  stamp.day = val;
  if (!ParseNext(val, &str, ':'))
    return false;
  stamp.hour = val;
  if (!ParseNext(val, &str, ':'))
    return false;
  stamp.minute = val;
  if (!ParseNext(val, &str, 'Z'))
    return false;
  stamp.second = val;
  return true;
};
