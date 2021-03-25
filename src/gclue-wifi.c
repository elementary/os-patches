/* vim: set et ts=8 sw=8: */
/*
 * Copyright 2014 Red Hat, Inc.
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

#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <config.h>
#include "gclue-wifi.h"
#include "gclue-config.h"
#include "gclue-error.h"
#include "gclue-mozilla.h"

#define WIFI_SCAN_TIMEOUT_HIGH_ACCURACY 10
/* Since this is only used for city-level accuracy, 5 minutes between each
 * scan is more than enough.
 */
#define WIFI_SCAN_TIMEOUT_LOW_ACCURACY  300

#define BSSID_LEN 7
#define BSSID_STR_LEN 18
#define MAX_SSID_LEN 32

/**
 * SECTION:gclue-wifi
 * @short_description: WiFi-based geolocation
 * @include: gclue-glib/gclue-wifi.h
 *
 * Contains functions to get the geolocation based on nearby WiFi networks.
 **/

static gboolean
gclue_wifi_start (GClueLocationSource *source);
static gboolean
gclue_wifi_stop (GClueLocationSource *source);

struct _GClueWifiPrivate {
        WPASupplicant *supplicant;
        WPAInterface *interface;
        GHashTable *bss_proxies;
        GHashTable *ignored_bss_proxies;
        gboolean bss_list_changed;

        gulong bss_added_id;
        gulong bss_removed_id;
        gulong scan_done_id;

        guint scan_timeout;

        GClueAccuracyLevel accuracy_level;
};

enum
{
        PROP_0,
        PROP_ACCURACY_LEVEL,
        LAST_PROP
};
static GParamSpec *gParamSpecs[LAST_PROP];

static SoupMessage *
gclue_wifi_create_query (GClueWebSource *source,
                         GError        **error);
static SoupMessage *
gclue_wifi_create_submit_query (GClueWebSource  *source,
                                GClueLocation   *location,
                                GError         **error);
static GClueLocation *
gclue_wifi_parse_response (GClueWebSource *source,
                           const char     *json,
                           GError        **error);
static GClueAccuracyLevel
gclue_wifi_get_available_accuracy_level (GClueWebSource *source,
                                         gboolean        net_available);

G_DEFINE_TYPE_WITH_CODE (GClueWifi,
                         gclue_wifi,
                         GCLUE_TYPE_WEB_SOURCE,
                         G_ADD_PRIVATE (GClueWifi))

static void
disconnect_bss_signals (GClueWifi *wifi);
static void
on_scan_call_done (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data);
static void
on_scan_done (WPAInterface *object,
              gboolean      success,
              gpointer      user_data);

static void
gclue_wifi_finalize (GObject *gwifi)
{
        GClueWifi *wifi = (GClueWifi *) gwifi;

        G_OBJECT_CLASS (gclue_wifi_parent_class)->finalize (gwifi);

        disconnect_bss_signals (wifi);
        g_clear_object (&wifi->priv->supplicant);
        g_clear_object (&wifi->priv->interface);
        g_clear_pointer (&wifi->priv->bss_proxies, g_hash_table_unref);
        g_clear_pointer (&wifi->priv->ignored_bss_proxies, g_hash_table_unref);
}

static void
gclue_wifi_constructed (GObject *object);

