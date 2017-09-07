/* cloudprovidersmanager.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 * Copyright (C) 2017 Julius Haertl <jus@bitgrid.net>
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
#ifndef CLOUD_PROVIDERS_MANAGER_H
#define CLOUD_PROVIDERS_MANAGER_H

#include <gio/gio.h>
#include "cloudproviders-generated.h"

#define CLOUD_PROVIDERS_MANAGER_DBUS_IFACE "org.freedesktop.CloudProviders.Manager1"
#define CLOUD_PROVIDERS_MANAGER_DBUS_NAME  "org.freedesktop.CloudProviders.Manager"
#define CLOUD_PROVIDERS_MANAGER_DBUS_PATH  "/org/freedesktop/CloudProviders/Manager"

G_BEGIN_DECLS

#define CLOUD_PROVIDERS_TYPE_MANAGER (cloud_providers_manager_get_type())
G_DECLARE_FINAL_TYPE (CloudProvidersManager, cloud_providers_manager, CLOUD_PROVIDERS, MANAGER, GObject);

struct _CloudProvidersManagerClass
{
  GObjectClass parent_class;
};

CloudProvidersManager *cloud_providers_manager_new (GDBusConnection *connection);
G_END_DECLS

#endif /* CLOUD_PROVIDERS_MANAGER_H */
