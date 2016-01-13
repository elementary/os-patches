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
 * (C) Copyright 2005 - 2010 Red Hat, Inc.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

/* libgnome-keyring is deprecated. */
#include "utils.h"
NM_PRAGMA_WARNING_DISABLE("-Wdeprecated-declarations")
#include <gnome-keyring.h>

#include <nm-setting-bluetooth.h>
#include <nm-setting-connection.h>
#include <nm-setting-wired.h>
#include <nm-setting-wireless.h>
#include <nm-setting-wireless-security.h>
#include <nm-setting-8021x.h>
#include <nm-setting-vpn.h>
#include <nm-setting-ip4-config.h>
#include <nm-setting-cdma.h>
#include <nm-setting-gsm.h>
#include <nm-setting-ppp.h>
#include <nm-setting-pppoe.h>
#include <nm-utils.h>

#include "gconf-helpers.h"
#include "gconf-upgrade.h"
#include "nm-glib-compat.h"

#define S390_OPT_KEY_PREFIX "s390-opt-"

#define DBUS_TYPE_G_ARRAY_OF_OBJECT_PATH    (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH))
#define DBUS_TYPE_G_ARRAY_OF_STRING         (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRING))
#define DBUS_TYPE_G_ARRAY_OF_UINT           (dbus_g_type_get_collection ("GArray", G_TYPE_UINT))
#define DBUS_TYPE_G_ARRAY_OF_ARRAY_OF_UCHAR (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_UCHAR_ARRAY))
#define DBUS_TYPE_G_ARRAY_OF_ARRAY_OF_UINT  (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_ARRAY_OF_UINT))
#define DBUS_TYPE_G_MAP_OF_VARIANT          (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))
#define DBUS_TYPE_G_MAP_OF_MAP_OF_VARIANT   (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, DBUS_TYPE_G_MAP_OF_VARIANT))
#define DBUS_TYPE_G_MAP_OF_STRING           (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_STRING))
#define DBUS_TYPE_G_LIST_OF_STRING          (dbus_g_type_get_collection ("GSList", G_TYPE_STRING))
#define DBUS_TYPE_G_IP6_ADDRESS             (dbus_g_type_get_struct ("GValueArray", DBUS_TYPE_G_UCHAR_ARRAY, G_TYPE_UINT, DBUS_TYPE_G_UCHAR_ARRAY, G_TYPE_INVALID))
#define DBUS_TYPE_G_ARRAY_OF_IP6_ADDRESS    (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_IP6_ADDRESS))
#define DBUS_TYPE_G_IP6_ROUTE               (dbus_g_type_get_struct ("GValueArray", DBUS_TYPE_G_UCHAR_ARRAY, G_TYPE_UINT, DBUS_TYPE_G_UCHAR_ARRAY, G_TYPE_UINT, G_TYPE_INVALID))
#define DBUS_TYPE_G_ARRAY_OF_IP6_ROUTE      (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_IP6_ROUTE))

#define APPLET_PREFS_PATH "/apps/nm-applet"

/* The stamp is a mechanism for determining which applet version last
 * updated GConf for various GConf update tasks in newer applet versions.
 */
#define APPLET_CURRENT_STAMP 3
#define APPLET_PREFS_STAMP "/apps/nm-applet/stamp"

const char *applet_8021x_cert_keys[] = {
	"ca-cert",
	"client-cert",
	"private-key",
	"phase2-ca-cert",
	"phase2-client-cert",
	"phase2-private-key",
	NULL
};

const char *vpn_ignore_keys[] = {
	"user-name",
	NULL
};

static GnomeKeyringAttributeList *
_create_keyring_add_attr_list (const char *connection_uuid,
                               const char *connection_id,
                               const char *setting_name,
                               const char *setting_key,
                               char **out_display_name)
{
	GnomeKeyringAttributeList *attrs = NULL;

	g_return_val_if_fail (connection_uuid != NULL, NULL);
	g_return_val_if_fail (connection_id != NULL, NULL);
	g_return_val_if_fail (setting_name != NULL, NULL);
	g_return_val_if_fail (setting_key != NULL, NULL);

	if (out_display_name) {
		*out_display_name = g_strdup_printf ("Network secret for %s/%s/%s",
		                                     connection_id,
		                                     setting_name,
		                                     setting_key);
	}

	attrs = gnome_keyring_attribute_list_new ();
	gnome_keyring_attribute_list_append_string (attrs,
	                                            KEYRING_UUID_TAG,
	                                            connection_uuid);
	gnome_keyring_attribute_list_append_string (attrs,
	                                            KEYRING_SN_TAG,
	                                            setting_name);
	gnome_keyring_attribute_list_append_string (attrs,
	                                            KEYRING_SK_TAG,
	                                            setting_key);
	return attrs;
}

gboolean
nm_gconf_get_int_helper (GConfClient *client,
                         const char *path,
                         const char *key,
                         const char *setting,
                         int *value)
{
	char *		gc_key;
	GConfValue *	gc_value;
	gboolean		success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if ((gc_value = gconf_client_get (client, gc_key, NULL)))
	{
		if (gc_value->type == GCONF_VALUE_INT)
		{
			*value = gconf_value_get_int (gc_value);
			success = TRUE;
		}
		gconf_value_free (gc_value);
	}
	g_free (gc_key);

	return success;
}


gboolean
nm_gconf_get_float_helper (GConfClient *client,
                           const char *path,
                           const char *key,
                           const char *setting,
                           gfloat *value)
{
	char *		gc_key;
	GConfValue *	gc_value;
	gboolean		success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if ((gc_value = gconf_client_get (client, gc_key, NULL)))
	{
		if (gc_value->type == GCONF_VALUE_FLOAT)
		{
			*value = gconf_value_get_float (gc_value);
			success = TRUE;
		}
		gconf_value_free (gc_value);
	}
	g_free (gc_key);

	return success;
}


