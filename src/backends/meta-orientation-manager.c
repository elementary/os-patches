/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "backends/meta-orientation-manager.h"

#include <gio/gio.h>

enum
{
  ORIENTATION_CHANGED,

  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum
{
  PROP_0,

  PROP_HAS_ACCELEROMETER,

  PROP_LAST
};

static GParamSpec *props[PROP_LAST];

struct _MetaOrientationManager
{
  GObject parent_instance;

  GCancellable *cancellable;

  guint iio_watch_id;
  guint sync_idle_id;
  GDBusProxy *iio_proxy;
  MetaOrientation prev_orientation;
  MetaOrientation curr_orientation;
  guint has_accel : 1;

  GSettings *settings;
};

G_DEFINE_TYPE (MetaOrientationManager, meta_orientation_manager, G_TYPE_OBJECT)

#define CONF_SCHEMA "org.gnome.settings-daemon.peripherals.touchscreen"
#define ORIENTATION_LOCK_KEY "orientation-lock"

static MetaOrientation
orientation_from_string (const char *orientation)
{
  if (g_strcmp0 (orientation, "normal") == 0)
    return META_ORIENTATION_NORMAL;
  if (g_strcmp0 (orientation, "bottom-up") == 0)
    return META_ORIENTATION_BOTTOM_UP;
  if (g_strcmp0 (orientation, "left-up") == 0)
    return META_ORIENTATION_LEFT_UP;
  if (g_strcmp0 (orientation, "right-up") == 0)
    return META_ORIENTATION_RIGHT_UP;

  return META_ORIENTATION_UNDEFINED;
}

static void
read_iio_proxy (MetaOrientationManager *self)
{
  GVariant *v;

  self->curr_orientation = META_ORIENTATION_UNDEFINED;

  if (!self->iio_proxy)
    {
      self->has_accel = FALSE;
      return;
    }

  v = g_dbus_proxy_get_cached_property (self->iio_proxy, "HasAccelerometer");
  if (v)
    {
      self->has_accel = !!g_variant_get_boolean (v);
      g_variant_unref (v);
    }

  if (self->has_accel)
    {
      v = g_dbus_proxy_get_cached_property (self->iio_proxy, "AccelerometerOrientation");
      if (v)
        {
          self->curr_orientation = orientation_from_string (g_variant_get_string (v, NULL));
          g_variant_unref (v);
        }
    }
}

static void
sync_state (MetaOrientationManager *self)
{
  gboolean had_accel = self->has_accel;

  read_iio_proxy (self);

  if (had_accel != self->has_accel)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_ACCELEROMETER]);

  if (g_settings_get_boolean (self->settings, ORIENTATION_LOCK_KEY))
    return;

  if (self->prev_orientation == self->curr_orientation)
    return;

  self->prev_orientation = self->curr_orientation;

  if (self->curr_orientation == META_ORIENTATION_UNDEFINED)
    return;

  g_signal_emit (self, signals[ORIENTATION_CHANGED], 0);
}

static gboolean
changed_idle (gpointer user_data)
{
  MetaOrientationManager *self = user_data;

  self->sync_idle_id = 0;
  sync_state (self);

  return G_SOURCE_REMOVE;
}

static void
queue_sync_state (MetaOrientationManager *self)
{
  /* We need this idle to avoid triggering events happening while the session
   * is not active (under X11), ideally this should be handled by stopping
   * events if the session is not active, but we'll need a MetaLogind available
   * in all the backends for having this working.
   */

  if (self->sync_idle_id)
    return;

  self->sync_idle_id = g_idle_add (changed_idle, self);
}

static void
orientation_lock_changed (GSettings *settings,
                          gchar     *key,
                          gpointer   user_data)
{
  MetaOrientationManager *self = user_data;
  queue_sync_state (self);
}

static void
iio_properties_changed (GDBusProxy *proxy,
                        GVariant   *changed_properties,
                        GStrv       invalidated_properties,
                        gpointer    user_data)
{
  MetaOrientationManager *self = user_data;
  queue_sync_state (self);
}

static void
accelerometer_claimed (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  MetaOrientationManager *self = user_data;
  GVariant *v;
  GError *error = NULL;

  v = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (!v)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to claim accelerometer: %s", error->message);
      g_error_free (error);
      return;
    }

  g_variant_unref (v);

  sync_state (self);
}

static void
iio_proxy_ready (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  MetaOrientationManager *self = user_data;
  GDBusProxy *proxy;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_finish (res, &error);
  if (!proxy)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to obtain IIO DBus proxy: %s", error->message);
      g_error_free (error);
      return;
    }

  self->iio_proxy = proxy;
  g_signal_connect_object (self->iio_proxy, "g-properties-changed",
                           G_CALLBACK (iio_properties_changed), self, 0);
  g_dbus_proxy_call (self->iio_proxy,
                     "ClaimAccelerometer",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     accelerometer_claimed,
                     self);
}

static void
iio_sensor_appeared_cb (GDBusConnection *connection,
                        const gchar     *name,
                        const gchar     *name_owner,
                        gpointer         user_data)
{
  MetaOrientationManager *self = user_data;

  self->cancellable = g_cancellable_new ();
  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    "net.hadess.SensorProxy",
                    "/net/hadess/SensorProxy",
                    "net.hadess.SensorProxy",
                    self->cancellable,
                    iio_proxy_ready,
                    self);
}

static void
iio_sensor_vanished_cb (GDBusConnection *connection,
                        const gchar     *name,
                        gpointer         user_data)
{
  MetaOrientationManager *self = user_data;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->iio_proxy);

  sync_state (self);
}

static void
meta_orientation_manager_init (MetaOrientationManager *self)
{
  self->iio_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                         "net.hadess.SensorProxy",
                                         G_BUS_NAME_WATCHER_FLAGS_NONE,
                                         iio_sensor_appeared_cb,
                                         iio_sensor_vanished_cb,
                                         self,
                                         NULL);

  self->settings = g_settings_new (CONF_SCHEMA);
  g_signal_connect_object (self->settings, "changed::"ORIENTATION_LOCK_KEY,
                           G_CALLBACK (orientation_lock_changed), self, 0);
  sync_state (self);
}

static void
meta_orientation_manager_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  MetaOrientationManager *self = META_ORIENTATION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_HAS_ACCELEROMETER:
      g_value_set_boolean (value, self->has_accel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_orientation_manager_finalize (GObject *object)
{
  MetaOrientationManager *self = META_ORIENTATION_MANAGER (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_bus_unwatch_name (self->iio_watch_id);
  g_clear_handle_id (&self->sync_idle_id, g_source_remove);
  g_clear_object (&self->iio_proxy);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (meta_orientation_manager_parent_class)->finalize (object);
}

static void
meta_orientation_manager_class_init (MetaOrientationManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = meta_orientation_manager_finalize;
  gobject_class->get_property = meta_orientation_manager_get_property;

  signals[ORIENTATION_CHANGED] =
    g_signal_new ("orientation-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  props[PROP_HAS_ACCELEROMETER] =
    g_param_spec_boolean ("has-accelerometer",
                          "Has accelerometer",
                          "Has accelerometer",
                          FALSE,
                          G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (gobject_class, PROP_LAST, props);
}

MetaOrientation
meta_orientation_manager_get_orientation (MetaOrientationManager *self)
{
  return self->curr_orientation;
}

gboolean
meta_orientation_manager_has_accelerometer (MetaOrientationManager *self)
{
  return self->has_accel;
}