static void
gclue_wifi_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
        GClueWifi *wifi = GCLUE_WIFI (object);

        switch (prop_id) {
        case PROP_ACCURACY_LEVEL:
                g_value_set_enum (value, wifi->priv->accuracy_level);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_wifi_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
        GClueWifi *wifi = GCLUE_WIFI (object);

        switch (prop_id) {
        case PROP_ACCURACY_LEVEL:
                wifi->priv->accuracy_level = g_value_get_enum (value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_wifi_class_init (GClueWifiClass *klass)
{
        GClueWebSourceClass *web_class = GCLUE_WEB_SOURCE_CLASS (klass);
        GClueLocationSourceClass *source_class = GCLUE_LOCATION_SOURCE_CLASS (klass);
        GObjectClass *gwifi_class = G_OBJECT_CLASS (klass);

        source_class->start = gclue_wifi_start;
        source_class->stop = gclue_wifi_stop;
        web_class->create_submit_query = gclue_wifi_create_submit_query;
        web_class->create_query = gclue_wifi_create_query;
        web_class->parse_response = gclue_wifi_parse_response;
        web_class->get_available_accuracy_level =
                gclue_wifi_get_available_accuracy_level;
        gwifi_class->get_property = gclue_wifi_get_property;
        gwifi_class->set_property = gclue_wifi_set_property;
        gwifi_class->finalize = gclue_wifi_finalize;
        gwifi_class->constructed = gclue_wifi_constructed;

        gParamSpecs[PROP_ACCURACY_LEVEL] = g_param_spec_enum ("accuracy-level",
                                                              "AccuracyLevel",
                                                              "Max accuracy level",
                                                              GCLUE_TYPE_ACCURACY_LEVEL,
                                                              GCLUE_ACCURACY_LEVEL_CITY,
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (gwifi_class,
                                         PROP_ACCURACY_LEVEL,
                                         gParamSpecs[PROP_ACCURACY_LEVEL]);
}

static void
on_bss_added (WPAInterface *object,
              const gchar  *path,
              GVariant     *properties,
              gpointer      user_data);

static guint
variant_to_string (GVariant *variant, guint max_len, char *ret)
{
        guint i;
        guint len;

        len = g_variant_n_children (variant);
        if (len == 0)
                return 0;
        g_return_val_if_fail(len < max_len, 0);
        ret[len] = '\0';

        for (i = 0; i < len; i++)
                g_variant_get_child (variant,
                                     i,
                                     "y",
                                     &ret[i]);

        return len;
}

static guint
get_ssid_from_bss (WPABSS *bss, char *ssid)
{
        GVariant *variant = wpa_bss_get_ssid (bss);

        return variant_to_string (variant, MAX_SSID_LEN, ssid);
}

static gboolean
get_bssid_from_bss (WPABSS *bss, char *bssid)
{
        GVariant *variant;
        char raw_bssid[BSSID_LEN] = { 0 };
        guint raw_len, i;

        variant = wpa_bss_get_bssid (bss);
        if (variant == NULL)
                return FALSE;

        raw_len = variant_to_string (variant, BSSID_LEN, raw_bssid);
        g_return_val_if_fail (raw_len == BSSID_LEN - 1, FALSE);

        for (i = 0; i < BSSID_LEN - 1; i++) {
                unsigned char c = (unsigned char) raw_bssid[i];

                if (i == BSSID_LEN - 2) {
                        g_snprintf (bssid + (i * 3), 3, "%02x", c);
                } else {
                        g_snprintf (bssid + (i * 3), 4, "%02x:", c);
                }
        }

        return TRUE;
}

static void
add_bss_proxy (GClueWifi *wifi,
               WPABSS    *bss)
{
        const char *path;

        path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (bss));
        if (g_hash_table_replace (wifi->priv->bss_proxies,
                                  g_strdup (path),
                                  bss)) {
                char ssid[MAX_SSID_LEN] = { 0 };

                wifi->priv->bss_list_changed = TRUE;
                get_ssid_from_bss (bss, ssid);
                g_debug ("WiFi AP '%s' added.", ssid);
        }
}

static void
on_bss_signal_notify (GObject    *gobject,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        WPABSS *bss = WPA_BSS (gobject);
        const char *path;

        if (wpa_bss_get_signal (bss) <= -90) {
                char bssid[BSSID_STR_LEN] = { 0 };

                get_bssid_from_bss (bss, bssid);
                g_debug ("WiFi AP '%s' still has very low strength (%u dBm)"
                         ", ignoring again…",
                         bssid,
                         wpa_bss_get_signal (bss));
                return;
        }

        g_signal_handlers_disconnect_by_func (G_OBJECT (bss),
                                              on_bss_signal_notify,
                                              user_data);
        add_bss_proxy (wifi, g_object_ref (bss));
        path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (bss));
        g_hash_table_remove (wifi->priv->ignored_bss_proxies, path);
}

static void
on_bss_proxy_ready (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        WPABSS *bss;
        GError *error = NULL;
        char ssid[MAX_SSID_LEN] = { 0 };

        bss = wpa_bss_proxy_new_for_bus_finish (res, &error);
        if (bss == NULL) {
                g_debug ("%s", error->message);
                g_error_free (error);

                return;
        }

        if (gclue_mozilla_should_ignore_bss (bss)) {
                g_object_unref (bss);

                return;
        }

        get_ssid_from_bss (bss, ssid);
        g_debug ("WiFi AP '%s' added.", ssid);

        if (wpa_bss_get_signal (bss) <= -90) {
                const char *path;
                char bssid[BSSID_STR_LEN] = { 0 };

                get_bssid_from_bss (bss, bssid);
                g_debug ("WiFi AP '%s' has very low strength (%u dBm)"
                         ", ignoring for now…",
                         bssid,
                         wpa_bss_get_signal (bss));
                g_signal_connect (G_OBJECT (bss),
                                  "notify::signal",
                                  G_CALLBACK (on_bss_signal_notify),
                                  user_data);
                path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (bss));
                g_hash_table_replace (wifi->priv->ignored_bss_proxies,
                                      g_strdup (path),
                                      bss);
                return;
        }

        add_bss_proxy (wifi, bss);
}

static void
on_bss_added (WPAInterface *object,
              const gchar  *path,
              GVariant     *properties,
              gpointer      user_data)
{
        wpa_bss_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   "fi.w1.wpa_supplicant1",
                                   path,
                                   NULL,
                                   on_bss_proxy_ready,
                                   user_data);
}

