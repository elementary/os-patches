/* cloudprovidersaccount.h
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

#ifndef CLOUD_PROVIDERS_ACCOUNT_H
#define CLOUD_PROVIDERS_ACCOUNT_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define CLOUD_PROVIDERS_ACCOUNT_DBUS_IFACE "org.freedesktop.CloudProviders.Account"

/**
 * CloudProviderStatus:
 * @CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID: Set if the initial state of the account is unknown
 * @CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE: Set if the account is in idle mode
 * @CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING: Set if the account is currently synchronizing data
 * @CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR: Set if an error occured
 *
 * Enum values used to describe the sync status of a cloud providers account
 **/
typedef enum {
  CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID,
  CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE,
  CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING,
  CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR
} CloudProvidersAccountStatus;

#define CLOUD_PROVIDERS_TYPE_ACCOUNT (cloud_providers_account_get_type())
G_DECLARE_FINAL_TYPE (CloudProvidersAccount, cloud_providers_account, CLOUD_PROVIDERS, ACCOUNT, GObject);

struct _CloudProvidersAccountClass
{
  GObjectClass parent_class;
};

CloudProvidersAccount *cloud_providers_account_new (GDBusProxy *proxy);

gchar* cloud_providers_account_get_name (CloudProvidersAccount *self);
CloudProvidersAccountStatus cloud_providers_account_get_status (CloudProvidersAccount *self);
gchar* cloud_providers_account_get_status_details (CloudProvidersAccount *self);
GIcon *cloud_providers_account_get_icon (CloudProvidersAccount *self);
GMenuModel *cloud_providers_account_get_menu_model (CloudProvidersAccount *self);
GActionGroup* cloud_providers_account_get_action_group (CloudProvidersAccount *self);
gchar *cloud_providers_account_get_path (CloudProvidersAccount *self);
G_END_DECLS


#endif /* CLOUD_PROVIDERS_ACCOUNT_H */
