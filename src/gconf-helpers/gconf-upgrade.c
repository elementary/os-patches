/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
 *
 * Dan Williams <dcbw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2005 - 2008 Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>

/* libgnome-keyring is deprecated. */
#include "utils.h"
NM_PRAGMA_WARNING_DISABLE("-Wdeprecated-declarations")
#include <gnome-keyring.h>

#include <nm-setting-connection.h>
#include <nm-setting-wireless.h>
#include <nm-setting-wireless-security.h>
#include <nm-setting-8021x.h>
#include <nm-setting-vpn.h>
#include <nm-setting-ip4-config.h>
#include <nm-utils.h>

#include "gconf-upgrade.h"
#include "gconf-helpers.h"

#include <nm-connection.h>

#define APPLET_PREFS_PATH "/apps/nm-applet"

/* Old wireless.h defs */

/* IW_AUTH_80211_AUTH_ALG values (bit field) */
#define IW_AUTH_ALG_OPEN_SYSTEM 0x00000001
#define IW_AUTH_ALG_SHARED_KEY  0x00000002

/* IW_AUTH_WPA_VERSION values (bit field) */
#define IW_AUTH_WPA_VERSION_WPA  0x00000002
#define IW_AUTH_WPA_VERSION_WPA2 0x00000004

/* IW_AUTH_PAIRWISE_CIPHER and IW_AUTH_GROUP_CIPHER values (bit field) */
#define IW_AUTH_CIPHER_WEP40  0x00000002
#define IW_AUTH_CIPHER_TKIP   0x00000004
#define IW_AUTH_CIPHER_CCMP   0x00000008
#define IW_AUTH_CIPHER_WEP104 0x00000010

/* NM 0.6 compat defines */

#define NM_AUTH_TYPE_WPA_PSK_AUTO 0x00000000
#define NM_AUTH_TYPE_NONE         0x00000001
#define NM_AUTH_TYPE_WEP40        0x00000002
#define NM_AUTH_TYPE_WPA_PSK_TKIP 0x00000004
#define NM_AUTH_TYPE_WPA_PSK_CCMP 0x00000008
#define NM_AUTH_TYPE_WEP104       0x00000010
#define NM_AUTH_TYPE_WPA_EAP      0x00000020
#define NM_AUTH_TYPE_LEAP         0x00000040

#define NM_EAP_METHOD_MD5         0x00000001
#define NM_EAP_METHOD_MSCHAP      0x00000002
#define NM_EAP_METHOD_OTP         0x00000004
#define NM_EAP_METHOD_GTC         0x00000008
#define NM_EAP_METHOD_PEAP        0x00000010
#define NM_EAP_METHOD_TLS         0x00000020
#define NM_EAP_METHOD_TTLS        0x00000040

#define NM_PHASE2_AUTH_NONE       0x00000000
#define NM_PHASE2_AUTH_PAP        0x00010000
#define NM_PHASE2_AUTH_MSCHAP     0x00020000
#define NM_PHASE2_AUTH_MSCHAPV2   0x00030000
#define NM_PHASE2_AUTH_GTC        0x00040000

#define NMA_CA_CERT_IGNORE_TAG  "nma-ca-cert-ignore"
#define NMA_PHASE2_CA_CERT_IGNORE_TAG  "nma-phase2-ca-cert-ignore"
#define NMA_PRIVATE_KEY_PASSWORD_TAG "nma-private-key-password"
#define NMA_PHASE2_PRIVATE_KEY_PASSWORD_TAG "nma-phase2-private-key-password"
#define NMA_PATH_CA_CERT_TAG "nma-path-ca-cert"
#define NMA_PATH_PHASE2_CA_CERT_TAG "nma-path-phase2-ca-cert"
#define NMA_PATH_CLIENT_CERT_TAG "nma-path-client-cert"
#define NMA_PATH_PHASE2_CLIENT_CERT_TAG "nma-path-phase2-client-cert"
#define NMA_PATH_PRIVATE_KEY_TAG "nma-path-private-key"
#define NMA_PATH_PHASE2_PRIVATE_KEY_TAG "nma-path-phase2-private-key"


struct flagnames {
	const char * const name;
	guint value;
};

/* Reads an enum value stored as an integer and returns the
 * corresponding string from @names.
 */
static gboolean
get_enum_helper (GConfClient             *client,
			  const char              *path,
			  const char              *key,
			  const char              *network,
			  const struct flagnames  *names,
			  char                   **value)
{
	int ival, i;

	if (!nm_gconf_get_int_helper (client, path, key, network, &ival)) {
		g_warning ("Missing key '%s' on NM 0.6 connection %s", key, network);
		return FALSE;
	}

	for (i = 0; names[i].name; i++) {
		if (names[i].value == ival) {
			*value = g_strdup (names[i].name);
			return TRUE;
		}
	}

	g_warning ("Bad value '%d' for key '%s' on NM 0.6 connection %s", ival, key, network);
	return FALSE;
}

/* Reads a bitfield value stored as an integer and returns a list of
 * names from @names corresponding to the bits that are set.
 */
static gboolean
get_bitfield_helper (GConfClient             *client,
				 const char              *path,
				 const char              *key,
				 const char              *network,
				 const struct flagnames  *names,
				 GSList                 **value)
{
	int ival, i;

	if (!nm_gconf_get_int_helper (client, path, key, network, &ival)) {
		g_warning ("Missing key '%s' on NM 0.6 connection %s", key, network);
		return FALSE;
	}

	*value = NULL;
	for (i = 0; names[i].name; i++) {
		if (names[i].value & ival) {
			*value = g_slist_prepend (*value, g_strdup (names[i].name));
			ival = ival & ~names[i].value;
		}
	}

	if (ival) {
		g_slist_free_full (*value, g_free);
		g_warning ("Bad value '%d' for key '%s' on NM 0.6 connection %s", ival, key, network);
		return FALSE;
	}

	return TRUE;
}

static gboolean
get_mandatory_string_helper (GConfClient  *client,
					    const char   *path,
					    const char   *key,
					    const char   *network,
					    char        **value)
{
	if (!nm_gconf_get_string_helper (client, path, key, network, value)) {
		g_warning ("Missing key '%s' on NM 0.6 connection %s", key, network);
		return FALSE;
	}
	return TRUE;
}

static char *
get_06_keyring_secret (const char *network, const char *attr_name)
{
	GnomeKeyringResult result;
	GList *found_list = NULL;
	char *secret = NULL;

	/* Get the PSK out of the keyring */
	result = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                         &found_list,
	                                         attr_name ? attr_name : "essid",
	                                         GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                         network,
	                                         NULL);
	if ((result == GNOME_KEYRING_RESULT_OK) && (g_list_length (found_list) > 0)) {
		GnomeKeyringFound *found = (GnomeKeyringFound *) found_list->data;

		secret = g_strdup (found->secret);
		gnome_keyring_found_list_free (found_list);
	}
	return secret;
}

static void
clear_06_keyring_secret (char *secret)
{
	if (secret) {
		memset (secret, 0, strlen (secret));
		g_free (secret);
	}
}

static const struct flagnames wep_auth_algorithms[] = {
	{ "open",   IW_AUTH_ALG_OPEN_SYSTEM },
	{ "shared", IW_AUTH_ALG_SHARED_KEY },
	{ NULL, 0 }
};

static NMSettingWirelessSecurity *
nm_gconf_read_0_6_wep_settings (GConfClient *client,
                                const char *path,
                                const char *network,
                                const char *uuid,
                                const char *id)
{
	NMSettingWirelessSecurity *s_wireless_sec;
	char *auth_alg, *secret = NULL;

	if (!get_enum_helper (client, path, "wep_auth_algorithm", network, wep_auth_algorithms, &auth_alg))
		return NULL;

	s_wireless_sec = NM_SETTING_WIRELESS_SECURITY (nm_setting_wireless_security_new ());
	g_object_set (s_wireless_sec,
	              NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "none",
	              NM_SETTING_WIRELESS_SECURITY_WEP_TX_KEYIDX, 0,
	              NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, auth_alg,
	              NULL);
	g_free (auth_alg);

	secret = get_06_keyring_secret (network, NULL);
	if (secret) {
		nm_gconf_add_keyring_item (uuid, id,
		                           NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
		                           NM_SETTING_WIRELESS_SECURITY_WEP_KEY0,
		                           secret);
		clear_06_keyring_secret (secret);
	}

	return s_wireless_sec;
}

static NMSettingWirelessSecurity *
nm_gconf_read_0_6_wpa_settings (GConfClient *client,
                                const char *path,
                                const char *network,
                                const char *uuid,
                                const char *id)
{
	NMSettingWirelessSecurity *s_wireless_sec = NULL;
	char *secret = NULL;

	s_wireless_sec = NM_SETTING_WIRELESS_SECURITY (nm_setting_wireless_security_new ());
	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk", NULL);
	nm_setting_wireless_security_add_proto (s_wireless_sec, "wpa");
	nm_setting_wireless_security_add_proto (s_wireless_sec, "rsn");

	secret = get_06_keyring_secret (network, NULL);
	if (secret) {
		nm_gconf_add_keyring_item (uuid, id,
		                           NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
		                           NM_SETTING_WIRELESS_SECURITY_PSK,
		                           secret);
		clear_06_keyring_secret (secret);
	}

	return s_wireless_sec;
}

static const struct flagnames eap_methods[] = {
	{ "md5",    NM_EAP_METHOD_MD5 },
	{ "mschap", NM_EAP_METHOD_MSCHAP },
	{ "otp",    NM_EAP_METHOD_OTP },
	{ "gtc",    NM_EAP_METHOD_GTC },
	{ "peap",   NM_EAP_METHOD_PEAP },
	{ "tls",    NM_EAP_METHOD_TLS },
	{ "ttls",   NM_EAP_METHOD_TTLS },
	{ NULL, 0 }
};

static const struct flagnames eap_key_types[] = {
	{ "wep40",  IW_AUTH_CIPHER_WEP40 },
	{ "wep104", IW_AUTH_CIPHER_WEP104 },
	{ "tkip",   IW_AUTH_CIPHER_TKIP },
	{ "ccmp",   IW_AUTH_CIPHER_CCMP },
	{ NULL, 0 }
};

static const struct flagnames eap_phase2_types[] = {
	{ "none",     NM_PHASE2_AUTH_NONE },
	{ "pap",      NM_PHASE2_AUTH_PAP },
	{ "mschap",   NM_PHASE2_AUTH_MSCHAP },
	{ "mschapv2", NM_PHASE2_AUTH_MSCHAPV2 },
	{ "gtc",      NM_PHASE2_AUTH_GTC },
	{ NULL, 0 }
};

