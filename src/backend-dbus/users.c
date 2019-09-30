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

#include "dbus-user.h"

#include "users.h"

struct _IndicatorSessionUsersDbusPriv
{
  Login1Manager * login1_manager;
  Login1Seat * login1_seat;
  DisplayManagerSeat * dm_seat;
  Accounts * accounts;

  /* hash table of int uids to UserRecord* */
  GHashTable * uid_to_account;

  /* a hashset of int uids of users who are logged in */
  GHashTable * logins;

  /* the user-id of the owner of the active session */
  guint active_uid;

  /* true if this is a live session */
  gboolean is_live;

  GCancellable * cancellable;

  guint update_list_tag;
};

typedef IndicatorSessionUsersDbusPriv priv_t;

G_DEFINE_TYPE (IndicatorSessionUsersDbus,
               indicator_session_users_dbus,
               INDICATOR_TYPE_SESSION_USERS)

/***
****
***/

struct UserRecord
{
  AccountsUser * user;

  gulong signal_id;
};

struct UserRecord *
user_record_new (AccountsUser * user, gulong signal_id)
{
  struct UserRecord * rec;
  rec = g_new (struct UserRecord, 1);
  rec->user = g_object_ref (user);
  rec->signal_id = signal_id;
  return rec;
}

static void
user_record_free (struct UserRecord * rec)
{
  g_signal_handler_disconnect (rec->user, rec->signal_id);
  g_object_unref (G_OBJECT (rec->user));
  g_free (rec);
}

/***
****
***/

/* get our private org.freedesktop.Accounts.User proxy for the given uid */
static AccountsUser *
get_user_for_uid (IndicatorSessionUsersDbus * self, guint uid)
{
  struct UserRecord * rec;

  if ((rec = g_hash_table_lookup (self->priv->uid_to_account,
                                  GUINT_TO_POINTER(uid))))
    return rec->user;

  return NULL;
}

static gboolean
is_tracked_uid (IndicatorSessionUsersDbus * self, guint uid)
{
  return get_user_for_uid (self, uid) != NULL;
}

static void
emit_user_added (IndicatorSessionUsersDbus * self, guint32 uid)
{
  if (is_tracked_uid (self, uid))
    indicator_session_users_added (INDICATOR_SESSION_USERS(self), uid);
}

static void
emit_user_changed (IndicatorSessionUsersDbus * self, guint32 uid)
{
  if (is_tracked_uid (self, uid))
    indicator_session_users_changed (INDICATOR_SESSION_USERS(self), uid);
}

static void
emit_user_removed (IndicatorSessionUsersDbus * self, guint32 uid)
{
  indicator_session_users_removed (INDICATOR_SESSION_USERS(self), uid);
}

/***
****
***/

static void
set_is_live_session_flag (IndicatorSessionUsersDbus * self, gboolean b)
{
  priv_t * p = self->priv;

  if (p->is_live != b)
    {
      p->is_live = b;

      indicator_session_users_notify_is_live_session (INDICATOR_SESSION_USERS (self));
    }
}

static void
set_active_uid (IndicatorSessionUsersDbus * self, guint uid)
{
  priv_t * p = self->priv;

  if (p->active_uid != uid)
    {
      const guint old_uid = p->active_uid;

      p->active_uid = uid;

      emit_user_changed (self, old_uid);
      emit_user_changed (self, uid);
    }
}

static void
set_logins (IndicatorSessionUsersDbus * self, GHashTable * logins)
{
  GHashTable * old_logins = self->priv->logins;
  gpointer uid;
  GHashTableIter iter;

  self->priv->logins = g_hash_table_ref (logins);

  /* fire 'user changed' event for users who logged out */
  g_hash_table_iter_init (&iter, old_logins);
  while ((g_hash_table_iter_next (&iter, &uid, NULL)))
    if (!g_hash_table_contains (logins, uid))
      emit_user_changed (self, GPOINTER_TO_UINT(uid));

  /* fire 'user changed' event for users who logged in */
  g_hash_table_iter_init (&iter, logins);
  while ((g_hash_table_iter_next (&iter, &uid, NULL)))
    if (!g_hash_table_contains (old_logins, uid))
      emit_user_changed (self, GPOINTER_TO_UINT(uid));

  g_hash_table_destroy (old_logins);
}

/***
****  User Account Tracking
***/

static void create_user_proxy_for_path (IndicatorSessionUsersDbus *, const char *);

/* called when a user proxy gets the 'Changed' signal */
static void
on_user_changed (AccountsUser * user, gpointer gself)
{
  /* Accounts.User doesn't update properties in the standard way,
   * so create a new proxy to pull in the new properties.
   * The older proxy is freed when it's replaced in our accounts hash */
  const char * path = g_dbus_proxy_get_object_path (G_DBUS_PROXY(user));
  create_user_proxy_for_path (gself, path);
}

