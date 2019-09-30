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

#include "backend-mock.h"
#include "backend-mock-users.h"

struct _IndicatorSessionUsersMockPriv
{
  GHashTable * users;
};

typedef IndicatorSessionUsersMockPriv priv_t;

G_DEFINE_TYPE (IndicatorSessionUsersMock,
               indicator_session_users_mock,
               INDICATOR_TYPE_SESSION_USERS)

/***
****  IndicatorSessionUsers virtual functions
***/

static gboolean
my_is_live_session (IndicatorSessionUsers * users G_GNUC_UNUSED)
{
  return g_settings_get_boolean (mock_settings, "is-live-session");
}

static void
my_activate_user (IndicatorSessionUsers * users, guint uid)
{
  g_message ("%s %s users %p uid %u FIXME", G_STRLOC, G_STRFUNC, (void*)users, uid);
}

static GList *
my_get_uids (IndicatorSessionUsers * users)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_USERS_MOCK(users), NULL);

  return g_hash_table_get_keys (INDICATOR_SESSION_USERS_MOCK(users)->priv->users);
}

static IndicatorSessionUser *
my_get_user (IndicatorSessionUsers * self, guint uid)
{
  priv_t * p;
  const IndicatorSessionUser * src;
  IndicatorSessionUser * ret = NULL;

  g_return_val_if_fail (INDICATOR_IS_SESSION_USERS_MOCK(self), NULL);
  p = INDICATOR_SESSION_USERS_MOCK (self)->priv;

  if ((src = g_hash_table_lookup (p->users, GUINT_TO_POINTER(uid))))
    {
      ret = g_new0 (IndicatorSessionUser, 1);
      ret->is_current_user = src->is_current_user;
      ret->is_logged_in = src->is_logged_in;
      ret->uid = src->uid;
      ret->login_frequency = src->login_frequency;
      ret->user_name = g_strdup (src->user_name);
      ret->real_name = g_strdup (src->real_name);
      ret->icon_file = g_strdup (src->icon_file);
    }

  return ret;
}

/***
****  GObject virtual functions
***/

static void
my_dispose (GObject * o)
{
  G_OBJECT_CLASS (indicator_session_users_mock_parent_class)->dispose (o);
}

static void
my_finalize (GObject * o)
{
  priv_t * p = INDICATOR_SESSION_USERS_MOCK (o)->priv;

  g_hash_table_destroy (p->users);

  G_OBJECT_CLASS (indicator_session_users_mock_parent_class)->finalize (o);
}

/***
****  GObject boilerplate
***/

static void
/* cppcheck-suppress unusedFunction */
indicator_session_users_mock_class_init (IndicatorSessionUsersMockClass * klass)
{
  GObjectClass * object_class;
  IndicatorSessionUsersClass * users_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = my_dispose;
  object_class->finalize = my_finalize;

  users_class = INDICATOR_SESSION_USERS_CLASS (klass);
  users_class->is_live_session = my_is_live_session;
  users_class->get_uids = my_get_uids;
  users_class->get_user = my_get_user;
  users_class->activate_user = my_activate_user;

  g_type_class_add_private (klass, sizeof (IndicatorSessionUsersMockPriv));
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_users_mock_init (IndicatorSessionUsersMock * self)
{
  priv_t * p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_SESSION_USERS_MOCK,
                                   IndicatorSessionUsersMockPriv);
  self->priv = p;

  p->users = g_hash_table_new_full (g_direct_hash,
                                    g_direct_equal,
                                    NULL,
                                    (GDestroyNotify)indicator_session_user_free);

  g_signal_connect_swapped (mock_settings, "changed::is-live-session",
                            G_CALLBACK(indicator_session_users_notify_is_live_session), self);
}

/***
****  Public
***/

IndicatorSessionUsers *
indicator_session_users_mock_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_SESSION_USERS_MOCK, NULL);

  return INDICATOR_SESSION_USERS (o);
}


void
indicator_session_users_mock_add_user (IndicatorSessionUsersMock * self,
                                       IndicatorSessionUser      * user)
{
  g_return_if_fail (INDICATOR_IS_SESSION_USERS_MOCK (self));
  g_return_if_fail (user != NULL);
  g_return_if_fail (user->uid > 0);
  g_return_if_fail (!g_hash_table_contains (self->priv->users, GUINT_TO_POINTER(user->uid)));

  g_hash_table_insert (self->priv->users, GUINT_TO_POINTER(user->uid), user);
  indicator_session_users_added (INDICATOR_SESSION_USERS (self), user->uid);
}

void
indicator_session_users_mock_remove_user (IndicatorSessionUsersMock * self,
                                          guint                       uid)
{
  g_return_if_fail (INDICATOR_IS_SESSION_USERS_MOCK (self));
  g_return_if_fail (uid > 0);

  g_hash_table_remove (self->priv->users, GUINT_TO_POINTER(uid));
  indicator_session_users_removed (INDICATOR_SESSION_USERS (self), uid);
}

