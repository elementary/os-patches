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

#include <datetime/actions.h>
#include <datetime/utils.h> // split_settings_location()

#include <glib.h>
#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

namespace
{

DateTime datetime_from_timet_variant(GVariant* v)
{
    int64_t t = 0;

    if (v != nullptr)
        if (g_variant_type_equal(G_VARIANT_TYPE_INT64,g_variant_get_type(v)))
            t = g_variant_get_int64(v);

    if (t != 0)
        return DateTime(t);
    else
        return DateTime::NowLocal();
}

bool lookup_appointment_by_uid_variant(const std::shared_ptr<State>& state, GVariant* vuid, Appointment& setme)
{
    g_return_val_if_fail(vuid != nullptr, false);
    g_return_val_if_fail(g_variant_type_equal(G_VARIANT_TYPE_STRING,g_variant_get_type(vuid)), false);
    const auto uid = g_variant_get_string(vuid, nullptr);
    g_return_val_if_fail(uid && *uid, false);

    for(const auto& appt : state->calendar_upcoming->appointments().get())
    {
        if (appt.uid == uid)
        {
            setme = appt;
            return true;
        }
    }

    return false;
}

void on_desktop_appointment_activated (GSimpleAction*, GVariant *vuid, gpointer gself)
{
    auto self = static_cast<Actions*>(gself);
    Appointment appt;
    if (lookup_appointment_by_uid_variant(self->state(), vuid, appt))
        self->desktop_open_appointment(appt);
}
void on_desktop_alarm_activated (GSimpleAction*, GVariant*, gpointer gself)
{
    static_cast<Actions*>(gself)->desktop_open_alarm_app();
}
void on_desktop_calendar_activated (GSimpleAction*, GVariant* vt, gpointer gself)
{
    const auto dt = datetime_from_timet_variant(vt);
    static_cast<Actions*>(gself)->desktop_open_calendar_app(dt);
}
void on_desktop_settings_activated (GSimpleAction*, GVariant*, gpointer gself)
{
    static_cast<Actions*>(gself)->desktop_open_settings_app();
}

void on_phone_appointment_activated (GSimpleAction*, GVariant *vuid, gpointer gself)
{
    auto self = static_cast<Actions*>(gself);
    Appointment appt;
    if (lookup_appointment_by_uid_variant(self->state(), vuid, appt))
        self->phone_open_appointment(appt);
}
void on_phone_alarm_activated (GSimpleAction*, GVariant*, gpointer gself)
{
    static_cast<Actions*>(gself)->phone_open_alarm_app();
}
void on_phone_calendar_activated (GSimpleAction*, GVariant* vt, gpointer gself)
{
    const auto dt = datetime_from_timet_variant(vt);
    static_cast<Actions*>(gself)->phone_open_calendar_app(dt);
}
void on_phone_settings_activated (GSimpleAction*, GVariant*, gpointer gself)
{
    static_cast<Actions*>(gself)->phone_open_settings_app();
}


void on_set_location(GSimpleAction * /*action*/,
                     GVariant      * param,
                     gpointer        gself)
{
    char * zone;
    char * name;
    split_settings_location(g_variant_get_string(param, nullptr), &zone, &name);
    static_cast<Actions*>(gself)->set_location(zone, name);
    g_free(name);
    g_free(zone);
}

void on_calendar_active_changed(GSimpleAction * /*action*/,
                                GVariant      * state,
                                gpointer        gself)
{
    // reset the date when the menu is shown
    if (g_variant_get_boolean(state))
    {
        auto self = static_cast<Actions*>(gself);

        self->set_calendar_date(self->state()->clock->localtime());
    }
}

void on_calendar_activated(GSimpleAction * /*action*/,
                           GVariant      * state,
                           gpointer        gself)
{
    const time_t t = g_variant_get_int64(state);

    g_return_if_fail(t != 0);

    // the client gave us a date; remove the HMS component from the resulting DateTime
    auto dt = DateTime(t);
    dt = dt.add_full (0, 0, 0, -dt.hour(), -dt.minute(), -dt.seconds());
    static_cast<Actions*>(gself)->set_calendar_date(dt);
}

GVariant* create_default_header_state()
{
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&b, "{sv}", "accessible-desc", g_variant_new_string("accessible-desc"));
    g_variant_builder_add(&b, "{sv}", "label", g_variant_new_string("label"));
    g_variant_builder_add(&b, "{sv}", "title", g_variant_new_string("title"));
    g_variant_builder_add(&b, "{sv}", "visible", g_variant_new_boolean(true));
    return g_variant_builder_end(&b);
}

