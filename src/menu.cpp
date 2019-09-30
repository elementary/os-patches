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

#include <datetime/menu.h>

#include <datetime/formatter.h>
#include <datetime/state.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

/****
*****
****/

Menu::Menu (Profile profile_in, const std::string& name_in):
    m_profile(profile_in),
    m_name(name_in)
{
}

const std::string& Menu::name() const
{
    return m_name;
}

Menu::Profile Menu::profile() const
{
    return m_profile;
}

GMenuModel* Menu::menu_model()
{
    return G_MENU_MODEL(m_menu);
}


/****
*****
****/


#define ALARM_ICON_NAME "alarm-clock"
#define CALENDAR_ICON_NAME "calendar"

class MenuImpl: public Menu
{
protected:
    MenuImpl(const Menu::Profile profile_in,
             const std::string& name_in,
             std::shared_ptr<const State>& state,
             std::shared_ptr<Actions>& actions,
             std::shared_ptr<const Formatter> formatter):
        Menu(profile_in, name_in),
        m_state(state),
        m_actions(actions),
        m_formatter(formatter)
    {
        // initialize the menu
        create_gmenu();
        for (int i=0; i<NUM_SECTIONS; i++)
            update_section(Section(i));

        // listen for state changes so we can update the menu accordingly
        m_formatter->header.changed().connect([this](const std::string&){
            update_header();
        });
        m_formatter->header_format.changed().connect([this](const std::string&){
            update_section(Locations); // need to update x-canonical-time-format
        });
        m_formatter->relative_format_changed.connect([this](){
            update_section(Appointments); // uses formatter.relative_format()
            update_section(Locations); // uses formatter.relative_format()
        });
        m_state->settings->show_clock.changed().connect([this](bool){
            update_header(); // update header's label
            update_section(Locations); // locations' relative time may have changed
        });
        m_state->settings->show_calendar.changed().connect([this](bool){
            update_section(Calendar);
        });
        m_state->settings->show_events.changed().connect([this](bool){
            update_section(Appointments); // showing events got toggled
        });
        m_state->calendar_upcoming->date().changed().connect([this](const DateTime&){
            update_upcoming(); // our m_upcoming is planner->upcoming() filtered by time
        });
        m_state->calendar_upcoming->appointments().changed().connect([this](const std::vector<Appointment>&){
            update_upcoming(); // our m_upcoming is planner->upcoming() filtered by time
        });
        m_state->clock->date_changed.connect([this](){
            update_section(Calendar); // need to update the Date menuitem
            update_section(Locations); // locations' relative time may have changed
        });
        m_state->clock->minute_changed.connect([this](){
            update_upcoming(); // our m_upcoming is planner->upcoming() filtered by time
        });
        m_state->locations->locations.changed().connect([this](const std::vector<Location>&) {
            update_section(Locations); // "locations" is the list of Locations we show
        });
    }

    virtual ~MenuImpl()
    {
        g_clear_object(&m_menu);
        g_clear_pointer(&m_serialized_alarm_icon, g_variant_unref);
        g_clear_pointer(&m_serialized_calendar_icon, g_variant_unref);
    }

    virtual GVariant* create_header_state() =0;

    void update_header()
    {
        auto action_group = m_actions->action_group();
        auto action_name = name() + "-header";
        auto state = create_header_state();
        g_action_group_change_action_state(action_group, action_name.c_str(), state);
    }

    void update_upcoming()
    {
        // The usual case is on desktop (and /only/ case on phone)
        // is that we're looking at the current date and want to see
        // "the next five calendar events, if any."
        //
        // However on the Desktop when the user clicks onto a different
        // calendar date, show the next five calendar events starting
        // from the beginning of that clicked day.
        DateTime begin;
        const auto now = m_state->clock->localtime();
        const auto calendar_day = m_state->calendar_month->month().get();
        if ((profile() == Desktop) && !DateTime::is_same_day(now, calendar_day))
            begin = calendar_day.add_full (0, 0, 0, -calendar_day.hour(), -calendar_day.minute(), -calendar_day.seconds());
        else
            begin = now.add_full (0, 0, 0, 0, 0, -now.seconds());

        std::vector<Appointment> upcoming;
        for(const auto& a : m_state->calendar_upcoming->appointments().get())
            if (begin <= a.begin)
                upcoming.push_back(a);
 
        if (m_upcoming != upcoming)
        {
            m_upcoming.swap(upcoming);
            update_header(); // show an 'alarm' icon if there are upcoming alarms
            update_section(Appointments); // "upcoming" is the list of Appointments we show
        }
    }

    std::shared_ptr<const State> m_state;
    std::shared_ptr<Actions> m_actions;
    std::shared_ptr<const Formatter> m_formatter;
    GMenu* m_submenu = nullptr;

