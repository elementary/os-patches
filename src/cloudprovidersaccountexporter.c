/* cloudprovidersaccountexporter.c
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

#include <gio/gio.h>
#include "cloudprovidersaccount.h"
#include "cloudprovidersaccountexporter.h"
#include "cloudprovidersproviderexporter.h"
#include "cloudprovidersaccountexporterpriv.h"
#include "cloudprovidersproviderexporterpriv.h"
#include "cloudproviders-generated.h"
#include "enums.h"

struct _CloudProvidersAccountExporter
{
  GObject parent;

  gchar *bus_name;
  CloudProvidersDbusAccount *skeleton;

  GDBusConnection *bus;
  gchar *object_path;
  gchar *name;
  gchar *path;
  CloudProvidersAccountStatus status;
  gchar *status_details;
  gchar *icon;
  GMenuModel *menu_model;
  GActionGroup *action_group;

  gint menu_model_export_id;
  gint action_group_export_id;
  CloudProvidersProviderExporter *provider;
};

G_DEFINE_TYPE (CloudProvidersAccountExporter, cloud_providers_account_exporter, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_NAME,
    PROP_BUS_NAME,
    PROP_PROVIDER,
    PROP_ICON,
    PROP_STATUS,
    PROP_STATUS_DETAILS,
    PROP_MENU_MODEL,
    PROP_ACTION_GROUP,
    PROP_PATH,
    N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * SECTION:cloudprovidersaccountexporter
 * @title: CloudProvidersAccountExporter
 * @short_description: Base object for representing a cloud providers account
 * @include: src/cloudprovidersaccountexporter.h
 */

static void
export_menu_model (CloudProvidersAccountExporter *self)
{
  GError *error = NULL;

  self->menu_model_export_id = g_dbus_connection_export_menu_model (self->bus,
                                                                    self->object_path,
                                                                    self->menu_model,
                                                                    &error);
  if (self->menu_model_export_id == 0)
    {
      g_warning ("Menu export failed: %s", error->message);
    }
}

#if 0
static void
unexport_menu_model (CloudProvidersAccountExporter *self)
{
    if(self->menu_model_export_id != 0)
    {
        g_dbus_connection_unexport_menu_model(self->bus, self->menu_model_export_id);
    }
}
#endif

static void
export_action_group(CloudProvidersAccountExporter *self)
{
  GError *error = NULL;

  self->action_group_export_id = g_dbus_connection_export_action_group (self->bus,
                                                                        self->object_path,
                                                                        self->action_group,
                                                                        &error);
  if (self->action_group_export_id == 0)
    {
      g_warning ("Action export failed: %s", error->message);
    }
}

#if 0
/**
 * cloud_providers_account_exporter_unexport_action_group:
 * @self: The cloud provider
 * @account_name: The name of the account
 *
 * Unexport the GActionGroup exported by cloud_providers_account_exporter_export_action_group
 */
static void
unexport_action_group(CloudProvidersAccountExporter *self)
{
  if (self->action_group_export_id != 0)
  {
    g_dbus_connection_unexport_action_group(self->bus, self->action_group_export_id);
  }
}
#endif

gchar *
cloud_providers_account_exporter_get_object_path (CloudProvidersAccountExporter *self)
{
  return self->object_path;
}

gchar *
cloud_providers_account_exporter_get_bus_name (CloudProvidersAccountExporter *self)
{
    return self->bus_name;
}

CloudProvidersDbusAccount*
cloud_providers_account_exporter_get_skeleton (CloudProvidersAccountExporter *self)
{
  return self->skeleton;
}

/**
 * cloud_providers_account_exporter_new:
 * @bus_name: A unique name for the account
 *               must be a valid DBus object name
 *
 * Create a new #CloudProvidersAccountExporter object
 */
CloudProvidersAccountExporter*
cloud_providers_account_exporter_new (CloudProvidersProviderExporter *provider,
                                      const gchar                    *bus_name)
{
  CloudProvidersAccountExporter *self;

  self = g_object_new (CLOUD_PROVIDERS_TYPE_ACCOUNT_EXPORTER,
                       "provider", provider,
                       "bus-name", bus_name,
                       NULL);
  return self;
}

