/* cloudprovidersexporter.h
 *
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

#ifndef CLOUD_PROVIDERS_PROVIDER_EXPORTER_H
#define CLOUD_PROVIDERS_PROVIDER_EXPORTER_H

#include <gio/gio.h>
#include "cloudprovidersaccountexporter.h"

G_BEGIN_DECLS

#define CLOUD_PROVIDERS_TYPE_PROVIDER_EXPORTER (cloud_providers_provider_exporter_get_type())
G_DECLARE_FINAL_TYPE (CloudProvidersProviderExporter, cloud_providers_provider_exporter, CLOUD_PROVIDERS, PROVIDER_EXPORTER, GObject);

struct _CloudProvidersProviderExporterClass
{
  GObjectClass parent_class;
};

void
cloud_providers_provider_exporter_remove_account (CloudProvidersProviderExporter *self,
                                                  CloudProvidersAccountExporter  *account);

void
cloud_providers_provider_exporter_set_name (CloudProvidersProviderExporter *self,
                                            const gchar                    *name);
const gchar*
cloud_providers_provider_exporter_get_name (CloudProvidersProviderExporter *self);

CloudProvidersProviderExporter*
cloud_providers_provider_exporter_new (GDBusConnection *bus,
                                       const gchar     *bus_name,
                                       const gchar     *object_path);

G_END_DECLS

#endif /* CLOUD_PROVIDERS_PROVIDER_EXPORTER_H */
