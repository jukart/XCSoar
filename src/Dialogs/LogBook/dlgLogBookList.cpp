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

#include "IO/DataFile.hpp"
#include "IO/CSVLine.hpp"
#include "Time/BrokenDateTime.hpp"
#include "Geo/GeoPoint.hpp"
#include "Parser/TimeParser.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "Logger/FlightLogger.hpp"

#include "Dialogs/LogBook/LogBook.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Language/Language.hpp"
#include "Widget/ListWidget.hpp"
#include "Screen/Canvas.hpp"
#include "Screen/Layout.hpp"


struct ListItem
{
  char simulated;
  BrokenDateTime start_time;
  StaticString<14> start_longitude;
  StaticString<13> start_latitude;
  StaticString<50> start_airfield_name;
  BrokenDateTime landing_time;
  StaticString<14> landing_longitude;
  StaticString<13> landing_latitude;
  StaticString<50> landing_airfield_name;
  StaticString<50> pilot_name;
  StaticString<32> plane_type;
  StaticString<32> plane_registration;
  StaticString<6>  plane_competition;

  void clear() {
      simulated = '\0';
      start_time = BrokenDateTime::Invalid();
      start_longitude.clear();
      start_latitude.clear();
      start_airfield_name.clear();
      landing_time = BrokenDateTime::Invalid();
      landing_longitude.clear();
      landing_latitude.clear();
      landing_airfield_name.clear();
      pilot_name.clear();
      plane_type.clear();
      plane_registration.clear();
      plane_competition.clear();
  };
};


class LogBookListWidget : public ListWidget {
  unsigned int linecount;
  std::vector<ListItem> lines;

public:
  LogBookListWidget() : linecount(0) {
    lines.reserve(100);
  }

  /* virtual methods from class Widget */
  virtual void Prepare(ContainerWindow &parent,
                       const PixelRect &rc) override {
    ListControl &list = CreateList(parent, UIGlobals::GetDialogLook(), rc,
                                   Layout::Scale(33u));
    TLineReader* reader = OpenDataTextFile("flights.log");
    if (reader != NULL) {
      // the log file exists
      char buffer[50];
      ListItem item;
      item.clear();
      TCHAR *line;
      while ((line = reader->ReadLine()) != NULL) {
        // Skip empty line
        if (StringIsEmpty(line))
          continue;
        CSVLine csv(line);
        csv.Read(buffer, 50);
        EventType event_type;
        char sim = 'R';
        // The first column must be handled in a special way because we want to
        // also use the "old" log format used. This "old" format provides a
        // line with the time and the string "start" or "landing" separated with
        // a space.
        if (buffer[19] == ' ') {
          event_type = buffer[20] == 's' ? EventType::START : EventType::LANDING;
          buffer[19] = 'Z';
          buffer[20] = '\0';
        } else {
          event_type = (EventType)csv.ReadOneChar();
          sim = csv.ReadOneChar(); // simulator flag
        }
        if (event_type == EventType::START) {
          // a start line always creates a new item
          if (item.simulated) {
            lines.push_back(item);
            linecount++;
            item.clear();
          }
          BrokenDateTime start_time;
          if (ParseISO8601(buffer, start_time)) {
            item.simulated = sim;
            item.start_time = start_time;
            csv.Read(item.pilot_name.buffer(), 20);
            csv.Read(item.plane_type.buffer(), 32);
            csv.Read(item.plane_registration.buffer(), 32);
            csv.Read(item.plane_competition.buffer(), 6);
            csv.Read(item.start_longitude.buffer(), 14);
            csv.Read(item.start_latitude.buffer(), 13);
            csv.Read(item.start_airfield_name.buffer(), 50);
          }
        } else if (event_type == EventType::LANDING) {
          BrokenDateTime landing_time;
          if (ParseISO8601(buffer, landing_time)) {
            if (item.simulated) {
              bool related = item.simulated == sim;
              if (related) {
                // If there is an item we have a start time.
                // We check if the start time is older than 24 hours.
                if (item.start_time.IsPlausible()) {
                  int seconds = landing_time - item.start_time;
                  if (seconds > 60*60*24) {
                    // More than 24 hours, we assume landing is not related to
                    // the start
                    related = false;
                  }
                }
              }
              if (!related) {
                lines.push_back(item);
                linecount++;
                item.clear();
              }
            }
            if (!item.simulated) {
              // This is an item with only a landing time, get the pilot data
              // from the line.
              item.simulated = sim;
              csv.Read(item.pilot_name.buffer(), 20);
              csv.Read(item.plane_type.buffer(), 32);
              csv.Read(item.plane_registration.buffer(), 32);
              csv.Read(item.plane_competition.buffer(), 6);
            } else {
              // Skip the pilot data because we got it from the start line.
              csv.Skip();
              csv.Skip();
              csv.Skip();
              csv.Skip();
            }
            item.landing_time = landing_time;
            csv.Read(item.landing_longitude.buffer(), 14);
            csv.Read(item.landing_latitude.buffer(), 13);
            csv.Read(item.landing_airfield_name.buffer(), 50);
            lines.push_back(item);
            linecount++;
            item.clear();
          }
        }
      }
      if (item.simulated) {
        lines.push_back(item);
        linecount++;
      }
      delete reader;
      std::reverse(lines.begin(), lines.end());
    }
    list.SetLength(linecount);
  }

