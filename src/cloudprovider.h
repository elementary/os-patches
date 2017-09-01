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

#ifndef CLOUD_PROVIDER_H
#define CLOUD_PROVIDER_H

#include "cloudprovider-generated.h"
/* for CloudProviderStatus enum */
#include "cloudprovideraccount.h"
#include "cloudprovideraccountexporter.h"

G_BEGIN_DECLS

#define TYPE_CLOUD_PROVIDER             (cloud_provider_get_type())
#define CLOUD_PROVIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOUD_PROVIDER, CloudProvider))
#define CLOUD_PROVIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CLOUD_PROVIDER, CloudProviderClass))
#define IS_CLOUD_PROVIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOUD_PROVIDER))
#define IS_CLOUD_PROVIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CLOUD_PROVIDER))
#define CLOUD_PROVIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOUD_PROVIDER, CloudProviderClass))

typedef struct _CloudProvider CloudProvider;
typedef struct _CloudProviderClass CloudProviderClass;


struct _CloudProviderClass
{
  GObjectClass parent_class;
};

struct _CloudProvider
{
  GObject parent_instance;
};

GType
cloud_provider_get_type (void) G_GNUC_CONST;
void
cloud_provider_export_account (CloudProvider* cloud_provider,
                               const gchar *account_name,
                               CloudProviderAccount1 *account);
void
cloud_provider_unexport_account (CloudProvider* cloud_provider,
                                 const gchar *account_name);
guint
cloud_provider_export_menu (CloudProvider* cloud_provider,
                            const gchar *account_name,
                            GMenuModel *model);
void
cloud_provider_unexport_menu (CloudProvider* cloud_provider,
                              const gchar *account_name);
guint
cloud_provider_export_action_group (CloudProvider* cloud_provider,
                              const gchar *account_name,
                              GActionGroup *action_group);
void
cloud_provider_unexport_action_group (CloudProvider *cloud_provider,
                                      const gchar   *account_name);

void
cloud_provider_add_account (CloudProvider                *cloud_provider,
                            CloudProviderAccountExporter *account);

void
cloud_provider_export_objects (CloudProvider* cloud_provider);

void
cloud_provider_emit_changed (CloudProvider *cloud_provider, const gchar *account_name);

CloudProvider*
cloud_provider_new (GDBusConnection *bus,
                    const gchar *bus_name,
                    const gchar *object_path);

G_END_DECLS


#endif /* CLOUD_PROVIDER_H */