static gboolean
remove_bss_from_hashtable (const gchar *path, GHashTable *hash_table)
{
        char ssid[MAX_SSID_LEN] = { 0 };
        WPABSS *bss = NULL;

        bss = g_hash_table_lookup (hash_table, path);
        if (bss == NULL)
                return FALSE;

        get_ssid_from_bss (bss, ssid);
        g_debug ("WiFi AP '%s' removed.", ssid);

        g_hash_table_remove (hash_table, path);

        return TRUE;
}

static void
on_bss_removed (WPAInterface *object,
                const gchar  *path,
                gpointer      user_data)
{
        GClueWifiPrivate *priv = GCLUE_WIFI (user_data)->priv;

        if (remove_bss_from_hashtable (path, priv->bss_proxies))
                priv->bss_list_changed = TRUE;
        remove_bss_from_hashtable (path, priv->ignored_bss_proxies);
}

static void
start_wifi_scan (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;
        GVariantBuilder builder;
        GVariant *args;

        g_debug ("Starting WiFi scan…");

        if (priv->scan_done_id == 0)
                priv->scan_done_id = g_signal_connect
                                        (priv->interface,
                                         "scan-done",
                                         G_CALLBACK (on_scan_done),
                                         wifi);

        g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
        g_variant_builder_add (&builder,
                               "{sv}",
                               "Type", g_variant_new ("s", "passive"));
        args = g_variant_builder_end (&builder);

        wpa_interface_call_scan (WPA_INTERFACE (priv->interface),
                                 args,
                                 NULL,
                                 on_scan_call_done,
                                 wifi);
}

static void
cancel_wifi_scan (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;

        if (priv->scan_timeout != 0) {
                g_source_remove (priv->scan_timeout);
                priv->scan_timeout = 0;
        }

        if (priv->scan_done_id != 0) {
                g_signal_handler_disconnect (priv->interface,
                                             priv->scan_done_id);
                priv->scan_done_id = 0;
        }
}

static gboolean
on_scan_timeout (gpointer user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        GClueWifiPrivate *priv = wifi->priv;

        g_debug ("WiFi scan timeout.");
        priv->scan_timeout = 0;

        if (priv->interface == NULL)
                return G_SOURCE_REMOVE;

        start_wifi_scan (wifi);

        return FALSE;
}

