/*
 * Copyright 2013 Canonical Ltd.
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
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#include <datetime/actions-live.h>

#include <url-dispatcher.h>

#include <glib.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

LiveActions::LiveActions(const std::shared_ptr<State>& state_in):
    Actions(state_in)
{
}

void LiveActions::execute_command(const std::string& cmdstr)
{
    const auto cmd = cmdstr.c_str();
    g_debug("Issuing command '%s'", cmd);

    GError* error = nullptr;
    if (!g_spawn_command_line_async(cmd, &error))
    {
        g_warning("Unable to start \"%s\": %s", cmd, error->message);
        g_error_free(error);
    }
}

void LiveActions::dispatch_url(const std::string& url)
{
    g_debug("Dispatching url '%s'", url.c_str());
    url_dispatch_send(url.c_str(), nullptr, nullptr);
}

/***
****
***/

void LiveActions::desktop_open_settings_app()
{
    auto path = g_find_program_in_path("unity-control-center");

    if ((path != nullptr) && (g_strcmp0 (g_getenv ("XDG_CURRENT_DESKTOP"), "Unity") == 0))
    {
        execute_command("unity-control-center datetime");       
    }
    else
    {
        execute_command("gnome-control-center datetime");
    }

    g_free (path);
}

bool LiveActions::desktop_has_calendar_app() const
{
    static bool inited = false;
    static bool have_calendar = false;

    if (G_UNLIKELY(!inited))
    {
        inited = true;

#if 0
        auto all = g_app_info_get_all_for_type ("text/calendar");
        for(auto l=all; !have_calendar && l!=nullptr; l=l->next)
        {
            auto app_info = static_cast<GAppInfo*>(l->data);

            if (!g_strcmp0("evolution.desktop", g_app_info_get_id(app_info)))
                have_calendar = true;
        }

        g_list_free_full(all, (GDestroyNotify)g_object_unref);
#else
        /* Work around http://pad.lv/1296233 for Trusty...
           let's revert this when the GIO bug is fixed. */
        auto executable = g_find_program_in_path("evolution");
        have_calendar = executable != nullptr;
        g_free(executable);
#endif
    }

    return have_calendar;
}

void LiveActions::desktop_open_alarm_app()
{
    execute_command("evolution -c calendar");
}

void LiveActions::desktop_open_appointment(const Appointment& appt)
{
    desktop_open_calendar_app(appt.begin);
}

void LiveActions::desktop_open_calendar_app(const DateTime& dt)
{
    const auto day_begins = dt.add_full(0, 0, 0, -dt.hour(), -dt.minute(), -dt.seconds());
    const auto gmt = day_begins.to_timezone("UTC");
    auto cmd = gmt.format("evolution \"calendar:///?startdate=%Y%m%dT%H%M%SZ\"");
    execute_command(cmd.c_str());
}

/***
****
***/

void LiveActions::phone_open_alarm_app()
{
    dispatch_url("appid://com.ubuntu.clock/clock/current-user-version");
}

void LiveActions::phone_open_appointment(const Appointment& appt)
{
    if (!appt.url.empty())
        dispatch_url(appt.url);
    else if (appt.has_alarms)
        phone_open_alarm_app();
    else
        phone_open_calendar_app(DateTime::NowLocal());
}

void LiveActions::phone_open_calendar_app(const DateTime&)
{
    // does calendar app have a mechanism for specifying dates?
    dispatch_url("appid://com.ubuntu.calendar/calendar/current-user-version");
}

void LiveActions::phone_open_settings_app()
{
    dispatch_url("settings:///system/time-date");
}

/***
****
***/

namespace
{

struct setlocation_data
{
  std::string tzid;
  std::string name;
  std::shared_ptr<Settings> settings;
};

static void
on_datetime1_set_timezone_response(GObject       * object,
                                   GAsyncResult  * res,
                                   gpointer        gdata)
{
  GError* err = nullptr;
  auto response = g_dbus_proxy_call_finish(G_DBUS_PROXY(object), res, &err);
  auto data = static_cast<struct setlocation_data*>(gdata);

  if (err != nullptr)
    {
      if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning("Could not set new timezone: %s", err->message);

      g_error_free(err);
    }
  else
    {
      data->settings->timezone_name.set(data->tzid + " " + data->name);
      g_variant_unref(response);
    }

  delete data;
}

static void
on_datetime1_proxy_ready (GObject      * object G_GNUC_UNUSED,
                          GAsyncResult * res,
                          gpointer       gdata)
{
  auto data = static_cast<struct setlocation_data*>(gdata);

  GError * err = nullptr;
  auto proxy = g_dbus_proxy_new_for_bus_finish(res, &err);
  if (err != nullptr)
    {
      if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning("Could not grab DBus proxy for timedated: %s", err->message);

      g_error_free(err);

      delete data;
    }
  else
    {
      g_dbus_proxy_call(proxy,
                        "SetTimezone",
                        g_variant_new ("(sb)", data->tzid.c_str(), TRUE),
                        G_DBUS_CALL_FLAGS_NONE,
                        -1,
                        nullptr,
                        on_datetime1_set_timezone_response,
                        data);

      g_object_unref (proxy);
    }
}

} // unnamed namespace


void LiveActions::set_location(const std::string& tzid, const std::string& name)
{
    g_return_if_fail(!tzid.empty());
    g_return_if_fail(!name.empty());

    auto data = new struct setlocation_data;
    data->tzid = tzid;
    data->name = name;
    data->settings = state()->settings;

    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                              G_DBUS_PROXY_FLAGS_NONE,
                              nullptr,
                              "org.freedesktop.timedate1",
                              "/org/freedesktop/timedate1",
                              "org.freedesktop.timedate1",
                              nullptr,
                              on_datetime1_proxy_ready,
                              data);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
