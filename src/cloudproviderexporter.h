/* cloudprovider.h
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

#ifndef CLOUD_PROVIDER_EXPORTER_H
#define CLOUD_PROVIDER_EXPORTER_H

/* for CloudProviderStatus enum */
#include "cloudprovideraccount.h"
#include "cloudprovideraccountexporter.h"

G_BEGIN_DECLS

#define TYPE_CLOUD_PROVIDER_EXPORTER             (cloud_provider_exporter_get_type())
#define CLOUD_PROVIDER_EXPORTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOUD_PROVIDER_EXPORTER, CloudProviderExporter))
#define CLOUD_PROVIDER_EXPORTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CLOUD_PROVIDER_EXPORTER, CloudProviderExporterClass))
#define IS_CLOUD_PROVIDER_EXPORTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOUD_PROVIDER_EXPORTER))
#define IS_CLOUD_PROVIDER_EXPORTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CLOUD_PROVIDER_EXPORTER))
#define CLOUD_PROVIDER_EXPORTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOUD_PROVIDER_EXPORTER, CloudProviderExporterClass))

typedef struct _CloudProviderExporter CloudProviderExporter;
typedef struct _CloudProviderExporterClass CloudProviderExporterClass;


struct _CloudProviderExporterClass
{
  GObjectClass parent_class;
};

struct _CloudProviderExporter
{
  GObject parent_instance;
};

GType
cloud_provider_exporter_get_type (void) G_GNUC_CONST;

void
cloud_provider_exporter_add_account (CloudProviderExporter        *self,
                                     CloudProviderAccountExporter *account);
void
cloud_provider_exporter_remove_account (CloudProviderExporter        *self,
                                        CloudProviderAccountExporter *account);


void
cloud_provider_exporter_export_objects (CloudProviderExporter *self);

void
cloud_provider_exporter_emit_changed (CloudProviderExporter *self,
                                      const gchar           *account_name);
void
cloud_provider_exporter_emit_account_changed (CloudProviderExporter       *self,
                                              CloudProviderAccountExporter *account);

CloudProviderExporter*
cloud_provider_exporter_new (GDBusConnection *bus,
                             const gchar     *bus_name,
                             const gchar     *object_path);

G_END_DECLS


#endif /* CLOUD_PROVIDER_EXPORTER_H */
