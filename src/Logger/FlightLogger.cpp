/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2016 The XCSoar Project
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

#include "FlightLogger.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "Simulator.hpp"
#include "Interface.hpp"
#include "Components.hpp"
#include "Blackboard/LiveBlackboard.hpp"
#include "IO/TextWriter.hpp"
#include "Formatter/GeoPointFormatter.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Engine/Waypoint/Waypoints.hpp"
#include "MapWindow/Items/MapItem.hpp"
#include "MapWindow/Items/List.hpp"
#include "MapWindow/Items/Builder.hpp"

void
FlightLogger::Reset()
{
  last_time = 0;
  seen_on_ground = seen_flying = false;
  start_time.Clear();
  landing_time.Clear();
}

void
FlightLogger::LogEvent(const FlyingState &flight,
                       const BrokenDateTime &date_time,
                       const EventType event_type)
{
  assert(type != nullptr);

  TextWriter writer(path, true);
  if (!writer.IsOpen())
    /* Shall we log this error?  Not sure, because when this happens,
       usually the log file cannot be written either .. */
    return;

  const ComputerSettings &settings_computer = CommonInterface::GetComputerSettings();
  const LoggerSettings &logger = settings_computer.logger;
  const Plane &plane = settings_computer.plane;

  char time_buffer[32];
  FormatISO8601(time_buffer, date_time);

  const GeoPoint *location;
  if (event_type == EventType::START)
      location = &flight.takeoff_location;
  else
      location = &flight.landing_location;

  const Waypoint *airfield = NULL;

  StaticString<30> lat_buffer;
  lat_buffer.clear();
  StaticString<30> lon_buffer;
  lon_buffer.clear();
  if (location->IsValid()) {
    TCHAR sign = negative(location->longitude.Native()) ? _T('W') : _T('E');
    fixed mlong(location->longitude.AbsoluteDegrees());
    lon_buffer.Format(_T("%08.4f%c"), mlong, sign);
    sign = negative(location->latitude.Native()) ? _T('S') : _T('N');
    mlong = location->latitude.AbsoluteDegrees();
    lat_buffer.Format(_T("%08.4f%c"), mlong, sign);
    
    // get map items near the current location
    GeoPoint takeoff(location->longitude,
                     location->latitude);

    airfield = way_points.GetNearestLandable(*location, fixed(1000));
  }

  writer.FormatLine("%s,%c,%c,%s,%s,%s,%s,%s,%s,%s",
                    time_buffer,
                    event_type,
                    is_simulator() ? 'S' : 'R',
                    logger.pilot_name.c_str(),
                    plane.type.c_str(),
                    plane.registration.c_str(),
                    plane.competition_id.c_str(),
                    lat_buffer.c_str(),
                    lon_buffer.c_str(),
                    airfield ? airfield->name.c_str() : ""
                    );
}

void
FlightLogger::TickInternal(const MoreData &basic,
                           const DerivedInfo &calculated)
{
  const FlyingState &flight = calculated.flight;

  if (seen_on_ground && flight.flying) {
    /* store preliminary start time */
    start_time = basic.date_time_utc;

    if (!flight.on_ground) {
      /* start was confirmed (not on ground anymore): log it */
      seen_on_ground = false;

      LogEvent(flight, start_time, EventType::START);

      start_time.Clear();
    }
  }

  if (seen_flying && flight.on_ground) {
    /* store preliminary landing time */
    landing_time = basic.date_time_utc;

    if (!flight.flying) {
      /* landing was confirmed (not on ground anymore): log it */
      seen_flying = false;

      LogEvent(flight, landing_time, EventType::LANDING);

      landing_time.Clear();
    }
  }

  if (flight.flying && !flight.on_ground)
    seen_flying = true;

  if (!flight.flying && flight.on_ground)
    seen_on_ground = true;
}

void
FlightLogger::Tick(const MoreData &basic, const DerivedInfo &calculated)
{
  assert(!path.IsNull());

  if (basic.gps.replay || basic.gps.simulator)
    return;

  if (!basic.time_available || !basic.date_time_utc.IsDatePlausible())
    /* can't work without these */
    return;

  if (last_time > 0) {
    auto time_delta = basic.time - last_time;
    if (time_delta < 0 || time_delta > 300)
      /* reset on time warp (positive or negative) */
      Reset();
    else if (time_delta < 0.5)
      /* not enough time has passed since the last call: ignore this
         GPS fix, don't update last_time, just return */
      return;
    else
      TickInternal(basic, calculated);
  }

  last_time = basic.time;
}