static void
on_scan_done (WPAInterface *object,
              gboolean      success,
              gpointer      user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        GClueWifiPrivate *priv = wifi->priv;
        guint timeout;

        if (!success) {
                g_warning ("WiFi scan failed");

                return;
        }
        g_debug ("WiFi scan completed");

        if (priv->interface == NULL)
                return;

        if (priv->bss_list_changed) {
                priv->bss_list_changed = FALSE;
                g_debug ("Refreshing location…");
                gclue_web_source_refresh (GCLUE_WEB_SOURCE (wifi));
        }

        /* If there was another scan already scheduled, cancel that and
         * re-schedule. Regardless of our internal book-keeping, this can happen
         * if wpa_supplicant emits the `ScanDone` signal due to a scan being
         * initiated by another client. */
        if (priv->scan_timeout != 0) {
                g_source_remove (priv->scan_timeout);
                priv->scan_timeout = 0;
        }

        /* With high-enough accuracy requests, we need to scan more often since
         * user's location can change quickly. With low accuracy, we don't since
         * we wouldn't want to drain power unnecessarily.
         */
        if (priv->accuracy_level >= GCLUE_ACCURACY_LEVEL_STREET)
                timeout = WIFI_SCAN_TIMEOUT_HIGH_ACCURACY;
        else
                timeout = WIFI_SCAN_TIMEOUT_LOW_ACCURACY;
        priv->scan_timeout = g_timeout_add_seconds (timeout,
                                                    on_scan_timeout,
                                                    wifi);
        g_debug ("Next scan scheduled in %u seconds", timeout);
}

static void
on_scan_call_done (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        GError *error = NULL;

        if (!wpa_interface_call_scan_finish
                (WPA_INTERFACE (source_object),
                 res,
                 &error)) {
                g_warning ("Scanning of WiFi networks failed: %s",
                           error->message);
                g_error_free (error);

                cancel_wifi_scan (wifi);

                return;
        }
}

static void
connect_bss_signals (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;
        const gchar *const *bss_list;
        guint i;

        if (priv->bss_added_id != 0)
                return;
        if (priv->interface == NULL) {
                gclue_web_source_refresh (GCLUE_WEB_SOURCE (wifi));

                return;
        }

        start_wifi_scan (wifi);

        priv->bss_list_changed = TRUE;
        priv->bss_added_id = g_signal_connect (priv->interface,
                                               "bss-added",
                                               G_CALLBACK (on_bss_added),
                                               wifi);
        priv->bss_removed_id = g_signal_connect (priv->interface,
                                                "bss-removed",
                                                G_CALLBACK (on_bss_removed),
                                                wifi);

        bss_list = wpa_interface_get_bsss (WPA_INTERFACE (priv->interface));
        if (bss_list == NULL)
                return;

        for (i = 0; bss_list[i] != NULL; i++)
                on_bss_added (WPA_INTERFACE (priv->interface),
                              bss_list[i],
                              NULL,
                              wifi);
}

static void
disconnect_bss_signals (GClueWifi *wifi)
{
        GClueWifiPrivate *priv = wifi->priv;

        cancel_wifi_scan (wifi);

        if (priv->bss_added_id != 0) {
                g_signal_handler_disconnect (priv->interface,
                                             priv->bss_added_id);
                priv->bss_added_id = 0;
        }
        if (priv->bss_removed_id != 0) {
                g_signal_handler_disconnect (priv->interface,
                                             priv->bss_removed_id);
                priv->bss_removed_id = 0;
        }

        g_hash_table_remove_all (priv->bss_proxies);
        g_hash_table_remove_all (priv->ignored_bss_proxies);
}

static gboolean
gclue_wifi_start (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;

        g_return_val_if_fail (GCLUE_IS_WIFI (source), FALSE);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_wifi_parent_class);
        if (!base_class->start (source))
                return FALSE;

        connect_bss_signals (GCLUE_WIFI (source));
        return TRUE;
}

static gboolean
gclue_wifi_stop (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;

        g_return_val_if_fail (GCLUE_IS_WIFI (source), FALSE);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_wifi_parent_class);
        if (!base_class->stop (source))
                return FALSE;

        disconnect_bss_signals (GCLUE_WIFI (source));
        return TRUE;
}

static GClueAccuracyLevel
gclue_wifi_get_available_accuracy_level (GClueWebSource *source,
                                         gboolean        net_available)
{
        GClueWifiPrivate *priv = GCLUE_WIFI (source)->priv;

        if (!net_available)
                return GCLUE_ACCURACY_LEVEL_NONE;
        else if (priv->interface != NULL &&
                 priv->accuracy_level != GCLUE_ACCURACY_LEVEL_CITY)
                return GCLUE_ACCURACY_LEVEL_STREET;
        else
                return GCLUE_ACCURACY_LEVEL_CITY;
}

