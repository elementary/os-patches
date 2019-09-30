
/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <datetime/appointment.h>
#include <datetime/snap.h>

#include <glib.h>

using namespace unity::indicator::datetime;

/***
****
***/

int main()
{
    Appointment a;
    a.color = "green";
    a.summary = "Alarm";
    a.url = "alarm:///hello-world";
    a.uid = "D4B57D50247291478ED31DED17FF0A9838DED402";
    a.has_alarms = true;
    auto begin = g_date_time_new_local(2014,12,25,0,0,0);
    auto end = g_date_time_add_full(begin,0,0,1,0,0,-1);
    a.begin = begin;
    a.end = end;
    g_date_time_unref(end);
    g_date_time_unref(begin);

    auto loop = g_main_loop_new(nullptr, false);
    auto show = [loop](const Appointment& appt){
        g_message("You clicked 'show' for appt url '%s'", appt.url.c_str());
        g_main_loop_quit(loop);
    };
    auto dismiss = [loop](const Appointment&){
        g_message("You clicked 'dismiss'");
        g_main_loop_quit(loop);
    };
    
    Snap snap;
    snap(a, show, dismiss);
    g_main_loop_run(loop);
    return 0;
}