GVariant* create_calendar_state(const std::shared_ptr<State>& state)
{
    gboolean days[32] = { 0 };
    for (const auto& appt : state->calendar_month->appointments().get())
        days[appt.begin.day_of_month()] = true;

    GVariantBuilder day_builder;
    g_variant_builder_init(&day_builder, G_VARIANT_TYPE("ai"));
    for (guint i=0; i<G_N_ELEMENTS(days); i++)
        if (days[i])
            g_variant_builder_add(&day_builder, "i", i);

    GVariantBuilder dict_builder;
    g_variant_builder_init(&dict_builder, G_VARIANT_TYPE_DICTIONARY);

    auto key = "appointment-days";
    auto v = g_variant_builder_end(&day_builder);
    g_variant_builder_add(&dict_builder, "{sv}", key, v);

    key = "calendar-day";
    v = g_variant_new_int64(state->calendar_month->month().get().to_unix());
    g_variant_builder_add(&dict_builder, "{sv}", key, v);

    key = "show-week-numbers";
    v = g_variant_new_boolean(state->settings->show_week_numbers.get());
    g_variant_builder_add(&dict_builder, "{sv}", key, v);

    return g_variant_builder_end(&dict_builder);
}
} // unnamed namespace

/***
****
***/

Actions::Actions(const std::shared_ptr<State>& state):
    m_state(state),
    m_actions(g_simple_action_group_new())
{
    GActionEntry entries[] = {

        { "desktop.open-appointment",  on_desktop_appointment_activated, "s", nullptr },
        { "desktop.open-alarm-app",    on_desktop_alarm_activated },
        { "desktop.open-calendar-app", on_desktop_calendar_activated, "x", nullptr },
        { "desktop.open-settings-app", on_desktop_settings_activated },

        { "phone.open-appointment",    on_phone_appointment_activated, "s", nullptr },
        { "phone.open-alarm-app",      on_phone_alarm_activated },
        { "phone.open-calendar-app",   on_phone_calendar_activated, "x", nullptr },
        { "phone.open-settings-app",   on_phone_settings_activated },

        { "calendar-active", nullptr, nullptr, "false", on_calendar_active_changed },
        { "set-location", on_set_location, "s" }
    };

    g_action_map_add_action_entries(G_ACTION_MAP(m_actions),
                                    entries,
                                    G_N_ELEMENTS(entries),
                                    this);

    // add the header actions
    auto gam = G_ACTION_MAP(m_actions);
    auto v = create_default_header_state();
    auto a = g_simple_action_new_stateful("desktop-header", nullptr, v);
    g_action_map_add_action(gam, G_ACTION(a));
    a = g_simple_action_new_stateful("desktop_greeter-header", nullptr, v);
    g_action_map_add_action(gam, G_ACTION(a));
    a = g_simple_action_new_stateful("phone-header", nullptr, v);
    g_action_map_add_action(gam, G_ACTION(a));
    a = g_simple_action_new_stateful("phone_greeter-header", nullptr, v);
    g_action_map_add_action(gam, G_ACTION(a));

    // add the calendar action
    v = create_calendar_state(state);
    a = g_simple_action_new_stateful("calendar", G_VARIANT_TYPE_INT64, v);
    g_action_map_add_action(gam, G_ACTION(a));
    g_signal_connect(a, "activate", G_CALLBACK(on_calendar_activated), this);

    ///
    ///  Keep our GActionGroup's action's states in sync with m_state
    ///

    m_state->calendar_month->month().changed().connect([this](const DateTime&){
        update_calendar_state();
    });
    m_state->calendar_month->appointments().changed().connect([this](const std::vector<Appointment>&){
        update_calendar_state();
    });
    m_state->settings->show_week_numbers.changed().connect([this](bool){
        update_calendar_state();
    });

    // FIXME: rebuild the calendar state when show-week-number changes
}

Actions::~Actions()
{
    g_clear_object(&m_actions);
}

void Actions::update_calendar_state()
{
    g_action_group_change_action_state(action_group(),
                                       "calendar",
                                       create_calendar_state(m_state));
}

void Actions::set_calendar_date(const DateTime& date)
{
    m_state->calendar_month->month().set(date);
    m_state->calendar_upcoming->date().set(date);
}

GActionGroup* Actions::action_group()
{
    return G_ACTION_GROUP(m_actions);
}

const std::shared_ptr<State> Actions::state() const
{
    return m_state;
}



} // namespace datetime
} // namespace indicator
} // namespace unity
