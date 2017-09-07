/* cloudprovidersaccount.c
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

#include "cloudprovidersaccount.h"
#include "cloudprovidersprovider.h"
#include "cloudproviders-generated.h"
#include "enums.h"

struct _CloudProvidersAccount
{
  GObject parent;

  gchar *name;
  gchar *path;
  CloudProvidersAccountStatus status;
  gchar *status_details;
  GIcon *icon;
  GMenuModel *menu_model;
  GActionGroup *action_group;

  GDBusConnection *bus;
  CloudProvidersDbusAccount *proxy;
  gchar *bus_name;
  gchar *object_path;
};

G_DEFINE_TYPE (CloudProvidersAccount, cloud_providers_account, G_TYPE_OBJECT)

/**
 * SECTION:cloudprovideraccount
 * @title: CloudProvidersAccount
 * @short_description: Base object for representing a single account for clients.
 * @include: src/cloudprovideraccount.h
 *
 * #CloudProvidersAccount is the basic object used to construct the integrator UI
 * and actions that a provider will present to the user, from the client side.
 * Integrators of the cloud providers can use this object to poll the
 * #CloudProvider menus, status and actions.
 */

enum
{
    PROP_0,
    PROP_NAME,
    PROP_ICON,
    PROP_STATUS,
    PROP_STATUS_DETAILS,
    PROP_MENU_MODEL,
    PROP_ACTION_GROUP,
    PROP_PATH,
    N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
on_name_changed (GObject               *object,
                 GParamSpec            *pspec,
                 CloudProvidersAccount *self)
{
    g_free (self->name);
    self->name = cloud_providers_dbus_account_dup_name (self->proxy);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

static void
on_status_changed (GObject               *object,
                   GParamSpec            *pspec,
                   CloudProvidersAccount *self)
{
    self->status = cloud_providers_dbus_account_get_status (self->proxy);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATUS]);
}

static void
on_status_details_changed (GObject               *object,
                           GParamSpec            *pspec,
                           CloudProvidersAccount *self)
{
    g_free (self->status_details);
    self->status_details = cloud_providers_dbus_account_dup_status_details (self->proxy);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATUS_DETAILS]);
}

static void
on_icon_changed (GObject               *object,
                 GParamSpec            *pspec,
                 CloudProvidersAccount *self)
{
    GError *error = NULL;

    self->icon = g_icon_new_for_string (cloud_providers_dbus_account_get_icon (self->proxy), &error);
    g_print ("setting icon %d", g_icon_hash (self->icon));
    if (error != NULL)
    {
        g_printerr ("Error getting the icon in the client %s", error->message);
    }
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
}

static void
on_path_changed (GObject               *object,
                 GParamSpec            *pspec,
                 CloudProvidersAccount *self)
{
    g_free (self->path);
    self->path = cloud_providers_dbus_account_dup_path (self->proxy);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

static void
setup_proxy (CloudProvidersAccount *self)
{
    GMenuModel *menu_model;
    GActionGroup *action_group;
    GIcon *icon = NULL;
    GError *error = NULL;

    g_signal_connect (self->proxy, "notify::name", G_CALLBACK (on_name_changed), self);
    g_signal_connect (self->proxy, "notify::status", G_CALLBACK (on_status_changed), self);
    g_signal_connect (self->proxy, "notify::status-details", G_CALLBACK (on_status_details_changed), self);
    g_signal_connect (self->proxy, "notify::icon", G_CALLBACK (on_icon_changed), self);
    g_signal_connect (self->proxy, "notify::path", G_CALLBACK (on_path_changed), self);

    icon = g_icon_new_for_string (cloud_providers_dbus_account_get_icon (self->proxy), &error);
    if (error != NULL)
    {
        g_printerr ("Error getting the icon in the client %s", error->message);
    }
    menu_model = (GMenuModel*) g_dbus_menu_model_get (self->bus,
                                                      self->bus_name,
                                                      self->object_path);
    action_group = (GActionGroup*) g_dbus_action_group_get (self->bus,
                                                            self->bus_name,
                                                            self->object_path);
    self->name = cloud_providers_dbus_account_dup_name (self->proxy);
    self->status = cloud_providers_dbus_account_get_status (self->proxy);
    self->status_details = cloud_providers_dbus_account_dup_status_details (self->proxy);
    self->icon = icon;
    self->path = cloud_providers_dbus_account_dup_path (self->proxy);
    self->menu_model = menu_model;
    self->action_group = action_group;

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATUS]);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATUS_DETAILS]);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTION_GROUP]);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_MODEL]);
}

static void
cloud_providers_account_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
    CloudProvidersAccount *self = CLOUD_PROVIDERS_ACCOUNT (object);

    switch (prop_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, self->name);
        }
        break;

        case PROP_STATUS:
        {
            g_value_set_enum (value, self->status);
        }
        break;

        case PROP_STATUS_DETAILS:
        {
            g_value_set_string (value, self->status_details);
        }
        break;

        case PROP_ICON:
        {
            g_value_set_object (value, self->icon);
        }
        break;

        case PROP_PATH:
        {
            g_value_set_string (value, self->path);
        }
        break;

        case PROP_ACTION_GROUP:
        {
            g_value_set_object (value, self->action_group);
        }
        break;

        case PROP_MENU_MODEL:
        {
            g_value_set_object (value, self->menu_model);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
cloud_providers_account_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
    switch (prop_id)
    {
        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }

    }
}