gboolean
nm_gconf_get_string_helper (GConfClient *client,
                            const char *path,
                            const char *key,
                            const char *setting,
                            char **value)
{
	char *		gc_key;
	GConfValue *	gc_value;
	gboolean		success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (*value == NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if ((gc_value = gconf_client_get (client, gc_key, NULL)))
	{
		if (gc_value->type == GCONF_VALUE_STRING)
		{
			*value = g_strdup (gconf_value_get_string (gc_value));
			success = TRUE;
		}
		gconf_value_free (gc_value);
	}
	g_free (gc_key);

	return success;
}


gboolean
nm_gconf_get_bool_helper (GConfClient *client,
                          const char *path,
                          const char *key,
                          const char *setting,
                          gboolean *value)
{
	char *		gc_key;
	GConfValue *	gc_value;
	gboolean		success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if ((gc_value = gconf_client_get (client, gc_key, NULL)))
	{
		if (gc_value->type == GCONF_VALUE_BOOL)
		{
			*value = gconf_value_get_bool (gc_value);
			success = TRUE;
		}
		else if (gc_value->type == GCONF_VALUE_STRING && !*gconf_value_get_string (gc_value))
		{
			/* This is a kludge to deal with VPN connections migrated from NM 0.6 */
			*value = TRUE;
			success = TRUE;
		}

		gconf_value_free (gc_value);
	}
	g_free (gc_key);

	return success;
}

gboolean
nm_gconf_get_stringlist_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GSList **value)
{
	char *gc_key;
	GConfValue *gc_value;
	gboolean success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (gc_value->type == GCONF_VALUE_LIST
	    && gconf_value_get_list_type (gc_value) == GCONF_VALUE_STRING)
	{
		GSList *elt;

		for (elt = gconf_value_get_list (gc_value); elt != NULL; elt = g_slist_next (elt))
		{
			const char *string = gconf_value_get_string ((GConfValue *) elt->data);

			*value = g_slist_append (*value, g_strdup (string));
		}

		success = TRUE;
	}

out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_get_stringarray_helper (GConfClient *client,
                                 const char *path,
                                 const char *key,
                                 const char *setting,
                                 GPtrArray **value)
{
	char *gc_key;
	GConfValue *gc_value;
	gboolean success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (*value == NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (gc_value->type == GCONF_VALUE_LIST
	    && gconf_value_get_list_type (gc_value) == GCONF_VALUE_STRING)
	{
		GSList *iter, *list;
		const char *string;

		*value = g_ptr_array_sized_new (3);

		list = gconf_value_get_list (gc_value);
		for (iter = list; iter != NULL; iter = g_slist_next (iter)) {
			string = gconf_value_get_string ((GConfValue *) iter->data);
			g_ptr_array_add (*value, g_strdup (string));
		}

		success = TRUE;
	}

out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

typedef struct {
	const char *setting_name;
	const char *key_name;
} MacAddressKey;

static MacAddressKey mac_keys[] = {
	{ NM_SETTING_BLUETOOTH_SETTING_NAME, NM_SETTING_BLUETOOTH_BDADDR },
	{ NM_SETTING_WIRED_SETTING_NAME,     NM_SETTING_WIRED_MAC_ADDRESS },
	{ NM_SETTING_WIRELESS_SETTING_NAME,  NM_SETTING_WIRELESS_MAC_ADDRESS },
	{ NULL, NULL }
};

static gboolean
nm_gconf_get_mac_address_helper (GConfClient *client,
                                 const char *path,
                                 const char *key,
                                 const char *setting,
                                 GByteArray **value)
{
	char *gc_key;
	GConfValue *gc_value;
	gboolean success = FALSE;
	MacAddressKey *tmp = &mac_keys[0];
	gboolean found = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	/* Match against know setting/key combos that can be MAC addresses */
	while (tmp->setting_name) {
		if (!strcmp (tmp->setting_name, setting) && !strcmp (tmp->key_name, key)) {
			found = TRUE;
			break;
		}
		tmp++;
	}
	if (!found)
		return FALSE;

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (gc_value && (gc_value->type == GCONF_VALUE_STRING)) {
		const char *str;
		struct ether_addr *addr;

		str = gconf_value_get_string (gc_value);
		addr = ether_aton (str);
		if (addr) {
			*value = g_byte_array_sized_new (ETH_ALEN);
			g_byte_array_append (*value, (const guint8 *) addr->ether_addr_octet, ETH_ALEN);
			success = TRUE;
		}
	}

out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_get_bytearray_helper (GConfClient *client,
                               const char *path,
                               const char *key,
                               const char *setting,
                               GByteArray **value)
{
	char *gc_key;
	GConfValue *gc_value;
	GByteArray *array;
	gboolean success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (gc_value->type == GCONF_VALUE_LIST
	    && gconf_value_get_list_type (gc_value) == GCONF_VALUE_INT)
	{
		GSList *elt;

		array = g_byte_array_new ();
		for (elt = gconf_value_get_list (gc_value); elt != NULL; elt = g_slist_next (elt))
		{
			int i = gconf_value_get_int ((GConfValue *) elt->data);
			unsigned char val = (unsigned char) (i & 0xFF);

			if (i < 0 || i > 255) {
				g_log (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				       "value %d out-of-range for a byte value", i);
				g_byte_array_free (array, TRUE);
				goto out;
			}

			g_byte_array_append (array, (const unsigned char *) &val, sizeof (val));
		}

		*value = array;
		success = TRUE;
	}

out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_get_uint_array_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GArray **value)
{
	char *gc_key;
	GConfValue *gc_value;
	GArray *array;
	gboolean success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (gc_value->type == GCONF_VALUE_LIST
	    && gconf_value_get_list_type (gc_value) == GCONF_VALUE_INT)
	{
		GSList *elt;

		array = g_array_new (FALSE, FALSE, sizeof (gint));
		for (elt = gconf_value_get_list (gc_value); elt != NULL; elt = g_slist_next (elt))
		{
			int i = gconf_value_get_int ((GConfValue *) elt->data);
			g_array_append_val (array, i);
		}

		*value = array;
		success = TRUE;
	}
	
out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_get_stringhash_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GHashTable **value)
{
	char *gc_key;
	GSList *gconf_entries;
	GSList *iter;
	int path_len;
	const char *key_prefix = NULL;

	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s", path, setting);
	path_len = strlen (gc_key);
	gconf_entries = gconf_client_all_entries (client, gc_key, NULL);
	g_free (gc_key);

	if (!gconf_entries)
		return FALSE;

	if (   !strcmp (setting, NM_SETTING_WIRED_SETTING_NAME)
	    && !strcmp (key, NM_SETTING_WIRED_S390_OPTIONS))
		key_prefix = S390_OPT_KEY_PREFIX;

	*value = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                (GDestroyNotify) g_free,
	                                (GDestroyNotify) g_free);

	for (iter = gconf_entries; iter; iter = iter->next) {
		GConfEntry *entry = (GConfEntry *) iter->data;

		gc_key = (char *) gconf_entry_get_key (entry);
		gc_key += path_len + 1; /* get rid of the full path */

		if (   !strcmp (setting, NM_SETTING_VPN_SETTING_NAME)
		    && (!strcmp (gc_key, NM_SETTING_VPN_SERVICE_TYPE) || !strcmp (gc_key, NM_SETTING_NAME))) {
			/* Ignore; these handled elsewhere since they are not part of the
			 * vpn service specific data
			 */
		} else {
			GConfValue *gc_val = gconf_entry_get_value (entry);
			gboolean ignore = FALSE;

			/* If we have a key prefix and the GConf item has that prefix,
			 * strip off the prefix.  Otherwise, if the GConf items does not
			 * have this prefix, it's not for this key and we should ignore it.
			 */
			if (key_prefix) {
				if (g_str_has_prefix (gc_key, key_prefix))
					gc_key += strlen (key_prefix);
				else
					ignore = TRUE;
			}

			if (gc_val && (ignore == FALSE)) {
				const char *gc_str = gconf_value_get_string (gc_val);

				if (gc_str && strlen (gc_str))
					g_hash_table_insert (*value, gconf_unescape_key (gc_key, -1), g_strdup (gc_str));
			}
		}
		gconf_entry_unref (entry);
	}

	g_slist_free (gconf_entries);
	return TRUE;
}

gboolean
nm_gconf_get_ip4_helper (GConfClient *client,
                         const char *path,
                         const char *key,
                         const char *setting,
                         guint32 tuple_len,
                         GPtrArray **value)
{
	char *gc_key;
	GConfValue *gc_value = NULL;
	GPtrArray *array;
	gboolean success = FALSE;
	GSList *values, *iter;
	GArray *tuple = NULL;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (tuple_len > 0, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (   (gc_value->type != GCONF_VALUE_LIST)
	    || (gconf_value_get_list_type (gc_value) != GCONF_VALUE_INT))
		goto out;

	values = gconf_value_get_list (gc_value);
	if (g_slist_length (values) % tuple_len != 0) {
		g_warning ("%s: %s format invalid; # elements not divisible by %d",
		           __func__, gc_key, tuple_len);
		goto out;
	}

	array = g_ptr_array_sized_new (1);
	for (iter = values; iter; iter = g_slist_next (iter)) {
		int i = gconf_value_get_int ((GConfValue *) iter->data);

		if (tuple == NULL)
			tuple = g_array_sized_new (FALSE, TRUE, sizeof (guint32), tuple_len);

		g_array_append_val (tuple, i);

		/* Got all members; add to the array */
		if (tuple->len == tuple_len) {
			g_ptr_array_add (array, tuple);
			tuple = NULL;
		}
	}

	*value = array;
	success = TRUE;

out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_get_ip6dns_array_helper (GConfClient *client,
								  const char *path,
								  const char *key,
								  const char *setting,
								  GPtrArray **value)
{
	char *gc_key;
	GConfValue *gc_value = NULL;
	GPtrArray *array;
	gboolean success = FALSE;
	GSList *values, *iter;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (   (gc_value->type != GCONF_VALUE_LIST)
	    || (gconf_value_get_list_type (gc_value) != GCONF_VALUE_STRING))
		goto out;

	values = gconf_value_get_list (gc_value);
	array = g_ptr_array_sized_new (1);
	for (iter = values; iter; iter = g_slist_next (iter)) {
		const char *straddr = gconf_value_get_string ((GConfValue *) iter->data);
		struct in6_addr rawaddr;
		GByteArray *ba;

		if (inet_pton (AF_INET6, straddr, &rawaddr) <= 0) {
			g_warning ("%s: %s contained bad address: %s",
					   __func__, gc_key, straddr);
			continue;
		}

		ba = g_byte_array_new ();
		g_byte_array_append (ba, (guchar *)&rawaddr, sizeof (rawaddr));

		g_ptr_array_add (array, ba);
	}

	*value = array;
	success = TRUE;

out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_get_ip6addr_array_helper (GConfClient *client,
                                   const char *path,
                                   const char *key,
                                   const char *setting,
                                   GPtrArray **value)
{
	char *gc_key;
	GConfValue *gc_value = NULL;
	GPtrArray *array;
	gboolean success = FALSE;
	GSList *values, *iter;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (   (gc_value->type != GCONF_VALUE_LIST)
	    || (gconf_value_get_list_type (gc_value) != GCONF_VALUE_STRING))
		goto out;

	values = gconf_value_get_list (gc_value);
	array = g_ptr_array_sized_new (1);
	for (iter = values; iter; iter = g_slist_next (iter)) {
		const char *addr_prefix = gconf_value_get_string ((GConfValue *) iter->data);
		char *addr, *gw = NULL, *p;
		guint prefix;
		struct in6_addr rawaddr, rawgw;
		GValueArray *valarr;
		GValue element = {0, };
		GByteArray *ba;

		addr = g_strdup (addr_prefix);
		p = strchr (addr, '/');
		if (!p) {
			g_warning ("%s: %s contained bad address/prefix: %s",
					   __func__, gc_key, addr_prefix);
			g_free (addr);
			continue;
		}
		*p++ = '\0';
		prefix = strtoul (p, NULL, 10);

		/* Gateway */
		p = strchr (p, ',');
		if (p)
			gw = p + 1;

		if (inet_pton (AF_INET6, addr, &rawaddr) <= 0 && prefix > 128) {
			g_warning ("%s: %s contained bad address: %s",
					   __func__, gc_key, addr_prefix);
			g_free (addr);
			continue;
		}

		memset (&rawgw, 0, sizeof (rawgw));
		if (gw) {
			if (inet_pton (AF_INET6, gw, &rawgw) <= 0) {
				g_warning ("%s: %s contained bad gateway address: %s",
						   __func__, gc_key, gw);
				g_free (addr);
				continue;
			}
		}
		g_free (addr);

		valarr = g_value_array_new (3);

		g_value_init (&element, DBUS_TYPE_G_UCHAR_ARRAY);
		ba = g_byte_array_new ();
		g_byte_array_append (ba, (guint8 *) &rawaddr, 16);
		g_value_take_boxed (&element, ba);
		g_value_array_append (valarr, &element);
		g_value_unset (&element);

		g_value_init (&element, G_TYPE_UINT);
		g_value_set_uint (&element, prefix);
		g_value_array_append (valarr, &element);
		g_value_unset (&element);

		g_value_init (&element, DBUS_TYPE_G_UCHAR_ARRAY);
		ba = g_byte_array_new ();
		g_byte_array_append (ba, (guint8 *) &rawgw, 16);
		g_value_take_boxed (&element, ba);
		g_value_array_append (valarr, &element);
		g_value_unset (&element);

		g_ptr_array_add (array, valarr);
	}

	*value = array;
	success = TRUE;

out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_get_ip6route_array_helper (GConfClient *client,
									const char *path,
									const char *key,
									const char *setting,
									GPtrArray **value)
{
	char *gc_key;
	GConfValue *gc_value = NULL;
	GPtrArray *array;
	gboolean success = FALSE;
	GSList *values, *iter;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!(gc_value = gconf_client_get (client, gc_key, NULL)))
		goto out;

	if (   (gc_value->type != GCONF_VALUE_LIST)
	    || (gconf_value_get_list_type (gc_value) != GCONF_VALUE_STRING))
		goto out;

	values = gconf_value_get_list (gc_value);
	array = g_ptr_array_sized_new (1);
	for (iter = values; iter; iter = g_slist_next (iter)) {
		const char *route_str = gconf_value_get_string ((GConfValue *) iter->data);
		char **parts, *addr, *p;
		guint prefix, metric;
		struct in6_addr rawaddr;
		GValueArray *valarr;
		GValue element = {0, };
		GByteArray *dest, *next_hop;

		parts = g_strsplit (route_str, ",", -1);
		if (g_strv_length (parts) != 3) {
			g_warning ("%s: %s contained bad route: %s",
					   __func__, gc_key, route_str);
			g_strfreev (parts);
			continue;
		}

		addr = parts[0];
		p = strchr (addr, '/');
		if (!p) {
			g_warning ("%s: %s contained bad address/prefix: %s",
					   __func__, gc_key, addr);
			g_strfreev (parts);
			continue;
		}
		*p++ = '\0';
		prefix = strtoul (p, NULL, 10);

		if (inet_pton (AF_INET6, addr, &rawaddr) <= 0 && prefix > 128) {
			g_warning ("%s: %s contained bad address: %s",
					   __func__, gc_key, addr);
			g_strfreev (parts);
			continue;
		}
		dest = g_byte_array_new ();
		g_byte_array_append (dest, (guint8 *) &rawaddr, 16);

		if (inet_pton (AF_INET6, parts[1], &rawaddr) <= 0 && prefix > 128) {
			g_warning ("%s: %s contained bad address: %s",
					   __func__, gc_key, addr);
			g_byte_array_free (dest, TRUE);
			g_strfreev (parts);
			continue;
		}
		next_hop = g_byte_array_new ();
		g_byte_array_append (next_hop, (guint8 *) &rawaddr, 16);

		metric = strtoul (parts[2], NULL, 10);

		valarr = g_value_array_new (4);

		g_value_init (&element, DBUS_TYPE_G_UCHAR_ARRAY);
		g_value_take_boxed (&element, dest);
		g_value_array_append (valarr, &element);
		g_value_unset (&element);

		g_value_init (&element, G_TYPE_UINT);
		g_value_set_uint (&element, prefix);
		g_value_array_append (valarr, &element);
		g_value_unset (&element);

		g_value_init (&element, DBUS_TYPE_G_UCHAR_ARRAY);
		g_value_take_boxed (&element, next_hop);
		g_value_array_append (valarr, &element);
		g_value_unset (&element);

		g_value_init (&element, G_TYPE_UINT);
		g_value_set_uint (&element, metric);
		g_value_array_append (valarr, &element);
		g_value_unset (&element);

		g_ptr_array_add (array, valarr);
		g_strfreev (parts);
	}

	*value = array;
	success = TRUE;

out:
	if (gc_value)
		gconf_value_free (gc_value);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_set_int_helper (GConfClient *client,
                         const char *path,
                         const char *key,
                         const char *setting,
                         int value)
{
	char * gc_key;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}
	gconf_client_set_int (client, gc_key, value, NULL);
	g_free (gc_key);
	return TRUE;
}

gboolean
nm_gconf_set_float_helper (GConfClient *client,
                           const char *path,
                           const char *key,
                           const char *setting,
                           gfloat value)
{
	char * gc_key;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}
	gconf_client_set_float (client, gc_key, value, NULL);
	g_free (gc_key);
	return TRUE;
}

gboolean
nm_gconf_set_string_helper (GConfClient *client,
                            const char *path,
                            const char *key,
                            const char *setting,
                            const char *value)
{
	char * gc_key;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	if (value)
		gconf_client_set_string (client, gc_key, value, NULL);
	else
		gconf_client_unset (client, gc_key, NULL);

	g_free (gc_key);
	return TRUE;
}

gboolean
nm_gconf_set_bool_helper (GConfClient *client,
                          const char *path,
                          const char *key,
                          const char *setting,
                          gboolean value)
{
	char * gc_key;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}
	gconf_client_set_bool (client, gc_key, value, NULL);
	g_free (gc_key);
	return TRUE;
}

gboolean
nm_gconf_set_stringlist_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GSList *value)
{
	char *gc_key;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	gconf_client_set_list (client, gc_key, GCONF_VALUE_STRING, value, NULL);
	g_free (gc_key);
	return TRUE;
}

gboolean
nm_gconf_set_stringarray_helper (GConfClient *client,
                                 const char *path,
                                 const char *key,
                                 const char *setting,
                                 GPtrArray *value)
{
	char *gc_key;
	GSList *list = NULL;
	int i;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	for (i = 0; i < value->len; i++)
		list = g_slist_append (list, g_ptr_array_index (value, i));

	gconf_client_set_list (client, gc_key, GCONF_VALUE_STRING, list, NULL);
	g_slist_free (list);
	g_free (gc_key);
	return TRUE;
}

static gboolean
nm_gconf_set_mac_address_helper (GConfClient *client,
                                 const char *path,
                                 const char *key,
                                 const char *setting,
                                 GByteArray *value)
{
	char *gc_key;
	MacAddressKey *tmp = &mac_keys[0];
	gboolean found = FALSE;
	char *str;

	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	/* Match against know setting/key combos that can be MAC addresses */
	while (tmp->setting_name) {
		if (!strcmp (tmp->setting_name, setting) && !strcmp (tmp->key_name, key)) {
			found = TRUE;
			break;
		}
		tmp++;
	}
	if (!found || !value)
		return FALSE;

	g_return_val_if_fail (value->len == ETH_ALEN, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	str = g_strdup_printf ("%02X:%02X:%02X:%02X:%02X:%02X",
	                       value->data[0], value->data[1], value->data[2],
	                       value->data[3], value->data[4], value->data[5]);
	gconf_client_set_string (client, gc_key, str, NULL);
	g_free (str);

	g_free (gc_key);
	return TRUE;
}

gboolean
nm_gconf_set_bytearray_helper (GConfClient *client,
                               const char *path,
                               const char *key,
                               const char *setting,
                               GByteArray *value)
{
	char *gc_key;
	int i;
	GSList *list = NULL;

	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	if (!value)
		return TRUE;

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	for (i = 0; i < value->len; i++)
		list = g_slist_append(list, GINT_TO_POINTER ((int) value->data[i]));

	gconf_client_set_list (client, gc_key, GCONF_VALUE_INT, list, NULL);

	g_slist_free (list);
	g_free (gc_key);
	return TRUE;
}

gboolean
nm_gconf_set_uint_array_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GArray *value)
{
	char *gc_key;
	int i;
	GSList *list = NULL;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	if (!value)
		return TRUE;

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	for (i = 0; i < value->len; i++)
		list = g_slist_append (list, GUINT_TO_POINTER (g_array_index (value, guint, i)));

	gconf_client_set_list (client, gc_key, GCONF_VALUE_INT, list, NULL);

	g_slist_free (list);
	g_free (gc_key);
	return TRUE;
}

typedef struct {
	GConfClient *client;
	char *path;
} WritePropertiesInfo;

gboolean
nm_gconf_set_stringhash_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GHashTable *value)
{
	char *gc_key;
	GSList *existing, *iter;
	const char *key_prefix = NULL;
	GHashTableIter hash_iter;
	gpointer name, data;

	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s", path, setting);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	if (   !strcmp (setting, NM_SETTING_WIRED_SETTING_NAME)
	    && !strcmp (key, NM_SETTING_WIRED_S390_OPTIONS))
		key_prefix = S390_OPT_KEY_PREFIX;

	/* Delete GConf entries that are not in the hash table to be written */
	existing = gconf_client_all_entries (client, gc_key, NULL);
	for (iter = existing; iter; iter = g_slist_next (iter)) {
		GConfEntry *entry = (GConfEntry *) iter->data;
		const char *basename = strrchr (entry->key, '/');

		if (!basename) {
			g_warning ("GConf key '%s' had no basename", entry->key);
			continue;
		}
		basename++; /* Advance past the '/' */

		/* Don't delete special VPN keys that aren't part of the VPN-plugin
		 * specific data.
		 */
		if (!strcmp (setting, NM_SETTING_VPN_SETTING_NAME)) {
			if (!strcmp (basename, NM_SETTING_VPN_SERVICE_TYPE))
				continue;
			if (!strcmp (basename, NM_SETTING_VPN_USER_NAME))
				continue;
		}

		/* And if we have a key prefix, don't delete anything that does not
		 * have the prefix.
		 */
		if (key_prefix && (g_str_has_prefix (basename, key_prefix) == FALSE))
			continue;

		gconf_client_unset (client, entry->key, NULL);
		gconf_entry_unref (entry);
	}
	g_slist_free (existing);

	/* Now update entries and write new ones */
	g_hash_table_iter_init (&hash_iter, value);
	while (g_hash_table_iter_next (&hash_iter, &name, &data)) {
		char *esc_key, *full_key;

		esc_key = gconf_escape_key ((char *) name, -1);
		full_key = g_strdup_printf ("%s/%s%s",
		                            gc_key,
		                            key_prefix ? key_prefix : "",
		                            esc_key);
		gconf_client_set_string (client, full_key, (char *) data, NULL);
		g_free (esc_key);
		g_free (full_key);
	}

	g_free (gc_key);
	return TRUE;
}

gboolean
nm_gconf_set_ip4_helper (GConfClient *client,
                         const char *path,
                         const char *key,
                         const char *setting,
                         guint32 tuple_len,
                         GPtrArray *value)
{
	char *gc_key;
	int i;
	GSList *list = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);
	g_return_val_if_fail (tuple_len > 0, FALSE);

	if (!value)
		return TRUE;

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	for (i = 0; i < value->len; i++) {
		GArray *tuple = g_ptr_array_index (value, i);
		int j;

		if (tuple->len != tuple_len) {
			g_warning ("%s: invalid IPv4 address/route structure!", __func__);
			goto out;
		}

		for (j = 0; j < tuple_len; j++)
			list = g_slist_append (list, GUINT_TO_POINTER (g_array_index (tuple, guint32, j)));
	}

	gconf_client_set_list (client, gc_key, GCONF_VALUE_INT, list, NULL);
	success = TRUE;

out:
	g_slist_free (list);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_set_ip6dns_array_helper (GConfClient *client,
								  const char *path,
								  const char *key,
								  const char *setting,
								  GPtrArray *value)
{
	char *gc_key;
	int i;
	GSList *list = NULL, *l;
	gboolean success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	if (!value)
		return TRUE;

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	for (i = 0; i < value->len; i++) {
		GByteArray *ba = g_ptr_array_index (value, i);
		char addr[INET6_ADDRSTRLEN];

		if (!inet_ntop (AF_INET6, ba->data, addr, sizeof (addr))) {
			g_warning ("%s: invalid IPv6 DNS server address!", __func__);
			goto out;
		}

		list = g_slist_append (list, g_strdup (addr));
	}

	gconf_client_set_list (client, gc_key, GCONF_VALUE_STRING, list, NULL);
	success = TRUE;

out:
	for (l = list; l; l = l->next)
		g_free (l->data);
	g_slist_free (list);
	g_free (gc_key);
	return success;
}

static gboolean
validate_gvalue_array (GValueArray *elements, guint n_expected, ...)
{
	va_list args;
	GValue *tmp;
	int i;
	gboolean valid = FALSE;

	if (n_expected != elements->n_values)
		return FALSE;

	va_start (args, n_expected);
	for (i = 0; i < n_expected; i++) {
		tmp = g_value_array_get_nth (elements, i);
		if (G_VALUE_TYPE (tmp) != va_arg (args, GType))
			goto done;
	}
	valid = TRUE;

done:
	va_end (args);
	return valid;
}

gboolean
nm_gconf_set_ip6addr_array_helper (GConfClient *client,
								   const char *path,
								   const char *key,
								   const char *setting,
								   GPtrArray *value)
{
	char *gc_key;
	int i;
	GSList *list = NULL, *l;
	gboolean success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	if (!value)
		return TRUE;

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	for (i = 0; i < value->len; i++) {
		GValueArray *elements = (GValueArray *) g_ptr_array_index (value, i);
		GValue *tmp;
		GByteArray *ba;
		guint prefix;
		char addr[INET6_ADDRSTRLEN];
		char gw[INET6_ADDRSTRLEN];
		gboolean have_gw = FALSE;
		char *gconf_str;

		if (elements->n_values < 1 || elements->n_values > 3) {
			g_warning ("%s: invalid IPv6 address!", __func__);
			goto out;
		}

		if (!validate_gvalue_array (elements, 2, DBUS_TYPE_G_UCHAR_ARRAY, G_TYPE_UINT)) {
			g_warning ("%s: invalid IPv6 address!", __func__);
			goto out;
		}

		if (   elements->n_values == 3
		    && !validate_gvalue_array (elements, 3, DBUS_TYPE_G_UCHAR_ARRAY, G_TYPE_UINT, DBUS_TYPE_G_UCHAR_ARRAY)) {
			g_warning ("%s: invalid IPv6 gateway!", __func__);
			goto out;
		}

		tmp = g_value_array_get_nth (elements, 0);
		ba = g_value_get_boxed (tmp);
		tmp = g_value_array_get_nth (elements, 1);
		prefix = g_value_get_uint (tmp);
		if (prefix > 128) {
			g_warning ("%s: invalid IPv6 address prefix %u", __func__, prefix);
			goto out;
		}

		if (!inet_ntop (AF_INET6, ba->data, addr, sizeof (addr))) {
			g_warning ("%s: invalid IPv6 address!", __func__);
			goto out;
		}

		if (elements->n_values == 3) {
			tmp = g_value_array_get_nth (elements, 2);
			ba = g_value_get_boxed (tmp);
			if (ba && !IN6_IS_ADDR_UNSPECIFIED (ba->data)) {
				if (!inet_ntop (AF_INET6, ba->data, gw, sizeof (gw))) {
					g_warning ("%s: invalid IPv6 gateway!", __func__);
					goto out;
				}
				have_gw = TRUE;
			}
		}

		gconf_str = g_strdup_printf ("%s/%u%s%s",
		                             addr,
		                             prefix,
		                             have_gw ? "," : "",
		                             have_gw ? gw : "");
		list = g_slist_append (list, gconf_str);
	}

	gconf_client_set_list (client, gc_key, GCONF_VALUE_STRING, list, NULL);
	success = TRUE;

out:
	for (l = list; l; l = l->next)
		g_free (l->data);
	g_slist_free (list);
	g_free (gc_key);
	return success;
}

gboolean
nm_gconf_set_ip6route_array_helper (GConfClient *client,
									const char *path,
									const char *key,
									const char *setting,
									GPtrArray *value)
{
	char *gc_key;
	int i;
	GSList *list = NULL, *l;
	gboolean success = FALSE;

	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	if (!value)
		return TRUE;

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	if (!gc_key) {
		g_warning ("Not enough memory to create gconf path");
		return FALSE;
	}

	for (i = 0; i < value->len; i++) {
		GValueArray *elements = (GValueArray *) g_ptr_array_index (value, i);
		GValue *tmp;
		GByteArray *ba;
		guint prefix, metric;
		char dest[INET6_ADDRSTRLEN], next_hop[INET6_ADDRSTRLEN];

		if (!validate_gvalue_array (elements, 4,
		                            DBUS_TYPE_G_UCHAR_ARRAY,
		                            G_TYPE_UINT,
		                            DBUS_TYPE_G_UCHAR_ARRAY,
		                            G_TYPE_UINT)) {
			g_warning ("%s: invalid IPv6 route!", __func__);
			goto out;
		}

		tmp = g_value_array_get_nth (elements, 0);
		ba = g_value_get_boxed (tmp);
		if (!inet_ntop (AF_INET6, ba->data, dest, sizeof (dest))) {
			g_warning ("%s: invalid IPv6 dest address!", __func__);
			goto out;
		}
		tmp = g_value_array_get_nth (elements, 1);
		prefix = g_value_get_uint (tmp);
		if (prefix > 128) {
			g_warning ("%s: invalid IPv6 dest prefix %u", __func__, prefix);
			goto out;
		}
		tmp = g_value_array_get_nth (elements, 2);
		ba = g_value_get_boxed (tmp);
		if (!inet_ntop (AF_INET6, ba->data, next_hop, sizeof (next_hop))) {
			g_warning ("%s: invalid IPv6 next_hop address!", __func__);
			goto out;
		}
		tmp = g_value_array_get_nth (elements, 3);
		metric = g_value_get_uint (tmp);

		list = g_slist_append (list,
							   g_strdup_printf ("%s/%u,%s,%u", dest, prefix,
												next_hop, metric));
	}

	gconf_client_set_list (client, gc_key, GCONF_VALUE_STRING, list, NULL);
	success = TRUE;

out:
	for (l = list; l; l = l->next)
		g_free (l->data);
	g_slist_free (list);
	g_free (gc_key);
	return success;
}


gboolean
nm_gconf_key_is_set (GConfClient *client,
                     const char *path,
                     const char *key,
                     const char *setting)
{
	char *gc_key;
	GConfValue *gc_value;
	gboolean exists = FALSE;

	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (setting != NULL, FALSE);

	gc_key = g_strdup_printf ("%s/%s/%s", path, setting, key);
	gc_value = gconf_client_get (client, gc_key, NULL);
	if (gc_value) {
		exists = TRUE;
		gconf_value_free (gc_value);
	}
	g_free (gc_key);
	return exists;
}

static void
move_to_system (GConfClient *client,
                GSList *connections,
                AddToSettingsFunc add_func,
                gpointer user_data)
{
	GSList *iter;
	NMConnection *connection;
	NMSettingConnection *s_con;

	for (iter = connections; iter; iter = iter->next) {
		connection = nm_gconf_read_connection (client, (const char *) iter->data);
		if (!connection)
			continue;

		/* Set this connection visible only to this user */
		s_con = nm_connection_get_setting_connection (connection);
		g_assert (s_con);
		nm_setting_connection_add_permission (s_con, "user", g_get_user_name (), NULL);

		/* Next, any secrets for the connection need to be marked user-owned */
		nm_gconf_migrate_09_secret_flags (client, connection, NM_SETTING_802_1X_SETTING_NAME);
		nm_gconf_migrate_09_secret_flags (client, connection, NM_SETTING_CDMA_SETTING_NAME);
		nm_gconf_migrate_09_secret_flags (client, connection, NM_SETTING_GSM_SETTING_NAME);
		nm_gconf_migrate_09_secret_flags (client, connection, NM_SETTING_PPP_SETTING_NAME);
		nm_gconf_migrate_09_secret_flags (client, connection, NM_SETTING_PPPOE_SETTING_NAME);
		nm_gconf_migrate_09_secret_flags (client, connection, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME);
		nm_gconf_migrate_09_secret_flags (client, connection, NM_SETTING_VPN_SETTING_NAME);

		/* Now add the connection to the system settings service */
		if (add_func)
			add_func (connection, user_data);
	}
}

void
nm_gconf_move_connections_to_system (AddToSettingsFunc add_func, gpointer user_data)
{
	GConfClient *client;
	GSList *connections;
	guint32 stamp = 0;
	GError *error = NULL;

	client = gconf_client_get_default ();
	if (!client)
		return;

	stamp = (guint32) gconf_client_get_int (client, APPLET_PREFS_STAMP, &error);
	if (error) {
		g_error_free (error);
		stamp = 0;
	}

	if (stamp < 3) {
		nm_gconf_migrate_0_7_connection_uuid (client);
		nm_gconf_migrate_0_7_keyring_items (client);
		nm_gconf_migrate_0_7_wireless_security (client);
		nm_gconf_migrate_0_7_netmask_to_prefix (client);
		nm_gconf_migrate_0_7_ip4_method (client);
		nm_gconf_migrate_0_7_ignore_dhcp_dns (client);
		nm_gconf_migrate_0_7_vpn_routes (client);
		nm_gconf_migrate_0_7_vpn_properties (client);
		nm_gconf_migrate_0_7_openvpn_properties (client);

		if (stamp < 1) {
			nm_gconf_migrate_0_7_vpn_never_default (client);
			nm_gconf_migrate_0_7_autoconnect_default (client);
		}

		nm_gconf_migrate_0_7_ca_cert_ignore (client);
		nm_gconf_migrate_0_7_certs (client);
	}

	connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	if (!connections && (stamp < 3)) {
		nm_gconf_migrate_0_6_connections (client);
		/* Try again */
		connections = gconf_client_all_dirs (client, GCONF_PATH_CONNECTIONS, NULL);
	}

	if (connections) {
		/* Move to system connections for 0.9 */
		if (stamp < 3)
			move_to_system (client, connections, add_func, user_data);
		g_slist_foreach (connections, (GFunc) g_free, NULL);
		g_slist_free (connections);
	}

	/* Update the applet GConf stamp */
	if (stamp != APPLET_CURRENT_STAMP)
		gconf_client_set_int (client, APPLET_PREFS_STAMP, APPLET_CURRENT_STAMP, NULL);

	g_object_unref (client);
}

static gboolean
string_in_list (const char *str, const char **valid_strings)
{
	int i;

	for (i = 0; valid_strings[i]; i++)
		if (strcmp (str, valid_strings[i]) == 0)
			break;

	return valid_strings[i] != NULL;
}

static void
free_one_addr (gpointer data)
{
	g_array_free ((GArray *) data, TRUE);
}

static void
free_one_bytearray (gpointer data)
{
	g_byte_array_free (data, TRUE);
}

static void
free_one_struct (gpointer data)
{
	g_value_array_free (data);
}

typedef struct ReadFromGConfInfo {
	NMConnection *connection;
	GConfClient *client;
	const char *dir;
	guint32 dir_len;
} ReadFromGConfInfo;

#define FILE_TAG "file://"

static void
read_one_setting_value_from_gconf (NMSetting *setting,
                                   const char *key,
                                   const GValue *value,
                                   GParamFlags flags,
                                   gpointer user_data)
{
	ReadFromGConfInfo *info = (ReadFromGConfInfo *) user_data;
	const char *setting_name;
	GType type = G_VALUE_TYPE (value);

	/* The 'name' key is ignored when reading, because it's pulled from the
	 * gconf directory name instead.
	 */
	if (!strcmp (key, NM_SETTING_NAME))
		return;

	/* Secrets don't get stored in GConf */
	if (   (flags & NM_SETTING_PARAM_SECRET)
	    && !(NM_IS_SETTING_802_1X (setting) && string_in_list (key, applet_8021x_cert_keys)))
		return;

	/* Don't read the NMSettingConnection object's 'read-only' property */
	if (   NM_IS_SETTING_CONNECTION (setting)
	    && !strcmp (key, NM_SETTING_CONNECTION_READ_ONLY))
		return;

	setting_name = nm_setting_get_name (setting);

	/* Some keys (like certs) aren't read directly from GConf but are handled
	 * separately.
	 */
	/* Some VPN keys are ignored */
	if (NM_IS_SETTING_VPN (setting)) {
		if (string_in_list (key, vpn_ignore_keys))
			return;
	}

	if (   NM_IS_SETTING_802_1X (setting)
	    && string_in_list (key, applet_8021x_cert_keys)
	    && (type == DBUS_TYPE_G_UCHAR_ARRAY)) {
	    char *str_val = NULL;

		/* Certificate/key paths are stored as paths in GConf, but we need to
		 * take that path and use the special functions to set them on the
		 * setting.
		 */
		if (nm_gconf_get_string_helper (info->client, info->dir, key, setting_name, &str_val)) {
			GByteArray *ba_val;

			ba_val = g_byte_array_sized_new (strlen (FILE_TAG) + strlen (str_val) + 1);
			g_byte_array_append (ba_val, (const guint8 *) FILE_TAG, strlen (FILE_TAG));
			g_byte_array_append (ba_val, (const guint8 *) str_val, strlen (str_val) + 1);  /* +1 for the trailing NULL */
			g_object_set (setting, key, ba_val, NULL);
			g_byte_array_free (ba_val, TRUE);
			g_free (str_val);
		}
	} else if (type == G_TYPE_STRING) {
		char *str_val = NULL;

		if (nm_gconf_get_string_helper (info->client, info->dir, key, setting_name, &str_val)) {
			g_object_set (setting, key, str_val, NULL);
			g_free (str_val);
		}
	} else if (type == G_TYPE_UINT) {
		int int_val = 0;

		if (nm_gconf_get_int_helper (info->client, info->dir, key, setting_name, &int_val)) {
			if (int_val < 0)
				g_warning ("Casting negative value (%i) to uint", int_val);

			g_object_set (setting, key, int_val, NULL);
		}
	} else if (type == G_TYPE_INT) {
		int int_val;

		if (nm_gconf_get_int_helper (info->client, info->dir, key, setting_name, &int_val))
			g_object_set (setting, key, int_val, NULL);
	} else if (type == G_TYPE_UINT64) {
		char *tmp_str = NULL;

		/* GConf doesn't do 64-bit values, so use strings instead */
		if (nm_gconf_get_string_helper (info->client, info->dir, key, setting_name, &tmp_str) && tmp_str) {
			guint64 uint_val = g_ascii_strtoull (tmp_str, NULL, 10);
			
			if (!(uint_val == G_MAXUINT64 && errno == ERANGE))
				g_object_set (setting, key, uint_val, NULL);
			g_free (tmp_str);
		}
	} else if (type == G_TYPE_INT64) {
		char *tmp_str = NULL;

		/* GConf doesn't do 64-bit values, so use strings instead */
		if (nm_gconf_get_string_helper (info->client, info->dir, key, setting_name, &tmp_str) && tmp_str) {
			gint64 int_val = g_ascii_strtoll (tmp_str, NULL, 10);
			
			if (!(int_val == G_MAXUINT64 && errno == ERANGE))
				g_object_set (setting, key, int_val, NULL);
			g_free (tmp_str);
		}
	} else if (type == G_TYPE_BOOLEAN) {
		gboolean bool_val;

		if (nm_gconf_get_bool_helper (info->client, info->dir, key, setting_name, &bool_val))
			g_object_set (setting, key, bool_val, NULL);
	} else if (type == G_TYPE_CHAR) {
		int int_val = 0;

		if (nm_gconf_get_int_helper (info->client, info->dir, key, setting_name, &int_val)) {
			if (int_val < G_MININT8 || int_val > G_MAXINT8)
				g_warning ("Casting value (%i) to char", int_val);

			g_object_set (setting, key, int_val, NULL);
		}
	} else if (type == DBUS_TYPE_G_UCHAR_ARRAY) {
		GByteArray *ba_val = NULL;
		gboolean success = FALSE;

		success = nm_gconf_get_mac_address_helper (info->client, info->dir, key, setting_name, &ba_val);
		if (!success)
			success = nm_gconf_get_bytearray_helper (info->client, info->dir, key, setting_name, &ba_val);

		if (success) {
			g_object_set (setting, key, ba_val, NULL);
			g_byte_array_free (ba_val, TRUE);
		}
	} else if (type == DBUS_TYPE_G_LIST_OF_STRING) {
		GSList *sa_val = NULL;

		if (nm_gconf_get_stringlist_helper (info->client, info->dir, key, setting_name, &sa_val)) {
			g_object_set (setting, key, sa_val, NULL);
			g_slist_foreach (sa_val, (GFunc) g_free, NULL);
			g_slist_free (sa_val);
		}
	} else if (type == DBUS_TYPE_G_MAP_OF_STRING) {
		GHashTable *sh_val = NULL;

		if (nm_gconf_get_stringhash_helper (info->client, info->dir, key, setting_name, &sh_val)) {
			g_object_set (setting, key, sh_val, NULL);
			g_hash_table_destroy (sh_val);
		}
	} else if (type == DBUS_TYPE_G_ARRAY_OF_STRING) {
		GPtrArray *pa_val = NULL;

		if (nm_gconf_get_stringarray_helper (info->client, info->dir, key, setting_name, &pa_val)) {
			g_object_set (setting, key, pa_val, NULL);
			g_ptr_array_foreach (pa_val, (GFunc) g_free, NULL);
			g_ptr_array_free (pa_val, TRUE);
		}
	} else if (type == DBUS_TYPE_G_UINT_ARRAY) {
		GArray *a_val = NULL;

		if (nm_gconf_get_uint_array_helper (info->client, info->dir, key, setting_name, &a_val)) {
			g_object_set (setting, key, a_val, NULL);
			g_array_free (a_val, TRUE);
		}
	} else if (type == DBUS_TYPE_G_ARRAY_OF_ARRAY_OF_UINT) {
		GPtrArray *pa_val = NULL;
		guint32 tuple_len = 0;

		if (!strcmp (key, NM_SETTING_IP4_CONFIG_ADDRESSES))
			tuple_len = 3;
		else if (!strcmp (key, NM_SETTING_IP4_CONFIG_ROUTES))
			tuple_len = 4;

		if (nm_gconf_get_ip4_helper (info->client, info->dir, key, setting_name, tuple_len, &pa_val)) {
			g_object_set (setting, key, pa_val, NULL);
			g_ptr_array_foreach (pa_val, (GFunc) free_one_addr, NULL);
			g_ptr_array_free (pa_val, TRUE);
		}
	} else if (type == DBUS_TYPE_G_ARRAY_OF_ARRAY_OF_UCHAR) {
		GPtrArray *pa_val = NULL;

		if (nm_gconf_get_ip6dns_array_helper (info->client, info->dir, key, setting_name, &pa_val)) {
			g_object_set (setting, key, pa_val, NULL);
			g_ptr_array_foreach (pa_val, (GFunc) free_one_bytearray, NULL);
			g_ptr_array_free (pa_val, TRUE);
		}
	} else if (type == DBUS_TYPE_G_ARRAY_OF_IP6_ADDRESS) {
		GPtrArray *pa_val = NULL;

		if (nm_gconf_get_ip6addr_array_helper (info->client, info->dir, key, setting_name, &pa_val)) {
			g_object_set (setting, key, pa_val, NULL);
			g_ptr_array_foreach (pa_val, (GFunc) free_one_struct, NULL);
			g_ptr_array_free (pa_val, TRUE);
		}
	} else if (type == DBUS_TYPE_G_ARRAY_OF_IP6_ROUTE) {
		GPtrArray *pa_val = NULL;

		if (nm_gconf_get_ip6route_array_helper (info->client, info->dir, key, setting_name, &pa_val)) {
			g_object_set (setting, key, pa_val, NULL);
			g_ptr_array_foreach (pa_val, (GFunc) free_one_struct, NULL);
			g_ptr_array_free (pa_val, TRUE);
		}
	} else {
		g_warning ("Unhandled setting property type (read): '%s/%s' : '%s'",
				 setting_name, key, G_VALUE_TYPE_NAME (value));
	}
}

static void
read_one_setting (gpointer data, gpointer user_data)
{
	char *name;
	ReadFromGConfInfo *info = (ReadFromGConfInfo *) user_data;
	NMSetting *setting;

	/* Setting name is the gconf directory name. Since "data" here contains
	   full gconf path plus separator ('/'), omit that. */
	name =  (char *) data + info->dir_len + 1;
	setting = nm_connection_create_setting (name);
	if (setting) {
		nm_setting_enumerate_values (setting,
							    read_one_setting_value_from_gconf,
							    info);
		nm_connection_add_setting (info->connection, setting);
	}

	g_free (data);
}

NMConnection *
nm_gconf_read_connection (GConfClient *client,
                          const char *dir)
{
	ReadFromGConfInfo info;
	GSList *list;
	GError *err = NULL;

	list = gconf_client_all_dirs (client, dir, &err);
	if (err) {
		g_warning ("Error while reading connection: %s", err->message);
		g_error_free (err);
		return NULL;
	}

	if (!list) {
		g_warning ("Invalid connection (empty)");
		return NULL;
	}

	info.connection = nm_connection_new ();
	info.client = client;
	info.dir = dir;
	info.dir_len = strlen (dir);

	g_slist_foreach (list, read_one_setting, &info);
	g_slist_free (list);

	return info.connection;
}


void
nm_gconf_add_keyring_item (const char *connection_uuid,
                           const char *connection_name,
                           const char *setting_name,
                           const char *setting_key,
                           const char *secret)
{
	GnomeKeyringResult ret;
	char *display_name = NULL;
	GnomeKeyringAttributeList *attrs = NULL;
	guint32 id = 0;

	g_return_if_fail (connection_uuid != NULL);
	g_return_if_fail (setting_name != NULL);
	g_return_if_fail (setting_key != NULL);
	g_return_if_fail (secret != NULL);

	attrs = _create_keyring_add_attr_list (connection_uuid,
	                                       connection_name,
	                                       setting_name,
	                                       setting_key,
	                                       &display_name);
	g_assert (attrs);
	ret = gnome_keyring_item_create_sync (NULL,
	                                      GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                      display_name,
	                                      attrs,
	                                      secret,
	                                      TRUE,
	                                      &id);
	if (ret != GNOME_KEYRING_RESULT_OK) {
		g_warning ("Failed to add keyring item (%s/%s/%s/%s): %d",
		           connection_uuid, connection_name, setting_name, setting_key, ret);
	}
	gnome_keyring_attribute_list_free (attrs);
	g_free (display_name);
}

static void
keyring_delete_item (const char *connection_uuid,
                     const char *setting_name,
                     const char *setting_key)
{
	GList *found_list = NULL;
	GnomeKeyringResult ret;
	GList *iter;

	ret = gnome_keyring_find_itemsv_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET,
	                                      &found_list,
	                                      KEYRING_UUID_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      connection_uuid,
	                                      KEYRING_SN_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      setting_name,
	                                      KEYRING_SK_TAG,
	                                      GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
	                                      setting_key,
	                                      NULL);
	if (ret == GNOME_KEYRING_RESULT_OK) {
		for (iter = found_list; iter != NULL; iter = g_list_next (iter)) {
			GnomeKeyringFound *found = (GnomeKeyringFound *) iter->data;

			gnome_keyring_item_delete_sync (found->keyring, found->item_id);
		}
		gnome_keyring_found_list_free (found_list);
	}
}

