/*
 * Copyright (C) 2011 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#include "bamf-daemon.h"
#include "bamf-matcher.h"
#include "bamf-control.h"

G_DEFINE_TYPE (BamfDaemon, bamf_daemon, G_TYPE_OBJECT);
#define BAMF_DAEMON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE(obj, \
                                      BAMF_TYPE_DAEMON, BamfDaemonPrivate))

static BamfDaemon *instance = NULL;

struct _BamfDaemonPrivate
{
  BamfMatcher *matcher;
  BamfControl *control;
  GMainLoop *loop;
};

gboolean
bamf_daemon_is_running (BamfDaemon *self)
{
  g_return_val_if_fail (self, FALSE);

  if (self->priv->loop && g_main_loop_is_running (self->priv->loop))
    {
      return TRUE;
    }

  return FALSE;
}

static void
bamf_on_bus_acquired (GDBusConnection *connection, const gchar *name,
                      BamfDaemon *self)
{
  GError *error = NULL;
  g_return_if_fail (BAMF_IS_DAEMON (self));

  g_debug ("Acquired a message bus connection");

  g_dbus_connection_set_exit_on_close (connection, TRUE);

  self->priv->matcher = bamf_matcher_get_default ();
  self->priv->control = bamf_control_get_default ();

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->priv->matcher),
                                    connection,
                                    BAMF_DBUS_MATCHER_PATH,
                                    &error);

  if (error)
    {
      g_critical ("Can't register BAMF matcher at path %s: %s", BAMF_DBUS_MATCHER_PATH,
                                                                error->message);
      g_clear_error (&error);
    }

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->priv->control),
                                    connection,
                                    BAMF_DBUS_CONTROL_PATH,
                                    &error);

  if (error)
    {
      g_critical ("Can't register BAMF control at path %s: %s", BAMF_DBUS_CONTROL_PATH,
                                                                error->message);
      g_clear_error (&error);
    }
}

static void
bamf_on_name_acquired (GDBusConnection *connection, const gchar *name,
                       BamfDaemon *self)
{
  g_debug ("Acquired the name %s", name);
}

static void
bamf_on_name_lost (GDBusConnection *connection, const gchar *name, BamfDaemon *self)
{
  g_critical ("Lost the name %s, another BAMF daemon is currently running", name);

  bamf_daemon_stop (self);
}

void
bamf_daemon_start (BamfDaemon *self)
{
  g_return_if_fail (BAMF_IS_DAEMON (self));

  if (bamf_daemon_is_running (self))
    return;

  g_bus_own_name (G_BUS_TYPE_SESSION, BAMF_DBUS_SERVICE_NAME,
                  G_BUS_NAME_OWNER_FLAGS_NONE,
                  (GBusAcquiredCallback) bamf_on_bus_acquired,
                  (GBusNameAcquiredCallback) bamf_on_name_acquired,
                  (GBusNameLostCallback) bamf_on_name_lost,
                  self, NULL);

  g_main_loop_run (self->priv->loop);
}

void
bamf_daemon_stop (BamfDaemon *self)
{
  g_return_if_fail (BAMF_IS_DAEMON (self));

  if (self->priv->matcher)
    {
      g_object_unref (self->priv->matcher);
      self->priv->matcher = NULL;
    }

  if (self->priv->control)
    {
      g_object_unref (self->priv->control);
      self->priv->control = NULL;
    }

  g_main_loop_quit (self->priv->loop);
}

static void
bamf_daemon_dispose (GObject *object)
{
  BamfDaemon *self = BAMF_DAEMON (object);

  bamf_daemon_stop (self);

  if (self->priv->loop)
    {
      g_main_loop_unref (self->priv->loop);
      self->priv->loop = NULL;
    }

  G_OBJECT_CLASS (bamf_daemon_parent_class)->dispose (object);
}

static void
bamf_daemon_finalize (GObject *object)
{
  instance = NULL;
}

static void
bamf_daemon_init (BamfDaemon *self)
{
  BamfDaemonPrivate *priv;
  priv = self->priv = BAMF_DAEMON_GET_PRIVATE (self);

  priv->loop = g_main_loop_new (NULL, FALSE);
}

static void
bamf_daemon_class_init (BamfDaemonClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose  = bamf_daemon_dispose;
  object_class->finalize = bamf_daemon_finalize;

  g_type_class_add_private (klass, sizeof (BamfDaemonPrivate));
}

BamfDaemon *
bamf_daemon_get_default (void)
{
  if (!BAMF_IS_DAEMON (instance))
    instance = (BamfDaemon *) g_object_new (BAMF_TYPE_DAEMON, NULL);

  return instance;
}