static NMSettingWirelessSecurity *
nm_gconf_read_0_6_eap_settings (GConfClient *client,
                                const char *path,
                                const char *network,
                                const char *uuid,
                                const char *id,
                                NMSetting8021x **s_8021x)
{
	NMSettingWirelessSecurity *wsec = NULL;
	GSList *eaps = NULL, *ciphers = NULL, *iter;
	char *phase2 = NULL, *identity = NULL, *anon_identity = NULL, *secret = NULL;
	const char *eap = NULL;
	gboolean wep_ciphers = FALSE, wpa_ciphers = FALSE;

	if (!get_bitfield_helper (client, path, "wpa_eap_eap_method", network, eap_methods, &eaps))
		goto out;
	/* Default to TTLS */
	eap = (eaps && eaps->data) ? (const char *) eaps->data : "ttls";

	if (!get_enum_helper (client, path, "wpa_eap_phase2_type", network, eap_phase2_types, &phase2))
		goto out;
	/* Default to MSCHAPv2 */
	phase2 = phase2 ? phase2 : g_strdup ("mschapv2");

	if (!get_bitfield_helper (client, path, "wpa_eap_key_type", network, eap_key_types, &ciphers))
		goto out;
	for (iter = ciphers; iter; iter = g_slist_next (iter)) {
		if (   !strcmp ((const char *) iter->data, "wep104")
		    || !strcmp ((const char *) iter->data, "wep40"))
			wep_ciphers = TRUE;
		if (   !strcmp ((const char *) iter->data, "ccmp")
		    || !strcmp ((const char *) iter->data, "tkip"))
			wpa_ciphers = TRUE;
	}

	if (!get_mandatory_string_helper (client, path, "wpa_eap_identity", network, &identity))
		goto out;
	nm_gconf_get_string_helper (client, path, "wpa_eap_anon_identity", network, &anon_identity);

	wsec = NM_SETTING_WIRELESS_SECURITY (nm_setting_wireless_security_new ());
	*s_8021x = NM_SETTING_802_1X (nm_setting_802_1x_new ());

	/* Dynamic WEP or WPA? */
	if (wep_ciphers && !wpa_ciphers)
		g_object_set (wsec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "ieee8021x", NULL);
	else {
		g_object_set (wsec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap", NULL);
		nm_setting_wireless_security_add_proto (wsec, "wpa");
		nm_setting_wireless_security_add_proto (wsec, "rsn");
	}

	g_object_set (*s_8021x,
	              NM_SETTING_802_1X_IDENTITY, identity,
	              NM_SETTING_802_1X_ANONYMOUS_IDENTITY, anon_identity,
	              NULL);
	nm_setting_802_1x_add_eap_method (*s_8021x, eap);

	secret = get_06_keyring_secret (network, NULL);
	if (secret) {
		nm_gconf_add_keyring_item (uuid, id,
		                           NM_SETTING_802_1X_SETTING_NAME,
		                           NM_SETTING_802_1X_PASSWORD,
		                           secret);
		clear_06_keyring_secret (secret);
	}

	/* Add phase2 if the eap method uses inner auth */
	if (!strcmp (eap, "ttls") || !strcmp (eap, "peap")) {
		/* If the method is actually unsupported in NM 0.7, default to mschapv2 */
		if (   strcmp (phase2, "pap")
		    && strcmp (phase2, "mschap")
		    && strcmp (phase2, "mschapv2")) {
			g_free (phase2);
			phase2 = g_strdup ("mschapv2");
		}
		g_object_set (*s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, phase2, NULL);

		secret = get_06_keyring_secret (network, "private-key-passwd");
		if (secret) {
			nm_gconf_add_keyring_item (uuid, id,
			                           NM_SETTING_802_1X_SETTING_NAME,
			                           NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD,
			                           secret);
			clear_06_keyring_secret (secret);
		}
	} else if (!strcmp (eap, "tls")) {
		secret = get_06_keyring_secret (network, "private-key-passwd");
		if (secret) {
			nm_gconf_add_keyring_item (uuid, id,
			                           NM_SETTING_802_1X_SETTING_NAME,
			                           NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD,
			                           secret);
			clear_06_keyring_secret (secret);
		}
	}

out:
	g_slist_free_full (eaps, g_free);
	g_slist_free_full (ciphers, g_free);
	g_free (phase2);
	g_free (identity);
	g_free (anon_identity);
	return wsec;
}

static NMSettingWirelessSecurity *
nm_gconf_read_0_6_leap_settings (GConfClient *client,
                                 const char *path,
                                 const char *network,
                                 const char *uuid,
                                 const char *id,
                                 NMSetting8021x **s_8021x)
{
	NMSettingWirelessSecurity *s_wireless_sec = NULL;
	char *username = NULL, *key_mgmt = NULL, *secret = NULL;

	if (!get_mandatory_string_helper (client, path, "leap_key_mgmt", network, &key_mgmt))
		goto out;
	if (!get_mandatory_string_helper (client, path, "leap_username", network, &username))
		goto out;

	s_wireless_sec = NM_SETTING_WIRELESS_SECURITY (nm_setting_wireless_security_new ());

	secret = get_06_keyring_secret (network, NULL);

	if (!strcmp (key_mgmt, "WPA-EAP")) {
		g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap", NULL);

		*s_8021x = NM_SETTING_802_1X (nm_setting_802_1x_new ());
		nm_setting_802_1x_add_eap_method (*s_8021x, "leap");
		g_object_set (*s_8021x, NM_SETTING_802_1X_IDENTITY, username, NULL);

		if (secret) {
			nm_gconf_add_keyring_item (uuid, id,
			                           NM_SETTING_802_1X_SETTING_NAME,
			                           NM_SETTING_802_1X_PASSWORD,
			                           secret);
		}
	} else {
		/* Traditional LEAP */
		g_object_set (s_wireless_sec,
		              NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "ieee8021x",
		              NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, "leap",
		              NM_SETTING_WIRELESS_SECURITY_LEAP_USERNAME, username,
		              NULL);

		if (secret) {
			nm_gconf_add_keyring_item (uuid, id,
			                           NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
			                           NM_SETTING_WIRELESS_SECURITY_LEAP_PASSWORD,
			                           secret);
		}
	}
	clear_06_keyring_secret (secret);

out:
	g_free (username);
	g_free (key_mgmt);
	return s_wireless_sec;
}

static NMConnection *
nm_gconf_read_0_6_wireless_connection (GConfClient *client,
							    const char *dir)
{
	NMConnection *connection;
	NMSettingConnection *s_con;
	NMSettingWireless *s_wireless;
	NMSettingWirelessSecurity *s_wireless_sec;
	NMSetting8021x *s_8021x = NULL;
	GByteArray *ssid;
	char *path, *network, *essid = NULL;
	char *uuid, *id;
	int timestamp, we_cipher;
	GSList *iter;
	GSList *bssids = NULL;
	char *private_key_path = NULL, *client_cert_path = NULL, *ca_cert_path = NULL;

	path = g_path_get_dirname (dir);
	network = g_path_get_basename (dir);

	if (!get_mandatory_string_helper (client, path, "essid", network, &essid)) {
		g_free (path);
		g_free (network);
		return NULL;
	}

	if (!nm_gconf_get_int_helper (client, path, "timestamp", network, &timestamp))
		timestamp = 0;
	if (!nm_gconf_get_stringlist_helper (client, path, "bssids", network, &bssids))
		bssids = NULL;
	if (!nm_gconf_get_int_helper (client, path, "we_cipher", network, &we_cipher))
		we_cipher = NM_AUTH_TYPE_NONE;

	s_con = (NMSettingConnection *)nm_setting_connection_new ();
	g_object_set (s_con,
				  NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
				  NM_SETTING_CONNECTION_AUTOCONNECT, (gboolean) (timestamp != 0),
				  NM_SETTING_CONNECTION_TIMESTAMP, timestamp >= 0 ? (guint64) timestamp : 0,
				  NULL);

	id = g_strdup_printf ("Auto %s", essid);
	g_object_set (s_con, NM_SETTING_CONNECTION_ID, id, NULL);

	uuid = nm_utils_uuid_generate ();
	g_object_set (s_con, NM_SETTING_CONNECTION_UUID, uuid, NULL);

	s_wireless = (NMSettingWireless *)nm_setting_wireless_new ();

	ssid = g_byte_array_new ();
	g_byte_array_append (ssid, (unsigned char *)essid, strlen (essid));
	g_free (essid);
	g_object_set (s_wireless,
				  NM_SETTING_WIRELESS_SSID, ssid,
				  NM_SETTING_WIRELESS_MODE, "infrastructure",
				  NULL);
	g_byte_array_free (ssid, TRUE);

	for (iter = bssids; iter; iter = iter->next)
		nm_setting_wireless_add_seen_bssid (s_wireless, (char *) iter->data);
	g_slist_free_full (bssids, g_free);

	if (we_cipher != NM_AUTH_TYPE_NONE) {
		switch (we_cipher) {
		case NM_AUTH_TYPE_WEP40:
		case NM_AUTH_TYPE_WEP104:
			s_wireless_sec = nm_gconf_read_0_6_wep_settings (client, path, network, uuid, id);
			break;
		case NM_AUTH_TYPE_WPA_PSK_AUTO:
		case NM_AUTH_TYPE_WPA_PSK_TKIP:
		case NM_AUTH_TYPE_WPA_PSK_CCMP:
			s_wireless_sec = nm_gconf_read_0_6_wpa_settings (client, path, network, uuid, id);
			break;
		case NM_AUTH_TYPE_WPA_EAP:
			s_wireless_sec = nm_gconf_read_0_6_eap_settings (client, path, network, uuid, id, &s_8021x);
			break;
		case NM_AUTH_TYPE_LEAP:
			s_wireless_sec = nm_gconf_read_0_6_leap_settings (client, path, network, uuid, id, &s_8021x);
			break;
		default:
			g_warning ("Unknown NM 0.6 auth type %d on connection %s", we_cipher, dir);
			s_wireless_sec = NULL;
			break;
		}

		if (!s_wireless_sec) {
			g_object_unref (s_con);
			g_object_unref (s_wireless);
			g_free (path);
			g_free (network);
			return NULL;
		}
	} else
		s_wireless_sec = NULL;

	connection = nm_connection_new ();
	nm_connection_add_setting (connection, (NMSetting *)s_con);
	nm_connection_add_setting (connection, (NMSetting *)s_wireless);
	if (s_wireless_sec)
		nm_connection_add_setting (connection, (NMSetting *)s_wireless_sec);
	if (s_8021x)
		nm_connection_add_setting (connection, (NMSetting *)s_8021x);

	/* Would be better in nm_gconf_read_0_6_eap_settings, except that
	 * the connection object doesn't exist at that point. Hrmph.
	 */
	if (nm_gconf_get_string_helper (client, path, "wpa_eap_private_key_file", network, &private_key_path))
		g_object_set_data_full (G_OBJECT (connection), NMA_PATH_PRIVATE_KEY_TAG, private_key_path, g_free);
	if (nm_gconf_get_string_helper (client, path, "wpa_eap_client_cert_file", network, &client_cert_path))
		g_object_set_data_full (G_OBJECT (connection), NMA_PATH_CLIENT_CERT_TAG, client_cert_path, g_free);
	if (nm_gconf_get_string_helper (client, path, "wpa_eap_ca_cert_file", network, &ca_cert_path))
		g_object_set_data_full (G_OBJECT (connection), NMA_PATH_CA_CERT_TAG, ca_cert_path, g_free);

	g_free (path);
	g_free (network);
	g_free (uuid);
	g_free (id);

	return connection;
}

static void
vpn_helpers_save_secret (const char *vpn_uuid,
                         const char *vpn_name,
                         const char *secret_name,
                         const char *secret,
                         const char *vpn_service_name)
{
	char *display_name;
	GnomeKeyringAttributeList *attrs = NULL;
	guint id;

	display_name = g_strdup_printf ("VPN %s secret for %s/%s/" NM_SETTING_VPN_SETTING_NAME,
	                                secret_name, vpn_name, vpn_service_name);

	attrs = gnome_keyring_attribute_list_new ();
	gnome_keyring_attribute_list_append_string (attrs,
	                                            KEYRING_UUID_TAG,
	                                            vpn_uuid);
	gnome_keyring_attribute_list_append_string (attrs,
	                                            KEYRING_SN_TAG,
	                                            NM_SETTING_VPN_SETTING_NAME);
	gnome_keyring_attribute_list_append_string (attrs,
	                                            KEYRING_SK_TAG,
	                                            secret_name);

	gnome_keyring_item_create_sync (NULL, GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                display_name, attrs, secret, TRUE, &id);
	gnome_keyring_attribute_list_free (attrs);
	g_free (display_name);
}


#define NM_VPNC_SERVICE "org.freedesktop.NetworkManager.vpnc"
#define VPNC_USER_PASSWORD "password"
#define VPNC_GROUP_PASSWORD "group-password"
#define VPNC_OLD_USER_PASSWORD "password"
#define VPNC_OLD_GROUP_PASSWORD "group_password"

static void
nm_gconf_0_6_vpnc_settings (NMSettingVPN *s_vpn,
                            GSList *vpn_data,
                            const char *uuid,
                            const char *id)
{
	GSList *iter;
	GList *found_list;
	GnomeKeyringResult result;

	for (iter = vpn_data; iter && iter->next; iter = iter->next->next) {
		const char *key = iter->data;
		const char *value = iter->next->data;

		if (*value) {
			/* A string value */
			nm_setting_vpn_add_data_item (s_vpn, key, value);
		} else {
			/* A boolean; 0.6 treated key-without-value as "true" */
			nm_setting_vpn_add_data_item (s_vpn, key, "yes");
		}
	}

	/* Try to convert secrets */
	result = gnome_keyring_find_network_password_sync (g_get_user_name (), /* user */
	                                                   NULL,               /* domain */
	                                                   id,                 /* server */
	                                                   NULL,               /* object */
	                                                   NM_VPNC_SERVICE,    /* protocol */
	                                                   NULL,               /* authtype */
	                                                   0,                  /* port */
	                                                   &found_list);
	if ((result == GNOME_KEYRING_RESULT_OK) && g_list_length (found_list)) {
		GnomeKeyringNetworkPasswordData *data1 = found_list->data;
		GnomeKeyringNetworkPasswordData *data2 = NULL;
		const char *password = NULL, *group_password = NULL;

		if (g_list_next (found_list))
			data2 = g_list_next (found_list)->data;

		if (!strcmp (data1->object, VPNC_OLD_GROUP_PASSWORD))
			group_password = data1->password;
		else if (!strcmp (data1->object, VPNC_OLD_USER_PASSWORD))
			password = data1->password;

		if (data2) {
			if (!strcmp (data2->object, VPNC_OLD_GROUP_PASSWORD))
				group_password = data2->password;
			else if (!strcmp (data2->object, VPNC_OLD_USER_PASSWORD))
				password = data2->password;
		}

		if (password)
			vpn_helpers_save_secret (uuid, id, VPNC_USER_PASSWORD, password, NM_VPNC_SERVICE);
		if (group_password)
			vpn_helpers_save_secret (uuid, id, VPNC_GROUP_PASSWORD, group_password, NM_VPNC_SERVICE);

		gnome_keyring_network_password_list_free (found_list);
	}
}

static void
nm_gconf_0_6_openvpn_settings (NMSettingVPN *s_vpn, GSList *vpn_data)
{
	GSList *iter;

	for (iter = vpn_data; iter && iter->next; iter = iter->next->next) {
		const char *key = iter->data;
		const char *value = iter->next->data;

		if (!strcmp (key, "connection-type")) {
			if (!strcmp (value, "x509"))
				nm_setting_vpn_add_data_item (s_vpn, key, "tls");
			else if (!strcmp (value, "shared-key"))
				nm_setting_vpn_add_data_item (s_vpn, key, "static-key");
			else if (!strcmp (value, "password"))
				nm_setting_vpn_add_data_item (s_vpn, key, "password");
		} else if (!strcmp (key, "comp-lzo")) {
			nm_setting_vpn_add_data_item (s_vpn, key, "yes");
		} else if (!strcmp (key, "dev")) {
			if (!strcmp (value, "tap"))
				nm_setting_vpn_add_data_item (s_vpn, "tap-dev", "yes");
		} else if (!strcmp (key, "proto")) {
			if (!strcmp (value, "tcp"))
				nm_setting_vpn_add_data_item (s_vpn, "proto-tcp", "yes");
		} else
			nm_setting_vpn_add_data_item (s_vpn, key, value);
	}
}

static void
add_routes (NMSettingIP4Config *s_ip4, GSList *str_routes)
{
	GSList *iter;

	for (iter = str_routes; iter; iter = g_slist_next (iter)) {
		struct in_addr tmp;
		char *p, *str_route;
		long int prefix = 32;

		str_route = g_strdup (iter->data);
		p = strchr (str_route, '/');
		if (!p || !(*(p + 1))) {
			g_warning ("Ignoring invalid route '%s'", str_route);
			goto next;
		}

		errno = 0;
		prefix = strtol (p + 1, NULL, 10);
		if (errno || prefix <= 0 || prefix > 32) {
			g_warning ("Ignoring invalid route '%s'", str_route);
			goto next;
		}

		/* don't pass the prefix to inet_pton() */
		*p = '\0';
		if (inet_pton (AF_INET, str_route, &tmp) > 0) {
			NMIP4Route *route;

			route = nm_ip4_route_new ();
			nm_ip4_route_set_dest (route, tmp.s_addr);
			nm_ip4_route_set_prefix (route, (guint32) prefix);

			nm_setting_ip4_config_add_route (s_ip4, route);
			nm_ip4_route_unref (route);
		} else
			g_warning ("Ignoring invalid route '%s'", str_route);

next:
		g_free (str_route);
	}
}

static NMConnection *
nm_gconf_read_0_6_vpn_connection (GConfClient *client,
						    const char *dir)
{
	NMConnection *connection;
	NMSettingConnection *s_con;
	NMSettingVPN *s_vpn;
	NMSettingIP4Config *s_ip4 = NULL;
	char *path, *network, *id = NULL, *uuid = NULL, *service_name = NULL;
	GSList *str_routes = NULL, *vpn_data = NULL;

	path = g_path_get_dirname (dir);
	network = g_path_get_basename (dir);

	if (!get_mandatory_string_helper (client, path, "name", network, &id)) {
		g_free (path);
		g_free (network);
		return NULL;
	}
	if (!get_mandatory_string_helper (client, path, "service_name", network, &service_name)) {
		g_free (id);
		g_free (path);
		g_free (network);
		return NULL;
	}

	if (!nm_gconf_get_stringlist_helper (client, path, "routes", network, &str_routes))
		str_routes = NULL;
	if (!nm_gconf_get_stringlist_helper (client, path, "vpn_data", network, &vpn_data))
		vpn_data = NULL;

	s_con = (NMSettingConnection *)nm_setting_connection_new ();
	g_object_set (s_con,
				  NM_SETTING_CONNECTION_ID, id,
				  NM_SETTING_CONNECTION_TYPE, NM_SETTING_VPN_SETTING_NAME,
				  NULL);

	uuid = nm_utils_uuid_generate ();
	g_object_set (s_con, NM_SETTING_CONNECTION_UUID, uuid, NULL);

	s_vpn = (NMSettingVPN *)nm_setting_vpn_new ();
	g_object_set (s_vpn, NM_SETTING_VPN_SERVICE_TYPE, service_name, NULL);

	if (!strcmp (service_name, NM_VPNC_SERVICE))
		nm_gconf_0_6_vpnc_settings (s_vpn, vpn_data, uuid, id);
	else if (!strcmp (service_name, "org.freedesktop.NetworkManager.openvpn"))
		nm_gconf_0_6_openvpn_settings (s_vpn, vpn_data);
	else
		g_warning ("unmatched service name %s\n", service_name);

	g_slist_free_full (vpn_data, g_free);
	g_free (path);
	g_free (network);
	g_free (service_name);

	if (str_routes) {
		s_ip4 = NM_SETTING_IP4_CONFIG (nm_setting_ip4_config_new ());
		g_object_set (s_ip4, NM_SETTING_IP4_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL);
		add_routes (s_ip4, str_routes);
	}

	connection = nm_connection_new ();
	nm_connection_add_setting (connection, NM_SETTING (s_con));
	nm_connection_add_setting (connection, NM_SETTING (s_vpn));
	if (s_ip4)
		nm_connection_add_setting (connection, NM_SETTING (s_ip4));

	g_free (id);
	g_free (uuid);

	return connection;
}

static void
nm_gconf_write_0_6_connection (NMConnection *connection, GConfClient *client, int n)
{
	char *dir;

	dir = g_strdup_printf ("%s/%d", GCONF_PATH_CONNECTIONS, n);
	nm_gconf_write_connection (connection, client, dir, FALSE);
	g_free (dir);
}

#define GCONF_PATH_0_6_WIRELESS_NETWORKS "/system/networking/wireless/networks"
#define GCONF_PATH_0_6_VPN_CONNECTIONS   "/system/networking/vpn_connections"

void
nm_gconf_migrate_0_6_connections (GConfClient *client)
{
	GSList *connections, *iter;
	NMConnection *conn;
	int n;

	n = 1;

	connections = gconf_client_all_dirs (client, GCONF_PATH_0_6_WIRELESS_NETWORKS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		conn = nm_gconf_read_0_6_wireless_connection (client, iter->data);
		if (conn) {
			nm_gconf_write_0_6_connection (conn, client, n++);
			g_object_unref (conn);
		}
	}
	g_slist_free_full (connections, g_free);

	connections = gconf_client_all_dirs (client, GCONF_PATH_0_6_VPN_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		conn = nm_gconf_read_0_6_vpn_connection (client, iter->data);
		if (conn) {
			nm_gconf_write_0_6_connection (conn, client, n++);
			g_object_unref (conn);
		}
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

static void
unset_one_setting_property (GConfClient *client,
                            const char *dir,
                            const char *setting,
                            const char *key)
{
	GConfValue *val;
	char *path;

	path = g_strdup_printf ("%s/%s/%s", dir, setting, key);
	val = gconf_client_get_without_default (client, path, NULL);
	if (val) {
		if (val->type != GCONF_VALUE_INVALID)
			gconf_client_unset (client, path, NULL);
		gconf_value_free (val);
	}
	g_free (path);
}

static void
copy_stringlist_to_8021x (GConfClient *client, const char *dir, const char *key)
{
	GSList *sa_val = NULL;

	if (!nm_gconf_get_stringlist_helper (client, dir, key, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, &sa_val))
		return;

	if (!nm_gconf_set_stringlist_helper (client, dir, key, NM_SETTING_802_1X_SETTING_NAME, sa_val))
		g_warning ("Could not convert string list value '%s' from wireless-security to 8021x setting", key);

	g_slist_foreach (sa_val, (GFunc) g_free, NULL);
	g_slist_free (sa_val);

	unset_one_setting_property (client, dir, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, key);
}

static void
copy_string_to_8021x (GConfClient *client, const char *dir, const char *key)
{
	char *val = NULL;

	if (!nm_gconf_get_string_helper (client, dir, key, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, &val))
		return;

	if (!nm_gconf_set_string_helper (client, dir, key, NM_SETTING_802_1X_SETTING_NAME, val))
		g_warning ("Could not convert string value '%s' from wireless-security to 8021x setting", key);

	g_free (val);

	unset_one_setting_property (client, dir, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, key);
}

static void
copy_bool_to_8021x (GConfClient *client, const char *dir, const char *key)
{
	gboolean val;

	if (!nm_gconf_get_bool_helper (client, dir, key, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, &val))
		return;

	if (val && !nm_gconf_set_bool_helper (client, dir, key, NM_SETTING_802_1X_SETTING_NAME, val))
		g_warning ("Could not convert string value '%s' from wireless-security to 8021x setting", key);

	unset_one_setting_property (client, dir, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, key);
}

static gboolean
try_convert_leap (GConfClient *client, const char *dir, const char *uuid)
{
	char *val = NULL;
	GnomeKeyringResult ret;
	GList *found_list = NULL;
	GnomeKeyringFound *found;

	if (nm_gconf_get_string_helper (client, dir,
	                                NM_SETTING_WIRELESS_SECURITY_LEAP_USERNAME,
	                                NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                                &val)) {
		/* Alredy converted */
		g_free (val);
		return TRUE;
	}

	if (!nm_gconf_get_string_helper (client, dir,
	                                 NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
	                                 NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                                 &val))
		return FALSE;

	if (strcmp (val, "ieee8021x")) {
		g_free (val);
		return FALSE;
	}
	g_free (val);
	val = NULL;

	if (!nm_gconf_get_string_helper (client, dir,
	                                 NM_SETTING_WIRELESS_SECURITY_AUTH_ALG,
	                                 NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                                 &val))
		return FALSE;

	if (strcmp (val, "leap")) {
		g_free (val);
		return FALSE;
	}
	g_free (val);
	val = NULL;

	/* Copy leap username */
	if (!nm_gconf_get_string_helper (client, dir,
	                                 "identity",
	                                 NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                                 &val))
		return FALSE;

	if (!nm_gconf_set_string_helper (client, dir,
	                                 NM_SETTING_WIRELESS_SECURITY_LEAP_USERNAME,
	                                 NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                                 val))
		g_warning ("Could not convert leap-username.");

	g_free (val);
	val = NULL;

	unset_one_setting_property (client, dir, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                            NM_SETTING_802_1X_IDENTITY);

	if (!nm_gconf_get_string_helper (client, dir,
	                                 "id",
	                                 NM_SETTING_CONNECTION_SETTING_NAME,
	                                 &val))
		goto done;

	/* Copy the LEAP password */
	ret = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                      &found_list,
	                                      KEYRING_UUID_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      uuid,
	                                      KEYRING_SN_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                                      KEYRING_SK_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      "password",
	                                      NULL);
	if ((ret != GNOME_KEYRING_RESULT_OK) || (g_list_length (found_list) == 0))
		goto done;

	found = (GnomeKeyringFound *) found_list->data;
	nm_gconf_add_keyring_item (uuid,
	                           val,
	                           NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                           NM_SETTING_WIRELESS_SECURITY_LEAP_PASSWORD,
	                           found->secret);
	gnome_keyring_item_delete_sync (found->keyring, found->item_id);

done:
	g_free (val);
	gnome_keyring_found_list_free (found_list);
	return TRUE;
}

static void
copy_keyring_to_8021x (GConfClient *client,
                       const char *dir,
                       const char *uuid,
                       const char *key)
{
	char *name = NULL;
	GnomeKeyringResult ret;
	GList *found_list = NULL;
	GnomeKeyringFound *found;

	if (!nm_gconf_get_string_helper (client, dir,
	                                 "id",
	                                 NM_SETTING_CONNECTION_SETTING_NAME,
	                                 &name))
		return;

	/* Copy the LEAP password */
	ret = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                      &found_list,
	                                      KEYRING_UUID_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      uuid,
	                                      KEYRING_SN_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
	                                      KEYRING_SK_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      key,
	                                      NULL);
	if ((ret != GNOME_KEYRING_RESULT_OK) || (g_list_length (found_list) == 0))
		goto done;

	found = (GnomeKeyringFound *) found_list->data;
	nm_gconf_add_keyring_item (uuid, name, NM_SETTING_802_1X_SETTING_NAME, key, found->secret);

	gnome_keyring_item_delete_sync (found->keyring, found->item_id);

done:
	g_free (name);
	gnome_keyring_found_list_free (found_list);
}

void
nm_gconf_migrate_0_7_wireless_security (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		char *key_mgmt = NULL;
		GSList *eap = NULL;
		char *uuid = NULL;

		if (!nm_gconf_get_string_helper (client, iter->data,
		                                 NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
		                                 NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
		                                 &key_mgmt))
			goto next;

		if (!nm_gconf_get_string_helper (client, iter->data,
		                                 NM_SETTING_CONNECTION_UUID,
		                                 NM_SETTING_CONNECTION_SETTING_NAME,
		                                 &uuid))
			goto next;

		/* Only convert 802.1x-based connections */
		if (strcmp (key_mgmt, "ieee8021x") && strcmp (key_mgmt, "wpa-eap")) {
			g_free (key_mgmt);
			goto next;
		}
		g_free (key_mgmt);

		/* Leap gets converted differently */
		if (try_convert_leap (client, iter->data, uuid))
			goto next;

		/* Otherwise straight 802.1x */
		if (nm_gconf_get_stringlist_helper (client, iter->data,
		                                NM_SETTING_802_1X_EAP,
		                                NM_SETTING_802_1X_SETTING_NAME,
		                                &eap)) {
			/* Already converted */
			g_slist_foreach (eap, (GFunc) g_free, NULL);
			g_slist_free (eap);
			goto next;
		}

		copy_stringlist_to_8021x (client, iter->data, NM_SETTING_802_1X_EAP);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_IDENTITY);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_ANONYMOUS_IDENTITY);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_CA_PATH);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_PHASE1_PEAPVER);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_PHASE1_PEAPLABEL);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_PHASE1_FAST_PROVISIONING);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_PHASE2_AUTH);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_PHASE2_AUTHEAP);
		copy_string_to_8021x (client, iter->data, NM_SETTING_802_1X_PHASE2_CA_PATH);
		copy_string_to_8021x (client, iter->data, NMA_PATH_CA_CERT_TAG);
		copy_string_to_8021x (client, iter->data, NMA_PATH_CLIENT_CERT_TAG);
		copy_string_to_8021x (client, iter->data, NMA_PATH_PRIVATE_KEY_TAG);
		copy_string_to_8021x (client, iter->data, NMA_PATH_PHASE2_CA_CERT_TAG);
		copy_string_to_8021x (client, iter->data, NMA_PATH_PHASE2_CLIENT_CERT_TAG);
		copy_string_to_8021x (client, iter->data, NMA_PATH_PHASE2_PRIVATE_KEY_TAG);

		copy_bool_to_8021x (client, iter->data, NMA_CA_CERT_IGNORE_TAG);
		copy_bool_to_8021x (client, iter->data, NMA_PHASE2_CA_CERT_IGNORE_TAG);

		copy_keyring_to_8021x (client, iter->data, uuid, NM_SETTING_802_1X_PASSWORD);
		copy_keyring_to_8021x (client, iter->data, uuid, NM_SETTING_802_1X_PIN);
		copy_keyring_to_8021x (client, iter->data, uuid, NMA_PRIVATE_KEY_PASSWORD_TAG);
		copy_keyring_to_8021x (client, iter->data, uuid, NMA_PHASE2_PRIVATE_KEY_PASSWORD_TAG);