static void
cloud_providers_account_exporter_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
    CloudProvidersAccountExporter *self = CLOUD_PROVIDERS_ACCOUNT_EXPORTER (object);

    switch (prop_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, self->name);
        }
        break;

        case PROP_BUS_NAME:
        {
            g_value_set_string (value, self->bus_name);
        }
        break;

        case PROP_PROVIDER:
        {
            g_value_set_object (value, self->provider);
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
            g_autoptr (GIcon) icon = NULL;

            icon = g_icon_new_for_string (self->icon, NULL);
            g_value_set_object (value, icon);
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
cloud_providers_account_exporter_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
    CloudProvidersAccountExporter *self = CLOUD_PROVIDERS_ACCOUNT_EXPORTER (object);

    switch (prop_id)
    {
        case PROP_NAME:
        {
            g_free (self->name);
            self->name = g_strdup (g_value_get_string (value));
            cloud_providers_dbus_account_set_name (self->skeleton, self->name);
        }
        break;

        case PROP_BUS_NAME:
        {
            g_return_if_fail (self->bus_name == NULL);
            self->bus_name = g_strdup (g_value_get_string (value));
        }
        break;

        case PROP_PROVIDER:
        {
            g_return_if_fail (self->provider == NULL);
            self->provider = g_object_ref (g_value_get_object (value));
        }
        break;

        case PROP_STATUS:
        {
            self->status = g_value_get_enum (value);
            cloud_providers_dbus_account_set_status (self->skeleton, self->status);
        }
        break;

        case PROP_STATUS_DETAILS:
        {
            g_free (self->status_details);
            self->status_details = g_strdup (g_value_get_string (value));
            cloud_providers_dbus_account_set_status_details (self->skeleton, self->status_details);
        }
        break;

        case PROP_ICON:
        {
            g_free (self->icon);
            self->icon = g_icon_to_string (g_value_get_object (value));
            cloud_providers_dbus_account_set_icon (self->skeleton,
                                                   self->icon);
        }
        break;

        case PROP_PATH:
        {
            g_free (self->path);
            self->path = g_strdup (g_value_get_string (value));
            cloud_providers_dbus_account_set_path (self->skeleton, self->path);
        }
        break;

        case PROP_ACTION_GROUP:
        {
            g_return_if_fail (self->action_group == NULL);

            self->action_group = g_object_ref (g_value_get_object (value));
            export_action_group (self);
        }
        break;

        case PROP_MENU_MODEL:
        {
            g_return_if_fail (self->menu_model == NULL);

            self->menu_model = g_object_ref (g_value_get_object (value));
            export_menu_model (self);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

void
cloud_providers_account_exporter_set_name (CloudProvidersAccountExporter *self,
                                           const gchar                   *name)
{
    g_object_set (self, "name", name, NULL);
}

void
cloud_providers_account_exporter_set_status (CloudProvidersAccountExporter *self,
                                             CloudProvidersAccountStatus    status)
{
    g_object_set (self, "status", status, NULL);
}

void
cloud_providers_account_exporter_set_status_details (CloudProvidersAccountExporter *self,
                                                     const gchar                   *status_details)
{
    g_object_set (self, "status_details", status_details, NULL);
}

void
cloud_providers_account_exporter_set_icon (CloudProvidersAccountExporter *self,
                                           GIcon                         *icon)
{
    g_object_set (self, "icon", icon, NULL);
}

/**
 * cloud_providers_account_exporter_set_menu_model:
 * @self: The account
 * @menu_model: The menu model to export
 *
 * One of the benefits of the integration is to display a menu with available
 * options for an account. Use this function to export a GMenuModel menu to be
 * displayed by the choosen integration by the desktop environment or application.
 */
void
cloud_providers_account_exporter_set_menu_model (CloudProvidersAccountExporter *self,
                                                 GMenuModel                    *menu_model)
{
    g_object_set (self, "menu_model", menu_model, NULL);
}

/**
 * cloud_providers_account_exporter_set_action_group:
 * @self: The cloud provider
 * @account_name: The name of the account
 * @action_group: The GActionGroup to be used by the menu exported by cloud_providers_account_exporter_export_menu
 *
 * In order for a menu exported with cloud_providers_account_exporter_export_menu to receive events
 * that will eventually call your callbacks, it needs the corresponding GAcionGroup.
 * Use this function to export it.
 */
void
cloud_providers_account_exporter_set_action_group (CloudProvidersAccountExporter *self,
                                                   GActionGroup                  *action_group)
{
    g_object_set (self, "action_group", action_group, NULL);
}

void
cloud_providers_account_exporter_set_path (CloudProvidersAccountExporter *self,
                                           const gchar                   *path)
{
    g_object_set (self, "path", path, NULL);
}

static void
cloud_providers_account_exporter_finalize (GObject *object)
{
    CloudProvidersAccountExporter *self = (CloudProvidersAccountExporter *)object;

    g_free (self->bus_name);
    g_object_unref (self->skeleton);

    g_free (self->name);
    g_free (self->object_path);
    g_free (self->status_details);
    g_free (self->path);
    g_free (self->icon);
    g_object_unref (self->action_group);
    g_object_unref (self->menu_model);
    g_object_unref (self->provider);

    G_OBJECT_CLASS (cloud_providers_account_exporter_parent_class)->finalize (object);
}

static void
cloud_providers_account_exporter_constructed (GObject *object)
{
  const gchar *provider_object_path;
  g_autofree gchar *object_path = NULL;
  CloudProvidersAccountExporter *self = CLOUD_PROVIDERS_ACCOUNT_EXPORTER (object);

  self->bus = cloud_providers_provider_exporter_get_bus (self->provider);
  self->skeleton = cloud_providers_dbus_account_skeleton_new ();
  provider_object_path = cloud_providers_provider_exporter_get_object_path (self->provider);
  self->object_path = g_strconcat (provider_object_path, "/", self->bus_name, NULL);
  cloud_providers_provider_exporter_add_account (self->provider, self);
}

static void
cloud_providers_account_exporter_init (CloudProvidersAccountExporter *self)
{
}

static void
cloud_providers_account_exporter_class_init (CloudProvidersAccountExporterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cloud_providers_account_exporter_get_property;
  object_class->set_property = cloud_providers_account_exporter_set_property;
  object_class->constructed = cloud_providers_account_exporter_constructed;
  object_class->finalize = cloud_providers_account_exporter_finalize;

    properties [PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The name of the account",
                             NULL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_NAME,
                                     properties [PROP_NAME]);
    properties [PROP_BUS_NAME] =
        g_param_spec_string ("bus-name",
                             "BusName",
                             "The bus name of the account",
                             NULL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS|
                              G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class, PROP_BUS_NAME,
                                     properties [PROP_BUS_NAME]);
    properties [PROP_PROVIDER] =
        g_param_spec_object ("provider",
                             "Provider",
                             "The provider associated with the account",
                             CLOUD_PROVIDERS_TYPE_PROVIDER_EXPORTER,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS |
                              G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class, PROP_PROVIDER,
                                     properties [PROP_PROVIDER]);
    properties [PROP_PATH] =
        g_param_spec_string ("path",
                             "Path",
                             "The path of the directory where files are located",
                             NULL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_PATH,
                                     properties [PROP_PATH]);
    properties [PROP_STATUS] =
        g_param_spec_enum ("status",
                          "Status",
                          "Status of the account",
                          CLOUD_TYPE_PROVIDERS_ACCOUNT_STATUS,
                          CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_STATUS,
                                     properties [PROP_STATUS]);
    properties [PROP_STATUS_DETAILS] =
        g_param_spec_string ("status-details",
                             "StatusDetails",
                             "The details of the account status",
                             NULL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_STATUS_DETAILS,
                                     properties [PROP_STATUS_DETAILS]);
    properties [PROP_ICON] =
        g_param_spec_object ("icon",
                             "Icon",
                             "The icon representing the account",
                             G_TYPE_ICON,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_ICON,
                                     properties [PROP_ICON]);
    properties [PROP_MENU_MODEL] =
        g_param_spec_object ("menu-model",
                             "MenuModel",
                             "The menu model associated with the account",
                             G_TYPE_MENU_MODEL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_MENU_MODEL,
                                     properties [PROP_MENU_MODEL]);
    properties [PROP_ACTION_GROUP] =
        g_param_spec_object ("action-group",
                             "ActionGroup",
                             "The action group associated with the account and menu model",
                             G_TYPE_ACTION_GROUP,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_ACTION_GROUP,
                                     properties [PROP_ACTION_GROUP]);
}

