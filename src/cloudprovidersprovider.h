/* cloudprovidersprovider.h
 *
 * Copyright (C) 2017 Carlos Soriano <csoriano@gnome.org>
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
#ifndef CLOUD_PROVIDERS_PROVIDER_H
#define CLOUD_PROVIDERS_PROVIDER_H

#include <gio/gio.h>
#include "cloudprovidersaccount.h"

G_BEGIN_DECLS

#define CLOUD_PROVIDERS_PROVIDER_DBUS_IFACE "org.freedesktop.CloudProviders.Provider"

#define CLOUD_PROVIDERS_TYPE_PROVIDER (cloud_providers_provider_get_type())

G_DECLARE_FINAL_TYPE (CloudProvidersProvider, cloud_providers_provider, CLOUD, PROVIDERS_PROVIDER, GObject)

CloudProvidersProvider *cloud_providers_provider_new (const gchar *bus_name,
                                                      const gchar *object_path);
const gchar* cloud_providers_provider_get_name (CloudProvidersProvider *self);
GList* cloud_providers_provider_get_accounts (CloudProvidersProvider *self);

G_END_DECLS

#endif /* CLOUD_PROVIDERS_PROVIDER_H */

