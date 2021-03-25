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
#include <json-glib/json-glib.h>
#include <string.h>
#include <config.h>
#include "gclue-mozilla.h"
#include "gclue-config.h"
#include "gclue-error.h"

/**
 * SECTION:gclue-mozilla
 * @short_description: Helpers to create queries for and parse response of,
 * Mozilla Location Service.
 *
 * Contains API to get the geolocation based on IP address, nearby WiFi networks
 * and 3GPP cell tower info. It uses
 * <ulink url="https://wiki.mozilla.org/CloudServices/Location">Mozilla Location
 * Service</ulink> to achieve that. The URL is kept in our configuration file so
 * its easy to switch to Google's API.
 **/

#define BSSID_LEN 7
#define BSSID_STR_LEN 18
#define MAX_SSID_LEN 32

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
        if (variant == NULL)
                return 0;

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

static const char *
get_url (void)
{
        GClueConfig *config;

        config = gclue_config_get_singleton ();

        return gclue_config_get_wifi_url (config);
}

SoupMessage *
gclue_mozilla_create_query (GList        *bss_list, /* As in Access Points */
                            GClue3GTower *tower,
                            GError      **error)
{
        SoupMessage *ret = NULL;
        JsonBuilder *builder;
        JsonGenerator *generator;
        JsonNode *root_node;
        char *data;
        gsize data_len;
        const char *uri;
        guint n_non_ignored_bsss;
        GList *iter;

        builder = json_builder_new ();
        json_builder_begin_object (builder);

        /* We send pure geoip query using empty object if both bss_list and
         * tower are NULL.
         *
         * If the list of non-ignored BSSs is <2, don’t bother submitting the
         * BSS list as MLS will only do a geoip lookup anyway.
         * See https://ichnaea.readthedocs.io/en/latest/api/geolocate.html#field-definition
         */
        n_non_ignored_bsss = 0;
        for (iter = bss_list; iter != NULL; iter = iter->next) {
                WPABSS *bss = WPA_BSS (iter->data);

                if (gclue_mozilla_should_ignore_bss (bss))
                        continue;

                n_non_ignored_bsss++;
        }

        if (tower != NULL) {
                json_builder_set_member_name (builder, "radioType");
                json_builder_add_string_value (builder, "gsm");

                json_builder_set_member_name (builder, "cellTowers");
                json_builder_begin_array (builder);

                json_builder_begin_object (builder);

                json_builder_set_member_name (builder, "cellId");
                json_builder_add_int_value (builder, tower->cell_id);
                json_builder_set_member_name (builder, "mobileCountryCode");
                json_builder_add_int_value (builder, tower->mcc);
                json_builder_set_member_name (builder, "mobileNetworkCode");
                json_builder_add_int_value (builder, tower->mnc);
                json_builder_set_member_name (builder, "locationAreaCode");
                json_builder_add_int_value (builder, tower->lac);

                json_builder_end_object (builder);

                json_builder_end_array (builder);
        }

        if (n_non_ignored_bsss >= 2) {
                json_builder_set_member_name (builder, "wifiAccessPoints");
                json_builder_begin_array (builder);

                for (iter = bss_list; iter != NULL; iter = iter->next) {
                        WPABSS *bss = WPA_BSS (iter->data);
                        char mac[BSSID_STR_LEN] = { 0 };
                        gint16 strength_dbm;

                        if (gclue_mozilla_should_ignore_bss (bss))
                                continue;

                        json_builder_begin_object (builder);
                        json_builder_set_member_name (builder, "macAddress");
                        get_bssid_from_bss (bss, mac);
                        json_builder_add_string_value (builder, mac);

                        json_builder_set_member_name (builder, "signalStrength");
                        strength_dbm = wpa_bss_get_signal (bss);
                        json_builder_add_int_value (builder, strength_dbm);
                        json_builder_end_object (builder);
                }
                json_builder_end_array (builder);
        }
        json_builder_end_object (builder);

        generator = json_generator_new ();
        root_node = json_builder_get_root (builder);
        json_generator_set_root (generator, root_node);
        data = json_generator_to_data (generator, &data_len);

        json_node_free (root_node);
        g_object_unref (builder);
        g_object_unref (generator);

        uri = get_url ();
        ret = soup_message_new ("POST", uri);
        soup_message_set_request (ret,
                                  "application/json",
                                  SOUP_MEMORY_TAKE,
                                  data,
                                  data_len);
        g_debug ("Sending following request to '%s':\n%s", uri, data);

        return ret;
}

