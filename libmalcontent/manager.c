/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2019 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <libmalcontent/app-filter.h>
#include <libmalcontent/manager.h>
#include <libmalcontent/session-limits.h>

#include "libmalcontent/app-filter-private.h"
#include "libmalcontent/session-limits-private.h"


G_DEFINE_QUARK (MctManagerError, mct_manager_error)

/**
 * MctManager:
 *
 * #MctManager is a top-level management object which is used to query and
 * monitor #MctAppFilters for different users.
 *
 * Since: 0.3.0
 */
struct _MctManager
{
  GObject parent_instance;

  GDBusConnection *connection;  /* (owned) */
  guint user_changed_id;
};

G_DEFINE_TYPE (MctManager, mct_manager, G_TYPE_OBJECT)

typedef enum
{
  PROP_CONNECTION = 1,
} MctManagerProperty;

static GParamSpec *props[PROP_CONNECTION + 1] = { NULL, };

static void
mct_manager_init (MctManager *self)
{
  /* Nothing to do here. */
}

static void
mct_manager_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *spec)
{
  MctManager *self = MCT_MANAGER (object);

  switch ((MctManagerProperty) property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
      break;
    }
}

static void
mct_manager_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *spec)
{
  MctManager *self = MCT_MANAGER (object);

  switch ((MctManagerProperty) property_id)
    {
    case PROP_CONNECTION:
      /* Construct-only. May not be %NULL. */
      g_assert (self->connection == NULL);
      self->connection = g_value_dup_object (value);
      g_assert (self->connection != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
      break;
    }
}

static void _mct_manager_user_changed_cb (GDBusConnection *connection,
                                          const gchar     *sender_name,
                                          const gchar     *object_path,
                                          const gchar     *interface_name,
                                          const gchar     *signal_name,
                                          GVariant        *parameters,
                                          gpointer         user_data);

static void
mct_manager_constructed (GObject *object)
{
  MctManager *self = MCT_MANAGER (object);

  /* Chain up. */
  G_OBJECT_CLASS (mct_manager_parent_class)->constructed (object);

  /* Connect to notifications from AccountsService. */
  g_assert (self->connection != NULL);
  self->user_changed_id =
      g_dbus_connection_signal_subscribe (self->connection,
                                          "org.freedesktop.Accounts",  /* sender */
                                          "org.freedesktop.Accounts.User",  /* interface name */
                                          "Changed",  /* signal name */
                                          NULL,  /* object path */
                                          NULL,  /* arg0 */
                                          G_DBUS_SIGNAL_FLAGS_NONE,
                                          _mct_manager_user_changed_cb,
                                          self, NULL);
}

static void
mct_manager_dispose (GObject *object)
{
  MctManager *self = MCT_MANAGER (object);

  if (self->user_changed_id != 0 && self->connection != NULL)
    {
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->user_changed_id);
      self->user_changed_id = 0;
    }
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (mct_manager_parent_class)->dispose (object);
}

static void
mct_manager_class_init (MctManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = mct_manager_constructed;
  object_class->dispose = mct_manager_dispose;
  object_class->get_property = mct_manager_get_property;
  object_class->set_property = mct_manager_set_property;

  /**
   * MctManager:connection: (not nullable)
   *
   * A connection to the system bus, where accounts-service runs. It’s provided
   * mostly for testing purposes, or to allow an existing connection to be
   * re-used.
   *
   * Since: 0.3.0
   */
  props[PROP_CONNECTION] = g_param_spec_object ("connection",
                                                "D-Bus Connection",
                                                "A connection to the system bus.",
                                                G_TYPE_DBUS_CONNECTION,
                                                G_PARAM_READWRITE |
                                                G_PARAM_CONSTRUCT_ONLY |
                                                G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     G_N_ELEMENTS (props),
                                     props);

  /**
   * MctManager::app-filter-changed:
   * @self: a #MctManager
   * @user_id: UID of the user whose app filter has changed
   *
   * Emitted when the app filter stored for a user changes.
   * The new app filter for the user should be requested again from
   * the #MctManager instance.
   *
   * Since: 0.3.0
   */
  g_signal_new ("app-filter-changed", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1,
                G_TYPE_UINT64);
}

