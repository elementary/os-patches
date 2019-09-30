
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

#include "actions.h"

/***
****  GObject Boilerplate
***/

G_DEFINE_TYPE (IndicatorSessionActions, indicator_session_actions, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_CAN_SWITCH,
  PROP_CAN_HIBERNATE,
  PROP_CAN_SUSPEND,
  PROP_CAN_LOCK,
  PROP_CAN_LOGOUT,
  PROP_CAN_REBOOT,
  PROP_CAN_PROMPT,
  PROP_HAS_ONLINE_ACCOUNT_ERROR,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IndicatorSessionActions * self = INDICATOR_SESSION_ACTIONS (o);

  switch (property_id)
    {
      case PROP_CAN_SWITCH:
        g_value_set_boolean (value, indicator_session_actions_can_switch (self));
        break;

      case PROP_CAN_HIBERNATE:
        g_value_set_boolean (value, indicator_session_actions_can_hibernate (self));
        break;

      case PROP_CAN_SUSPEND:
        g_value_set_boolean (value, indicator_session_actions_can_suspend (self));
        break;

      case PROP_CAN_LOCK:
        g_value_set_boolean (value, indicator_session_actions_can_lock (self));
        break;

      case PROP_CAN_LOGOUT:
        g_value_set_boolean (value, indicator_session_actions_can_logout (self));
        break;

      case PROP_CAN_REBOOT:
        g_value_set_boolean (value, indicator_session_actions_can_reboot (self));
        break;

      case PROP_CAN_PROMPT:
        g_value_set_boolean (value, indicator_session_actions_can_prompt (self));
        break;

      case PROP_HAS_ONLINE_ACCOUNT_ERROR:
        g_value_set_boolean (value, indicator_session_actions_has_online_account_error (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_actions_class_init (IndicatorSessionActionsClass * klass)
{
  GObjectClass * object_class;
  const GParamFlags flags = G_PARAM_READABLE | G_PARAM_STATIC_STRINGS;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = my_get_property;

  klass->can_lock = NULL;
  klass->can_logout = NULL;
  klass->can_reboot = NULL;
  klass->can_switch = NULL;
  klass->can_suspend = NULL;
  klass->can_hibernate = NULL;
  klass->has_online_account_error = NULL;
  klass->logout = NULL;
  klass->suspend = NULL;
  klass->hibernate = NULL;
  klass->reboot = NULL;
  klass->power_off = NULL;
  klass->switch_to_screensaver = NULL;
  klass->switch_to_greeter = NULL;
  klass->switch_to_guest = NULL;
  klass->switch_to_username = NULL;

  /* properties */

  properties[PROP_0] = NULL;

  properties[PROP_CAN_SWITCH] =
    g_param_spec_boolean (INDICATOR_SESSION_ACTIONS_PROP_CAN_SWITCH,
                          "Can Switch Sessions",
                          "Whether or not the system services allow session switching",
                          TRUE, flags);

  properties[PROP_CAN_HIBERNATE] =
    g_param_spec_boolean (INDICATOR_SESSION_ACTIONS_PROP_CAN_HIBERNATE,
                          "Can Hibernate",
                          "Whether or not the system services allow the user to hibernate",
                          TRUE, flags);

  properties[PROP_CAN_SUSPEND] =
    g_param_spec_boolean (INDICATOR_SESSION_ACTIONS_PROP_CAN_SUSPEND,
                          "Can Suspend",
                          "Whether or not the system services allow the user to suspend",
                          TRUE, flags);

  properties[PROP_CAN_LOCK] =
    g_param_spec_boolean (INDICATOR_SESSION_ACTIONS_PROP_CAN_LOCK,
                          "Can Lock",
                          "Whether or not the system services allow the user to lock the screen",
                          TRUE, flags);

  properties[PROP_CAN_LOGOUT] =
    g_param_spec_boolean (INDICATOR_SESSION_ACTIONS_PROP_CAN_LOGOUT,
                          "Can Logout",
                          "Whether or not the system services allow the user to logout",
                          TRUE, flags);

  properties[PROP_CAN_REBOOT] =
    g_param_spec_boolean (INDICATOR_SESSION_ACTIONS_PROP_CAN_REBOOT,
                          "Can Reboot",
                          "Whether or not the system services allow the user to reboot",
                          TRUE, flags);

  properties[PROP_CAN_PROMPT] =
    g_param_spec_boolean (INDICATOR_SESSION_ACTIONS_PROP_CAN_PROMPT,
                          "Can Show End Session Dialog",
                          "Whether or not we can show an End Session dialog",
                          TRUE, flags);

  properties[PROP_HAS_ONLINE_ACCOUNT_ERROR] =
    g_param_spec_boolean (INDICATOR_SESSION_ACTIONS_PROP_HAS_ONLINE_ACCOUNT_ERROR,
                          "Has Online Account Error",
                          "Whether or not an online account setting requires attention from the user",
                          FALSE, flags);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_actions_init (IndicatorSessionActions * self G_GNUC_UNUSED)
{
}

/***
**** 
***/

gboolean
indicator_session_actions_can_lock (IndicatorSessionActions * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_ACTIONS (self), FALSE);

  return INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->can_lock (self);
}

gboolean
indicator_session_actions_can_logout (IndicatorSessionActions * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_ACTIONS (self), FALSE);

  return INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->can_logout (self);
}

gboolean
indicator_session_actions_can_reboot (IndicatorSessionActions * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_ACTIONS (self), FALSE);

  return INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->can_reboot (self);
}

gboolean
indicator_session_actions_can_switch (IndicatorSessionActions * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_ACTIONS (self), FALSE);

  return INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->can_switch (self);
}

gboolean
indicator_session_actions_can_suspend (IndicatorSessionActions * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_ACTIONS (self), FALSE);

  return INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->can_suspend (self);
}

