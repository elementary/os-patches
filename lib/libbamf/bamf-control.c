/*
 * Copyright 2010-2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of either or both of the following licenses:
 *
 * 1) the GNU Lesser General Public License version 3, as published by the
 * Free Software Foundation; and/or
 * 2) the GNU Lesser General Public License version 2.1, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the applicable version of the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License version 3 and version 2.1 along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 *
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Neil Jagdish Patel <neil.patel@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */
/**
 * SECTION:bamf-control
 * @short_description: The base class for all controls
 *
 * #BamfControl is the base class that all controls need to derive from.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <libbamf-private/bamf-private.h>
#include "bamf-control.h"
#include "bamf-view-private.h"

G_DEFINE_TYPE (BamfControl, bamf_control, G_TYPE_OBJECT);

#define BAMF_CONTROL_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BAMF_TYPE_CONTROL, BamfControlPrivate))

struct _BamfControlPrivate
{
  BamfDBusControl *proxy;
};

/* Globals */
static BamfControl * default_control = NULL;

/* Forwards */

/*
 * GObject stuff
 */

static void
bamf_control_dispose (GObject *object)
{
  BamfControl *self = BAMF_CONTROL (object);

  if (self->priv->proxy)
    {
      g_object_unref (self->priv->proxy);
      self->priv->proxy = NULL;
    }

  G_OBJECT_CLASS (bamf_control_parent_class)->dispose (object);
}

static void
bamf_control_finalize (GObject *object)
{
  default_control = NULL;

  G_OBJECT_CLASS (bamf_control_parent_class)->finalize (object);
}

static void
bamf_control_class_init (BamfControlClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  obj_class->dispose = bamf_control_dispose;
  obj_class->finalize = bamf_control_finalize;

  g_type_class_add_private (obj_class, sizeof (BamfControlPrivate));
  obj_class->dispose = bamf_control_dispose;
}

static void
bamf_control_init (BamfControl *self)
{
  BamfControlPrivate *priv;
  GError           *error = NULL;

  priv = self->priv = BAMF_CONTROL_GET_PRIVATE (self);

  priv->proxy = _bamf_dbus_control_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                           G_DBUS_PROXY_FLAGS_NONE,
                                                           BAMF_DBUS_SERVICE_NAME,
                                                           BAMF_DBUS_CONTROL_PATH,
                                                           NULL, &error);

  if (error)
    {
      g_error ("Unable to get "BAMF_DBUS_CONTROL_PATH" controller: %s", error->message);
      g_error_free (error);
      return;
    }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (priv->proxy), BAMF_DBUS_DEFAULT_TIMEOUT);
}

/**
 * bamf_control_get_default:
 *
 * Returns: (transfer full): The default #BamfControl reference.
 */
BamfControl *
bamf_control_get_default (void)
{
  if (BAMF_IS_CONTROL (default_control))
    return g_object_ref (default_control);

  default_control = g_object_new (BAMF_TYPE_CONTROL, NULL);

  return default_control;
}

void
bamf_control_insert_desktop_file (BamfControl *control, const gchar *desktop_file)
{
  BamfControlPrivate *priv;
  GError *error = NULL;

  g_return_if_fail (BAMF_IS_CONTROL (control));
  priv = control->priv;

  if (!_bamf_dbus_control_call_insert_desktop_file_sync (priv->proxy, desktop_file,
                                                         NULL, &error))
    {
      g_warning ("Failed to insert desktop file: %s", error->message);
      g_error_free (error);
    }
}

void
bamf_control_create_local_desktop_file (BamfControl *control, BamfApplication *app)
{
  BamfControlPrivate *priv;
  const gchar *app_path;
  GError *error = NULL;

  g_return_if_fail (BAMF_IS_CONTROL (control));
  g_return_if_fail (BAMF_IS_APPLICATION (app));

  priv = control->priv;
  app_path = _bamf_view_get_path (BAMF_VIEW (app));

  if (!app_path)
    return;

  if (!_bamf_dbus_control_call_create_local_desktop_file_sync (priv->proxy, app_path,
                                                               NULL, &error))
    {
      g_warning ("Failed to create local desktop file: %s", error->message);
      g_error_free (error);
    }
}

void
bamf_control_register_application_for_pid (BamfControl  *control,
                                           const gchar  *desktop_file,
                                           gint32        pid)
{
  BamfControlPrivate *priv;
  GError *error = NULL;

  g_return_if_fail (BAMF_IS_CONTROL (control));
  priv = control->priv;

  if (!_bamf_dbus_control_call_register_application_for_pid_sync (priv->proxy,
                                                                  desktop_file,
                                                                  pid, NULL,
                                                                  &error))
    {
      g_warning ("Failed to register application: %s", error->message);
      g_error_free (error);
    }
}