/**
 * mct_manager_new:
 * @connection: (transfer none): a #GDBusConnection to use
 *
 * Create a new #MctManager.
 *
 * Returns: (transfer full): a new #MctManager
 * Since: 0.3.0
 */
MctManager *
mct_manager_new (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

  return g_object_new (MCT_TYPE_MANAGER,
                       "connection", connection,
                       NULL);
}

static void
_mct_manager_user_changed_cb (GDBusConnection *connection,
                              const gchar     *sender_name,
                              const gchar     *object_path,
                              const gchar     *interface_name,
                              const gchar     *signal_name,
                              GVariant        *parameters,
                              gpointer         user_data)
{
  MctManager *manager = MCT_MANAGER (user_data);
  g_autoptr(GError) local_error = NULL;
  const gchar *uid_str;
  guint64 uid;

  g_assert (g_str_equal (interface_name, "org.freedesktop.Accounts.User"));
  g_assert (g_str_equal (signal_name, "Changed"));

  /* Extract the UID from the object path. This is a bit hacky, but probably
   * better than depending on libaccountsservice just for this. */
  if (!g_str_has_prefix (object_path, "/org/freedesktop/Accounts/User"))
    return;

  uid_str = object_path + strlen ("/org/freedesktop/Accounts/User");
  if (!g_ascii_string_to_unsigned (uid_str, 10, 0, G_MAXUINT64, &uid, &local_error))
    {
      g_warning ("Error converting object path ‘%s’ to user ID: %s",
                 object_path, local_error->message);
      g_clear_error (&local_error);
    }

  g_signal_emit_by_name (manager, "app-filter-changed", uid);
}

/* Check if @error is a D-Bus remote error matching @expected_error_name. */
static gboolean
bus_remote_error_matches (const GError *error,
                          const gchar  *expected_error_name)
{
  g_autofree gchar *error_name = NULL;

  if (!g_dbus_error_is_remote_error (error))
    return FALSE;

  error_name = g_dbus_error_get_remote_error (error);

  return g_str_equal (error_name, expected_error_name);
}

/* Convert a #GDBusError into a #MctManagerError. */
static GError *
bus_error_to_manager_error (const GError *bus_error,
                            uid_t         user_id)
{
  if (g_error_matches (bus_error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED) ||
      bus_remote_error_matches (bus_error, "org.freedesktop.Accounts.Error.PermissionDenied"))
    return g_error_new (MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_PERMISSION_DENIED,
                        _("Not allowed to query parental controls data for user %u"),
                        (guint) user_id);
  else if (g_error_matches (bus_error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD) ||
           bus_remote_error_matches (bus_error, "org.freedesktop.Accounts.Error.Failed"))
    return g_error_new (MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_INVALID_USER,
                        _("User %u does not exist"), (guint) user_id);
  else if (g_error_matches (bus_error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN) ||
           g_error_matches (bus_error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER))
    /* If accountsservice is not available on the system bus, then the
     * com.endlessm.ParentalControls.AppFilter extension interface
     * certainly can't be available. */
    return g_error_new_literal (MCT_MANAGER_ERROR,
                                MCT_MANAGER_ERROR_DISABLED,
                                _("System accounts service not available"));
  else
    return g_error_copy (bus_error);
}

/* Find the object path for the given @user_id on the accountsservice D-Bus
 * interface, by calling its FindUserById() method. This is a synchronous,
 * blocking function. */