typedef struct CopyOneSettingValueInfo {
	NMConnection *connection;
	GConfClient *client;
	const char *dir;
	const char *connection_uuid;
	const char *connection_name;
} CopyOneSettingValueInfo;

static void
write_one_secret_to_keyring (NMSetting *setting,
                             const char *key,
                             const GValue *value,
                             GParamFlags flags,
                             gpointer user_data)
{
	CopyOneSettingValueInfo *info = (CopyOneSettingValueInfo *) user_data;
	GType type = G_VALUE_TYPE (value);
	const char *secret;
	const char *setting_name;

	/* non-secrets and private key paths don't get stored in the keyring */
	if (   !(flags & NM_SETTING_PARAM_SECRET)
	    || (NM_IS_SETTING_802_1X (setting) && string_in_list (key, applet_8021x_cert_keys)))
		return;

	setting_name = nm_setting_get_name (setting);

	/* VPN secrets are handled by the VPN plugins */
	if (   (type == DBUS_TYPE_G_MAP_OF_STRING)
	    && NM_IS_SETTING_VPN (setting)
	    && !strcmp (key, NM_SETTING_VPN_SECRETS))
		return;

	if (type != G_TYPE_STRING) {
		g_warning ("Unhandled setting secret type (write) '%s/%s' : '%s'", 
				 setting_name, key, g_type_name (type));
		return;
	}

	secret = g_value_get_string (value);
	if (secret && strlen (secret)) {
		nm_gconf_add_keyring_item (info->connection_uuid,
		                           info->connection_name,
		                           setting_name,
		                           key,
		                           secret);
	} else {
		/* We have to be careful about this, since if the connection we're
		 * given doesn't include secrets we'll blow anything in the keyring
		 * away here.  We rely on the caller knowing whether or not to do this.
		 */
		keyring_delete_item (info->connection_uuid,
		                     setting_name,
		                     key);
	}
}

