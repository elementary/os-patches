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

#ifndef __USERS_H__
#define __USERS_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define INDICATOR_TYPE_SESSION_USERS          (indicator_session_users_get_type())
#define INDICATOR_SESSION_USERS(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_USERS, IndicatorSessionUsers))
#define INDICATOR_SESSION_USERS_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_USERS, IndicatorSessionUsersClass))
#define INDICATOR_SESSION_USERS_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), INDICATOR_TYPE_SESSION_USERS, IndicatorSessionUsersClass))
#define INDICATOR_IS_SESSION_USERS(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_USERS))


typedef struct _IndicatorSessionUser         IndicatorSessionUser;
typedef struct _IndicatorSessionUsers        IndicatorSessionUsers;
typedef struct _IndicatorSessionUsersClass   IndicatorSessionUsersClass;

/**
 * A base class for monitoring the system's users and active sessions.
 * Use backend.h's get_backend() to get an instance.
 */
struct _IndicatorSessionUsers
{
  /*< private >*/
  GObject parent;
};

struct _IndicatorSessionUser
{
  gboolean is_current_user;
  gboolean is_logged_in;
  guint uid;
  guint64 login_frequency;
  gchar * user_name;
  gchar * real_name;
  gchar * icon_file;
};

/* signal keys */
#define INDICATOR_SESSION_USERS_SIGNAL_USER_ADDED   "user-added"
#define INDICATOR_SESSION_USERS_SIGNAL_USER_REMOVED "user-removed"
#define INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED "user-changed"

/* property keys */
#define INDICATOR_SESSION_USERS_PROP_IS_LIVE_SESSION "is-live-session"

struct _IndicatorSessionUsersClass
{
  GObjectClass parent_class;

  /* signals */

  void (* user_added)    (IndicatorSessionUsers * self,
                          guint                   uid);

  void (* user_removed)   (IndicatorSessionUsers * self,
                           guint                   uid);

  void (* user_changed)   (IndicatorSessionUsers * self,
                           guint                   uid);


  /* pure virtual functions */

  gboolean               (* is_live_session) (IndicatorSessionUsers * self);


  GList*                 (* get_uids)        (IndicatorSessionUsers * self);

  IndicatorSessionUser * (* get_user)        (IndicatorSessionUsers * self,
                                              guint                   uid);

  void                   ( * activate_user)  (IndicatorSessionUsers * self,
                                              guint                   uid);
};

/***
****
***/

GType indicator_session_users_get_type (void);

/* emits the "user-added" signal */
void indicator_session_users_added   (IndicatorSessionUsers * self,
                                      guint                   uid);

/* emits the "user-removed" signal */
void indicator_session_users_removed (IndicatorSessionUsers * self,
                                      guint                   uid);

/* emits the "user-changed" signal */
void indicator_session_users_changed (IndicatorSessionUsers * self,
                                      guint                   uid);

/* notify listeners of a change to the 'is-live-session' property */
void indicator_session_users_notify_is_live_session (IndicatorSessionUsers * self);



/***
****
***/

gboolean indicator_session_users_is_live_session (IndicatorSessionUsers * users);

/**
 * Get a list of the users to show in the indicator
 *
 * Return value: (transfer container): a GList of guint user ids.
 * Free with g_slist_free() when done.
 */
GList * indicator_session_users_get_uids (IndicatorSessionUsers * users);

/**
 * Get information about a particular user.
 *
 * Return value: (transfer full): an IndicatorSessionUser struct
 * populated with information about the specified user.
 * Free with indicator_session_user_free() when done.
 */
IndicatorSessionUser *
indicator_session_users_get_user (IndicatorSessionUsers * users,
                                  guint                   uid);

/* frees a IndicatorSessionUser struct */
void indicator_session_user_free (IndicatorSessionUser * user);

/* activate to a different session */
void indicator_session_users_activate_user (IndicatorSessionUsers * self,
                                            guint                   uid);


G_END_DECLS

#endif