static gboolean
parse_server_error (JsonObject *object, GError **error)
{
        JsonObject *error_obj;
        int code;
        const char *message;

        if (!json_object_has_member (object, "error"))
            return FALSE;

        error_obj = json_object_get_object_member (object, "error");
        code = json_object_get_int_member (error_obj, "code");
        message = json_object_get_string_member (error_obj, "message");

        g_set_error_literal (error, G_IO_ERROR, code, message);

        return TRUE;
}

GClueLocation *
gclue_mozilla_parse_response (const char *json,
                              GError    **error)
{
        JsonParser *parser;
        JsonNode *node;
        JsonObject *object, *loc_object;
        GClueLocation *location;
        gdouble latitude, longitude, accuracy;

        parser = json_parser_new ();

        if (!json_parser_load_from_data (parser, json, -1, error))
                return NULL;

        node = json_parser_get_root (parser);
        object = json_node_get_object (node);

        if (parse_server_error (object, error))
                return NULL;

        loc_object = json_object_get_object_member (object, "location");
        latitude = json_object_get_double_member (loc_object, "lat");
        longitude = json_object_get_double_member (loc_object, "lng");

        accuracy = json_object_get_double_member (object, "accuracy");

        location = gclue_location_new (latitude, longitude, accuracy);

        g_object_unref (parser);

        return location;
}

static const char *
get_submit_config (const char **nick)
{
        GClueConfig *config;

        config = gclue_config_get_singleton ();
        if (!gclue_config_get_wifi_submit_data (config))
                return NULL;

        *nick = gclue_config_get_wifi_submit_nick (config);

        return gclue_config_get_wifi_submit_url (config);
}