static gboolean
write_secret_file (const char *path,
                   const char *data,
                   gsize len,
                   GError **error)
{
	char *tmppath;
	int fd = -1, written;
	gboolean success = FALSE;

	tmppath = g_malloc0 (strlen (path) + 10);
	if (!tmppath) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
		             "Could not allocate memory for temporary file for '%s'",
		             path);
		return FALSE;
	}

	memcpy (tmppath, path, strlen (path));
	strcat (tmppath, ".XXXXXX");

	errno = 0;
	fd = mkstemp (tmppath);
	if (fd < 0) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
		             "Could not create temporary file for '%s': %d",
		             path, errno);
		goto out;
	}

	/* Only readable by root */
	errno = 0;
	if (fchmod (fd, S_IRUSR | S_IWUSR)) {
		close (fd);
		unlink (tmppath);
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
		             "Could not set permissions for temporary file '%s': %d",
		             path, errno);
		goto out;
	}

	errno = 0;
	written = write (fd, data, len);
	if (written != len) {
		close (fd);
		unlink (tmppath);
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
		             "Could not write temporary file for '%s': %d",
		             path, errno);
		goto out;
	}
	close (fd);

	/* Try to rename */
	errno = 0;
	if (rename (tmppath, path)) {
		unlink (tmppath);
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
		             "Could not rename temporary file to '%s': %d",
		             path, errno);
		goto out;
	}
	success = TRUE;