static gchar *
accounts_find_user_by_id (GDBusConnection  *connection,
                          uid_t             user_id,
                          gboolean          allow_interactive_authorization,
                          GCancellable     *cancellable,
                          GError          **error)
{
  g_autofree gchar *object_path = NULL;
  g_autoptr(GVariant) result_variant = NULL;
  g_autoptr(GError) local_error = NULL;

  result_variant =
      g_dbus_connection_call_sync (connection,
                                   "org.freedesktop.Accounts",
                                   "/org/freedesktop/Accounts",
                                   "org.freedesktop.Accounts",
                                   "FindUserById",
                                   g_variant_new ("(x)", (gint64) user_id),
                                   G_VARIANT_TYPE ("(o)"),
                                   allow_interactive_authorization
                                     ? G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION
                                     : G_DBUS_CALL_FLAGS_NONE,
                                   -1,  /* timeout, ms */
                                   cancellable,
                                   &local_error);
  if (local_error != NULL)
    {
      g_autoptr(GError) app_filter_error = bus_error_to_manager_error (local_error,
                                                                       user_id);
      g_propagate_error (error, g_steal_pointer (&app_filter_error));
      return NULL;
    }

  g_variant_get (result_variant, "(o)", &object_path);

  return g_steal_pointer (&object_path);
}

/**
 * mct_manager_get_app_filter:
 * @self: a #MctManager
 * @user_id: ID of the user to query, typically coming from getuid()
 * @flags: flags to affect the behaviour of the call
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronous version of mct_manager_get_app_filter_async().
 *
 * Returns: (transfer full): app filter for the queried user
 * Since: 0.3.0
 */
