/* vim: set et ts=8 sw=8: */
/* gclue-config.c
 *
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * Geoclue is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Geoclue is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Geoclue; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#include <glib/gi18n.h>

#include "gclue-config.h"

#define CONFIG_FILE_PATH SYSCONFDIR "/geoclue/geoclue.conf"

/* This class will be responsible for fetching configuration. */

G_DEFINE_TYPE (GClueConfig, gclue_config, G_TYPE_OBJECT)

typedef struct
{
        char *id;
        gboolean allowed;
        gboolean system;
        int* users;
        gsize num_users;
} AppConfig;

static void
app_config_free (AppConfig *app_config)
{
        g_free (app_config->id);
        g_free (app_config->users);
        g_slice_free (AppConfig, app_config);
}

struct _GClueConfigPrivate
{
        GKeyFile *key_file;

        char **agents;
        gsize num_agents;

        char *wifi_url;
        gboolean wifi_submit;
        gboolean enable_nmea_source;
        char *wifi_submit_url;
        char *wifi_submit_nick;

        GList *app_configs;
};

static void
gclue_config_finalize (GObject *object)
{
        GClueConfigPrivate *priv;

        priv = GCLUE_CONFIG (object)->priv;

        g_clear_pointer (&priv->key_file, g_key_file_unref);
        g_clear_pointer (&priv->agents, g_strfreev);
        g_clear_pointer (&priv->wifi_url, g_free);
        g_clear_pointer (&priv->wifi_submit_url, g_free);
        g_clear_pointer (&priv->wifi_submit_nick, g_free);

        g_list_foreach (priv->app_configs, (GFunc) app_config_free, NULL);

        G_OBJECT_CLASS (gclue_config_parent_class)->finalize (object);
}

static void
gclue_config_class_init (GClueConfigClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gclue_config_finalize;
        g_type_class_add_private (object_class, sizeof (GClueConfigPrivate));
}

static void
load_agent_config (GClueConfig *config)
{
        GClueConfigPrivate *priv = config->priv;
        GError *error = NULL;

        priv->agents = g_key_file_get_string_list (priv->key_file,
                                                   "agent",
                                                   "whitelist",
                                                   &priv->num_agents,
                                                   &error);
        if (error != NULL) {
                g_critical ("Failed to read 'agent/whitelist' key: %s",
                            error->message);
                g_error_free (error);
        }
}

static void
load_app_configs (GClueConfig *config)
{
        const char *known_groups[] = { "agent", "wifi", "network-nmea", NULL };
        GClueConfigPrivate *priv = config->priv;
        gsize num_groups = 0, i;
        char **groups;

        groups = g_key_file_get_groups (priv->key_file, &num_groups);
        if (num_groups == 0)
                return;

        for (i = 0; i < num_groups; i++) {
                AppConfig *app_config;
                int* users;
                gsize num_users = 0, j;
                gboolean allowed, system;
                gboolean ignore = FALSE;
                GError *error = NULL;

                for (j = 0; known_groups[j] != NULL; j++)
                        if (strcmp (groups[i], known_groups[j]) == 0) {
                                ignore = TRUE;

                                continue;
                        }

                if (ignore)
                        continue;

                allowed = g_key_file_get_boolean (priv->key_file,
                                                  groups[i],
                                                  "allowed",
                                                  &error);
                if (error != NULL)
                        goto error_out;

                system = g_key_file_get_boolean (priv->key_file,
                                                 groups[i],
                                                 "system",
                                                 &error);
                if (error != NULL)
                        goto error_out;

                users = g_key_file_get_integer_list (priv->key_file,
                                                     groups[i],
                                                     "users",
                                                     &num_users,
                                                     &error);
                if (error != NULL)
                        goto error_out;

                app_config = g_slice_new0 (AppConfig);
                app_config->id = g_strdup (groups[i]);
                app_config->allowed = allowed;
                app_config->system = system;
                app_config->users = users;
                app_config->num_users = num_users;

                priv->app_configs = g_list_prepend (priv->app_configs, app_config);

                continue;
error_out:
                g_warning ("Failed to load configuration for app '%s': %s",
                           groups[i],
                           error->message);
                g_error_free (error);
        }

        g_strfreev (groups);
}

#define DEFAULT_WIFI_URL "https://location.services.mozilla.com/v1/geolocate?key=geoclue"
#define DEFAULT_WIFI_SUBMIT_URL "https://location.services.mozilla.com/v1/submit?key=geoclue"

static void
load_wifi_config (GClueConfig *config)
{
        GClueConfigPrivate *priv = config->priv;
        GError *error = NULL;

        priv->wifi_url = g_key_file_get_string (priv->key_file,
                                                "wifi",
                                                "url",
                                                &error);
        if (error != NULL) {
                g_warning ("%s", error->message);
                g_clear_error (&error);
                priv->wifi_url = g_strdup (DEFAULT_WIFI_URL);
        }

        priv->wifi_submit = g_key_file_get_boolean (priv->key_file,
                                                    "wifi",
                                                    "submit-data",
                                                    &error);
        if (error != NULL) {
                g_debug ("Failed to get config wifi/submit-data: %s",
                         error->message);
                g_error_free (error);

                return;
        }

        priv->wifi_submit_url = g_key_file_get_string (priv->key_file,
                                                       "wifi",
                                                       "submission-url",
                                                       &error);
        if (error != NULL) {
                g_debug ("No wifi submission URL: %s", error->message);
                g_error_free (error);
                priv->wifi_submit_url = g_strdup (DEFAULT_WIFI_SUBMIT_URL);
        }

        priv->wifi_submit_nick = g_key_file_get_string (priv->key_file,
                                                        "wifi",
                                                        "submission-nick",
                                                        &error);
        if (error != NULL) {
                g_debug ("No wifi submission nick: %s", error->message);
                g_error_free (error);
        }
}