out:
	return success;
}

typedef NMSetting8021xCKScheme (*SchemeFunc)  (NMSetting8021x *setting);
typedef const char *           (*PathFunc)    (NMSetting8021x *setting);
typedef const GByteArray *     (*BlobFunc)    (NMSetting8021x *setting);
typedef NMSetting8021xCKFormat (*FormatFunc)  (NMSetting8021x *setting);
typedef const char *           (*PasswordFunc)(NMSetting8021x *setting);

typedef struct ObjectType {
	const char *setting_key;
	gboolean p12_type;
	SchemeFunc scheme_func;
	PathFunc path_func;
	BlobFunc blob_func;
	FormatFunc format_func;
	PasswordFunc password_func;
	const char *privkey_password_key;
	const char *suffix;
} ObjectType;

static const ObjectType ca_type = {
	NM_SETTING_802_1X_CA_CERT,
	FALSE,
	nm_setting_802_1x_get_ca_cert_scheme,
	nm_setting_802_1x_get_ca_cert_path,
	nm_setting_802_1x_get_ca_cert_blob,
	NULL,
	NULL,
	NULL,
	"ca-cert.der"
};

static const ObjectType phase2_ca_type = {
	NM_SETTING_802_1X_PHASE2_CA_CERT,
	FALSE,
	nm_setting_802_1x_get_phase2_ca_cert_scheme,
	nm_setting_802_1x_get_phase2_ca_cert_path,
	nm_setting_802_1x_get_phase2_ca_cert_blob,
	NULL,
	NULL,
	NULL,
	"inner-ca-cert.der"
};

