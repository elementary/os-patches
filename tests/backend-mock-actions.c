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

#include <glib.h>
#include <gio/gio.h>

#include "backend-mock.h"
#include "backend-mock-actions.h"

G_DEFINE_TYPE (IndicatorSessionActionsMock,
               indicator_session_actions_mock,
               INDICATOR_TYPE_SESSION_ACTIONS)

/***
****  Virtual Functions
***/

static gboolean
my_can_lock (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "can-lock");
}

static gboolean
my_can_logout (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "can-logout");
}

static gboolean
my_can_reboot (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "can-reboot");
}

static gboolean
my_can_switch (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "can-switch-sessions");
}

static gboolean
my_can_suspend (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "can-suspend");
}

static gboolean
my_can_hibernate (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "can-hibernate");
}

static void
my_logout (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "logout");
}

static void
my_suspend (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "suspend");
}

static void
my_hibernate (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "hibernate");
}

static void
my_reboot (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "reboot");
}

static void
my_power_off (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "power-off");
}

static void
my_switch_to_screensaver (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "switch-to-screensaver");
}

static void
my_switch_to_greeter (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "switch-to-greeter");
}

static void
my_switch_to_guest (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "switch-to-guest");
}

static void
my_switch_to_username (IndicatorSessionActions * self G_GNUC_UNUSED,
                       const char * username)
{
  gchar * str = g_strdup_printf ("switch-to-user::%s", username);
  g_settings_set_string (mock_settings, "last-command", str);
}

static void
my_help (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "help");
}

static void
my_about (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "about");
}

static void
my_settings (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "settings");
}

static void
my_online_accounts (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  g_settings_set_string (mock_settings, "last-command", "online-accounts");
}

static gboolean
my_can_prompt (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "can-prompt");
}

static gboolean
my_has_online_account_error (IndicatorSessionActions * self G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "has-online-account-error");
}

static void
my_dispose (GObject * o)
{
  G_OBJECT_CLASS (indicator_session_actions_mock_parent_class)->dispose (o);
}

static void
my_finalize (GObject * o)
{
  G_OBJECT_CLASS (indicator_session_actions_mock_parent_class)->finalize (o);
}

/***
****  GObject Boilerplate
***/

static void
/* cppcheck-suppress unusedFunction */
indicator_session_actions_mock_class_init (IndicatorSessionActionsMockClass * klass)
{
  GObjectClass * object_class;
  IndicatorSessionActionsClass * actions_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = my_dispose;
  object_class->finalize = my_finalize;

  actions_class = INDICATOR_SESSION_ACTIONS_CLASS (klass);
  actions_class->can_lock = my_can_lock;
  actions_class->can_logout = my_can_logout;
  actions_class->can_reboot = my_can_reboot;
  actions_class->can_switch = my_can_switch;
  actions_class->can_suspend = my_can_suspend;
  actions_class->can_hibernate = my_can_hibernate;
  actions_class->can_prompt = my_can_prompt;
  actions_class->has_online_account_error = my_has_online_account_error;
  actions_class->logout = my_logout;
  actions_class->suspend = my_suspend;
  actions_class->hibernate = my_hibernate;
  actions_class->reboot = my_reboot;
  actions_class->power_off = my_power_off;
  actions_class->settings = my_settings;
  actions_class->online_accounts = my_online_accounts;
  actions_class->help = my_help;
  actions_class->about = my_about;
  actions_class->switch_to_screensaver = my_switch_to_screensaver;
  actions_class->switch_to_greeter = my_switch_to_greeter;
  actions_class->switch_to_guest = my_switch_to_guest;
  actions_class->switch_to_username = my_switch_to_username;
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_actions_mock_init (IndicatorSessionActionsMock * self)
{
  g_signal_connect_swapped (mock_settings, "changed::can-lock",
                            G_CALLBACK(indicator_session_actions_notify_can_lock), self);
  g_signal_connect_swapped (mock_settings, "changed::can-logout",
                            G_CALLBACK(indicator_session_actions_notify_can_logout), self);
  g_signal_connect_swapped (mock_settings, "changed::can-switch-sessions",
                            G_CALLBACK(indicator_session_actions_notify_can_switch), self);
  g_signal_connect_swapped (mock_settings, "changed::can-suspend",
                            G_CALLBACK(indicator_session_actions_notify_can_suspend), self);
  g_signal_connect_swapped (mock_settings, "changed::can-hibernate",
                            G_CALLBACK(indicator_session_actions_notify_can_hibernate), self);
  g_signal_connect_swapped (mock_settings, "changed::can-prompt",
                            G_CALLBACK(indicator_session_actions_notify_can_prompt), self);
  g_signal_connect_swapped (mock_settings, "changed::has-online-account-error",
                            G_CALLBACK(indicator_session_actions_notify_has_online_account_error), self);
}

/***
****  Public
***/

IndicatorSessionActions *
indicator_session_actions_mock_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_SESSION_ACTIONS_MOCK, NULL);

  return INDICATOR_SESSION_ACTIONS (o);
}