static void
on_interface_proxy_ready (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        WPAInterface *interface;
        GError *error = NULL;

        interface = wpa_interface_proxy_new_for_bus_finish (res, &error);
        if (interface == NULL) {
                g_debug ("%s", error->message);
                g_error_free (error);

                return;
        }

        if (wifi->priv->interface != NULL) {
                g_object_unref (interface);
                return;
        }

        wifi->priv->interface = interface;
        g_debug ("WiFi device '%s' added.",
                 wpa_interface_get_ifname (interface));

        if (gclue_location_source_get_active (GCLUE_LOCATION_SOURCE (wifi)))
                connect_bss_signals (wifi);
        else
                gclue_web_source_refresh (GCLUE_WEB_SOURCE (wifi));
}

static void
on_interface_added (WPASupplicant *supplicant,
                    const gchar   *path,
                    GVariant      *properties,
                    gpointer       user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);

        if (wifi->priv->interface != NULL)
                return;

        wpa_interface_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         "fi.w1.wpa_supplicant1",
                                         path,
                                         NULL,
                                         on_interface_proxy_ready,
                                         wifi);
}

static void
on_interface_removed (WPASupplicant *supplicant,
                      const gchar   *path,
                      gpointer       user_data)
{
        GClueWifi *wifi = GCLUE_WIFI (user_data);
        GClueWifiPrivate *priv = wifi->priv;
        const char *object_path;

        if (priv->interface == NULL)
                return;

        object_path = g_dbus_proxy_get_object_path
                        (G_DBUS_PROXY (priv->interface));
        if (g_strcmp0 (object_path, path) != 0)
                return;

        g_debug ("WiFi device '%s' removed.",
                 wpa_interface_get_ifname (priv->interface));

        disconnect_bss_signals (wifi);
        g_clear_object (&wifi->priv->interface);

        gclue_web_source_refresh (GCLUE_WEB_SOURCE (wifi));
}

static void
gclue_wifi_init (GClueWifi *wifi)
{
        wifi->priv = G_TYPE_INSTANCE_GET_PRIVATE ((wifi), GCLUE_TYPE_WIFI, GClueWifiPrivate);

        wifi->priv->bss_proxies = g_hash_table_new_full (g_str_hash,
                                                         g_str_equal,
                                                         g_free,
                                                         g_object_unref);
        wifi->priv->ignored_bss_proxies = g_hash_table_new_full (g_str_hash,
                                                                 g_str_equal,
                                                                 g_free,
                                                                 g_object_unref);
}

static void
gclue_wifi_constructed (GObject *object)
{
        GClueWifi *wifi = GCLUE_WIFI (object);
        GClueWifiPrivate *priv = wifi->priv;
        const gchar *const *interfaces;
        GError *error = NULL;

        G_OBJECT_CLASS (gclue_wifi_parent_class)->constructed (object);

        if (wifi->priv->accuracy_level == GCLUE_ACCURACY_LEVEL_CITY) {
                GClueConfig *config = gclue_config_get_singleton ();

                if (!gclue_config_get_enable_wifi_source (config))
                        goto refresh_n_exit;
        }

        /* FIXME: We should be using async variant */
        priv->supplicant = wpa_supplicant_proxy_new_for_bus_sync
                        (G_BUS_TYPE_SYSTEM,
                         G_DBUS_PROXY_FLAGS_NONE,
                         "fi.w1.wpa_supplicant1",
                         "/fi/w1/wpa_supplicant1",
                         NULL,
                         &error);
        if (priv->supplicant == NULL) {
                g_warning ("Failed to connect to wpa_supplicant service: %s",
                           error->message);
                g_error_free (error);
                goto refresh_n_exit;
        }

        g_signal_connect (priv->supplicant,
                          "interface-added",
                          G_CALLBACK (on_interface_added),
                          wifi);
        g_signal_connect (priv->supplicant,
                          "interface-removed",
                          G_CALLBACK (on_interface_removed),
                          wifi);

        interfaces = wpa_supplicant_get_interfaces (priv->supplicant);
        if (interfaces != NULL && interfaces[0] != NULL)
                on_interface_added (priv->supplicant,
                                    interfaces[0],
                                    NULL,
                                    wifi);

refresh_n_exit:
        gclue_web_source_refresh (GCLUE_WEB_SOURCE (object));
}

