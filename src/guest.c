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

#include "guest.h"

G_DEFINE_TYPE (IndicatorSessionGuest,
               indicator_session_guest,
               G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_ALLOWED,
  PROP_LOGGED_IN,
  PROP_ACTIVE,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IndicatorSessionGuest * self = INDICATOR_SESSION_GUEST (o);

  switch (property_id)
    {
      case PROP_ALLOWED:
        g_value_set_boolean (value, indicator_session_guest_is_allowed (self));
        break;

      case PROP_LOGGED_IN:
        g_value_set_boolean (value, indicator_session_guest_is_logged_in (self));
        break;

      case PROP_ACTIVE:
        g_value_set_boolean (value, indicator_session_guest_is_active (self));
        break;
         
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
my_dispose (GObject *object)
{
  G_OBJECT_CLASS (indicator_session_guest_parent_class)->dispose (object);
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_guest_class_init (IndicatorSessionGuestClass * klass)
{
  GObjectClass * object_class;
  const GParamFlags flags = G_PARAM_READABLE | G_PARAM_STATIC_STRINGS;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = my_get_property;
  object_class->dispose = my_dispose;

  klass->is_allowed = NULL;
  klass->is_logged_in = NULL;
  klass->is_active = NULL;
  klass->switch_to_guest = NULL;

  properties[PROP_0] = NULL;

  properties[PROP_ALLOWED] =
    g_param_spec_boolean (INDICATOR_SESSION_GUEST_PROPERTY_ALLOWED,
                          "Is Allowed",
                          "Whether or not a Guest user is allowed",
                          FALSE, flags);

  properties[PROP_LOGGED_IN] =
    g_param_spec_boolean (INDICATOR_SESSION_GUEST_PROPERTY_LOGGED_IN,
                          "Is Logged In",
                          "Whether or not the Guest account is logged in",
                          FALSE, flags);

  properties[PROP_ACTIVE] =
    g_param_spec_boolean (INDICATOR_SESSION_GUEST_PROPERTY_ACTIVE,
                          "Is Active",
                          "If the Guest account has the current session",
                          FALSE, flags);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_guest_init (IndicatorSessionGuest *self G_GNUC_UNUSED)
{
}

/***
****
***/

gboolean
indicator_session_guest_is_active (IndicatorSessionGuest * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_GUEST (self), FALSE);

  return INDICATOR_SESSION_GUEST_GET_CLASS (self)->is_active (self);
}

gboolean
indicator_session_guest_is_allowed (IndicatorSessionGuest * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_GUEST (self), FALSE);

  return INDICATOR_SESSION_GUEST_GET_CLASS (self)->is_allowed (self);
}

gboolean
indicator_session_guest_is_logged_in (IndicatorSessionGuest * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_GUEST (self), FALSE);

  return INDICATOR_SESSION_GUEST_GET_CLASS (self)->is_logged_in (self);
}

/***
****
***/
static void
notify_func (IndicatorSessionGuest * self, int prop)
{
  g_return_if_fail (INDICATOR_IS_SESSION_GUEST (self));

  g_debug ("%s %s emitting '%s' prop notify", G_STRLOC, G_STRFUNC, properties[prop]->name);

  g_object_notify_by_pspec (G_OBJECT(self), properties[prop]);
}

void
indicator_session_guest_notify_active (IndicatorSessionGuest * self)
{
  notify_func (self, PROP_ACTIVE);
}

void
indicator_session_guest_notify_allowed (IndicatorSessionGuest * self)
{
  notify_func (self, PROP_ALLOWED);
}

void
indicator_session_guest_notify_logged_in (IndicatorSessionGuest * self)
{
  notify_func (self, PROP_LOGGED_IN);
}

/***
****
***/

void
indicator_session_guest_switch_to_guest (IndicatorSessionGuest * self)
{
  gboolean allowed;

  g_return_if_fail (INDICATOR_IS_SESSION_GUEST (self));

  g_object_get (self, INDICATOR_SESSION_GUEST_PROPERTY_ALLOWED, &allowed, NULL);
  g_return_if_fail (allowed);

  INDICATOR_SESSION_GUEST_GET_CLASS (self)->switch_to_guest (self);
}