next:
		g_free (uuid);
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

void
nm_gconf_migrate_0_7_netmask_to_prefix (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		char *id = g_path_get_basename ((const char *) iter->data);
		GArray *array, *new;
		int i;
		gboolean need_update = FALSE;

		if (!nm_gconf_get_uint_array_helper (client, iter->data,
		                                     NM_SETTING_IP4_CONFIG_ADDRESSES,
		                                     NM_SETTING_IP4_CONFIG_SETTING_NAME,
		                                     &array))
			goto next;

		new = g_array_sized_new (FALSE, TRUE, sizeof (guint32), array->len);
		for (i = 0; i < array->len; i+=3) {
			guint32 addr, netmask, prefix, gateway;

			addr = g_array_index (array, guint32, i);
			g_array_append_val (new, addr);

			/* get the second element of the 3-number IP address tuple */
			netmask = g_array_index (array, guint32, i + 1);
			if (netmask > 32) {
				/* convert it */
				prefix = nm_utils_ip4_netmask_to_prefix (netmask);
				g_array_append_val (new, prefix);
				need_update = TRUE;
			} else {
				/* Probably already a prefix */
				g_array_append_val (new, netmask);
			}

			gateway = g_array_index (array, guint32, i + 2);
			g_array_append_val (new, gateway);
		}

		/* Update GConf */
		if (need_update) {
			nm_gconf_set_uint_array_helper (client, iter->data,
			                                NM_SETTING_IP4_CONFIG_ADDRESSES,
			                                NM_SETTING_IP4_CONFIG_SETTING_NAME,
			                                new);
		}
		g_array_free (array, TRUE);
		g_array_free (new, TRUE);

next:
		g_free (id);
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

void
nm_gconf_migrate_0_7_ip4_method (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		char *id = g_path_get_basename ((const char *) iter->data);
		char *method = NULL;

		if (!nm_gconf_get_string_helper (client, iter->data,
		                                 NM_SETTING_IP4_CONFIG_METHOD,
		                                 NM_SETTING_IP4_CONFIG_SETTING_NAME,
		                                 &method))
			goto next;

		if (!strcmp (method, "autoip")) {
			nm_gconf_set_string_helper (client, iter->data,
			                            NM_SETTING_IP4_CONFIG_METHOD,
			                            NM_SETTING_IP4_CONFIG_SETTING_NAME,
			                            NM_SETTING_IP4_CONFIG_METHOD_LINK_LOCAL);
		} else if (!strcmp (method, "dhcp")) {
			nm_gconf_set_string_helper (client, iter->data,
			                            NM_SETTING_IP4_CONFIG_METHOD,
			                            NM_SETTING_IP4_CONFIG_SETTING_NAME,
			                            NM_SETTING_IP4_CONFIG_METHOD_AUTO);
		}

		g_free (method);

next:
		g_free (id);
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

#define IP4_KEY_IGNORE_DHCP_DNS "ignore-dhcp-dns"

void
nm_gconf_migrate_0_7_ignore_dhcp_dns (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		gboolean ignore_auto_dns = FALSE;

		if (!nm_gconf_get_bool_helper (client, iter->data,
		                               IP4_KEY_IGNORE_DHCP_DNS,
		                               NM_SETTING_IP4_CONFIG_SETTING_NAME,
		                               &ignore_auto_dns))
			continue;

		/* add new key with new name */
		if (ignore_auto_dns) {
			nm_gconf_set_bool_helper (client, iter->data,
			                          NM_SETTING_IP4_CONFIG_IGNORE_AUTO_DNS,
			                          NM_SETTING_IP4_CONFIG_SETTING_NAME,
			                          ignore_auto_dns);
		}

		/* delete old key */
		unset_one_setting_property (client,
		                            (const char *) iter->data,
		                            NM_SETTING_IP4_CONFIG_SETTING_NAME,
		                            IP4_KEY_IGNORE_DHCP_DNS);
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

static NMIP4Route *
convert_route (const char *in_route)
{
	NMIP4Route *route = NULL;
	struct in_addr tmp;
	char *p, *str_route;
	long int prefix = 32;

	str_route = g_strdup (in_route);
	p = strchr (str_route, '/');
	if (!p || !(*(p + 1))) {
		g_warning ("Ignoring invalid route '%s'", str_route);
		goto out;
	}

	errno = 0;
	prefix = strtol (p + 1, NULL, 10);
	if (errno || prefix <= 0 || prefix > 32) {
		g_warning ("Ignoring invalid route '%s'", str_route);
		goto out;
	}

	/* don't pass the prefix to inet_pton() */
	*p = '\0';
	if (inet_pton (AF_INET, str_route, &tmp) <= 0) {
		g_warning ("Ignoring invalid route '%s'", str_route);
		goto out;
	}

	route = nm_ip4_route_new ();
	nm_ip4_route_set_dest (route, tmp.s_addr);
	nm_ip4_route_set_prefix (route, (guint32) prefix);

out:
	g_free (str_route);
	return route;
}

#define VPN_KEY_ROUTES "routes"

static void
free_one_route (gpointer data, gpointer user_data)
{
	g_array_free ((GArray *) data, TRUE);
}

void
nm_gconf_migrate_0_7_vpn_routes (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		GSList *old_routes = NULL, *routes_iter;
		GPtrArray *new_routes = NULL;

		if (!nm_gconf_get_stringlist_helper (client, iter->data,
		                                     VPN_KEY_ROUTES,
		                                     NM_SETTING_VPN_SETTING_NAME,
		                                     &old_routes))
			continue;

		/* Convert 'x.x.x.x/x' into a route structure */
		for (routes_iter = old_routes; routes_iter; routes_iter = g_slist_next (routes_iter)) {
			NMIP4Route *route;

			route = convert_route (routes_iter->data);
			if (route) {
				GArray *tmp_route;
				guint32 tmp;

				if (!new_routes)
					new_routes = g_ptr_array_sized_new (3);

				tmp_route = g_array_sized_new (FALSE, TRUE, sizeof (guint32), 4);
				tmp = nm_ip4_route_get_dest (route);
				g_array_append_val (tmp_route, tmp);
				tmp = nm_ip4_route_get_prefix (route);
				g_array_append_val (tmp_route, tmp);
				tmp = nm_ip4_route_get_next_hop (route);
				g_array_append_val (tmp_route, tmp);
				tmp = nm_ip4_route_get_metric (route);
				g_array_append_val (tmp_route, tmp);
				g_ptr_array_add (new_routes, tmp_route);
				nm_ip4_route_unref (route);
			}
		}

		if (new_routes) {
			char *method = NULL;

			/* Set new routes */
			nm_gconf_set_ip4_helper (client, iter->data,
			                         NM_SETTING_IP4_CONFIG_ROUTES,
			                         NM_SETTING_IP4_CONFIG_SETTING_NAME,
			                         4,
			                         new_routes);

			g_ptr_array_foreach (new_routes, (GFunc) free_one_route, NULL);
			g_ptr_array_free (new_routes, TRUE);

			/* To make a valid ip4 setting, need a method too */
			if (!nm_gconf_get_string_helper (client, iter->data,
			                                 NM_SETTING_IP4_CONFIG_METHOD,
			                                 NM_SETTING_IP4_CONFIG_SETTING_NAME,
			                                 &method)) {				
				/* If no method was specified, use 'auto' */
				nm_gconf_set_string_helper (client, iter->data,
				                            NM_SETTING_IP4_CONFIG_METHOD,
				                            NM_SETTING_IP4_CONFIG_SETTING_NAME,
				                            NM_SETTING_IP4_CONFIG_METHOD_AUTO);
			}
			g_free (method);
		}

		/* delete old key */
		unset_one_setting_property (client,
		                            (const char *) iter->data,
		                            NM_SETTING_VPN_SETTING_NAME,
		                            VPN_KEY_ROUTES);

		g_slist_foreach (old_routes, (GFunc) g_free, NULL);
		g_slist_free (old_routes);
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

void
nm_gconf_migrate_0_7_vpn_properties (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		char *path;
		GSList *properties, *props_iter;

		path = g_strdup_printf ("%s/vpn-properties", (const char *) iter->data);
		properties = gconf_client_all_entries (client, path, NULL);

		for (props_iter = properties; props_iter; props_iter = props_iter->next) {
			GConfEntry *entry = (GConfEntry *) props_iter->data;
			char *tmp;
			char *key_name = g_path_get_basename (entry->key);

			/* 'service-type' is reserved */
			if (!strcmp (key_name, NM_SETTING_VPN_SERVICE_TYPE))
				goto next;

			/* Don't convert the setting name */
			if (!strcmp (key_name, NM_SETTING_NAME))
				goto next;

			switch (entry->value->type) {
			case GCONF_VALUE_STRING:
				tmp = (char *) gconf_value_get_string (entry->value);
				if (tmp && strlen (tmp)) {
					nm_gconf_set_string_helper (client, (const char *) iter->data,
					                            key_name,
					                            NM_SETTING_VPN_SETTING_NAME,
					                            gconf_value_get_string (entry->value));
				}
				break;
			case GCONF_VALUE_INT:
				tmp = g_strdup_printf ("%d", gconf_value_get_int (entry->value));
				nm_gconf_set_string_helper (client, (const char *) iter->data,
				                            key_name,
				                            NM_SETTING_VPN_SETTING_NAME,
				                            tmp);
				g_free (tmp);
				break;
			case GCONF_VALUE_BOOL:
				tmp = gconf_value_get_bool (entry->value) ? "yes" : "no";
				nm_gconf_set_string_helper (client, (const char *) iter->data,
				                            key_name,
				                            NM_SETTING_VPN_SETTING_NAME,
				                            tmp);
				break;
			default:
				g_warning ("%s: don't know how to convert type %d",
				           __func__, entry->value->type);
				break;
			}

		next:
			g_free (key_name);
			gconf_entry_unref (entry);
		}

		if (properties) {
			g_slist_free (properties);

			/* delete old vpn-properties dir */
			gconf_client_recursive_unset (client, path, 0, NULL);
		}

		g_free (path);
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

static void
move_one_vpn_string_bool (GConfClient *client,
                          const char *path,
                          const char *old_key,
                          const char *new_key)
{
	char *value = NULL;

	if (!nm_gconf_get_string_helper (client, path,
	                                 old_key,
	                                 NM_SETTING_VPN_SETTING_NAME,
	                                 &value))
		return;

	if (value && !strcmp (value, "yes")) {
		nm_gconf_set_string_helper (client, path,
		                            new_key,
		                            NM_SETTING_VPN_SETTING_NAME,
		                            "yes");
	}
	g_free (value);

	/* delete old key */
	unset_one_setting_property (client, path, NM_SETTING_VPN_SETTING_NAME, old_key);
}

static void
move_one_vpn_string_string (GConfClient *client,
                            const char *path,
                            const char *old_key,
                            const char *new_key)
{
	char *value = NULL;

	if (!nm_gconf_get_string_helper (client, path,
	                                 old_key,
	                                 NM_SETTING_VPN_SETTING_NAME,
	                                 &value))
		return;

	if (value && strlen (value)) {
		nm_gconf_set_string_helper (client, path,
		                            new_key,
		                            NM_SETTING_VPN_SETTING_NAME,
		                            value);
	}
	g_free (value);

	/* delete old key */
	unset_one_setting_property (client, path, NM_SETTING_VPN_SETTING_NAME, old_key);
}

void
nm_gconf_migrate_0_7_openvpn_properties (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		char *old_type = NULL, *new_type = NULL, *service = NULL;

		if (!nm_gconf_get_string_helper (client, (const char *) iter->data,
		                                 NM_SETTING_VPN_SERVICE_TYPE,
		                                 NM_SETTING_VPN_SETTING_NAME,
		                                 &service))
			continue;

		if (!service || strcmp (service, "org.freedesktop.NetworkManager.openvpn")) {
			g_free (service);
			continue;
		}
		g_free (service);

		move_one_vpn_string_bool (client, iter->data, "dev", "tap-dev");
		move_one_vpn_string_bool (client, iter->data, "proto", "proto-tcp");
		move_one_vpn_string_string (client, iter->data, "shared-key", "static-key");
		move_one_vpn_string_string (client, iter->data, "shared-key-direction", "static-key-direction");

		if (!nm_gconf_get_string_helper (client, (const char *) iter->data,
		                                 "connection-type",
		                                 NM_SETTING_VPN_SETTING_NAME,
		                                 &old_type))
			continue;

		/* Convert connection type from old integer to new string */
		if (!strcmp (old_type, "0"))
			new_type = "tls";
		else if (!strcmp (old_type, "1"))
			new_type = "static-key";
		else if (!strcmp (old_type, "2"))
			new_type = "password";
		else if (!strcmp (old_type, "3"))
			new_type = "password-tls";
		g_free (old_type);

		if (new_type) {
			nm_gconf_set_string_helper (client, (const char *) iter->data,
			                            "connection-type",
			                            NM_SETTING_VPN_SETTING_NAME,
			                            new_type);
		}
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

void
nm_gconf_migrate_0_7_connection_uuid (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		char *uuid = NULL;

		if (!nm_gconf_get_string_helper (client, iter->data,
		                                NM_SETTING_CONNECTION_UUID,
		                                NM_SETTING_CONNECTION_SETTING_NAME,
		                                &uuid)) {
			/* Give the connection a UUID */
			uuid = nm_utils_uuid_generate ();
			nm_gconf_set_string_helper (client, iter->data,
			                            NM_SETTING_CONNECTION_UUID,
			                            NM_SETTING_CONNECTION_SETTING_NAME,
			                            uuid);
		}

		g_free (uuid);
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

static void
migrate_openvpn_secrets (const char *name, const char *uuid)
{
	int status;
	GList *list = NULL;
	GList *iter;

	status = gnome_keyring_find_network_password_sync (g_get_user_name (),     /* user */
	                                                   NULL,                   /* domain */
	                                                   name,         /* server */
	                                                   NULL,                   /* object */
	                                                   "org.freedesktop.NetworkManager.openvpn", /* protocol */
	                                                   NULL,                   /* authtype */
	                                                   0,                      /* port */
	                                                   &list);
	if (status != GNOME_KEYRING_RESULT_OK || !g_list_length (list))
		return;

	/* Go through all passwords and assign to appropriate variable */
	for (iter = list; iter; iter = iter->next) {
		GnomeKeyringNetworkPasswordData *found = iter->data;

		/* Ignore session items */
		if (strcmp (found->keyring, "session") != 0)
			nm_gconf_add_keyring_item (uuid, name, NM_SETTING_VPN_SETTING_NAME, found->object, found->password);

		gnome_keyring_item_delete_sync (found->keyring, found->item_id);
	}

	gnome_keyring_network_password_list_free (list);
}

/* Move keyring items from 'connection-id' or 'connection-name' to 'connection-uuid' */
void
nm_gconf_migrate_0_7_keyring_items (GConfClient *client)
{
	GSList *connections, *iter;

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		GnomeKeyringResult ret;
		GList *found_list = NULL, *found_iter;
		char *uuid = NULL;
		char *old_id = NULL;
		char *name = NULL;

		/* Get the connection's UUID and name */
		if (!nm_gconf_get_string_helper (client, iter->data,
		                                 NM_SETTING_CONNECTION_UUID,
		                                 NM_SETTING_CONNECTION_SETTING_NAME,
		                                 &uuid))
			goto next;

		if (!nm_gconf_get_string_helper (client, iter->data,
		                                 NM_SETTING_CONNECTION_ID,
		                                 NM_SETTING_CONNECTION_SETTING_NAME,
		                                 &name))
			goto next;

		old_id = g_path_get_basename ((const char *) iter->data);

		/* Move any keyring keys associated with the connection */
		ret = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
		                                      &found_list,
		                                      "connection-id",
		                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
		                                      old_id,
		                                      NULL);
		if (ret != GNOME_KEYRING_RESULT_OK) {
			/* Or even older keyring items */
			ret = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
			                                      &found_list,
			                                      "connection-name",
			                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
			                                      name,
			                                      NULL);
			if (ret != GNOME_KEYRING_RESULT_OK)
				goto next;
		}

		for (found_iter = found_list; found_iter; found_iter = g_list_next (found_iter)) {
			GnomeKeyringFound *found = (GnomeKeyringFound *) found_iter->data;
			char *setting_name = NULL;
			char *setting_key = NULL;
			int i;

			for (i = 0; found->attributes && (i < found->attributes->len); i++) {
				GnomeKeyringAttribute *attr;

				attr = &(gnome_keyring_attribute_list_index (found->attributes, i));
				if (!strcmp (attr->name, KEYRING_SN_TAG) && (attr->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)) {
					/* Migrate old vpn-properties secrets too */
					if (!strcmp (attr->value.string, "vpn-properties"))
						setting_name = NM_SETTING_VPN_SETTING_NAME;
					else
						setting_name = attr->value.string;
				} else if (!strcmp (attr->name, KEYRING_SK_TAG) && (attr->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING))
					setting_key = attr->value.string;
			}

			if (setting_name && setting_key) {
				nm_gconf_add_keyring_item (uuid, name, setting_name, setting_key, found->secret);
				ret = gnome_keyring_item_delete_sync (found->keyring, found->item_id);
			}
		}
		gnome_keyring_found_list_free (found_list);

		/* Old OpenVPN secrets have a different keyring style */
		migrate_openvpn_secrets (name, uuid);

	next:
		g_free (name);
		g_free (old_id);
		g_free (uuid);
	}
	g_slist_free_full (connections, g_free);

	gconf_client_suggest_sync (client, NULL);
}

void
nm_gconf_migrate_0_7_vpn_never_default (GConfClient *client)
{
	GSList *connections, *iter;

	/* Between 0.7.0 and 0.7.1, the 'never-default' key was added to
	 * make which connections receive the default route less complicated
	 * and more reliable.  Previous to 0.7.1, a VPN connection whose
	 * server returned static routes, or for which the user had entered
	 * manual static routes, was never chosen as the default connection.
	 * With 0.7.1, all connections are candidates for the default connection
	 * unless 'never-default' is TRUE.  For 0.7.0 VPN connections, try to
	 * set 'never-default' when possible.  This doesn't cover all cases
	 * since we certainly don't know if the VPN server is returning
	 * any routes here, but it will work for some.
	 */

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		char *service = NULL;
		GArray *array = NULL;

		if (!nm_gconf_get_string_helper (client, (const char *) iter->data,
		                                 NM_SETTING_VPN_SERVICE_TYPE,
		                                 NM_SETTING_VPN_SETTING_NAME,
		                                 &service))
			continue;

		g_free (service);

		/* If the user entered manual static routes, NetworkManager 0.7.0
		 * would have never set this VPN connection as the default, so
		 * set 'never-default' to TRUE.
		 */

		if (!nm_gconf_get_uint_array_helper (client, iter->data,
		                                     NM_SETTING_IP4_CONFIG_ROUTES,
		                                     NM_SETTING_IP4_CONFIG_SETTING_NAME,
		                                     &array))
			continue;

		if (!array->len) {
			g_array_free (array, TRUE);
			continue;
		}

		/* Static routes found; set 'never-default' */
		nm_gconf_set_bool_helper (client, iter->data,
		                          NM_SETTING_IP4_CONFIG_NEVER_DEFAULT,
		                          NM_SETTING_IP4_CONFIG_SETTING_NAME,
		                          TRUE);
		g_array_free (array, TRUE);
	}
	g_slist_free_full (connections, g_free);
	gconf_client_suggest_sync (client, NULL);
}

void
nm_gconf_migrate_0_7_autoconnect_default (GConfClient *client)
{
	GSList *connections, *iter;

	/* Between 0.7.0 and 0.7.1, autoconnect was switched to TRUE by default.
	 * Since default values aren't saved in GConf to reduce clutter, when NM
	 * gets the connection from the applet, libnm-util will helpfully fill in
	 * autoconnect=TRUE, causing existing connections that used to be
	 * autoconnect=FALSE to be automatically activated.
	 */

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		gboolean autoconnect = FALSE;

		if (!nm_gconf_get_bool_helper (client, (const char *) iter->data,
		                               NM_SETTING_CONNECTION_AUTOCONNECT,
		                               NM_SETTING_CONNECTION_SETTING_NAME,
		                               &autoconnect)) {
			/* If the key wasn't present, that used to mean FALSE, but now
			 * we need to make that explicit.
			 */
			nm_gconf_set_bool_helper (client, iter->data,
			                          NM_SETTING_CONNECTION_AUTOCONNECT,
			                          NM_SETTING_CONNECTION_SETTING_NAME,
			                          FALSE);
		}
	}
	g_slist_free_full (connections, g_free);
	gconf_client_suggest_sync (client, NULL);
}

static void
_set_ignore_ca_cert (GConfClient *client, const char *uuid, gboolean phase2)
{
	char *key = NULL;

	g_return_if_fail (uuid != NULL);

	key = g_strdup_printf (APPLET_PREFS_PATH "/%s/%s",
	                       phase2 ? "ignore-phase2-ca-cert" : "ignore-ca-cert",
	                       uuid);
	gconf_client_set_bool (client, key, TRUE, NULL);
	g_free (key);
}

void
nm_gconf_migrate_0_7_ca_cert_ignore (GConfClient *client)
{
	GSList *connections, *iter;

	/* With 0.8, the applet stores the key that suppresses the nag dialog
	 * when the user elects to ignore CA certificates in a different place than
	 * the connection itself.  Move the old location to the new location.
	 */

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		const char *dir = iter->data;
		char *uuid = NULL;
		gboolean ignore_ca_cert = FALSE;
		gboolean ignore_phase2_ca_cert = FALSE;

		if (!nm_gconf_get_string_helper (client, dir,
		                                 NM_SETTING_CONNECTION_UUID,
		                                 NM_SETTING_CONNECTION_SETTING_NAME,
		                                 &uuid))
			continue;

		nm_gconf_get_bool_helper (client, dir,
		                          NMA_CA_CERT_IGNORE_TAG,
		                          NM_SETTING_802_1X_SETTING_NAME,
		                          &ignore_ca_cert);
		if (ignore_ca_cert)
			_set_ignore_ca_cert (client, uuid, FALSE);
		g_free (uuid);

		/* delete old key */
		unset_one_setting_property (client, dir,
		                            NM_SETTING_802_1X_SETTING_NAME,
		                            NMA_CA_CERT_IGNORE_TAG);

		nm_gconf_get_bool_helper (client, dir,
		                          NMA_PHASE2_CA_CERT_IGNORE_TAG,
		                          NM_SETTING_802_1X_SETTING_NAME,
		                          &ignore_phase2_ca_cert);
		if (ignore_phase2_ca_cert)
			_set_ignore_ca_cert (client, uuid, TRUE);
		unset_one_setting_property (client, dir,
		                            NM_SETTING_802_1X_SETTING_NAME,
		                            NMA_PHASE2_CA_CERT_IGNORE_TAG);
	}

	g_slist_free_full (connections, g_free);
	gconf_client_suggest_sync (client, NULL);
}

static void
copy_one_cert_value (GConfClient *client,
                     const char *dir,
                     const char *tag,
                     const char *key)
{
	char *path = NULL;

	/* Do nothing if already migrated */
	if (nm_gconf_key_is_set (client, dir, key, NM_SETTING_802_1X_SETTING_NAME))
		return;

	if (nm_gconf_get_string_helper (client, dir,
	                                tag,
	                                NM_SETTING_802_1X_SETTING_NAME,
	                                &path)) {
		nm_gconf_set_string_helper (client, dir, key, NM_SETTING_802_1X_SETTING_NAME, path);
		g_free (path);
	}
}

static void
copy_one_private_key_password (const char *uuid,
                               const char *id,
                               const char *old_key,
                               const char *new_key)
{
	GnomeKeyringResult ret;
	GList *found_list = NULL;

	/* Find the secret */
	ret = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                      &found_list,
	                                      KEYRING_UUID_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
										  uuid,
	                                      KEYRING_SN_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      NM_SETTING_802_1X_SETTING_NAME,
	                                      KEYRING_SK_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      old_key,
	                                      NULL);
	if ((ret == GNOME_KEYRING_RESULT_OK) && g_list_length (found_list)) {
		GnomeKeyringFound *found = found_list->data;

		nm_gconf_add_keyring_item (uuid,
		                           id,
		                           NM_SETTING_802_1X_SETTING_NAME,
		                           new_key,
		                           found->secret);
		gnome_keyring_item_delete_sync (found->keyring, found->item_id);
		gnome_keyring_found_list_free (found_list);
	}
}