static void
track_user (IndicatorSessionUsersDbus * self,
            AccountsUser              * user)
{
  const guint32 uid = accounts_user_get_uid (user);
  priv_t * p = self->priv;
  gulong id;
  const gboolean already_had_user = is_tracked_uid (self, uid);

  id  = g_signal_connect (user, "changed", G_CALLBACK(on_user_changed), self);
  g_hash_table_insert (p->uid_to_account,
                       GUINT_TO_POINTER (uid),
                       user_record_new (user, id));

  if (already_had_user)
    emit_user_changed (self, uid);
  else
    emit_user_added (self, uid);
}

static void
untrack_user (IndicatorSessionUsersDbus * self,
              const gchar               * path)
{
  guint uid;
  gpointer key;
  gpointer val;
  GHashTableIter iter;
  priv_t * p = self->priv;

  /* find the uid matching this object path */
  uid = 0;
  g_hash_table_iter_init (&iter, p->uid_to_account);
  while (!uid && g_hash_table_iter_next (&iter, &key, &val))
    {
      struct UserRecord * rec = val;
      GDBusProxy * proxy = G_DBUS_PROXY (rec->user);
      if (!g_strcmp0 (path, g_dbus_proxy_get_object_path (proxy)))
        uid = GPOINTER_TO_UINT (key);
    }

  if (uid)
    {
      g_hash_table_remove (p->uid_to_account, GUINT_TO_POINTER(uid)); 

      emit_user_removed (self, uid);
    }
}

/* We got a new org.freedesktop.Accounts.User proxy.
   If it's one we want to remember, pass it to track_user() */
static void
on_user_proxy_ready (GObject       * o G_GNUC_UNUSED,
                     GAsyncResult  * res,
                     gpointer        self)
{
  GError * err;
  AccountsUser * user;

  err = NULL;
  user = accounts_user_proxy_new_for_bus_finish (res, &err);
  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s: %s", G_STRFUNC, err->message);

      g_error_free (err);
    }
  else
    {
      if (!accounts_user_get_system_account (user))
        track_user (self, user);

      g_object_unref (user);
    }
}

static void
create_user_proxy_for_path (IndicatorSessionUsersDbus * self,
                            const char                * path)
{
  accounts_user_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                   G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                   "org.freedesktop.Accounts",
                                   path,
                                   self->priv->cancellable,
                                   on_user_proxy_ready, self);
}

/* create proxy objects for everything in Account's user-list */
static void
on_user_list_ready (GObject * o, GAsyncResult * res, gpointer gself)
{
  GError * err;
  gchar ** paths;

  err = NULL;
  paths = NULL;
  accounts_call_list_cached_users_finish (ACCOUNTS(o), &paths, res, &err);
  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s %s: %s", G_STRLOC, G_STRFUNC, err->message);

      g_error_free (err);
    }
  else
    {
      int i;

      for (i=0; paths && paths[i]; ++i)
        create_user_proxy_for_path (gself, paths[i]);

      g_strfreev (paths);
    }
}

static void
set_account_manager (IndicatorSessionUsersDbus * self, Accounts * a)
{
  priv_t * p = self->priv;

  if (p->accounts != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->accounts, self);
      g_clear_object (&p->accounts);
    }

  if (a != NULL)
    {
      p->accounts = g_object_ref (a);

      accounts_call_list_cached_users (a,
                                       self->priv->cancellable,
                                       on_user_list_ready,
                                       self);

      g_signal_connect_swapped (a, "user-added",
                                G_CALLBACK(create_user_proxy_for_path), self);

      g_signal_connect_swapped (a, "user-deleted",
                                G_CALLBACK(untrack_user), self);
    }
}

/***
****
***/

/* Based on the login1 manager's list of current sessions,
   update our 'logins', 'is_live', and 'active_uid' fields */
