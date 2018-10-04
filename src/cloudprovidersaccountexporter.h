/* cloudprovidersaccountexporter.h
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

#ifndef CLOUD_PROVIDERS_ACCOUNT_EXPORTER_H
#define CLOUD_PROVIDERS_ACCOUNT_EXPORTER_H

#include <gio/gio.h>
#include "cloudprovidersaccount.h"

G_BEGIN_DECLS

#define CLOUD_PROVIDERS_TYPE_ACCOUNT_EXPORTER (cloud_providers_account_exporter_get_type())
G_DECLARE_FINAL_TYPE (CloudProvidersAccountExporter, cloud_providers_account_exporter, CLOUD_PROVIDERS, ACCOUNT_EXPORTER, GObject)

typedef struct _CloudProvidersProviderExporter CloudProvidersProviderExporter;

struct _CloudProvidersAccountExporterClass
{
  GObjectClass parent_class;
};

CloudProvidersAccountExporter*
cloud_providers_account_exporter_new (CloudProvidersProviderExporter *provider,
                                      const gchar                    *bus_name);

void
cloud_providers_account_exporter_set_name (CloudProvidersAccountExporter *self,
                                           const gchar                    *name);

void
cloud_providers_account_exporter_set_status (CloudProvidersAccountExporter *self,
                                             CloudProvidersAccountStatus    status);

void
cloud_providers_account_exporter_set_status_details (CloudProvidersAccountExporter *self,
                                                     const gchar                   *status_details);

void
cloud_providers_account_exporter_set_icon (CloudProvidersAccountExporter *self,
                                           GIcon                          *icon);

void
cloud_providers_account_exporter_set_menu_model (CloudProvidersAccountExporter *self,
                                                 GMenuModel                     *menu_model);

void
cloud_providers_account_exporter_set_action_group (CloudProvidersAccountExporter *self,
                                                   GActionGroup                   *action_group);

void
cloud_providers_account_exporter_set_path (CloudProvidersAccountExporter *self,
                                           const gchar                    *path);

G_END_DECLS

#endif /* CLOUD_PROVIDERS_ACCOUNT_EXPORTER_H */