static const ObjectType client_type = {
	NM_SETTING_802_1X_CLIENT_CERT,
	FALSE,
	nm_setting_802_1x_get_client_cert_scheme,
	nm_setting_802_1x_get_client_cert_path,
	nm_setting_802_1x_get_client_cert_blob,
	NULL,
	NULL,
	NULL,
	"client-cert.der"
};

static const ObjectType phase2_client_type = {
	NM_SETTING_802_1X_PHASE2_CLIENT_CERT,
	FALSE,
	nm_setting_802_1x_get_phase2_client_cert_scheme,
	nm_setting_802_1x_get_phase2_client_cert_path,
	nm_setting_802_1x_get_phase2_client_cert_blob,
	NULL,
	NULL,
	NULL,
	"inner-client-cert.der"
};

static const ObjectType pk_type = {
	NM_SETTING_802_1X_PRIVATE_KEY,
	FALSE,
	nm_setting_802_1x_get_private_key_scheme,
	nm_setting_802_1x_get_private_key_path,
	nm_setting_802_1x_get_private_key_blob,
	nm_setting_802_1x_get_private_key_format,
	nm_setting_802_1x_get_private_key_password,
	NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD,
	"private-key.pem"
};

static const ObjectType phase2_pk_type = {
	NM_SETTING_802_1X_PHASE2_PRIVATE_KEY,
	FALSE,
	nm_setting_802_1x_get_phase2_private_key_scheme,
	nm_setting_802_1x_get_phase2_private_key_path,
	nm_setting_802_1x_get_phase2_private_key_blob,
	nm_setting_802_1x_get_phase2_private_key_format,
	nm_setting_802_1x_get_phase2_private_key_password,
	NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD,
	"inner-private-key.pem"
};