void
nm_gconf_migrate_0_7_certs (GConfClient *client)
{
	GSList *connections, *iter;

	/* With 0.8, the certificate/key path is stored in the value itself, not
	 * in the lookaside "nma" value.
	 */

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	for (iter = connections; iter; iter = iter->next) {
		const char *dir = iter->data;
		char *uuid = NULL, *id = NULL;

		if (!nm_gconf_get_string_helper (client, dir,
		                                 NM_SETTING_CONNECTION_UUID,
		                                 NM_SETTING_CONNECTION_SETTING_NAME,
		                                 &uuid))
			continue;

		if (!nm_gconf_get_string_helper (client, dir,
		                                 NM_SETTING_CONNECTION_ID,
		                                 NM_SETTING_CONNECTION_SETTING_NAME,
		                                 &id)) {
			g_free (uuid);
			continue;
		}

		copy_one_cert_value (client, dir, NMA_PATH_CA_CERT_TAG, NM_SETTING_802_1X_CA_CERT);
		copy_one_cert_value (client, dir, NMA_PATH_PHASE2_CA_CERT_TAG, NM_SETTING_802_1X_PHASE2_CA_CERT);
		copy_one_cert_value (client, dir, NMA_PATH_CLIENT_CERT_TAG, NM_SETTING_802_1X_CLIENT_CERT);
		copy_one_cert_value (client, dir, NMA_PATH_PHASE2_CLIENT_CERT_TAG, NM_SETTING_802_1X_PHASE2_CLIENT_CERT);
		copy_one_cert_value (client, dir, NMA_PATH_PRIVATE_KEY_TAG, NM_SETTING_802_1X_PRIVATE_KEY);
		copy_one_cert_value (client, dir, NMA_PATH_PHASE2_PRIVATE_KEY_TAG, NM_SETTING_802_1X_PHASE2_PRIVATE_KEY);

		copy_one_private_key_password (uuid, id, NMA_PRIVATE_KEY_PASSWORD_TAG, NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD);
		copy_one_private_key_password (uuid, id, NMA_PHASE2_PRIVATE_KEY_PASSWORD_TAG, NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD);

		g_free (uuid);
		g_free (id);
	}

	g_slist_free_full (connections, g_free);
	gconf_client_suggest_sync (client, NULL);
}

