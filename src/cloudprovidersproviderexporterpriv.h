/* cloudprovidersproviderexporterpriv.h
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

#ifndef CLOUD_PROVIDERS_PROVIDER_EXPORTER_PRIV_H
#define CLOUD_PROVIDERS_PROVIDER_EXPORTER_PRIV_H

#include <gio/gio.h>
#include "cloudprovidersproviderexporter.h"
#include "cloudproviders-generated.h"
G_BEGIN_DECLS

void
cloud_providers_provider_exporter_add_account (CloudProvidersProviderExporter *self,
                                               CloudProvidersAccountExporter  *account);
const gchar*
cloud_providers_provider_exporter_get_object_path (CloudProvidersProviderExporter *self);

GDBusConnection*
cloud_providers_provider_exporter_get_bus (CloudProvidersProviderExporter *self);

G_END_DECLS

#endif