MctAppFilter *
mct_manager_get_app_filter (MctManager            *self,
                            uid_t                  user_id,
                            MctManagerGetValueFlags flags,
                            GCancellable          *cancellable,
                            GError               **error)
{
  g_autofree gchar *object_path = NULL;
  g_autoptr(GVariant) result_variant = NULL;
  g_autoptr(GVariant) properties = NULL;
  g_autoptr(GError) local_error = NULL;

  g_return_val_if_fail (MCT_IS_MANAGER (self), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  object_path = accounts_find_user_by_id (self->connection, user_id,
                                          (flags & MCT_MANAGER_GET_VALUE_FLAGS_INTERACTIVE),
                                          cancellable, error);
  if (object_path == NULL)
    return NULL;

  result_variant =
      g_dbus_connection_call_sync (self->connection,
                                   "org.freedesktop.Accounts",
                                   object_path,
                                   "org.freedesktop.DBus.Properties",
                                   "GetAll",
                                   g_variant_new ("(s)", "com.endlessm.ParentalControls.AppFilter"),
                                   G_VARIANT_TYPE ("(a{sv})"),
                                   (flags & MCT_MANAGER_GET_VALUE_FLAGS_INTERACTIVE)
                                     ? G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION
                                     : G_DBUS_CALL_FLAGS_NONE,
                                   -1,  /* timeout, ms */
                                   cancellable,
                                   &local_error);
  if (local_error != NULL)
    {
      g_autoptr(GError) manager_error = NULL;

      if (g_error_matches (local_error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS))
        {
          /* o.fd.D.GetAll() will return InvalidArgs errors if
           * accountsservice doesn’t have the com.endlessm.ParentalControls.AppFilter
           * extension interface installed. */
          manager_error = g_error_new_literal (MCT_MANAGER_ERROR,
                                               MCT_MANAGER_ERROR_DISABLED,
                                               _("App filtering is globally disabled"));
        }
      else
        {
          manager_error = bus_error_to_manager_error (local_error, user_id);
        }

      g_propagate_error (error, g_steal_pointer (&manager_error));
      return NULL;
    }

  /* Extract the properties we care about. They may be silently omitted from the
   * results if we don’t have permission to access them. */
  properties = g_variant_get_child_value (result_variant, 0);
  if (!g_variant_lookup (properties, "AppFilter", "(b^as)", NULL, NULL))
    {
      g_set_error (error, MCT_MANAGER_ERROR,
                   MCT_MANAGER_ERROR_PERMISSION_DENIED,
                   _("Not allowed to query parental controls data for user %u"),
                   (guint) user_id);
      return NULL;
    }

  return mct_app_filter_deserialize (properties, user_id, error);
}

static void get_app_filter_thread_cb (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable);

typedef struct
{
  uid_t user_id;
  MctManagerGetValueFlags flags;
} GetAppFilterData;

static void
get_app_filter_data_free (GetAppFilterData *data)
{
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GetAppFilterData, get_app_filter_data_free)

/**
 * mct_manager_get_app_filter_async:
 * @self: a #MctManager
 * @user_id: ID of the user to query, typically coming from getuid()
 * @flags: flags to affect the behaviour of the call
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback
 * @user_data: user data to pass to @callback
 *
 * Asynchronously get a snapshot of the app filter settings for the given
 * @user_id.
 *
 * On failure, an #MctManagerError, a #GDBusError or a #GIOError will be
 * returned.
 *
 * Since: 0.3.0
 */
void
mct_manager_get_app_filter_async  (MctManager           *self,
                                   uid_t                 user_id,
                                   MctManagerGetValueFlags flags,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GetAppFilterData) data = NULL;

  g_return_if_fail (MCT_IS_MANAGER (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mct_manager_get_app_filter_async);

  data = g_new0 (GetAppFilterData, 1);
  data->user_id = user_id;
  data->flags = flags;
  g_task_set_task_data (task, g_steal_pointer (&data),
                        (GDestroyNotify) get_app_filter_data_free);

  g_task_run_in_thread (task, get_app_filter_thread_cb);
}

static void
get_app_filter_thread_cb (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  g_autoptr(MctAppFilter) filter = NULL;
  MctManager *manager = MCT_MANAGER (source_object);
  GetAppFilterData *data = task_data;
  g_autoptr(GError) local_error = NULL;

  filter = mct_manager_get_app_filter (manager, data->user_id,
                                       data->flags,
                                       cancellable, &local_error);

  if (local_error != NULL)
    g_task_return_error (task, g_steal_pointer (&local_error));
  else
    g_task_return_pointer (task, g_steal_pointer (&filter),
                           (GDestroyNotify) mct_app_filter_unref);
}

/**
 * mct_manager_get_app_filter_finish:
 * @self: a #MctManager
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finish an asynchronous operation to get the app filter for a user, started
 * with mct_manager_get_app_filter_async().
 *
 * Returns: (transfer full): app filter for the queried user
 * Since: 0.3.0
 */
MctAppFilter *
mct_manager_get_app_filter_finish (MctManager    *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (MCT_IS_MANAGER (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * mct_manager_set_app_filter:
 * @self: a #MctManager
 * @user_id: ID of the user to set the filter for, typically coming from getuid()
 * @app_filter: (transfer none): the app filter to set for the user
 * @flags: flags to affect the behaviour of the call
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronous version of mct_manager_set_app_filter_async().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.3.0
 */
gboolean
mct_manager_set_app_filter (MctManager            *self,
                            uid_t                  user_id,
                            MctAppFilter          *app_filter,
                            MctManagerSetValueFlags flags,
                            GCancellable          *cancellable,
                            GError               **error)
{
  g_autofree gchar *object_path = NULL;
  g_autoptr(GVariant) properties_variant = NULL;
  g_autoptr(GVariant) properties_value = NULL;
  const gchar *properties_key = NULL;
  GVariantIter iter;
  g_autoptr(GError) local_error = NULL;

  g_return_val_if_fail (MCT_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (app_filter != NULL, FALSE);
  g_return_val_if_fail (app_filter->ref_count >= 1, FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  object_path = accounts_find_user_by_id (self->connection, user_id,
                                          (flags & MCT_MANAGER_SET_VALUE_FLAGS_INTERACTIVE),
                                          cancellable, error);
  if (object_path == NULL)
    return FALSE;

  properties_variant = mct_app_filter_serialize (app_filter);

  g_variant_iter_init (&iter, properties_variant);
  while (g_variant_iter_loop (&iter, "{&sv}", &properties_key, &properties_value))
    {
      g_autoptr(GVariant) result_variant = NULL;

      result_variant =
          g_dbus_connection_call_sync (self->connection,
                                       "org.freedesktop.Accounts",
                                       object_path,
                                       "org.freedesktop.DBus.Properties",
                                       "Set",
                                       g_variant_new ("(ssv)",
                                                      "com.endlessm.ParentalControls.AppFilter",
                                                      properties_key,
                                                      properties_value),
                                       G_VARIANT_TYPE ("()"),
                                       (flags & MCT_MANAGER_SET_VALUE_FLAGS_INTERACTIVE)
                                         ? G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION
                                         : G_DBUS_CALL_FLAGS_NONE,
                                       -1,  /* timeout, ms */
                                       cancellable,
                                       &local_error);
      if (local_error != NULL)
        {
          g_propagate_error (error, bus_error_to_manager_error (local_error, user_id));
          return FALSE;
        }
    }

  return TRUE;
}

static void set_app_filter_thread_cb (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable);

typedef struct
{
  uid_t user_id;
  MctAppFilter *app_filter;  /* (owned) */
  MctManagerSetValueFlags flags;
} SetAppFilterData;

static void
set_app_filter_data_free (SetAppFilterData *data)
{
  mct_app_filter_unref (data->app_filter);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SetAppFilterData, set_app_filter_data_free)

/**
 * mct_manager_set_app_filter_async:
 * @self: a #MctManager
 * @user_id: ID of the user to set the filter for, typically coming from getuid()
 * @app_filter: (transfer none): the app filter to set for the user
 * @flags: flags to affect the behaviour of the call
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback
 * @user_data: user data to pass to @callback
 *
 * Asynchronously set the app filter settings for the given @user_id to the
 * given @app_filter instance. This will set all fields of the app filter.
 *
 * On failure, an #MctManagerError, a #GDBusError or a #GIOError will be
 * returned. The user’s app filter settings will be left in an undefined state.
 *
 * Since: 0.3.0
 */
void
mct_manager_set_app_filter_async (MctManager           *self,
                                  uid_t                 user_id,
                                  MctAppFilter         *app_filter,
                                  MctManagerSetValueFlags flags,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(SetAppFilterData) data = NULL;

  g_return_if_fail (MCT_IS_MANAGER (self));
  g_return_if_fail (app_filter != NULL);
  g_return_if_fail (app_filter->ref_count >= 1);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mct_manager_set_app_filter_async);

  data = g_new0 (SetAppFilterData, 1);
  data->user_id = user_id;
  data->app_filter = mct_app_filter_ref (app_filter);
  data->flags = flags;
  g_task_set_task_data (task, g_steal_pointer (&data),
                        (GDestroyNotify) set_app_filter_data_free);

  g_task_run_in_thread (task, set_app_filter_thread_cb);
}

static void
set_app_filter_thread_cb (GTask        *task,
                          gpointer      source_object,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  gboolean success;
  MctManager *manager = MCT_MANAGER (source_object);
  SetAppFilterData *data = task_data;
  g_autoptr(GError) local_error = NULL;

  success = mct_manager_set_app_filter (manager, data->user_id,
                                        data->app_filter, data->flags,
                                        cancellable, &local_error);

  if (local_error != NULL)
    g_task_return_error (task, g_steal_pointer (&local_error));
  else
    g_task_return_boolean (task, success);
}

/**
 * mct_manager_set_app_filter_finish:
 * @self: a #MctManager
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finish an asynchronous operation to set the app filter for a user, started
 * with mct_manager_set_app_filter_async().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.3.0
 */
gboolean
mct_manager_set_app_filter_finish (MctManager    *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (MCT_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * mct_manager_get_session_limits:
 * @self: a #MctManager
 * @user_id: ID of the user to query, typically coming from getuid()
 * @flags: flags to affect the behaviour of the call
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronous version of mct_manager_get_session_limits_async().
 *
 * Returns: (transfer full): session limits for the queried user
 * Since: 0.5.0
 */
MctSessionLimits *
mct_manager_get_session_limits (MctManager                *self,
                                uid_t                      user_id,
                                MctManagerGetValueFlags    flags,
                                GCancellable              *cancellable,
                                GError                   **error)
{
  g_autofree gchar *object_path = NULL;
  g_autoptr(GVariant) result_variant = NULL;
  g_autoptr(GVariant) properties = NULL;
  g_autoptr(GError) local_error = NULL;

  g_return_val_if_fail (MCT_IS_MANAGER (self), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  object_path = accounts_find_user_by_id (self->connection, user_id,
                                          (flags & MCT_MANAGER_GET_VALUE_FLAGS_INTERACTIVE),
                                          cancellable, error);
  if (object_path == NULL)
    return NULL;

  result_variant =
      g_dbus_connection_call_sync (self->connection,
                                   "org.freedesktop.Accounts",
                                   object_path,
                                   "org.freedesktop.DBus.Properties",
                                   "GetAll",
                                   g_variant_new ("(s)", "com.endlessm.ParentalControls.SessionLimits"),
                                   G_VARIANT_TYPE ("(a{sv})"),
                                   (flags & MCT_MANAGER_GET_VALUE_FLAGS_INTERACTIVE)
                                     ? G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION
                                     : G_DBUS_CALL_FLAGS_NONE,
                                   -1,  /* timeout, ms */
                                   cancellable,
                                   &local_error);
  if (local_error != NULL)
    {
      g_autoptr(GError) manager_error = NULL;

      if (g_error_matches (local_error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS))
        {
          /* o.fd.D.GetAll() will return InvalidArgs errors if
           * accountsservice doesn’t have the com.endlessm.ParentalControls.SessionLimits
           * extension interface installed. */
          manager_error = g_error_new_literal (MCT_MANAGER_ERROR,
                                               MCT_MANAGER_ERROR_DISABLED,
                                               _("Session limits are globally disabled"));
        }
      else
        {
          manager_error = bus_error_to_manager_error (local_error, user_id);
        }

      g_propagate_error (error, g_steal_pointer (&manager_error));
      return NULL;
    }

  /* Extract the properties we care about. They may be silently omitted from the
   * results if we don’t have permission to access them. */
  properties = g_variant_get_child_value (result_variant, 0);
  if (!g_variant_lookup (properties, "LimitType", "u", NULL))
    {
      g_set_error (error, MCT_MANAGER_ERROR,
                   MCT_MANAGER_ERROR_PERMISSION_DENIED,
                   _("Not allowed to query parental controls data for user %u"),
                   (guint) user_id);
      return NULL;
    }

  return mct_session_limits_deserialize (properties, user_id, error);
}

static void get_session_limits_thread_cb (GTask        *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable);

typedef struct
{
  uid_t user_id;
  MctManagerGetValueFlags flags;
} GetSessionLimitsData;

static void
get_session_limits_data_free (GetSessionLimitsData *data)
{
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GetSessionLimitsData, get_session_limits_data_free)

/**
 * mct_manager_get_session_limits_async:
 * @self: a #MctManager
 * @user_id: ID of the user to query, typically coming from getuid()
 * @flags: flags to affect the behaviour of the call
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback
 * @user_data: user data to pass to @callback
 *
 * Asynchronously get a snapshot of the session limit settings for the given
 * @user_id.
 *
 * On failure, an #MctManagerError, a #GDBusError or a #GIOError will be
 * returned via mct_manager_get_session_limits_finish().
 *
 * Since: 0.5.0
 */
void
mct_manager_get_session_limits_async  (MctManager               *self,
                                       uid_t                     user_id,
                                       MctManagerGetValueFlags   flags,
                                       GCancellable             *cancellable,
                                       GAsyncReadyCallback       callback,
                                       gpointer                  user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GetSessionLimitsData) data = NULL;

  g_return_if_fail (MCT_IS_MANAGER (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mct_manager_get_session_limits_async);

  data = g_new0 (GetSessionLimitsData, 1);
  data->user_id = user_id;
  data->flags = flags;
  g_task_set_task_data (task, g_steal_pointer (&data),
                        (GDestroyNotify) get_session_limits_data_free);

  g_task_run_in_thread (task, get_session_limits_thread_cb);
}

static void
get_session_limits_thread_cb (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
  g_autoptr(MctSessionLimits) limits = NULL;
  MctManager *manager = MCT_MANAGER (source_object);
  GetSessionLimitsData *data = task_data;
  g_autoptr(GError) local_error = NULL;

  limits = mct_manager_get_session_limits (manager, data->user_id,
                                           data->flags,
                                           cancellable, &local_error);

  if (local_error != NULL)
    g_task_return_error (task, g_steal_pointer (&local_error));
  else
    g_task_return_pointer (task, g_steal_pointer (&limits),
                           (GDestroyNotify) mct_session_limits_unref);
}

/**
 * mct_manager_get_session_limits_finish:
 * @self: a #MctManager
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finish an asynchronous operation to get the session limits for a user,
 * started with mct_manager_get_session_limits_async().
 *
 * Returns: (transfer full): session limits for the queried user
 * Since: 0.5.0
 */
MctSessionLimits *
mct_manager_get_session_limits_finish (MctManager    *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_return_val_if_fail (MCT_IS_MANAGER (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * mct_manager_set_session_limits:
 * @self: a #MctManager
 * @user_id: ID of the user to set the limits for, typically coming from getuid()
 * @session_limits: (transfer none): the session limits to set for the user
 * @flags: flags to affect the behaviour of the call
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Synchronous version of mct_manager_set_session_limits_async().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.5.0
 */
gboolean
mct_manager_set_session_limits (MctManager                *self,
                                uid_t                      user_id,
                                MctSessionLimits          *session_limits,
                                MctManagerSetValueFlags    flags,
                                GCancellable              *cancellable,
                                GError                   **error)
{
  g_autofree gchar *object_path = NULL;
  g_autoptr(GVariant) limit_type_variant = NULL;
  g_autoptr(GVariant) limit_type_result_variant = NULL;
  g_autoptr(GVariant) properties_variant = NULL;
  g_autoptr(GVariant) properties_value = NULL;
  const gchar *properties_key = NULL;
  GVariantIter iter;
  g_autoptr(GError) local_error = NULL;

  g_return_val_if_fail (MCT_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (session_limits != NULL, FALSE);
  g_return_val_if_fail (session_limits->ref_count >= 1, FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  object_path = accounts_find_user_by_id (self->connection, user_id,
                                          (flags & MCT_MANAGER_SET_VALUE_FLAGS_INTERACTIVE),
                                          cancellable, error);
  if (object_path == NULL)
    return FALSE;

  properties_variant = mct_session_limits_serialize (session_limits);

  g_variant_iter_init (&iter, properties_variant);
  while (g_variant_iter_loop (&iter, "{&sv}", &properties_key, &properties_value))
    {
      g_autoptr(GVariant) result_variant = NULL;

      /* Change the limit type last, so all the details of the new limit are
       * correct by the time it’s changed over. */
      if (g_str_equal (properties_key, "LimitType"))
        {
          limit_type_variant = g_steal_pointer (&properties_value);
          continue;
        }

      result_variant =
          g_dbus_connection_call_sync (self->connection,
                                       "org.freedesktop.Accounts",
                                       object_path,
                                       "org.freedesktop.DBus.Properties",
                                       "Set",
                                       g_variant_new ("(ssv)",
                                                      "com.endlessm.ParentalControls.SessionLimits",
                                                      properties_key,
                                                      properties_value),
                                       G_VARIANT_TYPE ("()"),
                                       (flags & MCT_MANAGER_SET_VALUE_FLAGS_INTERACTIVE)
                                         ? G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION
                                         : G_DBUS_CALL_FLAGS_NONE,
                                       -1,  /* timeout, ms */
                                       cancellable,
                                       &local_error);
      if (local_error != NULL)
        {
          g_propagate_error (error, bus_error_to_manager_error (local_error, user_id));
          return FALSE;
        }
    }

  limit_type_result_variant =
      g_dbus_connection_call_sync (self->connection,
                                   "org.freedesktop.Accounts",
                                   object_path,
                                   "org.freedesktop.DBus.Properties",
                                   "Set",
                                   g_variant_new ("(ssv)",
                                                  "com.endlessm.ParentalControls.SessionLimits",
                                                  "LimitType",
                                                  limit_type_variant),
                                   G_VARIANT_TYPE ("()"),
                                   (flags & MCT_MANAGER_SET_VALUE_FLAGS_INTERACTIVE)
                                     ? G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION
                                     : G_DBUS_CALL_FLAGS_NONE,
                                   -1,  /* timeout, ms */
                                   cancellable,
                                   &local_error);
  if (local_error != NULL)
    {
      g_propagate_error (error, bus_error_to_manager_error (local_error, user_id));
      return FALSE;
    }

  return TRUE;
}

static void set_session_limits_thread_cb (GTask        *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable);

typedef struct
{
  uid_t user_id;
  MctSessionLimits *session_limits;  /* (owned) */
  MctManagerSetValueFlags flags;
} SetSessionLimitsData;

static void
set_session_limits_data_free (SetSessionLimitsData *data)
{
  mct_session_limits_unref (data->session_limits);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SetSessionLimitsData, set_session_limits_data_free)

/**
 * mct_manager_set_session_limits_async:
 * @self: a #MctManager
 * @user_id: ID of the user to set the limits for, typically coming from getuid()
 * @session_limits: (transfer none): the session limits to set for the user
 * @flags: flags to affect the behaviour of the call
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback
 * @user_data: user data to pass to @callback
 *
 * Asynchronously set the session limits settings for the given @user_id to the
 * given @session_limits instance.
 *
 * On failure, an #MctManagerError, a #GDBusError or a #GIOError will be
 * returned via mct_manager_set_session_limits_finish(). The user’s session
 * limits settings will be left in an undefined state.
 *
 * Since: 0.5.0
 */
void
mct_manager_set_session_limits_async (MctManager               *self,
                                      uid_t                     user_id,
                                      MctSessionLimits         *session_limits,
                                      MctManagerSetValueFlags   flags,
                                      GCancellable             *cancellable,
                                      GAsyncReadyCallback       callback,
                                      gpointer                  user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(SetSessionLimitsData) data = NULL;

  g_return_if_fail (MCT_IS_MANAGER (self));
  g_return_if_fail (session_limits != NULL);
  g_return_if_fail (session_limits->ref_count >= 1);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mct_manager_set_session_limits_async);

  data = g_new0 (SetSessionLimitsData, 1);
  data->user_id = user_id;
  data->session_limits = mct_session_limits_ref (session_limits);
  data->flags = flags;
  g_task_set_task_data (task, g_steal_pointer (&data),
                        (GDestroyNotify) set_session_limits_data_free);

  g_task_run_in_thread (task, set_session_limits_thread_cb);
}

static void
set_session_limits_thread_cb (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
  gboolean success;
  MctManager *manager = MCT_MANAGER (source_object);
  SetSessionLimitsData *data = task_data;
  g_autoptr(GError) local_error = NULL;

  success = mct_manager_set_session_limits (manager, data->user_id,
                                            data->session_limits, data->flags,
                                            cancellable, &local_error);

  if (local_error != NULL)
    g_task_return_error (task, g_steal_pointer (&local_error));
  else
    g_task_return_boolean (task, success);
}

/**
 * mct_manager_set_session_limits_finish:
 * @self: a #MctManager
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finish an asynchronous operation to set the session limits for a user,
 * started with mct_manager_set_session_limits_async().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.5.0
 */
gboolean
mct_manager_set_session_limits_finish (MctManager    *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_return_val_if_fail (MCT_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
