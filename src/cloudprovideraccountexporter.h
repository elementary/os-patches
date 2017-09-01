/* cloudprovideraccount.h
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

#ifndef CLOUD_PROVIDER_ACCOUNT_EXPORTER_H
#define CLOUD_PROVIDER_ACCOUNT_EXPORTER_H

G_BEGIN_DECLS

#define TYPE_CLOUD_PROVIDER_ACCOUNT_EXPORTER            (cloud_provider_account_exporter_get_type())
#define CLOUD_PROVIDER_ACCOUNT_EXPORTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOUD_PROVIDER_ACCOUNT_EXPORTER, CloudProviderAccountExporter))
#define CLOUD_PROVIDER_ACCOUNT_EXPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CLOUD_PROVIDER_ACCOUNT_EXPORTER, CloudProviderAccountExporterClass))
#define IS_CLOUD_PROVIDER_ACCOUNT_EXPORTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOUD_PROVIDER_ACCOUNT_EXPORTER))
#define IS_CLOUD_PROVIDER_ACCOUNT_EXPORTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CLOUD_PROVIDER_ACCOUNT_EXPORTER))
#define CLOUD_PROVIDER_ACCOUNT_EXPORTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOUD_PROVIDER_ACCOUNT_EXPORTER, CloudProviderAccountExporterClass))

typedef struct _CloudProviderAccountExporter CloudProviderAccountExporter;
typedef struct _CloudProviderAccountExporterClass CloudProviderAccountExporterClass;


struct _CloudProviderAccountExporterClass
{
  GObjectClass parent_class;
};

struct _CloudProviderAccountExporter
{
  GObject parent_instance;
};

GType
cloud_provider_account_exporter_get_type (void) G_GNUC_CONST;

void
cloud_provider_account_exporter_emit_changed (CloudProviderAccountExporter *cloud_provider_account);

CloudProviderAccountExporter*
cloud_provider_account_exporter_new (const gchar *account_object_name);

G_END_DECLS

#endif /* CLOUD_PROVIDER_ACCOUNT_EXPORTER_H */
