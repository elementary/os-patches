/*
 * Copyright 2014 Canonical Ltd.
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

#include <datetime/appointment.h>
#include <datetime/formatter.h>
#include <datetime/snap.h>

#include <canberra.h>
#include <libnotify/notify.h>

#include <glib/gi18n.h>
#include <glib.h>

#include <set>
#include <string>

#define ALARM_SOUND_FILENAME "/usr/share/sounds/ubuntu/stereo/phone-incoming-call.ogg"

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

namespace
{

/** 
***  libcanberra -- play sounds
**/

// arbitrary number, but we need a consistent id for play/cancel
const int32_t alarm_ca_id = 1;

gboolean media_cached = FALSE;
ca_context *c_context = nullptr;
guint timeout_tag = 0;

ca_context* get_ca_context()
{
    if (G_UNLIKELY(c_context == nullptr))
    {
        int rv;

        if ((rv = ca_context_create(&c_context)) != CA_SUCCESS)
        {
            g_warning("Failed to create canberra context: %s\n", ca_strerror(rv));
            c_context = nullptr;
        }
        else
        {
            const char* filename = ALARM_SOUND_FILENAME;
            rv = ca_context_cache(c_context,
                                  CA_PROP_EVENT_ID, "alarm",
                                  CA_PROP_MEDIA_FILENAME, filename,
                                  CA_PROP_CANBERRA_CACHE_CONTROL, "permanent",
                                  NULL);
            media_cached = rv == CA_SUCCESS;
            if (rv != CA_SUCCESS)
                g_warning("Couldn't add '%s' to canberra cache: %s", filename, ca_strerror(rv));
        }
    }

    return c_context;
}

void play_alarm_sound();

gboolean play_alarm_sound_idle (gpointer)
{
    timeout_tag = 0;
    play_alarm_sound();
    return G_SOURCE_REMOVE;
}

void on_alarm_play_done (ca_context* /*context*/, uint32_t /*id*/, int rv, void* /*user_data*/)
{
    // wait one second, then play it again
    if ((rv == CA_SUCCESS) && (timeout_tag == 0))
        timeout_tag = g_timeout_add_seconds (1, play_alarm_sound_idle, nullptr);
}

void play_alarm_sound()
{
    const gchar* filename = ALARM_SOUND_FILENAME;
    auto context = get_ca_context();
    g_return_if_fail(context != nullptr);

    ca_proplist* props = nullptr;
    ca_proplist_create(&props);
    if (media_cached)
        ca_proplist_sets(props, CA_PROP_EVENT_ID, "alarm");
    ca_proplist_sets(props, CA_PROP_MEDIA_FILENAME, filename);

    const auto rv = ca_context_play_full(context, alarm_ca_id, props, on_alarm_play_done, nullptr);
    if (rv != CA_SUCCESS)
        g_warning("Failed to play file '%s': %s", filename, ca_strerror(rv));

    g_clear_pointer(&props, ca_proplist_destroy);
}

void stop_alarm_sound()
{
    auto context = get_ca_context();
    if (context != nullptr)
    {
        const auto rv = ca_context_cancel(context, alarm_ca_id);
        if (rv != CA_SUCCESS)
            g_warning("Failed to cancel alarm sound: %s", ca_strerror(rv));
    }

    if (timeout_tag != 0)
    {
        g_source_remove(timeout_tag);
        timeout_tag = 0;
    }
}

/** 
***  libnotify -- snap decisions
**/

void first_time_init()
{
    static bool inited = false;

    if (G_UNLIKELY(!inited))
    {
        inited = true;

        if(!notify_init("indicator-datetime-service"))
            g_critical("libnotify initialization failed");
    }
}

struct SnapData
{
    Snap::appointment_func show;
    Snap::appointment_func dismiss;
    Appointment appointment;
};

void on_snap_show(NotifyNotification*, gchar* /*action*/, gpointer gdata)
{
    stop_alarm_sound();
    auto data = static_cast<SnapData*>(gdata);
    data->show(data->appointment);
}