gboolean
indicator_session_actions_can_hibernate (IndicatorSessionActions * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_ACTIONS (self), FALSE);

  return INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->can_hibernate (self);
}

gboolean
indicator_session_actions_can_prompt (IndicatorSessionActions * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_ACTIONS (self), FALSE);

  return INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->can_prompt (self);
}

gboolean
indicator_session_actions_has_online_account_error (IndicatorSessionActions * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_ACTIONS (self), FALSE);

  return INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->has_online_account_error (self);
}

/***
****
***/

void
indicator_session_actions_online_accounts (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->online_accounts (self);
}

void
indicator_session_actions_settings (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->settings (self);
}

void
indicator_session_actions_logout (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->logout (self);
}

void
indicator_session_actions_power_off (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->power_off (self);
}

void
indicator_session_actions_help (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->help (self);
}

void
indicator_session_actions_about (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->about (self);
}

void
indicator_session_actions_reboot (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->reboot (self);
}

void
indicator_session_actions_suspend (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->suspend (self);
}

void
indicator_session_actions_hibernate (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->hibernate (self);
}

void
indicator_session_actions_switch_to_screensaver (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->switch_to_screensaver (self);
}

void
indicator_session_actions_switch_to_greeter (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->switch_to_greeter (self);
}

void
indicator_session_actions_switch_to_guest (IndicatorSessionActions * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->switch_to_guest (self);
}

void
indicator_session_actions_switch_to_username (IndicatorSessionActions * self,
                                              const gchar             * username)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  INDICATOR_SESSION_ACTIONS_GET_CLASS (self)->switch_to_username (self, username);
}

/***
****
***/

static void
notify_func (IndicatorSessionActions * self, int prop)
{
  g_return_if_fail (INDICATOR_IS_SESSION_ACTIONS (self));

  g_debug ("%s %s emitting '%s' prop notify", G_STRLOC, G_STRFUNC, properties[prop]->name);

  g_object_notify_by_pspec (G_OBJECT(self), properties[prop]);
}

void
indicator_session_actions_notify_can_lock (IndicatorSessionActions * self)
{
  notify_func (self, PROP_CAN_LOCK);
}

void
indicator_session_actions_notify_can_logout (IndicatorSessionActions * self)
{
  notify_func (self, PROP_CAN_LOGOUT);
}

void
indicator_session_actions_notify_can_reboot (IndicatorSessionActions * self)
{
  notify_func (self, PROP_CAN_REBOOT);
}

void
indicator_session_actions_notify_can_switch (IndicatorSessionActions * self)
{
  notify_func (self, PROP_CAN_SWITCH);
}

void
indicator_session_actions_notify_can_suspend (IndicatorSessionActions * self)
{
  notify_func (self, PROP_CAN_SUSPEND);
}

void
indicator_session_actions_notify_can_hibernate (IndicatorSessionActions * self)
{
  notify_func (self, PROP_CAN_HIBERNATE);
}

void
indicator_session_actions_notify_can_prompt (IndicatorSessionActions * self)
{
  notify_func (self, PROP_CAN_PROMPT);
}

void
indicator_session_actions_notify_has_online_account_error (IndicatorSessionActions * self)
{
  notify_func (self, PROP_HAS_ONLINE_ACCOUNT_ERROR);
}