static void
on_login1_manager_session_list_ready (GObject      * o,
                                      GAsyncResult * res,
                                      gpointer       gself)
{
  GVariant * sessions;
  GError * err;

  sessions = NULL;
  err = NULL;
  login1_manager_call_list_sessions_finish (LOGIN1_MANAGER(o),
                                            &sessions,
                                            res,
                                            &err);

  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s: %s", G_STRFUNC, err->message);

      g_error_free (err);
    }
  else
    {
      const gchar * const current_seat_id = g_getenv ("XDG_SEAT");
      const gchar * const current_session_id = g_getenv ("XDG_SESSION_ID");
      IndicatorSessionUsersDbus * self = INDICATOR_SESSION_USERS_DBUS (gself);
      const gchar * session_id = NULL;
      guint32 uid = 0;
      const gchar * user_name = NULL;
      const gchar * seat_id = NULL;
      const gchar * path = NULL;
      gboolean is_live_session = FALSE;
      GHashTable * logins = g_hash_table_new (g_direct_hash, g_direct_equal);
      GVariantIter iter;

      g_variant_iter_init (&iter, sessions);
      while (g_variant_iter_loop (&iter, "(&su&s&s&o)", &session_id,
                                                        &uid,
                                                        &user_name,
                                                        &seat_id,
                                                        &path))
        {
          /* only track sessions on our seat */
          if (g_strcmp0 (seat_id, current_seat_id))
            continue;

          if (!g_strcmp0 (session_id, current_session_id))
            {
              set_active_uid (self, uid);

              if ((uid==999) && !g_strcmp0 (user_name, "ubuntu"))
                is_live_session = TRUE;
            }

          g_hash_table_add (logins, GINT_TO_POINTER(uid));
        }

      set_is_live_session_flag (self, is_live_session);
      set_logins (self, logins);

      g_hash_table_unref (logins);
      g_variant_unref (sessions);
    }
}

static void
update_session_list (IndicatorSessionUsersDbus * self)
{
  priv_t * p = self->priv;

  if (p->login1_manager != NULL)
    {
      login1_manager_call_list_sessions (p->login1_manager,
                                         p->cancellable,
                                         on_login1_manager_session_list_ready,
                                         self);
    }
}

static gboolean
on_update_session_list_timer (gpointer gself)
{
  IndicatorSessionUsersDbus * self = INDICATOR_SESSION_USERS_DBUS (gself);

  update_session_list (self);

  self->priv->update_list_tag = 0;
  return G_SOURCE_REMOVE;
}

/* A dead session can still show up in list-sessions for a few seconds.
   So just to be safe, queue up a rebuild for a few seconds from now */
static void
update_session_list_twice (IndicatorSessionUsersDbus * self)
{
  priv_t * p = self->priv;

  update_session_list (self);

  if (p->update_list_tag == 0)
    p->update_list_tag = g_timeout_add_seconds (5,
                                                on_update_session_list_timer,
                                                self);
}

static void
set_login1_manager (IndicatorSessionUsersDbus * self,
                    Login1Manager             * login1_manager)
{
  priv_t * p = self->priv;

  if (p->login1_manager != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->login1_manager, self);

      g_clear_object (&p->login1_manager);
    }

  if (login1_manager != NULL)
    {
      p->login1_manager = g_object_ref (login1_manager);

      g_signal_connect_swapped (login1_manager, "session-new",
                                G_CALLBACK(update_session_list), self);
      g_signal_connect_swapped (login1_manager, "session-removed",
                                G_CALLBACK(update_session_list_twice), self);
      g_signal_connect_swapped (login1_manager, "user-new",
                                G_CALLBACK(update_session_list), self);
      g_signal_connect_swapped (login1_manager, "user-removed",
                                G_CALLBACK(update_session_list_twice), self);
      update_session_list (self);
    }
}

static void
set_login1_seat (IndicatorSessionUsersDbus * self,
                 Login1Seat                * login1_seat)
{
  priv_t * p = self->priv;

  if (p->login1_seat != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->login1_seat, self);

      g_clear_object (&p->login1_seat);
    }

  if (login1_seat != NULL)
    {
      p->login1_seat = g_object_ref (login1_seat);

      g_signal_connect_swapped (login1_seat, "notify::active-session",
                                G_CALLBACK(update_session_list), self);
      update_session_list (self);
    }
}

static void
set_display_manager_seat (IndicatorSessionUsersDbus * self,
                          DisplayManagerSeat        * dm_seat)
{
  priv_t * p = self->priv;

  g_clear_object (&p->dm_seat);

  if (dm_seat != NULL)
    p->dm_seat = g_object_ref (dm_seat);
}

/***
****  IndicatorSessionUsers virtual functions
***/

/* switch to (or create) a session for the specified user */
static void
my_activate_user (IndicatorSessionUsers * users, guint uid)
{
  IndicatorSessionUsersDbus * self = INDICATOR_SESSION_USERS_DBUS(users);
  priv_t * p = self->priv;
  AccountsUser * au;
  const char * username;

  au = get_user_for_uid (self, uid);
  username = au ? accounts_user_get_user_name (au) : NULL;

  if (!username)
    {
      g_warning ("%s %s can't find user '%u'", G_STRLOC, G_STRFUNC, uid);
    }
  else
    {
      g_return_if_fail (p->dm_seat != NULL);

      display_manager_seat_call_switch_to_user (p->dm_seat,
                                                username,
                                                "",
                                                p->cancellable,
                                                NULL,
                                                NULL);
    }
}