void on_snap_dismiss(NotifyNotification*, gchar* /*action*/, gpointer gdata)
{
    stop_alarm_sound();
    auto data = static_cast<SnapData*>(gdata);
    data->dismiss(data->appointment);
}

void on_snap_closed(NotifyNotification*, gpointer)
{
    stop_alarm_sound();
}

void snap_data_destroy_notify(gpointer gdata)
{
     delete static_cast<SnapData*>(gdata);
}

std::set<std::string> get_server_caps()
{
    std::set<std::string> caps_set;
    auto caps_gl = notify_get_server_caps();
    std::string caps_str;
    for(auto l=caps_gl; l!=nullptr; l=l->next)
    {
        caps_set.insert((const char*)l->data);

        caps_str += (const char*) l->data;;
        if (l->next != nullptr)
          caps_str += ", ";
    }
    g_debug ("%s notify_get_server() returned [%s]", G_STRFUNC, caps_str.c_str());
    g_list_free_full(caps_gl, g_free);
    return caps_set;
}

typedef enum
{
    // just a bubble... no actions, no audio
    NOTIFY_MODE_BUBBLE,

    // a snap decision popup dialog + audio
    NOTIFY_MODE_SNAP
}
NotifyMode;

NotifyMode get_notify_mode()
{
    static NotifyMode mode;
    static bool mode_inited = false;

    if (G_UNLIKELY(!mode_inited))
    {
        const auto caps = get_server_caps();

        if (caps.count("actions"))
            mode = NOTIFY_MODE_SNAP;
        else
            mode = NOTIFY_MODE_BUBBLE;

        mode_inited = true;
    }

    return mode;
}

bool show_notification (SnapData* data, NotifyMode mode)
{
    const Appointment& appointment = data->appointment;

    const auto timestr = appointment.begin.format("%a, %X");
    auto title = g_strdup_printf(_("Alarm %s"), timestr.c_str());
    const auto body = appointment.summary;
    const gchar* icon_name = "alarm-clock";

    auto nn = notify_notification_new(title, body.c_str(), icon_name);
    if (mode == NOTIFY_MODE_SNAP)
    {
        notify_notification_set_hint_string(nn, "x-canonical-snap-decisions", "true");
        notify_notification_set_hint_string(nn, "x-canonical-private-button-tint", "true");
        /* text for the alarm popup dialog's button to show the active alarm */
        notify_notification_add_action(nn, "show", _("Show"), on_snap_show, data, nullptr);
        /* text for the alarm popup dialog's button to shut up the alarm */
        notify_notification_add_action(nn, "dismiss", _("Dismiss"), on_snap_dismiss, data, nullptr);
        g_signal_connect(G_OBJECT(nn), "closed", G_CALLBACK(on_snap_closed), data);
    }
    g_object_set_data_full(G_OBJECT(nn), "snap-data", data, snap_data_destroy_notify);

    bool shown = true;
    GError * error = nullptr;
    notify_notification_show(nn, &error);
    if (error != NULL)
    {
        g_critical("Unable to show snap decision for '%s': %s", body.c_str(), error->message);
        g_error_free(error);
        data->show(data->appointment);
        shown = false;
    }

    g_free(title);
    return shown;
}

/** 
***
**/

void notify(const Appointment& appointment,
            Snap::appointment_func show,
            Snap::appointment_func dismiss)
{
    auto data = new SnapData;
    data->appointment = appointment;
    data->show = show;
    data->dismiss = dismiss;

    switch (get_notify_mode())
    {
        case NOTIFY_MODE_BUBBLE:
            show_notification(data, NOTIFY_MODE_BUBBLE);
            break;

        default:
            if (show_notification(data, NOTIFY_MODE_SNAP))
                play_alarm_sound();
            break;
     }
}

} // unnamed namespace


/***
****
***/

Snap::Snap()
{
    first_time_init();
}

Snap::~Snap()
{
    media_cached = false;
    g_clear_pointer(&c_context, ca_context_destroy);
}

void Snap::operator()(const Appointment& appointment,
                      appointment_func show,
                      appointment_func dismiss)
{
    if (appointment.has_alarms)
        notify(appointment, show, dismiss);
    else
        dismiss(appointment);
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