#define NM_VPNC_PW_TYPE_SAVE   "save"
#define NM_VPNC_PW_TYPE_ASK    "ask"
#define NM_VPNC_PW_TYPE_UNUSED "unused"

static NMSettingSecretFlags
vpnc_type_to_flag (NMSettingVPN *s_vpn, const char *type_key)
{
	const char *tmp;

	tmp = nm_setting_vpn_get_data_item (s_vpn, type_key);
	if (g_strcmp0 (tmp, NM_VPNC_PW_TYPE_SAVE) == 0)
		return NM_SETTING_SECRET_FLAG_NONE;
	if (g_strcmp0 (tmp, NM_VPNC_PW_TYPE_ASK) == 0)
		return NM_SETTING_SECRET_FLAG_NOT_SAVED;
	if (g_strcmp0 (tmp, NM_VPNC_PW_TYPE_UNUSED) == 0)
		return NM_SETTING_SECRET_FLAG_NOT_REQUIRED;
	return NM_SETTING_SECRET_FLAG_NONE;
}

#define NM_DBUS_SERVICE_VPNC "org.freedesktop.NetworkManager.vpnc"
#define NM_VPNC_KEY_SECRET "IPSec secret"
#define NM_VPNC_KEY_SECRET_TYPE "ipsec-secret-type"
#define NM_VPNC_KEY_XAUTH_PASSWORD "Xauth password"
#define NM_VPNC_KEY_XAUTH_PASSWORD_TYPE "xauth-password-type"