static void
on_wifi_destroyed (gpointer data,
                   GObject *where_the_object_was)
{
        GClueWifi **wifi = (GClueWifi **) data;

        *wifi = NULL;
}

/**
 * gclue_wifi_new:
 *
 * Get the #GClueWifi singleton, for the specified max accuracy level @level.
 *
 * Returns: (transfer full): a new ref to #GClueWifi. Use g_object_unref()
 * when done.
 **/
GClueWifi *
gclue_wifi_get_singleton (GClueAccuracyLevel level)
{
        static GClueWifi *wifi[] = { NULL, NULL };
        guint i;
        gboolean scramble_location = FALSE;

        g_return_val_if_fail (level >= GCLUE_ACCURACY_LEVEL_CITY, NULL);
        if (level == GCLUE_ACCURACY_LEVEL_NEIGHBORHOOD)
                level = GCLUE_ACCURACY_LEVEL_CITY;

        if (level == GCLUE_ACCURACY_LEVEL_CITY) {
                GClueConfig *config = gclue_config_get_singleton ();

                i = 0;
                if (gclue_config_get_enable_wifi_source (config))
                        scramble_location = TRUE;
        } else {
                i = 1;
        }

        if (wifi[i] == NULL) {
                wifi[i] = g_object_new (GCLUE_TYPE_WIFI,
                                        "accuracy-level", level,
                                        "scramble-location", scramble_location,
                                        NULL);
                g_object_weak_ref (G_OBJECT (wifi[i]),
                                   on_wifi_destroyed,
                                   &wifi[i]);
        } else
                g_object_ref (wifi[i]);

        return wifi[i];
}

GClueAccuracyLevel
gclue_wifi_get_accuracy_level (GClueWifi *wifi)
{
        g_return_val_if_fail (GCLUE_IS_WIFI (wifi),
                              GCLUE_ACCURACY_LEVEL_NONE);

        return wifi->priv->accuracy_level;
}

/* Can return NULL without setting @error, signifying an empty BSS list. */
static GList *
get_bss_list (GClueWifi *wifi,
              GError   **error)
{
        if (wifi->priv->interface == NULL) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "No WiFi devices available");
                return NULL;
        }

        return g_hash_table_get_values (wifi->priv->bss_proxies);
}

static SoupMessage *
gclue_wifi_create_query (GClueWebSource *source,
                         GError        **error)
{
        GList *bss_list; /* As in Access Points */
        SoupMessage *msg;
        g_autoptr(GError) local_error = NULL;

        bss_list = get_bss_list (GCLUE_WIFI (source), &local_error);
        if (local_error != NULL) {
                g_propagate_error (error, g_steal_pointer (&local_error));
                return NULL;
        }

        /* Empty list? */
        if (bss_list == NULL) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "No WiFi networks found");
                return NULL;
        }

        msg = gclue_mozilla_create_query (bss_list, NULL, error);
        g_list_free (bss_list);
        return msg;
}

static GClueLocation *
gclue_wifi_parse_response (GClueWebSource *source,
                           const char     *json,
                           GError        **error)
{
        return gclue_mozilla_parse_response (json, error);
}

static SoupMessage *
gclue_wifi_create_submit_query (GClueWebSource  *source,
                                GClueLocation   *location,
                                GError         **error)
{
        GList *bss_list; /* As in Access Points */
        SoupMessage * msg;
        g_autoptr(GError) local_error = NULL;

        bss_list = get_bss_list (GCLUE_WIFI (source), &local_error);
        if (local_error != NULL) {
                g_propagate_error (error, g_steal_pointer (&local_error));
                return NULL;
        }

        /* Empty list? */
        if (bss_list == NULL) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "No WiFi networks found");
                return NULL;
        }

        msg = gclue_mozilla_create_submit_query (location,
                                                 bss_list,
                                                 NULL,
                                                 error);
        g_list_free (bss_list);
        return msg;
}