SoupMessage *
gclue_mozilla_create_submit_query (GClueLocation   *location,
                                   GList           *bss_list, /* As in Access Points */
                                   GClue3GTower    *tower,
                                   GError         **error)
{
        SoupMessage *ret = NULL;
        JsonBuilder *builder;
        JsonGenerator *generator;
        JsonNode *root_node;
        char *data, *timestamp;
        const char *url, *nick;
        gsize data_len;
        GList *iter;
        gdouble lat, lon, accuracy, altitude;
        GTimeVal tv;

        url = get_submit_config (&nick);
        if (url == NULL)
                goto out;

        builder = json_builder_new ();
        json_builder_begin_object (builder);

        json_builder_set_member_name (builder, "items");
        json_builder_begin_array (builder);

        json_builder_begin_object (builder);

        lat = gclue_location_get_latitude (location);
        json_builder_set_member_name (builder, "lat");
        json_builder_add_double_value (builder, lat);

        lon = gclue_location_get_longitude (location);
        json_builder_set_member_name (builder, "lon");
        json_builder_add_double_value (builder, lon);

        accuracy = gclue_location_get_accuracy (location);
        if (accuracy != GCLUE_LOCATION_ACCURACY_UNKNOWN) {
                json_builder_set_member_name (builder, "accuracy");
                json_builder_add_double_value (builder, accuracy);
        }

        altitude = gclue_location_get_altitude (location);
        if (altitude != GCLUE_LOCATION_ALTITUDE_UNKNOWN) {
                json_builder_set_member_name (builder, "altitude");
                json_builder_add_double_value (builder, altitude);
        }

        tv.tv_sec = gclue_location_get_timestamp (location);
        tv.tv_usec = 0;
        timestamp = g_time_val_to_iso8601 (&tv);
        json_builder_set_member_name (builder, "time");
        json_builder_add_string_value (builder, timestamp);
        g_free (timestamp);

        json_builder_set_member_name (builder, "radioType");
        json_builder_add_string_value (builder, "gsm");

        if (bss_list != NULL) {
                json_builder_set_member_name (builder, "wifi");
                json_builder_begin_array (builder);

                for (iter = bss_list; iter != NULL; iter = iter->next) {
                        WPABSS *bss = WPA_BSS (iter->data);
                        char mac[BSSID_STR_LEN] = { 0 };
                        gint16 strength_dbm;
                        guint16 frequency;

                        if (gclue_mozilla_should_ignore_bss (bss))
                                continue;

                        json_builder_begin_object (builder);
                        json_builder_set_member_name (builder, "key");
                        get_bssid_from_bss (bss, mac);
                        json_builder_add_string_value (builder, mac);

                        json_builder_set_member_name (builder, "signal");
                        strength_dbm = wpa_bss_get_signal (bss);
                        json_builder_add_int_value (builder, strength_dbm);

                        json_builder_set_member_name (builder, "frequency");
                        frequency = wpa_bss_get_frequency (bss);
                        json_builder_add_int_value (builder, frequency);
                        json_builder_end_object (builder);
                }

                json_builder_end_array (builder); /* wifi */
        }

        if (tower != NULL) {
                json_builder_set_member_name (builder, "cell");
                json_builder_begin_array (builder);

                json_builder_begin_object (builder);

                json_builder_set_member_name (builder, "radio");
                json_builder_add_string_value (builder, "gsm");
                json_builder_set_member_name (builder, "cid");
                json_builder_add_int_value (builder, tower->cell_id);
                json_builder_set_member_name (builder, "mcc");
                json_builder_add_int_value (builder, tower->mcc);
                json_builder_set_member_name (builder, "mnc");
                json_builder_add_int_value (builder, tower->mnc);
                json_builder_set_member_name (builder, "lac");
                json_builder_add_int_value (builder, tower->lac);

                json_builder_end_object (builder);

                json_builder_end_array (builder); /* cell */
        }

        json_builder_end_object (builder);
        json_builder_end_array (builder); /* items */
        json_builder_end_object (builder);

        generator = json_generator_new ();
        root_node = json_builder_get_root (builder);
        json_generator_set_root (generator, root_node);
        data = json_generator_to_data (generator, &data_len);

        json_node_free (root_node);
        g_object_unref (builder);
        g_object_unref (generator);

        ret = soup_message_new ("POST", url);
        if (nick != NULL && nick[0] != '\0')
                soup_message_headers_append (ret->request_headers,
                                             "X-Nickname",
                                             nick);
        soup_message_set_request (ret,
                                  "application/json",
                                  SOUP_MEMORY_TAKE,
                                  data,
                                  data_len);
        g_debug ("Sending following request to '%s':\n%s", url, data);

out:
        return ret;
}

gboolean
gclue_mozilla_should_ignore_bss (WPABSS *bss)
{
        char ssid[MAX_SSID_LEN] = { 0 };
        char bssid[BSSID_STR_LEN] = { 0 };
        guint len;

        if (!get_bssid_from_bss (bss, bssid)) {
                g_debug ("Ignoring WiFi AP with unknown BSSID..");
                return TRUE;
        }

        len = get_ssid_from_bss (bss, ssid);
        if (len == 0 || g_str_has_suffix (ssid, "_nomap")) {
                g_debug ("SSID for WiFi AP '%s' missing or has '_nomap' suffix."
                         ", Ignoring..",
                         bssid);
                return TRUE;
        }

        return FALSE;
}