static const ObjectType p12_type = {
	NM_SETTING_802_1X_PRIVATE_KEY,
	TRUE,
	nm_setting_802_1x_get_private_key_scheme,
	nm_setting_802_1x_get_private_key_path,
	nm_setting_802_1x_get_private_key_blob,
	nm_setting_802_1x_get_private_key_format,
	nm_setting_802_1x_get_private_key_password,
	NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD,
	"private-key.p12"
};

static const ObjectType phase2_p12_type = {
	NM_SETTING_802_1X_PHASE2_PRIVATE_KEY,
	TRUE,
	nm_setting_802_1x_get_phase2_private_key_scheme,
	nm_setting_802_1x_get_phase2_private_key_path,
	nm_setting_802_1x_get_phase2_private_key_blob,
	nm_setting_802_1x_get_phase2_private_key_format,
	nm_setting_802_1x_get_phase2_private_key_password,
	NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD,
	"inner-private-key.p12"
};

static char *
generate_cert_path (const char *id, const char *suffix)
{
	return g_strdup_printf ("%s/.ssh/%s-%s", g_get_home_dir (), id, suffix);
}

static gboolean
write_object (GConfClient *client,
              const char *dir,
              const char *id,
              NMSetting8021x *s_8021x,
              const GByteArray *override_data,
              const ObjectType *objtype,
              GError **error)
{
	NMSetting8021xCKScheme scheme;
	const char *path = NULL;
	const GByteArray *blob = NULL;
	const char *setting_name = nm_setting_get_name (NM_SETTING (s_8021x));

	g_return_val_if_fail (objtype != NULL, FALSE);

	if (override_data) {
		/* if given explicit data to save, always use that instead of asking
		 * the setting what to do.
		 */
		blob = override_data;
	} else {
		scheme = (*(objtype->scheme_func))(s_8021x);
		switch (scheme) {
		case NM_SETTING_802_1X_CK_SCHEME_BLOB:
			blob = (*(objtype->blob_func))(s_8021x);
			break;
		case NM_SETTING_802_1X_CK_SCHEME_PATH:
			path = (*(objtype->path_func))(s_8021x);
			break;
		default:
			break;
		}
	}

	/* If certificate/private key wasn't sent, the connection may no longer be
	 * 802.1x and thus we clear out the paths and certs.
	 */
	if (!path && !blob) {
		char *standard_file;
		int ignored;

		/* Since no cert/private key is now being used, delete any standard file
		 * that was created for this connection, but leave other files alone.
		 * Thus, for example, ~/.ssh/My Company Network-ca-cert.der will be
		 * deleted, but /etc/pki/tls/cert.pem would not.
		 */
		standard_file = generate_cert_path (id, objtype->suffix);
		if (g_file_test (standard_file, G_FILE_TEST_EXISTS)) {
			ignored = unlink (standard_file);
			if (ignored) {};  /* shut gcc up */
		}
		g_free (standard_file);

		/* Delete the key from GConf */
		nm_gconf_set_string_helper (client, dir, objtype->setting_key, setting_name, NULL);
		return TRUE;
	}

	/* If the object path was specified, prefer that over any raw cert data that
	 * may have been sent.
	 */
	if (path) {
		nm_gconf_set_string_helper (client, dir, objtype->setting_key, setting_name, path);
		return TRUE;
	}

	/* If it's raw certificate data, write the cert data out to the standard file */
	if (blob) {
		gboolean success;
		char *new_file;
		GError *write_error = NULL;

		new_file = generate_cert_path (id, objtype->suffix);
		if (!new_file) {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
			             "Could not create file path for %s / %s",
			             setting_name, objtype->setting_key);
			return FALSE;
		}

		/* Write the raw certificate data out to the standard file so that we
		 * can use paths from now on instead of pushing around the certificate
		 * data itself.
		 */
		success = write_secret_file (new_file, (const char *) blob->data, blob->len, &write_error);
		if (success) {
			nm_gconf_set_string_helper (client, dir, objtype->setting_key, setting_name, new_file);
			return TRUE;
		} else {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
			             "Could not write certificate/key for %s / %s: %s",
			             setting_name, objtype->setting_key,
			             (write_error && write_error->message) ? write_error->message : "(unknown)");
			g_clear_error (&write_error);
		}
		g_free (new_file);
	}

	return FALSE;
}

static gboolean
write_one_certificate (GConfClient *client,
                       const char *dir,
                       const char *key,
                       NMSetting8021x *s_8021x,
                       NMConnection *connection,
                       GError **error)
{
	const char *id;
	NMSettingConnection *s_con;
	const ObjectType *cert_objects[] = {
		&ca_type,
		&phase2_ca_type,
		&client_type,
		&phase2_client_type,
		&pk_type,
		&phase2_pk_type,
		&p12_type,
		&phase2_p12_type,
		NULL
	};
	const ObjectType **obj = &cert_objects[0];
	gboolean handled = FALSE, success = FALSE;

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);
	id = nm_setting_connection_get_id (s_con);
	g_assert (id);

	while (*obj && !handled) {
		const GByteArray *blob = NULL;
		GByteArray *enc_key = NULL;

		if (strcmp (key, (*obj)->setting_key)) {
			obj++;
			continue;
		}

		/* Check for pkcs#12 format private keys; if the current ObjectType
		 * structure isn't for a pkcs#12 key but the key actually is
		 * pkcs#12, keep going to get the right pkcs#12 ObjectType.
		 */
		if (   (*obj)->format_func
		    && ((*obj)->format_func (s_8021x) == NM_SETTING_802_1X_CK_FORMAT_PKCS12)
		    && !(*obj)->p12_type) {
			obj++;
			continue;
		}

		if ((*obj)->scheme_func (s_8021x) == NM_SETTING_802_1X_CK_SCHEME_BLOB)
			blob = (*obj)->blob_func (s_8021x);

		/* Only do the private key re-encrypt dance if we got the raw key data, which
		 * by definition will be unencrypted.  If we're given a direct path to the
		 * private key file, it'll be encrypted, so we don't need to re-encrypt.
		 */
		if (blob && !(*obj)->p12_type) {
			const char *password;
			char *generated_pw;

			/* If the private key is an unencrypted blob, re-encrypt it with a
			 * random password since we don't store unencrypted private keys on disk.
			 */
			password = (*obj)->password_func (s_8021x);

			/* Encrypt the unencrypted private key */
			enc_key = nm_utils_rsa_key_encrypt (blob, password, &generated_pw, error);
			if (!enc_key)
				goto out;

			/* Save any generated private key back into the 802.1x setting so
			 * it'll get stored when secrets are written to the keyring.
			 */
			if (generated_pw) {
				g_object_set (G_OBJECT (s_8021x), (*obj)->privkey_password_key, generated_pw, NULL);
				memset (generated_pw, 0, strlen (generated_pw));
				g_free (generated_pw);
			}
		}

		success = write_object (client, dir, id, s_8021x, enc_key ? enc_key : blob, *obj, error);
		if (enc_key)
			g_byte_array_free (enc_key, TRUE);

		handled = TRUE;
	}

	if (!handled) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
		             "Unhandled certificate/private-key item '%s'",
		             key);
	}

