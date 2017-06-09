/* gtkcloudprovidermanager.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef GTK_CLOUD_PROVIDER_MANAGER_H
#define GTK_CLOUD_PROVIDER_MANAGER_H

#include <gio/gio.h>

#define GTK_CLOUD_PROVIDER_MANAGER_DBUS_IFACE "org.freedesktop.CloudProviderManager1"
#define GTK_CLOUD_PROVIDER_MANAGER_DBUS_NAME  "org.freedesktop.CloudProviderManager"
#define GTK_CLOUD_PROVIDER_MANAGER_DBUS_PATH  "/org/freedesktop/CloudProviderManager"

G_BEGIN_DECLS

#define GTK_TYPE_CLOUD_PROVIDER_MANAGER             (gtk_cloud_provider_manager_get_type())
#define GTK_CLOUD_PROVIDER_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CLOUD_PROVIDER_MANAGER, GtkCloudProviderManager))
#define GTK_CLOUD_PROVIDER_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CLOUD_PROVIDER_MANAGER, GtkCloudProviderManagerClass))
#define GTK_IS_CLOUD_PROVIDER_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CLOUD_PROVIDER_MANAGER))
#define GTK_IS_CLOUD_PROVIDER_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CLOUD_PROVIDER_MANAGER))
#define GTK_CLOUD_PROVIDER_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CLOUD_PROVIDER_MANAGER, GtkCloudProviderManagerClass))

typedef struct _GtkCloudProviderManager GtkCloudProviderManager;
typedef struct _GtkCloudProviderManagerClass GtkCloudProviderManagerClass;

struct _GtkCloudProviderManagerClass
{
  GObjectClass parent_class;
};

struct _GtkCloudProviderManager
{
  GObject parent_instance;
};

GType          gtk_cloud_provider_manager_get_type          (void) G_GNUC_CONST;
GtkCloudProviderManager *gtk_cloud_provider_manager_dup_singleton (void);
void gtk_cloud_provider_manager_update (GtkCloudProviderManager *self);
GList *gtk_cloud_provider_manager_get_providers (GtkCloudProviderManager *self);

G_END_DECLS

#endif /* GTK_CLOUD_PROVIDER_MANAGER_H */