  virtual void Unprepare() override {
    DeleteWindow();
  }

  /* virtual methods from class ListItemRenderer */
  virtual void OnPaintItem(Canvas &canvas, const PixelRect rc,
                           unsigned idx) override;

  /* virtual methods from class ListCursorHandler */
  virtual bool CanActivateItem(unsigned index) const override {
    return true;
  }
};

void
LogBookListWidget::OnPaintItem(Canvas &canvas, const PixelRect rc,
                               unsigned i)
{
  assert(i < linecount);

  ListItem *item = &(lines[i]);

  const DialogLook &look = UIGlobals::GetDialogLook();
  const Font &name_font = *look.list.font_bold;
  const Font &small_font = *look.small_font;

  BrokenDateTime *time = &item->start_time;
  if (!time->IsPlausible()) {
    time = &item->landing_time;
  };

  StaticString<12> buffer_date;
  if (time->IsPlausible()) {
    buffer_date.Format(_T("%04u-%02u-%02u"),
                       time->year, time->month, time->day);
  } else {
    buffer_date = _T("\?\?\?\?-\?\?-\?\?");
  }

  bool has_start = item->start_time.IsPlausible();
  bool has_landing = item->landing_time.IsPlausible();

  StaticString<10> buffer_started;
  if (has_start) {
    time = &item->start_time;
    buffer_started.Format(_T("%02u:%02u:%02u"),
                          time->hour, time->minute, time->second);
  } else {
    buffer_started = _("no time");
  }

  StaticString<10> buffer_landed;
  if (has_landing) {
    time = &item->landing_time;
    buffer_landed.Format(_T("%02u:%02u:%02u"),
                         time->hour, time->minute, time->second);
  } else {
    buffer_landed = _("no time");
  }

  StaticString<10> buffer_duration;
  if (has_start && has_landing) {
    int duration = item->landing_time - item->start_time;
    FormatSignedTimeHHMM(buffer_duration.buffer(), duration);
  } else {
    buffer_duration = _T("--:--");
  }

  StaticString<50> buffer_start_location;
  buffer_start_location.clear();
  if (has_start) {
    if (!item->start_airfield_name.empty()) {
      // show the airfield name
      buffer_start_location = item->start_airfield_name;
    } else if (!item->start_latitude.empty()) {
      // show lat/lon
      buffer_start_location.Format(_T("(%s,%s)"),
                                   item->start_latitude.c_str(),
                                   item->start_longitude.c_str());
    }
  }

  StaticString<50> buffer_landing_location;
  buffer_landing_location.clear();
  if (has_landing) {
    if (!item->landing_airfield_name.empty()) {
      // show the airfield name
      buffer_landing_location = item->landing_airfield_name;
    } else if (!item->landing_latitude.empty()) {
      // show lat/lon
      buffer_landing_location.Format(_T("(%s,%s)"),
                                     item->landing_latitude.c_str(),
                                     item->landing_longitude.c_str());
    }
  }

  if (item->simulated == 'S') {
    canvas.SetTextColor(Color(255, 0, 0));
  }

  canvas.Select(name_font);
  canvas.DrawText(rc.left + 2, rc.top, buffer_date);

  canvas.Select(small_font);

  StaticString<100> buffer;
  buffer.Format(_T("%s - %s (%s)"),
                buffer_started.c_str(),
                buffer_landed.c_str(),
                buffer_duration.c_str());
  UPixelScalar width;
  width = canvas.CalcTextWidth(buffer.c_str());
  canvas.DrawText(rc.right - 4 - width, rc.top, buffer);

  buffer.Format(item->pilot_name.empty() ? _("no pilot name") : item->pilot_name.c_str());

  if (item->plane_registration.empty()) {
    buffer.AppendFormat(", %s", _("no plane information"));
  } else {
    buffer.AppendFormat(_T(", %s, %s, %s"),
                        item->plane_registration.c_str(),
                        item->plane_competition.c_str(),
                        item->plane_type.c_str());
  }
  canvas.DrawText(rc.left + 15, rc.top + 22, buffer);

  buffer.clear();
  has_start = has_start && !buffer_start_location.empty();
  has_landing = has_landing && !buffer_landing_location.empty();
  if (has_start) {
      buffer = buffer_start_location;
  }
  if (has_start && has_landing) {
      buffer += _T(" - ");
  }
  if (has_landing) {
    buffer += buffer_landing_location.c_str();
  }
  canvas.DrawText(rc.left + 15, rc.top + 38, buffer);
};

void
dlgLogBookShowList(SingleWindow &parent)
{
  LogBookListWidget tw;
  WidgetDialog dialog(UIGlobals::GetDialogLook());
  dialog.CreateFull(UIGlobals::GetMainWindow(), _("Flying Logbook"), &tw);
  dialog.AddButton(_("Close"), mrOK);
  dialog.ShowModal();
  dialog.StealWidget();
};