out:
	return success;
}

static void
copy_one_setting_value_to_gconf (NMSetting *setting,
                                 const char *key,
                                 const GValue *value,
                                 GParamFlags flags,
                                 gpointer user_data)
{
	CopyOneSettingValueInfo *info = (CopyOneSettingValueInfo *) user_data;
	const char *setting_name;
	GType type = G_VALUE_TYPE (value);
	GParamSpec *pspec;

	/* Some VPN keys are ignored */
	if (NM_IS_SETTING_VPN (setting)) {
		if (string_in_list (key, vpn_ignore_keys))
			return;
	}

	/* Secrets don't get stored in GConf; but the 802.1x private keys,
	 * which are marked secret for backwards compat, do get stored in
	 * GConf because as of NM 0.8, they are just paths and not the decrypted
	 * private key blobs.
	 */
	if (   (flags & NM_SETTING_PARAM_SECRET)
	    && !(NM_IS_SETTING_802_1X (setting) && string_in_list (key, applet_8021x_cert_keys)))
		return;

	/* Don't write the NMSettingConnection object's 'read-only' property */
	if (   NM_IS_SETTING_CONNECTION (setting)
	    && !strcmp (key, NM_SETTING_CONNECTION_READ_ONLY))
		return;

	setting_name = nm_setting_get_name (setting);

	/* If the value is the default value, remove the item from GConf */
	pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (setting), key);
	if (pspec) {
		if (g_param_value_defaults (pspec, (GValue *) value)) {
			char *path;

			path = g_strdup_printf ("%s/%s/%s", info->dir, setting_name, key);
			if (path)
				gconf_client_unset (info->client, path, NULL);
			g_free (path);		
			return;
		}
	}

	if (   NM_IS_SETTING_802_1X (setting)
		&& string_in_list (key, applet_8021x_cert_keys)
		&& (type == DBUS_TYPE_G_UCHAR_ARRAY)) {
		GError *error = NULL;

		if (!write_one_certificate (info->client,
		                            info->dir,
		                            key,
		                            NM_SETTING_802_1X (setting),
		                            info->connection,
		                            &error)) {
			g_warning ("%s: error saving certificate/private key '%s': (%d) %s",
			           __func__,
			           key,
			           error ? error->code : -1,
			           error && error->message ? error->message : "(unknown)");
		}
	} else if (type == G_TYPE_STRING) {
		nm_gconf_set_string_helper (info->client, info->dir, key, setting_name, g_value_get_string (value));
	} else if (type == G_TYPE_UINT) {
		nm_gconf_set_int_helper (info->client, info->dir,
							key, setting_name,
							g_value_get_uint (value));
	} else if (type == G_TYPE_INT) {
		nm_gconf_set_int_helper (info->client, info->dir,
							key, setting_name,
							g_value_get_int (value));
	} else if (type == G_TYPE_UINT64) {
		char *numstr;

		/* GConf doesn't do 64-bit values, so use strings instead */
		numstr = g_strdup_printf ("%" G_GUINT64_FORMAT, g_value_get_uint64 (value));
		nm_gconf_set_string_helper (info->client, info->dir,
							   key, setting_name, numstr);
		g_free (numstr);
	} else if (type == G_TYPE_BOOLEAN) {
		nm_gconf_set_bool_helper (info->client, info->dir,
							 key, setting_name,
							 g_value_get_boolean (value));
	} else if (type == G_TYPE_CHAR) {
		nm_gconf_set_int_helper (info->client, info->dir,
							key, setting_name,
							g_value_get_schar (value));
	} else if (type == DBUS_TYPE_G_UCHAR_ARRAY) {
		GByteArray *ba_val = (GByteArray *) g_value_get_boxed (value);

		if (!nm_gconf_set_mac_address_helper (info->client, info->dir, key, setting_name, ba_val))
			nm_gconf_set_bytearray_helper (info->client, info->dir, key, setting_name, ba_val);
	} else if (type == DBUS_TYPE_G_LIST_OF_STRING) {
		nm_gconf_set_stringlist_helper (info->client, info->dir,
								  key, setting_name,
								  (GSList *) g_value_get_boxed (value));
	} else if (type == DBUS_TYPE_G_MAP_OF_STRING) {
		nm_gconf_set_stringhash_helper (info->client, info->dir, key,
		                                setting_name,
		                                (GHashTable *) g_value_get_boxed (value));
	} else if (type == DBUS_TYPE_G_ARRAY_OF_STRING) {
		nm_gconf_set_stringarray_helper (info->client, info->dir, key,
		                                setting_name,
		                                (GPtrArray *) g_value_get_boxed (value));
	} else if (type == DBUS_TYPE_G_UINT_ARRAY) {
		nm_gconf_set_uint_array_helper (info->client, info->dir,
								  key, setting_name,
								  (GArray *) g_value_get_boxed (value));
	} else if (type == DBUS_TYPE_G_ARRAY_OF_ARRAY_OF_UINT) {
		guint32 tuple_len = 0;

		if (!strcmp (key, NM_SETTING_IP4_CONFIG_ADDRESSES))
			tuple_len = 3;
		else if (!strcmp (key, NM_SETTING_IP4_CONFIG_ROUTES))
			tuple_len = 4;

		nm_gconf_set_ip4_helper (info->client, info->dir,
								  key, setting_name, tuple_len,
								  (GPtrArray *) g_value_get_boxed (value));
	} else if (type == DBUS_TYPE_G_ARRAY_OF_ARRAY_OF_UCHAR) {
		nm_gconf_set_ip6dns_array_helper (info->client, info->dir,
										  key, setting_name,
										  (GPtrArray *) g_value_get_boxed (value));
	} else if (type == DBUS_TYPE_G_ARRAY_OF_IP6_ADDRESS) {
		nm_gconf_set_ip6addr_array_helper (info->client, info->dir,
										   key, setting_name,
										   (GPtrArray *) g_value_get_boxed (value));
	} else if (type == DBUS_TYPE_G_ARRAY_OF_IP6_ROUTE) {
		nm_gconf_set_ip6route_array_helper (info->client, info->dir,
											key, setting_name,
											(GPtrArray *) g_value_get_boxed (value));
	} else
		g_warning ("Unhandled setting property type (write) '%s/%s' : '%s'", 
				 setting_name, key, g_type_name (type));
}

static void
remove_leftovers (CopyOneSettingValueInfo *info)
{
	GSList *dirs;
	GSList *iter;
	size_t prefix_len;

	prefix_len = strlen (info->dir) + 1;

	dirs = gconf_client_all_dirs (info->client, info->dir, NULL);
	for (iter = dirs; iter; iter = iter->next) {
		char *key = (char *) iter->data;
		NMSetting *setting;

		setting = nm_connection_get_setting_by_name (info->connection, key + prefix_len);
		if (!setting)
			gconf_client_recursive_unset (info->client, key, 0, NULL);

		g_free (key);
	}

	g_slist_free (dirs);
}

void
nm_gconf_write_connection (NMConnection *connection,
                           GConfClient *client,
                           const char *dir,
                           gboolean ignore_secrets)
{
	NMSettingConnection *s_con;
	CopyOneSettingValueInfo info;

	/* NOTE: as of 0.9, this method should only be called during upgrade of
	 * NM 0.6 connections.
	 */

	g_return_if_fail (NM_IS_CONNECTION (connection));
	g_return_if_fail (client != NULL);
	g_return_if_fail (dir != NULL);

	s_con = nm_connection_get_setting_connection (connection);
	if (!s_con)
		return;

	info.connection = connection;
	info.client = client;
	info.dir = dir;
	info.connection_uuid = nm_setting_connection_get_uuid (s_con);
	info.connection_name = nm_setting_connection_get_id (s_con);

	nm_connection_for_each_setting_value (connection,
	                                      copy_one_setting_value_to_gconf,
	                                      &info);
	remove_leftovers (&info);

	/* Write/clear secrets; the caller must know whether or not to do this
	 * based on how the connection was updated; if only something like the
	 * BSSID or timestamp is getting updated, then you want to ignore
	 * secrets, since the secrets could not possibly have changed.  On the
	 * other hand, if the user cleared out a secret in the connection editor,
	 * you want to ensure that secret gets deleted from the keyring.
	 */
	if (ignore_secrets == FALSE) {
		nm_connection_for_each_setting_value (connection,
		                                      write_one_secret_to_keyring,
		                                      &info);
	}
}