    GVariant* get_serialized_alarm_icon()
    {
        if (G_UNLIKELY(m_serialized_alarm_icon == nullptr))
        {
            auto i = g_themed_icon_new_with_default_fallbacks(ALARM_ICON_NAME);
            m_serialized_alarm_icon = g_icon_serialize(i);
            g_object_unref(i);
        }

        return m_serialized_alarm_icon;
    }

    std::vector<Appointment> m_upcoming;

private:

    GVariant* get_serialized_calendar_icon()
    {
        if (G_UNLIKELY(m_serialized_calendar_icon == nullptr))
        {
            auto i = g_themed_icon_new_with_default_fallbacks(CALENDAR_ICON_NAME);
            m_serialized_calendar_icon = g_icon_serialize(i);
            g_object_unref(i);
        }

        return m_serialized_calendar_icon;
    }

    void create_gmenu()
    {
        g_assert(m_submenu == nullptr);

        m_submenu = g_menu_new();

        // build placeholders for the sections
        for(int i=0; i<NUM_SECTIONS; i++)
        {
            GMenuItem * item = g_menu_item_new(nullptr, nullptr);
            g_menu_append_item(m_submenu, item);
            g_object_unref(item);
        }

        // add submenu to the header
        const auto detailed_action = std::string("indicator.") + name() + "-header";
        auto header = g_menu_item_new(nullptr, detailed_action.c_str());
        g_menu_item_set_attribute(header, "x-canonical-type", "s",
                                  "com.canonical.indicator.root");
        g_menu_item_set_attribute(header, "submenu-action", "s",
                                  "indicator.calendar-active");
        g_menu_item_set_submenu(header, G_MENU_MODEL(m_submenu));
        g_object_unref(m_submenu);

        // add header to the menu
        m_menu = g_menu_new();
        g_menu_append_item(m_menu, header);
        g_object_unref(header);
    }

    GMenuModel* create_calendar_section(Profile profile)
    {
        const bool show_calendar = m_state->settings->show_calendar.get() &&
                                   ((profile == Desktop) || (profile == DesktopGreeter));
        auto menu = g_menu_new();

        const char * action_name;

        if (profile == Phone)
            action_name = "indicator.phone.open-calendar-app";
        else if (profile == Desktop)
            action_name = "indicator.desktop.open-calendar-app";
        else
            action_name = nullptr;

        /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
           Format string for the day on the first menuitem in the datetime indicator.
           This format string gives the full weekday, date, month, and year.
           en_US example: "%A, %B %e %Y" --> Saturday, October 31 2020"
           en_GB example: "%A, %e %B %Y" --> Saturday, 31 October 2020" */
        auto label = m_state->clock->localtime().format(_("%A, %e %B %Y"));
        auto item = g_menu_item_new (label.c_str(), nullptr);
        auto v = get_serialized_calendar_icon();
        g_menu_item_set_attribute_value (item, G_MENU_ATTRIBUTE_ICON, v);
        if (action_name != nullptr)
        {
            v = g_variant_new_int64(0);
            g_menu_item_set_action_and_target_value (item, action_name, v);
        }
        g_menu_append_item(menu, item);
        g_object_unref(item);

        // add calendar
        if (show_calendar)
        {
            item = g_menu_item_new ("[calendar]", nullptr);
            v = g_variant_new_int64(0);
            g_menu_item_set_action_and_target_value (item, "indicator.calendar", v);
            g_menu_item_set_attribute (item, "x-canonical-type",
                                       "s", "com.canonical.indicator.calendar");
            if (action_name != nullptr)
                g_menu_item_set_attribute (item, "activation-action", "s", action_name);
            g_menu_append_item (menu, item);
            g_object_unref (item);
        }

        return G_MENU_MODEL(menu);
    }