static void
migrate_vpnc (NMConnection *connection, NMSettingVPN *s_vpn)
{
	NMSettingSecretFlags flags;

	flags = NM_SETTING_SECRET_FLAG_AGENT_OWNED;
	flags |= vpnc_type_to_flag (s_vpn, NM_VPNC_KEY_SECRET_TYPE);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_VPNC_KEY_SECRET, flags, NULL);

	flags = NM_SETTING_SECRET_FLAG_AGENT_OWNED;
	flags |= vpnc_type_to_flag (s_vpn, NM_VPNC_KEY_XAUTH_PASSWORD_TYPE);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_VPNC_KEY_XAUTH_PASSWORD, flags, NULL);
}

#define NM_DBUS_SERVICE_OPENVPN "org.freedesktop.NetworkManager.openvpn"
#define NM_OPENVPN_KEY_PASSWORD "password"
#define NM_OPENVPN_KEY_CERTPASS "cert-pass"
#define NM_OPENVPN_KEY_HTTP_PROXY_PASSWORD "http-proxy-password"
#define NM_OPENVPN_KEY_CONNECTION_TYPE "connection-type"
#define NM_OPENVPN_CONTYPE_TLS          "tls"
#define NM_OPENVPN_CONTYPE_PASSWORD     "password"
#define NM_OPENVPN_CONTYPE_PASSWORD_TLS "password-tls"
#define NM_OPENVPN_KEY_PROXY_TYPE "proxy-type"