/**
 * cloud_providers_account_new
 * @bus_name: DBus bus name
 * @object_path: Path to export the DBus object to
 *
 * A #CloudProvidersAccount object are used to fetch details about cloud providers from DBus.
 * Object are usually fetched from cloud_providers_get_providers() as a list.
 */
CloudProvidersAccount*
cloud_providers_account_new (CloudProvidersDbusAccountProxy *proxy)
{
  CloudProvidersAccount *self;

  self = g_object_new (CLOUD_PROVIDERS_TYPE_ACCOUNT, NULL);


  g_object_ref (proxy);
  self->proxy = CLOUD_PROVIDERS_DBUS_ACCOUNT (proxy);
  self->bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (self->proxy));
  self->bus_name = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (self->proxy));
  self->object_path = g_strdup (g_dbus_proxy_get_object_path (G_DBUS_PROXY (self->proxy)));

  setup_proxy (self);

  return self;
}

static void
cloud_providers_account_finalize (GObject *object)
{
  CloudProvidersAccount *self = (CloudProvidersAccount *)object;

  g_signal_handlers_disconnect_by_data (self->proxy, self);
  g_free (self->name);
  g_free (self->path);
  g_clear_object (&self->icon);
  g_clear_object (&self->action_group);
  g_clear_object (&self->bus);
  g_clear_object (&self->proxy);
  g_free (self->bus_name);
  g_free (self->object_path);

  G_OBJECT_CLASS (cloud_providers_account_parent_class)->finalize (object);
}

static void
cloud_providers_account_class_init (CloudProvidersAccountClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cloud_providers_account_set_property;
  object_class->get_property = cloud_providers_account_get_property;
  object_class->finalize = cloud_providers_account_finalize;

    properties [PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The name of the account",
                             NULL,
                             (G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_NAME,
                                     properties [PROP_NAME]);
    properties [PROP_PATH] =
        g_param_spec_string ("path",
                             "Path",
                             "The path of the directory where files are located",
                             NULL,
                             (G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_PATH,
                                     properties [PROP_PATH]);
    properties [PROP_STATUS] =
        g_param_spec_enum ("status",
                           "Status",
                           "Status of the account",
                           CLOUD_TYPE_PROVIDERS_ACCOUNT_STATUS,
                           CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID,
                           (G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_STATUS,
                                     properties [PROP_STATUS]);
    properties [PROP_STATUS_DETAILS] =
        g_param_spec_string ("status-details",
                             "StatusDetails",
                             "The details of the account status",
                             NULL,
                             (G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_STATUS_DETAILS,
                                     properties [PROP_STATUS_DETAILS]);
    properties [PROP_ICON] =
        g_param_spec_object ("icon",
                             "Icon",
                             "The icon representing the account",
                             G_TYPE_ICON,
                             (G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_ICON,
                                     properties [PROP_ICON]);
    properties [PROP_MENU_MODEL] =
        g_param_spec_object ("menu-model",
                             "MenuModel",
                             "The menu model associated with the account",
                             G_TYPE_MENU_MODEL,
                             (G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_MENU_MODEL,
                                     properties [PROP_MENU_MODEL]);
    properties [PROP_ACTION_GROUP] =
        g_param_spec_object ("action-group",
                             "ActionGroup",
                             "The action group associated with the account and menu model",
                             G_TYPE_ACTION_GROUP,
                             (G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_ACTION_GROUP,
                                     properties [PROP_ACTION_GROUP]);


}

static void
cloud_providers_account_init (CloudProvidersAccount *self)
{
}

/**
 * cloud_providers_account_get_name:
 * @self: A cloud provider account
 *
 * Get the name of the account
 *
 * Returns: The name of the cloud provider account
 */
gchar*
cloud_providers_account_get_name (CloudProvidersAccount *self)
{
  return self->name;
}

/**
 * cloud_providers_account_get_status:
 * @self: A cloud provider account
 *
 * Get the status of the account
 *
 * Returns: The status of the cloud provider account
 */
CloudProvidersAccountStatus
cloud_providers_account_get_status (CloudProvidersAccount *self)
{
  return self->status;
}

/**
 * cloud_providers_account_get_status_details:
 * @self: A cloud provider account
 *
 * Get the status details of the account
 *
 * Returns: The status detail description of the cloud provider account
 */
gchar*
cloud_providers_account_get_status_details (CloudProvidersAccount *self)
{
  return self->status_details;
}

/**
 * cloud_providers_account_get_icon:
 * @self: A cloud provider account
 *
 * Get the icon of the account
 *
 * Returns: The icon of the cloud provider account
 */
GIcon*
cloud_providers_account_get_icon (CloudProvidersAccount *self)
{
  return self->icon;
}

/**
 * cloud_providers_account_get_menu_model:
 * @self: A cloud provider account
 *
 * Get the menu model exported for the account
 *
 * Returns: The menu model exported by the cloud provider account
 */
GMenuModel*
cloud_providers_account_get_menu_model (CloudProvidersAccount *self)
{
  return self->menu_model;
}

/**
 * cloud_providers_account_get_action_group:
 * @self: A cloud provider account
 *
 * Get the action group exported in addition to the #GMenuModel from
 * cloud_providers_account_get_menu_model()
 *
 * Returns: The action group exported by the cloud provider account
 */
GActionGroup*
cloud_providers_account_get_action_group (CloudProvidersAccount *self)
{
  return self->action_group;
}

/**
 * cloud_providers_account_get_path:
 * @self: A cloud provider account
 *
 * Get the directory path of the account
 *
 * Returns: The directory path of the cloud provider account
 */
gchar *
cloud_providers_account_get_path (CloudProvidersAccount *self)
{
  return self->path;
}