static void
load_network_nmea_config (GClueConfig *config)
{
        GClueConfigPrivate *priv = config->priv;
        GError *error = NULL;

        priv->enable_nmea_source = g_key_file_get_boolean (priv->key_file,
                                                           "network-nmea",
                                                           "enable",
                                                           &error);
        if (error != NULL) {
                g_debug ("Failed to get config network-nmea/enable:"
                         " %s",
                         error->message);
                g_error_free (error);

                return;
        }
}

static void
gclue_config_init (GClueConfig *config)
{
        GError *error = NULL;

        config->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (config,
                                            GCLUE_TYPE_CONFIG,
                                            GClueConfigPrivate);
        config->priv->key_file = g_key_file_new ();
        g_key_file_load_from_file (config->priv->key_file,
                                   CONFIG_FILE_PATH,
                                   0,
                                   &error);
        if (error != NULL) {
                g_critical ("Failed to load configuration file '%s': %s",
                            CONFIG_FILE_PATH, error->message);
                g_error_free (error);

                return;
        }

        load_agent_config (config);
        load_app_configs (config);
        load_wifi_config (config);
        load_network_nmea_config (config);
}

GClueConfig *
gclue_config_get_singleton (void)
{
        static GClueConfig *config = NULL;

        if (config == NULL)
                config = g_object_new (GCLUE_TYPE_CONFIG, NULL);

        return config;
}

gboolean
gclue_config_is_agent_allowed (GClueConfig     *config,
                               const char      *desktop_id,
                               GClueClientInfo *agent_info)
{
        gsize i;

        for (i = 0; i < config->priv->num_agents; i++) {
                if (g_strcmp0 (desktop_id, config->priv->agents[i]) == 0)
                        return TRUE;
        }

        return FALSE;
}

GClueAppPerm
gclue_config_get_app_perm (GClueConfig     *config,
                           const char      *desktop_id,
                           GClueClientInfo *app_info)
{
        GClueConfigPrivate *priv = config->priv;
        GList *node;
        AppConfig *app_config = NULL;
        gsize i;
        guint64 uid;

        g_return_val_if_fail (desktop_id != NULL, GCLUE_APP_PERM_DISALLOWED);

        for (node = priv->app_configs; node != NULL; node = node->next) {
                if (strcmp (((AppConfig *) node->data)->id, desktop_id) == 0) {
                        app_config = (AppConfig *) node->data;

                        break;
                }
        }

        if (app_config == NULL) {
                g_debug ("'%s' not in configuration", desktop_id);

                return GCLUE_APP_PERM_ASK_AGENT;
        }

        if (!app_config->allowed) {
                g_debug ("'%s' disallowed by configuration", desktop_id);

                return GCLUE_APP_PERM_DISALLOWED;
        }

        if (app_config->num_users == 0)
                return GCLUE_APP_PERM_ALLOWED;

        uid = gclue_client_info_get_user_id (app_info);

        for (i = 0; i < app_config->num_users; i++) {
                if (app_config->users[i] == uid)
                        return GCLUE_APP_PERM_ALLOWED;
        }

        return GCLUE_APP_PERM_DISALLOWED;
}

gboolean
gclue_config_is_system_component (GClueConfig *config,
                                  const char  *desktop_id)
{
        GClueConfigPrivate *priv = config->priv;
        GList *node;
        AppConfig *app_config = NULL;

        g_return_val_if_fail (desktop_id != NULL, FALSE);

        for (node = priv->app_configs; node != NULL; node = node->next) {
                if (strcmp (((AppConfig *) node->data)->id, desktop_id) == 0) {
                        app_config = (AppConfig *) node->data;

                        break;
                }
        }

        return (app_config != NULL && app_config->system);
}

const char *
gclue_config_get_wifi_url (GClueConfig *config)
{
        return config->priv->wifi_url;
}

const char *
gclue_config_get_wifi_submit_url (GClueConfig *config)
{
        return config->priv->wifi_submit_url;
}

const char *
gclue_config_get_wifi_submit_nick (GClueConfig *config)
{
        return config->priv->wifi_submit_nick;
}

void
gclue_config_set_wifi_submit_nick (GClueConfig *config,
                                   const char  *nick)
{

        config->priv->wifi_submit_nick = g_strdup (nick);
}

gboolean
gclue_config_get_wifi_submit_data (GClueConfig *config)
{
        return config->priv->wifi_submit;
}

gboolean
gclue_config_get_enable_nmea_source (GClueConfig *config)
{
        return config->priv->enable_nmea_source;
}

void
gclue_config_set_wifi_submit_data (GClueConfig *config,
                                   gboolean     submit)
{

        config->priv->wifi_submit = submit;
}