static NMSettingSecretFlags
openvpn_get_secret_flags (const char *uuid, const char *secret_name)
{
	GList *found = NULL;
	GnomeKeyringResult ret;
	NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_AGENT_OWNED;

	ret = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
		                                  &found,
		                                  KEYRING_UUID_TAG,
		                                  GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
										  uuid,
		                                  KEYRING_SN_TAG,
		                                  GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
		                                  NM_SETTING_VPN_SETTING_NAME,
		                                  KEYRING_SK_TAG,
		                                  GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
		                                  secret_name,
		                                  NULL);
	if (ret != GNOME_KEYRING_RESULT_OK || found == NULL)
		flags |= NM_SETTING_SECRET_FLAG_NOT_SAVED;
	gnome_keyring_found_list_free (found);

	return flags;
}

static void
migrate_openvpn (NMConnection *connection, NMSettingVPN *s_vpn)
{
	NMSettingSecretFlags flags;
	const char *tmp;
	gboolean check_pw = FALSE, check_cp = FALSE;

	tmp = nm_setting_vpn_get_data_item (s_vpn, NM_OPENVPN_KEY_CONNECTION_TYPE);
	if (!tmp)
		return;

	if (!strcmp (tmp, NM_OPENVPN_CONTYPE_TLS))
		check_cp = TRUE;
	else if (!strcmp (tmp, NM_OPENVPN_CONTYPE_PASSWORD_TLS)) {
		check_pw = TRUE;
		check_cp = TRUE;
	} else if (!strcmp (tmp, NM_OPENVPN_CONTYPE_PASSWORD))
		check_pw = TRUE;

	/* For each secret, we need to check the keyring to see whether the secret
	 * is present or not, and if it's *not*, then we mark the secret as both
	 * not-saved and agent-owned.  If it is present, the secret is just marked
	 * agent-owned.
	 */

	if (check_pw) {
		flags = openvpn_get_secret_flags (nm_connection_get_uuid (connection), NM_OPENVPN_KEY_PASSWORD);
		nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENVPN_KEY_PASSWORD, flags, NULL);
	}

	if (check_cp) {
		flags = openvpn_get_secret_flags (nm_connection_get_uuid (connection), NM_OPENVPN_KEY_CERTPASS);
		nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENVPN_KEY_CERTPASS, flags, NULL);
	}

	/* HTTP proxy password */
	tmp = nm_setting_vpn_get_data_item (s_vpn, NM_OPENVPN_KEY_PROXY_TYPE);
	if (g_strcmp0 (tmp, "http") == 0 || g_strcmp0 (tmp, "socks") == 0) {
		flags = openvpn_get_secret_flags (nm_connection_get_uuid (connection), NM_OPENVPN_KEY_HTTP_PROXY_PASSWORD);
		nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENVPN_KEY_HTTP_PROXY_PASSWORD, flags, NULL);
	}
}

