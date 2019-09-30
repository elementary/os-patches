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

#include "users.h"

/* signals enum */
enum
{
  USER_ADDED,
  USER_REMOVED,
  USER_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (IndicatorSessionUsers, indicator_session_users, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_IS_LIVE_SESSION,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IndicatorSessionUsers * self = INDICATOR_SESSION_USERS (o);

  switch (property_id)
    {
      case PROP_IS_LIVE_SESSION:
        g_value_set_boolean (value, indicator_session_users_is_live_session (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
    }
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_users_class_init (IndicatorSessionUsersClass * klass)
{
  GObjectClass * object_class;
  const GParamFlags flags = G_PARAM_READABLE | G_PARAM_STATIC_STRINGS;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = my_get_property;

  signals[USER_ADDED] = g_signal_new (INDICATOR_SESSION_USERS_SIGNAL_USER_ADDED,
                                      G_TYPE_FROM_CLASS(klass),
                                      G_SIGNAL_RUN_LAST,
                                      G_STRUCT_OFFSET (IndicatorSessionUsersClass, user_added),
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__UINT,
                                      G_TYPE_NONE, 1, G_TYPE_UINT);

  signals[USER_REMOVED] = g_signal_new (INDICATOR_SESSION_USERS_SIGNAL_USER_REMOVED,
                                        G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (IndicatorSessionUsersClass, user_removed),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__UINT,
                                        G_TYPE_NONE, 1, G_TYPE_UINT);

  signals[USER_CHANGED] = g_signal_new (INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED,
                                        G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (IndicatorSessionUsersClass, user_changed),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__UINT,
                                        G_TYPE_NONE, 1, G_TYPE_UINT);


  properties[PROP_IS_LIVE_SESSION] =
    g_param_spec_boolean (INDICATOR_SESSION_USERS_PROP_IS_LIVE_SESSION,
                          "Is Live Session",
                          "Whether or this is a 'live session', such as booting from a live CD",
                          FALSE, flags);

  g_object_class_install_properties (object_class, PROP_LAST, properties);

}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_users_init (IndicatorSessionUsers * self G_GNUC_UNUSED)
{
}

/***
****  Virtual Functions
***/

GList *
indicator_session_users_get_uids (IndicatorSessionUsers * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_USERS (self), NULL);

  return INDICATOR_SESSION_USERS_GET_CLASS (self)->get_uids (self);
}

IndicatorSessionUser *
indicator_session_users_get_user (IndicatorSessionUsers * self,
                                  guint                   uid)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_USERS (self), NULL);

  return INDICATOR_SESSION_USERS_GET_CLASS (self)->get_user (self, uid);
}

void
indicator_session_users_activate_user (IndicatorSessionUsers * self, 
                                       guint                   uid)
{
  g_return_if_fail (INDICATOR_IS_SESSION_USERS (self));

  INDICATOR_SESSION_USERS_GET_CLASS (self)->activate_user (self, uid);
}

gboolean
indicator_session_users_is_live_session (IndicatorSessionUsers * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_USERS (self), FALSE);

  return INDICATOR_SESSION_USERS_GET_CLASS (self)->is_live_session (self);
}

void
indicator_session_user_free (IndicatorSessionUser * user)
{
  g_return_if_fail (user != NULL);

  g_free (user->real_name);
  g_free (user->user_name);
  g_free (user->icon_file);
  g_free (user);
}

/***
****  Signal Convenience
***/

void
indicator_session_users_added (IndicatorSessionUsers * self, guint uid)
{
  g_return_if_fail (INDICATOR_IS_SESSION_USERS (self));

  g_signal_emit (self, signals[USER_ADDED], 0, uid);
}

void
indicator_session_users_removed (IndicatorSessionUsers * self, guint uid)
{
  g_return_if_fail (INDICATOR_IS_SESSION_USERS (self));

  g_signal_emit (self, signals[USER_REMOVED], 0, uid);
}

void
indicator_session_users_changed (IndicatorSessionUsers * self, guint uid)
{
  g_return_if_fail (INDICATOR_IS_SESSION_USERS (self));

  g_signal_emit (self, signals[USER_CHANGED], 0, uid);
}

void
indicator_session_users_notify_is_live_session (IndicatorSessionUsers * self)
{
  g_return_if_fail (INDICATOR_IS_SESSION_USERS (self));

  g_object_notify_by_pspec (G_OBJECT(self), properties[PROP_IS_LIVE_SESSION]);
}