/* returns true if this is a live session */
static gboolean
my_is_live_session (IndicatorSessionUsers * users)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_USERS_DBUS(users), FALSE);

  return INDICATOR_SESSION_USERS_DBUS(users)->priv->is_live;
}

/* get a list of our user ids */
static GList *
my_get_uids (IndicatorSessionUsers * users)
{
  IndicatorSessionUsersDbus * self = INDICATOR_SESSION_USERS_DBUS (users);
  return g_hash_table_get_keys (self->priv->uid_to_account);
}

/* build a new struct populated with info on the specified user */
static IndicatorSessionUser *
my_get_user (IndicatorSessionUsers * users, guint uid)
{
  IndicatorSessionUsersDbus * self = INDICATOR_SESSION_USERS_DBUS (users);
  priv_t * p = self->priv;
  IndicatorSessionUser * ret;
  AccountsUser * au;

  ret = NULL;
  au = get_user_for_uid (self, uid);
  if (au && !accounts_user_get_system_account(au))
    {
      g_assert (uid == accounts_user_get_uid (au));

      ret = g_new0 (IndicatorSessionUser, 1);
      ret->uid = uid;
      ret->user_name = g_strdup (accounts_user_get_user_name (au));
      ret->real_name = g_strdup (accounts_user_get_real_name (au));
      ret->icon_file = g_strdup (accounts_user_get_icon_file (au));
      ret->login_frequency = accounts_user_get_login_frequency (au);
      ret->is_logged_in = g_hash_table_contains (p->logins, GINT_TO_POINTER(uid));
      ret->is_current_user = uid == p->active_uid;
    }

  return ret;
}

/***
****  GObject virtual functions
***/

static void
my_dispose (GObject * o)
{
  IndicatorSessionUsersDbus * self = INDICATOR_SESSION_USERS_DBUS (o);
  priv_t * p = self->priv;

  if (p->update_list_tag != 0)
    {
      g_source_remove (p->update_list_tag);
      p->update_list_tag = 0;
    }

  if (p->cancellable)
    {
      g_cancellable_cancel (p->cancellable);
      g_clear_object (&p->cancellable);
    }

  set_account_manager (self, NULL);
  set_display_manager_seat (self, NULL);
  set_login1_seat (self, NULL);
  set_login1_manager (self, NULL);

  g_hash_table_remove_all (p->uid_to_account);

  G_OBJECT_CLASS (indicator_session_users_dbus_parent_class)->dispose (o);
}

static void
my_finalize (GObject * o)
{
  IndicatorSessionUsersDbus * self = INDICATOR_SESSION_USERS_DBUS (o);
  priv_t * p = self->priv;

  g_hash_table_destroy (p->logins);
  g_hash_table_destroy (p->uid_to_account);

  G_OBJECT_CLASS (indicator_session_users_dbus_parent_class)->finalize (o);
}

static void
indicator_session_users_dbus_class_init (IndicatorSessionUsersDbusClass * klass)
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

  g_type_class_add_private (klass, sizeof (IndicatorSessionUsersDbusPriv));
}

static void
indicator_session_users_dbus_init (IndicatorSessionUsersDbus * self)
{
  priv_t * p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_SESSION_USERS_DBUS,
                                   IndicatorSessionUsersDbusPriv);
  self->priv = p;
  p->cancellable = g_cancellable_new ();

  p->uid_to_account = g_hash_table_new_full (g_direct_hash,
                                             g_direct_equal,
                                             NULL,
                                             (GDestroyNotify)user_record_free);

  p->logins = g_hash_table_new (g_direct_hash, g_direct_equal);
}

/***
****  Public
***/

IndicatorSessionUsers *
indicator_session_users_dbus_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_SESSION_USERS_DBUS, NULL);

  return INDICATOR_SESSION_USERS (o);
}

void
indicator_session_users_dbus_set_proxies (IndicatorSessionUsersDbus * self,
                                          Login1Manager             * login1_manager,
                                          Login1Seat                * login1_seat,
                                          DisplayManagerSeat        * dm_seat,
                                          Accounts                  * accounts)
{
  g_return_if_fail (INDICATOR_IS_SESSION_USERS_DBUS (self));

  set_login1_manager (self, login1_manager);
  set_login1_seat (self, login1_seat);
  set_display_manager_seat (self, dm_seat);
  set_account_manager (self, accounts);
}