#define NM_OPENSWAN_PW_TYPE_SAVE   "save"
#define NM_OPENSWAN_PW_TYPE_ASK    "ask"
#define NM_OPENSWAN_PW_TYPE_UNUSED "unused"

static NMSettingSecretFlags
openswan_type_to_flag (NMSettingVPN *s_vpn, const char *flags_key)
{
	const char *tmp;

	tmp = nm_setting_vpn_get_data_item (s_vpn, flags_key);
	if (g_strcmp0 (tmp, NM_OPENSWAN_PW_TYPE_SAVE) == 0)
		return NM_SETTING_SECRET_FLAG_NONE;
	if (g_strcmp0 (tmp, NM_OPENSWAN_PW_TYPE_ASK) == 0)
		return NM_SETTING_SECRET_FLAG_NOT_SAVED;
	if (g_strcmp0 (tmp, NM_OPENSWAN_PW_TYPE_UNUSED) == 0)
		return NM_SETTING_SECRET_FLAG_NOT_REQUIRED;
	return NM_SETTING_SECRET_FLAG_NONE;
}

#define NM_DBUS_SERVICE_OPENSWAN "org.freedesktop.NetworkManager.openswan"
#define NM_OPENSWAN_PSK_VALUE "pskvalue"
#define NM_OPENSWAN_PSK_INPUT_MODES "pskinputmodes"
#define NM_OPENSWAN_XAUTH_PASSWORD "xauthpassword"
#define NM_OPENSWAN_XAUTH_PASSWORD_INPUT_MODES "xauthpasswordinputmodes"

static void
migrate_openswan (NMConnection *connection, NMSettingVPN *s_vpn)
{
	NMSettingSecretFlags flags;

	flags = NM_SETTING_SECRET_FLAG_AGENT_OWNED;
	flags |= openswan_type_to_flag (s_vpn, NM_OPENSWAN_PSK_INPUT_MODES);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENSWAN_PSK_VALUE, flags, NULL);

	flags = NM_SETTING_SECRET_FLAG_AGENT_OWNED;
	flags |= openswan_type_to_flag (s_vpn, NM_OPENSWAN_XAUTH_PASSWORD_INPUT_MODES);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENSWAN_XAUTH_PASSWORD, flags, NULL);
}

#define NM_DBUS_SERVICE_OPENCONNECT    "org.freedesktop.NetworkManager.openconnect"
#define NM_OPENCONNECT_KEY_GATEWAY "gateway"
#define NM_OPENCONNECT_KEY_COOKIE "cookie"
#define NM_OPENCONNECT_KEY_GWCERT "gwcert"
#define NM_OPENCONNECT_KEY_XMLCONFIG "xmlconfig"
#define NM_OPENCONNECT_KEY_LASTHOST "lasthost"
#define NM_OPENCONNECT_KEY_AUTOCONNECT "autoconnect"
#define NM_OPENCONNECT_KEY_CERTSIGS "certsigs"

static void
migrate_datum_to_secret (const char *key, const char *value, gpointer user_data)
{
	NMSettingVPN *s_vpn = user_data;

	/* The xmlconfig "secret" is base64-encoded to escape it, although we
	   were just storing it "raw" in GConf before. */
	if (!strcmp (key, NM_OPENCONNECT_KEY_XMLCONFIG)) {
		gchar *b64 = g_base64_encode ((guchar *)value, strlen(value));
		nm_setting_vpn_add_secret (s_vpn, key, b64);
		g_free (b64);
	} else if (g_str_has_prefix (key, "form:") ||
			   !strcmp (key, NM_OPENCONNECT_KEY_LASTHOST) ||
			   !strcmp (key, NM_OPENCONNECT_KEY_AUTOCONNECT) ||
			   !strcmp (key, NM_OPENCONNECT_KEY_CERTSIGS)) {
		nm_setting_vpn_add_secret (s_vpn, key, value);
	}
}

static void
remove_old_data (const char *key, const char *value, gpointer user_data)
{
	NMSettingVPN *s_vpn = user_data;

	nm_setting_vpn_remove_data_item (s_vpn, key);
}

static void
migrate_openconnect (NMConnection *connection, NMSettingVPN *s_vpn)
{
	NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NOT_SAVED;

	/* These are different for every login session, and should not be stored */
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENCONNECT_KEY_GATEWAY, flags, NULL);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENCONNECT_KEY_COOKIE, flags, NULL);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENCONNECT_KEY_GWCERT, flags, NULL);

	/* These are purely internal data for the auth-dialog, and should be stored */
	flags = NM_SETTING_SECRET_FLAG_NONE;
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENCONNECT_KEY_XMLCONFIG, flags, NULL);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENCONNECT_KEY_LASTHOST, flags, NULL);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENCONNECT_KEY_AUTOCONNECT, flags, NULL);
	nm_setting_set_secret_flags (NM_SETTING (s_vpn), NM_OPENCONNECT_KEY_CERTSIGS, flags, NULL);

	/* Remove obsolete 'authtype' setting */
	nm_setting_vpn_remove_data_item (s_vpn, "authtype");

	/* Iterate over the settings which were in GConf, and convert the appropriate
	   ones to secrets */
	nm_setting_vpn_foreach_data_item (s_vpn, migrate_datum_to_secret, s_vpn);

	/* And now iterate over the new secrets, and remove the corresponding data
	   items that we couldn't remove from *inside* the previous foreach() */
	nm_setting_vpn_foreach_secret (s_vpn, remove_old_data, s_vpn);
}


#define NM_DBUS_SERVICE_PPTP "org.freedesktop.NetworkManager.pptp"
#define NM_PPTP_KEY_PASSWORD "password"

void
nm_gconf_migrate_09_secret_flags (GConfClient *client,
                                  NMConnection *connection,
                                  const char *setting_name)
{
	GList *found_list = NULL, *iter;
	GnomeKeyringResult ret;
	GError *error = NULL;
	NMSetting *setting;
	const char *uuid = nm_connection_get_uuid (connection);
	const char *id = nm_connection_get_id (connection);
	gboolean pk_pw_handled = FALSE;

	setting = nm_connection_get_setting_by_name (connection, setting_name);
	if (!setting)
		return;

	/* Migrate various VPN secret flags */
	if (NM_IS_SETTING_VPN (setting)) {
		NMSettingVPN *s_vpn = NM_SETTING_VPN (setting);
		const char *service;

		service = nm_setting_vpn_get_service_type (s_vpn);
		if (g_strcmp0 (service, NM_DBUS_SERVICE_VPNC) == 0) {
			migrate_vpnc (connection, s_vpn);
			return;
		} else if (g_strcmp0 (service, NM_DBUS_SERVICE_PPTP) == 0) {
			/* Mark the password as agent-owned */
			nm_setting_set_secret_flags (setting, NM_PPTP_KEY_PASSWORD, NM_SETTING_SECRET_FLAG_AGENT_OWNED, NULL);
			return;
		} else if (g_strcmp0 (service, NM_DBUS_SERVICE_OPENVPN) == 0) {
			migrate_openvpn (connection, s_vpn);
			return;
		} else if (g_strcmp0 (service, NM_DBUS_SERVICE_OPENSWAN) == 0) {
			migrate_openswan (connection, s_vpn);
			return;
		} else if (g_strcmp0 (service, NM_DBUS_SERVICE_OPENCONNECT) == 0) {
			migrate_openconnect (connection, s_vpn);
			return;
		}

		/* Other VPNs not handled specially here just go through the
		 * generic secrets flags stuff below.
		 */
	}

	/* 802.1x connections might be 'always-ask' */
	if (NM_IS_SETTING_802_1X (setting)) {
		char *path;
		gboolean ask;

		path = g_strdup_printf (APPLET_PREFS_PATH "/8021x-password-always-ask/%s", uuid);
		ask = gconf_client_get_bool (client, path, NULL);
		g_free (path);

		if (ask) {
			nm_setting_set_secret_flags (setting,
			                             NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD,
			                             NM_SETTING_SECRET_FLAG_NOT_SAVED,
			                             NULL);
			nm_setting_set_secret_flags (setting,
			                             NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD,
			                             NM_SETTING_SECRET_FLAG_NOT_SAVED,
			                             NULL);
			pk_pw_handled = TRUE;
		}
	}

	/* Find all secrets in the keyring */
	ret = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                      &found_list,
	                                      KEYRING_UUID_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
										  uuid,
	                                      KEYRING_SN_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      setting_name,
	                                      NULL);
	if (ret != GNOME_KEYRING_RESULT_OK)
		return;
	if (g_list_length (found_list) == 0)
		return;

	/* For each secret found in the keyring, stuff it into the hash table
	 * so we can update the connection's secrets.
	 */
	for (iter = found_list; iter != NULL; iter = g_list_next (iter)) {
		GnomeKeyringFound *found = (GnomeKeyringFound *) iter->data;
		GnomeKeyringAttribute *attr;
		int i;

		for (i = 0; i < found->attributes->len; i++) {
			attr = &(gnome_keyring_attribute_list_index (found->attributes, i));
			if (   (strcmp (attr->name, KEYRING_SK_TAG) == 0)
			    && (attr->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)) {
				const char *key = attr->value.string;

				/* Ignore handling of private key passwords if it was handled above */
				if (   NM_IS_SETTING_802_1X (setting)
				    && pk_pw_handled
				    && (   g_strcmp0 (key, NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD) == 0
				        || g_strcmp0 (key, NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD) == 0))
					continue;

				if (!nm_setting_set_secret_flags (setting,
				                                  key,
				                                  NM_SETTING_SECRET_FLAG_AGENT_OWNED,
				                                  &error)) {
					g_warning ("%s: failed to set secret flags for %s/%s",
							   id, setting_name, attr->value.string);
					g_clear_error (&error);
				}
				break;
			}
		}
	}

	gnome_keyring_found_list_free (found_list);
}