    void add_appointments(GMenu* menu, Profile profile)
    {
        const int MAX_APPTS = 5;
        std::set<std::string> added;

        const char * action_name;

        if (profile == Phone)
            action_name = "indicator.phone.open-appointment";
        else if ((profile == Desktop) && m_actions->desktop_has_calendar_app())
            action_name = "indicator.desktop.open-appointment";
        else
            action_name = nullptr;

        for (const auto& appt : m_upcoming)
        {
            // don't show duplicates
            if (added.count(appt.uid))
                continue;

            // don't show too many
            if (g_menu_model_get_n_items (G_MENU_MODEL(menu)) >= MAX_APPTS)
                break;

            added.insert(appt.uid);

            GDateTime* begin = appt.begin();
            GDateTime* end = appt.end();
            auto fmt = m_formatter->relative_format(begin, end);
            auto unix_time = g_date_time_to_unix(begin);

            auto menu_item = g_menu_item_new (appt.summary.c_str(), nullptr);
            g_menu_item_set_attribute (menu_item, "x-canonical-time", "x", unix_time);
            g_menu_item_set_attribute (menu_item, "x-canonical-time-format", "s", fmt.c_str());

            if (appt.has_alarms)
            {
                g_menu_item_set_attribute (menu_item, "x-canonical-type", "s", "com.canonical.indicator.alarm");
                g_menu_item_set_attribute_value(menu_item, G_MENU_ATTRIBUTE_ICON, get_serialized_alarm_icon());
            }
            else
            {
                g_menu_item_set_attribute (menu_item, "x-canonical-type", "s", "com.canonical.indicator.appointment");
            }

            if (!appt.color.empty())
                g_menu_item_set_attribute (menu_item, "x-canonical-color", "s", appt.color.c_str());

            if (action_name != nullptr)
                g_menu_item_set_action_and_target_value (menu_item, action_name,
                                                         g_variant_new_string (appt.uid.c_str()));

            g_menu_append_item (menu, menu_item);
            g_object_unref (menu_item);
        }
    }

    GMenuModel* create_appointments_section(Profile profile)
    {
        auto menu = g_menu_new();

        if ((profile==Desktop) && m_state->settings->show_events.get())
        {
            add_appointments (menu, profile);

            if (m_actions->desktop_has_calendar_app())
            {
                // add the 'Add Event…' menuitem
                auto menu_item = g_menu_item_new(_("Add Event…"), nullptr);
                const gchar* action_name = "indicator.desktop.open-calendar-app";
                auto v = g_variant_new_int64(0);
                g_menu_item_set_action_and_target_value(menu_item, action_name, v);
                g_menu_append_item(menu, menu_item);
                g_object_unref(menu_item);
            }
        }
        else if (profile==Phone)
        {
            auto menu_item = g_menu_item_new (_("Clock"), "indicator.phone.open-alarm-app");
            g_menu_item_set_attribute_value (menu_item, G_MENU_ATTRIBUTE_ICON, get_serialized_alarm_icon());
            g_menu_append_item (menu, menu_item);
            g_object_unref (menu_item);

            add_appointments (menu, profile);
        }

        return G_MENU_MODEL(menu);
    }

    GMenuModel* create_locations_section(Profile profile)
    {
        GMenu* menu = g_menu_new();

        if (profile == Desktop)
        {
            const auto now = m_state->clock->localtime();

            for(const auto& location : m_state->locations->locations.get())
            {
                const auto& zone = location.zone();
                const auto& name = location.name();
                const auto zone_now = now.to_timezone(zone);
                const auto fmt = m_formatter->relative_format(zone_now.get());
                auto detailed_action = g_strdup_printf("indicator.set-location::%s %s", zone.c_str(), name.c_str());
                auto i = g_menu_item_new (name.c_str(), detailed_action);
                g_menu_item_set_attribute(i, "x-canonical-type", "s", "com.canonical.indicator.location");
                g_menu_item_set_attribute(i, "x-canonical-timezone", "s", zone.c_str());
                g_menu_item_set_attribute(i, "x-canonical-time-format", "s", fmt.c_str());
                g_menu_append_item (menu, i);
                g_object_unref(i);
                g_free(detailed_action);
            }
        }

        return G_MENU_MODEL(menu);
    }

    GMenuModel* create_settings_section(Profile profile)
    {
        auto menu = g_menu_new();
        const char * action_name;

        if (profile == Desktop)
            action_name = "indicator.desktop.open-settings-app";
        else if (profile == Phone)
            action_name = "indicator.phone.open-settings-app";
        else
            action_name = nullptr;

        if (action_name != nullptr)
            g_menu_append (menu, _("Time & Date settings…"), action_name);

        return G_MENU_MODEL (menu);
    }

    void update_section(Section section)
    {
        GMenuModel * model;
        const auto p = profile();

        switch (section)
        {
            case Calendar: model = create_calendar_section(p); break;
            case Appointments: model = create_appointments_section(p); break;
            case Locations: model = create_locations_section(p); break;
            case Settings: model = create_settings_section(p); break;
            default: model = nullptr; g_warn_if_reached();
        }

        if (model)
        {
            g_menu_remove(m_submenu, section);
            g_menu_insert_section(m_submenu, section, nullptr, model);
            g_object_unref(model);
        }
    }

//private:
    GVariant * m_serialized_alarm_icon = nullptr;
    GVariant * m_serialized_calendar_icon = nullptr;

}; // class MenuImpl



/***
****
***/

