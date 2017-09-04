/* cloudprovideraccount.h
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

#ifndef CLOUD_PROVIDER_ACCOUNT_H
#define CLOUD_PROVIDER_ACCOUNT_H

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * CloudProviderStatus:
 * @CLOUD_PROVIDER_STATUS_INVALID
 * @CLOUD_PROVIDER_STATUS_IDLE
 * @CLOUD_PROVIDER_STATUS_SYNCING
 * @CLOUD_PROVIDER_STATUS_ERROR
 *
 * Enum values used to describe the sync status of a cloud providers account
 **/
typedef enum {
  CLOUD_PROVIDER_STATUS_INVALID,
  CLOUD_PROVIDER_STATUS_IDLE,
  CLOUD_PROVIDER_STATUS_SYNCING,
  CLOUD_PROVIDER_STATUS_ERROR
} CloudProviderStatus;

#define TYPE_CLOUD_PROVIDER_ACCOUNT             (cloud_provider_account_get_type())
#define CLOUD_PROVIDER_ACCOUNT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOUD_PROVIDER_ACCOUNT, CloudProviderAccount))
#define CLOUD_PROVIDER_ACCOUNT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CLOUD_PROVIDER_ACCOUNT, CloudProviderAccountClass))
#define IS_CLOUD_PROVIDER_ACCOUNT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOUD_PROVIDER_ACCOUNT))
#define IS_CLOUD_PROVIDER_ACCOUNT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CLOUD_PROVIDER_ACCOUNT))
#define CLOUD_PROVIDER_ACCOUNT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOUD_PROVIDER_ACCOUNT, CloudProviderAccountClass))

typedef struct _CloudProviderAccount CloudProviderAccount;
typedef struct _CloudProviderAccountClass CloudProviderAccountClass;


struct _CloudProviderAccountClass
{
  GObjectClass parent_class;
};

struct _CloudProviderAccount
{
  GObject parent_instance;
};


GType cloud_provider_account_get_type (void) G_GNUC_CONST;
CloudProviderAccount *cloud_provider_account_new (const gchar *bus_name,
                                          const gchar *object_path);

gchar* cloud_provider_account_get_name (CloudProviderAccount *self);
CloudProviderStatus cloud_provider_account_get_status (CloudProviderAccount *self);
gchar* cloud_provider_account_get_status_details (CloudProviderAccount *self);
GIcon *cloud_provider_account_get_icon (CloudProviderAccount *self);
GMenuModel *cloud_provider_account_get_menu_model (CloudProviderAccount *self);
GActionGroup* cloud_provider_account_get_action_group (CloudProviderAccount *self);
gchar *cloud_provider_account_get_path (CloudProviderAccount *self);
gchar *cloud_provider_account_get_owner (CloudProviderAccount *self);
gboolean cloud_provider_account_is_available(CloudProviderAccount *self);
void cloud_provider_account_update (CloudProviderAccount *self);
G_END_DECLS


#endif /* CLOUD_PROVIDER_ACCOUNT_H */
