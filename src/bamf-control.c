/*
 * Copyright (C) 2010-2011 Canonical Ltd
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
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */


#include <libbamf-private/bamf-private.h>

#include "bamf-control.h"
#include "bamf-application.h"
#include "bamf-daemon.h"
#include "bamf-matcher.h"

G_DEFINE_TYPE (BamfControl, bamf_control, BAMF_DBUS_TYPE_CONTROL_SKELETON);
#define BAMF_CONTROL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE(obj, \
BAMF_TYPE_CONTROL, BamfControlPrivate))

struct _BamfControlPrivate
{
  GDBusConnection *connection;
  guint launched_signal;
  GList *sources;
};

static void
bamf_control_on_launched_callback (GDBusConnection *connection,
                                   const gchar *sender_name,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *signal_name,
                                   GVariant *parameters,
                                   gpointer user_data)
{
  const gchar *desktop_file;
  gint64 pid;

  g_variant_get_child (parameters, 0, "^&ay", &desktop_file);
  g_variant_get_child (parameters, 2, "x", &pid);

  bamf_matcher_register_desktop_file_for_pid (bamf_matcher_get_default (),
                                              desktop_file, pid);
}

static void
on_bus_connection (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  BamfControl *self;
  GError *error = NULL;

  g_return_if_fail (BAMF_IS_CONTROL (user_data));

  self = BAMF_CONTROL (user_data);
  self->priv->connection = g_bus_get_finish (res, &error);

  if (error)
    {
      g_warning ("Got error when connecting to session bus: %s", error->message);
      g_clear_error (&error);
      self->priv->connection = NULL;
      return;
    }

  self->priv->launched_signal =
    g_dbus_connection_signal_subscribe  (self->priv->connection, NULL,
                                         "org.gtk.gio.DesktopAppInfo",
                                         "Launched",
                                         "/org/gtk/gio/DesktopAppInfo",
                                         NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                         bamf_control_on_launched_callback,
                                         self, NULL);
}

static void
bamf_control_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (bamf_control_parent_class)->constructed)
    G_OBJECT_CLASS (bamf_control_parent_class)->constructed (object);

  g_bus_get (G_BUS_TYPE_SESSION, NULL, on_bus_connection, object);
}

static gboolean
on_dbus_handle_quit (BamfDBusControl *interface,
                     GDBusMethodInvocation *invocation,
                     BamfControl *self)
{
  g_dbus_method_invocation_return_value (invocation, NULL);
  bamf_control_quit (self);

  return TRUE;
}

static gboolean
on_dbus_handle_insert_desktop_file (BamfDBusControl *interface,
                                    GDBusMethodInvocation *invocation,
                                    const gchar *desktop_file,
                                    BamfControl *self)
{
  g_dbus_method_invocation_return_value (invocation, NULL);
  bamf_control_insert_desktop_file (self, desktop_file);

  return TRUE;
}

static gboolean
on_dbus_handle_register_application_for_pid (BamfDBusControl *interface,
                                             GDBusMethodInvocation *invocation,
                                             const gchar *application,
                                             guint pid,
                                             BamfControl *self)
{
  g_dbus_method_invocation_return_value (invocation, NULL);
  bamf_control_register_application_for_pid (self, application, pid);

  return TRUE;
}

static gboolean
on_dbus_handle_create_local_desktop_file (BamfDBusControl *interface,
                                          GDBusMethodInvocation *invocation,
                                          const gchar *desktop_file,
                                          BamfControl *self)
{
  g_dbus_method_invocation_return_value (invocation, NULL);
  bamf_control_create_local_desktop_file (self, desktop_file);

  return TRUE;
}

static void
bamf_control_init (BamfControl * self)
{
  self->priv = BAMF_CONTROL_GET_PRIVATE (self);
  self->priv->sources = NULL;

  /* Registering signal callbacks to reply to dbus method calls */
  g_signal_connect (self, "handle-quit",
                    G_CALLBACK (on_dbus_handle_quit), self);

  g_signal_connect (self, "handle-om-nom-nom-desktop-file",
                    G_CALLBACK (on_dbus_handle_insert_desktop_file), self);

  g_signal_connect (self, "handle-insert-desktop-file",
                    G_CALLBACK (on_dbus_handle_insert_desktop_file), self);

  g_signal_connect (self, "handle-register-application-for-pid",
                    G_CALLBACK (on_dbus_handle_register_application_for_pid), self);

  g_signal_connect (self, "handle-create-local-desktop-file",
                    G_CALLBACK (on_dbus_handle_create_local_desktop_file), self);
}

static void
bamf_control_finalize (GObject *object)
{
  BamfControl *self = BAMF_CONTROL (object);

  if (self->priv->connection)
    {
      if (self->priv->launched_signal)
        {
          g_dbus_connection_signal_unsubscribe (self->priv->connection,
                                                self->priv->launched_signal);
        }

      g_object_unref (self->priv->connection);
    }

  g_list_free_full (self->priv->sources, g_object_unref);
}

static void
bamf_control_class_init (BamfControlClass * klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->constructed = bamf_control_constructed;
  obj_class->finalize = bamf_control_finalize;

  g_type_class_add_private (klass, sizeof (BamfControlPrivate));
}

void
bamf_control_register_application_for_pid (BamfControl *control,
                                           const char *application,
                                           gint32 pid)
{
  BamfMatcher *matcher = bamf_matcher_get_default ();
  bamf_matcher_register_desktop_file_for_pid (matcher, application, pid);
}

void
bamf_control_insert_desktop_file (BamfControl *control,
                                  const char *path)
{
  BamfMatcher *matcher = bamf_matcher_get_default ();
  bamf_matcher_load_desktop_file (matcher, path);
}

void
bamf_control_create_local_desktop_file (BamfControl *control, const char *app_path)
{
  BamfMatcher *matcher;
  BamfView *view;

  g_return_if_fail (BAMF_IS_CONTROL (control));
  g_return_if_fail (app_path);

  matcher = bamf_matcher_get_default ();
  view = bamf_matcher_get_view_by_path (matcher, app_path);

  if (BAMF_IS_APPLICATION (view))
    bamf_application_create_local_desktop_file (BAMF_APPLICATION (view));
}

static gboolean
bamf_control_on_quit (BamfControl *control)
{
  BamfDaemon *daemon = bamf_daemon_get_default ();
  bamf_daemon_stop (daemon);
  return FALSE;
}

void
bamf_control_quit (BamfControl *control)
{
  g_idle_add ((GSourceFunc) bamf_control_on_quit, control);
}

BamfControl *
bamf_control_get_default (void)
{
  static BamfControl *control;

  if (!BAMF_IS_CONTROL (control))
    {
      control = (BamfControl *) g_object_new (BAMF_TYPE_CONTROL, NULL);
    }

  return control;
}