class DesktopBaseMenu: public MenuImpl
{
protected:
    DesktopBaseMenu(Menu::Profile profile_,
                    const std::string& name_,
                    std::shared_ptr<const State>& state_,
                    std::shared_ptr<Actions>& actions_):
        MenuImpl(profile_, name_, state_, actions_,
                 std::shared_ptr<const Formatter>(new DesktopFormatter(state_->clock, state_->settings)))
    {
        update_header();
    }

    GVariant* create_header_state()
    {
        const auto visible = m_state->settings->show_clock.get();
        const auto title = _("Date and Time");
        auto label = g_variant_new_string(m_formatter->header.get().c_str());

        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&b, "{sv}", "accessible-desc", label);
        g_variant_builder_add(&b, "{sv}", "label", label);
        g_variant_builder_add(&b, "{sv}", "title", g_variant_new_string(title));
        g_variant_builder_add(&b, "{sv}", "visible", g_variant_new_boolean(visible));
        return g_variant_builder_end(&b);
    }
};

class DesktopMenu: public DesktopBaseMenu
{
public:
    DesktopMenu(std::shared_ptr<const State>& state_, std::shared_ptr<Actions>& actions_):
        DesktopBaseMenu(Desktop,"desktop", state_, actions_) {}
};

class DesktopGreeterMenu: public DesktopBaseMenu
{
public:
    DesktopGreeterMenu(std::shared_ptr<const State>& state_, std::shared_ptr<Actions>& actions_):
        DesktopBaseMenu(DesktopGreeter,"desktop_greeter", state_, actions_) {}
};

class PhoneBaseMenu: public MenuImpl
{
protected:
    PhoneBaseMenu(Menu::Profile profile_,
                  const std::string& name_,
                  std::shared_ptr<const State>& state_,
                  std::shared_ptr<Actions>& actions_):
        MenuImpl(profile_, name_, state_, actions_,
                 std::shared_ptr<Formatter>(new PhoneFormatter(state_->clock)))
    {
        update_header();
    }

    GVariant* create_header_state()
    {
        // are there alarms?
        bool has_alarms = false;
        for(const auto& appointment : m_upcoming)
            if((has_alarms = appointment.has_alarms))
                break;

        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&b, "{sv}", "title", g_variant_new_string (_("Upcoming")));
        g_variant_builder_add(&b, "{sv}", "visible", g_variant_new_boolean (TRUE));
        if (has_alarms)
        {
            auto label = m_formatter->header.get();
            auto a11y = g_strdup_printf(_("%s (has alarms)"), label.c_str());
            g_variant_builder_add(&b, "{sv}", "label", g_variant_new_string(label.c_str()));
            g_variant_builder_add(&b, "{sv}", "accessible-desc", g_variant_new_take_string(a11y));
            g_variant_builder_add(&b, "{sv}", "icon", get_serialized_alarm_icon());
        }
        else
        {
            auto v = g_variant_new_string(m_formatter->header.get().c_str());
            g_variant_builder_add(&b, "{sv}", "label", v);
            g_variant_builder_add(&b, "{sv}", "accessible-desc", v);
        }
        return g_variant_builder_end (&b);
    }
};

class PhoneMenu: public PhoneBaseMenu
{
public:
    PhoneMenu(std::shared_ptr<const State>& state_,
              std::shared_ptr<Actions>& actions_):
        PhoneBaseMenu(Phone, "phone", state_, actions_) {}
};

class PhoneGreeterMenu: public PhoneBaseMenu
{
public:
    PhoneGreeterMenu(std::shared_ptr<const State>& state_,
                     std::shared_ptr<Actions>& actions_):
        PhoneBaseMenu(PhoneGreeter, "phone_greeter", state_, actions_) {}
};

/****
*****
****/

MenuFactory::MenuFactory(const std::shared_ptr<Actions>& actions_,
                         const std::shared_ptr<const State>& state_):
    m_actions(actions_),
    m_state(state_)
{
}

std::shared_ptr<Menu>
MenuFactory::buildMenu(Menu::Profile profile)
{
    std::shared_ptr<Menu> menu;

    switch (profile)
    {
    case Menu::Desktop:
        menu.reset(new DesktopMenu(m_state, m_actions));
        break;

    case Menu::DesktopGreeter:
        menu.reset(new DesktopGreeterMenu(m_state, m_actions));
        break;

    case Menu::Phone:
        menu.reset(new PhoneMenu(m_state, m_actions));
        break;

    case Menu::PhoneGreeter:
        menu.reset(new PhoneGreeterMenu(m_state, m_actions));
        break;

    default:
        g_warn_if_reached();
        break;
    }
    
    return menu;
}

/****
*****
****/

} // namespace datetime
} // namespace indicator
} // namespace unity
