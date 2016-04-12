/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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
 * Copyright 2008 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <net/ethernet.h>
#include <netinet/ether.h>
#include <string.h>
#include <stdlib.h>

#include "ce-page.h"

G_DEFINE_ABSTRACT_TYPE (CEPage, ce_page, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_INITIALIZED,
	PROP_PARENT_WINDOW,

	LAST_PROP
};

enum {
	CHANGED,
	INITIALIZED,

	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gboolean
spin_output_with_default_string (GtkSpinButton *spin,
                                 int defvalue,
                                 const char *defstring)
{
	int val;
	gchar *buf = NULL;

	val = gtk_spin_button_get_value_as_int (spin);
	if (val == defvalue)
		buf = g_strdup (defstring);
	else
		buf = g_strdup_printf ("%d", val);

	if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin))))
		gtk_entry_set_text (GTK_ENTRY (spin), buf);

	g_free (buf);
	return TRUE;
}

gboolean
ce_spin_output_with_automatic (GtkSpinButton *spin, gpointer user_data)
{
	return spin_output_with_default_string (spin,
	                                        GPOINTER_TO_INT (user_data),
	                                        _("automatic"));
}

gboolean
ce_spin_output_with_default (GtkSpinButton *spin, gpointer user_data)
{
	return spin_output_with_default_string (spin,
	                                        GPOINTER_TO_INT (user_data),
	                                        _("default"));
}

int
ce_get_property_default (NMSetting *setting, const char *property_name)
{
	GParamSpec *spec;
	GValue value = { 0, };

	g_return_val_if_fail (NM_IS_SETTING (setting), -1);

	spec = g_object_class_find_property (G_OBJECT_GET_CLASS (setting), property_name);
	g_return_val_if_fail (spec != NULL, -1);

	g_value_init (&value, spec->value_type);
	g_param_value_set_default (spec, &value);

	if (G_VALUE_HOLDS_CHAR (&value))
		return (int) g_value_get_schar (&value);
	else if (G_VALUE_HOLDS_INT (&value))
		return g_value_get_int (&value);
	else if (G_VALUE_HOLDS_INT64 (&value))
		return (int) g_value_get_int64 (&value);
	else if (G_VALUE_HOLDS_LONG (&value))
		return (int) g_value_get_long (&value);
	else if (G_VALUE_HOLDS_UINT (&value))
		return (int) g_value_get_uint (&value);
	else if (G_VALUE_HOLDS_UINT64 (&value))
		return (int) g_value_get_uint64 (&value);
	else if (G_VALUE_HOLDS_ULONG (&value))
		return (int) g_value_get_ulong (&value);
	else if (G_VALUE_HOLDS_UCHAR (&value))
		return (int) g_value_get_uchar (&value);
	g_return_val_if_fail (FALSE, 0);
	return 0;
}

gboolean
ce_page_validate (CEPage *self, NMConnection *connection, GError **error)
{
	g_return_val_if_fail (CE_IS_PAGE (self), FALSE);
	g_return_val_if_fail (NM_IS_CONNECTION (connection), FALSE);

	if (CE_PAGE_GET_CLASS (self)->ce_page_validate_v) {
		if (!CE_PAGE_GET_CLASS (self)->ce_page_validate_v (self, connection, error)) {
			if (error && !*error)
				g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("unspecified error"));
			return FALSE;
		}
	}

	return TRUE;
}

gboolean
ce_page_last_update (CEPage *self, NMConnection *connection, GError **error)
{
	g_return_val_if_fail (CE_IS_PAGE (self), FALSE);
	g_return_val_if_fail (NM_IS_CONNECTION (connection), FALSE);

	if (CE_PAGE_GET_CLASS (self)->last_update)
		return CE_PAGE_GET_CLASS (self)->last_update (self, connection, error);

	return TRUE;
}

gboolean
ce_page_inter_page_change (CEPage *self)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (CE_IS_PAGE (self), FALSE);

	if (self->inter_page_change_running)
		return FALSE;

	self->inter_page_change_running = TRUE;
	if (CE_PAGE_GET_CLASS (self)->inter_page_change)
		ret = CE_PAGE_GET_CLASS (self)->inter_page_change (self);
	self->inter_page_change_running = FALSE;

	return ret;
}

static void
_set_active_combo_item (GtkComboBox *combo, const char *item,
                        const char *combo_item, int combo_idx)
{
	GtkWidget *entry;

	if (item) {
		/* set active item */
		gtk_combo_box_set_active (combo, combo_idx);

		if (!combo_item)
			gtk_combo_box_text_prepend_text (GTK_COMBO_BOX_TEXT (combo), item);

		entry = gtk_bin_get_child (GTK_BIN (combo));
		if (entry)
			gtk_entry_set_text (GTK_ENTRY (entry), combo_item ? combo_item : item);
	}
}

/* Combo box storing data in the form of "text1 (text2)" */
void
ce_page_setup_data_combo (CEPage *self, GtkComboBox *combo,
                          const char *data, char **list)
{
	char **iter, *active_item = NULL;
	int i, active_idx = -1;
	int data_len;

	if (data)
		data_len = strlen (data);
	else
		data_len = -1;

	for (iter = list, i = 0; iter && *iter; iter++, i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), *iter);
		if (   data
		    && g_ascii_strncasecmp (*iter, data, data_len) == 0
		    && ((*iter)[data_len] == '\0' || (*iter)[data_len] == ' ')) {
			active_item = *iter;
			active_idx = i;
		}
	}
	_set_active_combo_item (combo, data, active_item, active_idx);
}

/* Combo box storing MAC addresses only */
void
ce_page_setup_mac_combo (CEPage *self, GtkComboBox *combo,
                         const char *mac, char **mac_list)
{
	char **iter, *active_mac = NULL;
	int i, active_idx = -1;

	for (iter = mac_list, i = 0; iter && *iter; iter++, i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), *iter);
		if (mac && *iter && nm_utils_hwaddr_matches (mac, -1, *iter, -1)) {
			active_mac = *iter;
			active_idx = i;
		}
	}
	_set_active_combo_item (combo, mac, active_mac, active_idx);
}

gboolean
ce_page_mac_entry_valid (GtkEntry *entry, int type, const char *property_name, GError **error)
{
	const char *mac;

	g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

	mac = gtk_entry_get_text (entry);
	if (mac && *mac) {
		if (!nm_utils_hwaddr_valid (mac, nm_utils_hwaddr_len (type))) {
			const char *addr_type;

			addr_type = type == ARPHRD_ETHER ? _("MAC address") : _("HW address");
			if (property_name) {
				g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
				             _("invalid %s for %s (%s)"),
				             addr_type, property_name, mac);
			} else {
				g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
				             _("invalid %s (%s)"),
				             addr_type, mac);
			}
			return FALSE;
		}
	}
	return TRUE;
}

gboolean
ce_page_interface_name_valid (const char *iface, const char *property_name, GError **error)
{
	if (iface && *iface) {
		if (!nm_utils_iface_valid_name (iface)) {
			if (property_name) {
				g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
				             _("invalid interface-name for %s (%s)"),
				             property_name, iface);
			} else {
				g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
				             _("invalid interface-name (%s)"),
				             iface);
			}
			return FALSE;
		}
	}
	return TRUE;
}

static char **
_get_device_list (CEPage *self,
                  GType device_type,
                  gboolean set_ifname,
                  const char *mac_property,
                  gboolean ifname_first)
{
	const GPtrArray *devices;
	GPtrArray *interfaces;
	int i;

	g_return_val_if_fail (CE_IS_PAGE (self), NULL);
	g_return_val_if_fail (set_ifname || mac_property, NULL);

	if (!self->client)
		return NULL;

	interfaces = g_ptr_array_new ();
	devices = nm_client_get_devices (self->client);
	for (i = 0; i < devices->len; i++) {
		NMDevice *dev = g_ptr_array_index (devices, i);
		const char *ifname;
		char *mac = NULL;
		char *item;

		if (!G_TYPE_CHECK_INSTANCE_TYPE (dev, device_type))
			continue;

		if (device_type == NM_TYPE_DEVICE_BT)
			ifname = nm_device_bt_get_name (NM_DEVICE_BT (dev));
		else
			ifname = nm_device_get_iface (NM_DEVICE (dev));
		if (mac_property)
			g_object_get (G_OBJECT (dev), mac_property, &mac, NULL);

		if (set_ifname && mac_property) {
			if (ifname_first)
				item = g_strdup_printf ("%s (%s)", ifname, mac);
			else
				item = g_strdup_printf ("%s (%s)", mac, ifname);
		} else
			item = g_strdup (set_ifname ? ifname : mac);

		g_ptr_array_add (interfaces, item);
		if (mac_property)
			g_free (mac);
	}
	g_ptr_array_add (interfaces, NULL);

	return (char **)g_ptr_array_free (interfaces, FALSE);
}

static gboolean
_device_entry_parse (const char *entry_text, char **first, char **second)
{
	const char *sp, *left, *right;

	if (!entry_text || !*entry_text) {
		*first = NULL;
		*second = NULL;
		return TRUE;
	}

	sp = strstr (entry_text, " (");
	if (sp) {
		*first = g_strndup (entry_text, sp - entry_text);
		left = sp + 1;
		right = strchr (left, ')');
		if (*left == '(' && right && right > left)
			*second = g_strndup (left + 1, right - left - 1);
		else {
			*second = NULL;
			return FALSE;
		}
	} else {
		*first = g_strdup (entry_text);
		*second = NULL;
	}
	return TRUE;
}

static gboolean
_device_entries_match (const char *ifname, const char *mac, const char *entry)
{
	char *first, *second;
	gboolean ifname_match = FALSE, mac_match = FALSE;
	gboolean both;

	if (!ifname && !mac)
		return FALSE;

	_device_entry_parse (entry, &first, &second);
	both = first && second;

	if (   ifname
	    && (   !g_strcmp0 (ifname, first)
	        || !g_strcmp0 (ifname, second)))
		ifname_match = TRUE;

	if (   mac
	    && (   (first && nm_utils_hwaddr_matches (mac, -1, first, -1))
	        || (second && nm_utils_hwaddr_matches (mac, -1, second, -1))))
		mac_match = TRUE;

	g_free (first);
	g_free (second);

	if (both)
		return ifname_match && mac_match;
	else {
		if (ifname)
			return ifname_match;
		else
			return mac_match;
	}
}

/* Combo box storing ifname and/or MAC */
void
ce_page_setup_device_combo (CEPage *self,
                            GtkComboBox *combo,
                            GType device_type,
                            const char *ifname,
                            const char *mac,
                            const char *mac_property,
                            gboolean ifname_first)
{
	char **iter, *active_item = NULL;
	int i, active_idx = -1;
	char **device_list;
	char *item;

	device_list = _get_device_list (self, device_type, TRUE, mac_property, ifname_first);

	if (ifname && mac)
		item = g_strdup_printf ("%s (%s)", ifname, mac);
	else if (!ifname && !mac)
		item = NULL;
	else
		item = g_strdup (ifname ? ifname : mac);

	for (iter = device_list, i = 0; iter && *iter; iter++, i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), *iter);
		if (_device_entries_match (ifname, mac, *iter)) {
			active_item = *iter;
			active_idx = i;
		}
	}
	_set_active_combo_item (combo, item, active_item, active_idx);

	g_free (item);
	g_strfreev (device_list);
}

gboolean
ce_page_device_entry_get (GtkEntry *entry, int type, gboolean check_ifname,
                          char **ifname, char **mac, const char *device_name, GError **error)
{
	char *first, *second;
	const char *ifname_tmp = NULL, *mac_tmp = NULL;
	gboolean valid = TRUE;
	const char *str;

	g_return_val_if_fail (entry != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

	str = gtk_entry_get_text (entry);

	valid = _device_entry_parse (str, &first, &second);

	if (first) {
		if (nm_utils_hwaddr_valid (first, nm_utils_hwaddr_len (type)))
			mac_tmp = first;
		else if (!check_ifname || nm_utils_iface_valid_name (first))
			ifname_tmp = first;
		else
			valid = FALSE;
	}
	if (second) {
		if (nm_utils_hwaddr_valid (second, nm_utils_hwaddr_len (type))) {
			if (!mac_tmp)
				mac_tmp = second;
			else
				valid = FALSE;
		} else if (!check_ifname || nm_utils_iface_valid_name (second)) {
			if (!ifname_tmp)
				ifname_tmp = second;
			else
				valid = FALSE;
		} else
			valid = FALSE;
	}

	if (ifname)
		*ifname = g_strdup (ifname_tmp);
	if (mac)
		*mac = g_strdup (mac_tmp);

	g_free (first);
	g_free (second);

	if (!valid) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
		             _("invalid %s (%s)"),
		             device_name ? device_name : _("device"),
		             str);
	}

	return valid;
}

char *
ce_page_get_next_available_name (const GPtrArray *connections, const char *format)
{
	GSList *names = NULL, *iter;
	char *cname = NULL;
	int i = 0;

	for (i = 0; i < connections->len; i++) {
		const char *id;

		id = nm_connection_get_id (connections->pdata[i]);
		g_assert (id);
		names = g_slist_append (names, (gpointer) id);
	}

	/* Find the next available unique connection name */
	for (i = 1; !cname && i < 10000; i++) {
		char *temp;
		gboolean found = FALSE;

		temp = g_strdup_printf (format, i);
		for (iter = names; iter; iter = g_slist_next (iter)) {
			if (!strcmp (iter->data, temp)) {
				found = TRUE;
				break;
			}
		}
		if (!found)
			cname = temp;
		else
			g_free (temp);
	}

	g_slist_free (names);
	return cname;
}

static void
emit_initialized (CEPage *self, GError *error)
{
	self->initialized = TRUE;
	g_signal_emit (self, signals[INITIALIZED], 0, error);
}

void
ce_page_complete_init (CEPage *self,
                       const char *setting_name,
                       GVariant *secrets,
                       GError *error)
{
	GError *update_error = NULL;
	GVariant *setting_dict;
	char *dbus_err;
	gboolean ignore_error = FALSE;

	g_return_if_fail (self != NULL);
	g_return_if_fail (CE_IS_PAGE (self));

	if (error) {
		dbus_err = g_dbus_error_get_remote_error (error);
		ignore_error =    !g_strcmp0 (dbus_err, "org.freedesktop.NetworkManager.Settings.InvalidSetting")
		               || !g_strcmp0 (dbus_err, "org.freedesktop.NetworkManager.Settings.Connection.SettingNotFound")
		               || !g_strcmp0 (dbus_err, "org.freedesktop.NetworkManager.AgentManager.NoSecrets");
		g_free (dbus_err);
	}

	/* Ignore missing settings errors */
	if (error && !ignore_error) {
		emit_initialized (self, error);
		return;
	} else if (!setting_name || !secrets || g_variant_n_children (secrets) == 0) {
		/* Success, no secrets */
		emit_initialized (self, NULL);
		return;
	}

	g_assert (setting_name);
	g_assert (secrets);

	setting_dict = g_variant_lookup_value (secrets, setting_name, NM_VARIANT_TYPE_SETTING);
	if (!setting_dict) {
		/* Success, no secrets */
		emit_initialized (self, NULL);
		return;
	}
	g_variant_unref (setting_dict);

	/* Update the connection with the new secrets */
	if (nm_connection_update_secrets (self->connection,
	                                  setting_name,
	                                  secrets,
	                                  &update_error)) {
		/* Success */
		emit_initialized (self, NULL);
		return;
	}

	if (!update_error) {
		g_set_error_literal (&update_error, NMA_ERROR, NMA_ERROR_GENERIC,
		                     _("Failed to update connection secrets due to an unknown error."));
	}

	emit_initialized (self, update_error);
	g_clear_error (&update_error);
}

static void
ce_page_init (CEPage *self)
{
	self->builder = gtk_builder_new ();
}

static void
dispose (GObject *object)
{
	CEPage *self = CE_PAGE (object);

	g_clear_object (&self->page);
	g_clear_object (&self->builder);
	g_clear_object (&self->connection);

	G_OBJECT_CLASS (ce_page_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	CEPage *self = CE_PAGE (object);

	g_free (self->title);

	G_OBJECT_CLASS (ce_page_parent_class)->finalize (object);
}

GtkWidget *
ce_page_get_page (CEPage *self)
{
	g_return_val_if_fail (CE_IS_PAGE (self), NULL);

	return self->page;
}

const char *
ce_page_get_title (CEPage *self)
{
	g_return_val_if_fail (CE_IS_PAGE (self), NULL);

	return self->title;
}

gboolean
ce_page_get_initialized (CEPage *self)
{
	g_return_val_if_fail (CE_IS_PAGE (self), FALSE);

	return self->initialized;
}

void
ce_page_changed (CEPage *self)
{
	g_return_if_fail (CE_IS_PAGE (self));

	g_signal_emit (self, signals[CHANGED], 0);
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
	CEPage *self = CE_PAGE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, self->connection);
		break;
	case PROP_INITIALIZED:
		g_value_set_boolean (value, self->initialized);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
	CEPage *self = CE_PAGE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		if (self->connection)
			g_object_unref (self->connection);
		self->connection = g_value_dup_object (value);
		break;
	case PROP_PARENT_WINDOW:
		self->parent_window = g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ce_page_class_init (CEPageClass *page_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (page_class);

	/* virtual methods */
	object_class->dispose      = dispose;
	object_class->finalize     = finalize;
	object_class->get_property = get_property;
	object_class->set_property = set_property;

	/* Properties */
	g_object_class_install_property
		(object_class, PROP_CONNECTION,
		 g_param_spec_object (CE_PAGE_CONNECTION,
		                      "Connection",
		                      "Connection",
		                      NM_TYPE_CONNECTION,
		                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_INITIALIZED,
		 g_param_spec_boolean (CE_PAGE_INITIALIZED,
		                       "Initialized",
		                       "Initialized",
		                       FALSE,
		                       G_PARAM_READABLE));

	g_object_class_install_property
		(object_class, PROP_PARENT_WINDOW,
		 g_param_spec_pointer (CE_PAGE_PARENT_WINDOW,
		                       "Parent window",
		                       "Parent window",
		                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	/* Signals */
	signals[CHANGED] = 
		g_signal_new ("changed",
	                      G_OBJECT_CLASS_TYPE (object_class),
	                      G_SIGNAL_RUN_FIRST,
	                      G_STRUCT_OFFSET (CEPageClass, changed),
	                      NULL, NULL, NULL,
	                      G_TYPE_NONE, 0);

	signals[INITIALIZED] = 
		g_signal_new ("initialized",
	                      G_OBJECT_CLASS_TYPE (object_class),
	                      G_SIGNAL_RUN_FIRST,
	                      G_STRUCT_OFFSET (CEPageClass, initialized),
	                      NULL, NULL, NULL,
	                      G_TYPE_NONE, 1, G_TYPE_POINTER);
}


NMConnection *
ce_page_new_connection (const char *format,
                        const char *ctype,
                        gboolean autoconnect,
                        NMClient *client,
                        gpointer user_data)
{
	NMConnection *connection;
	NMSettingConnection *s_con;
	char *uuid, *id;
	const GPtrArray *connections;

	connection = nm_simple_connection_new ();

	s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());
	nm_connection_add_setting (connection, NM_SETTING (s_con));

	uuid = nm_utils_uuid_generate ();

	connections = nm_client_get_connections (client);
	id = ce_page_get_next_available_name (connections, format);

	g_object_set (s_con,
	              NM_SETTING_CONNECTION_UUID, uuid,
	              NM_SETTING_CONNECTION_ID, id,
	              NM_SETTING_CONNECTION_TYPE, ctype,
	              NM_SETTING_CONNECTION_AUTOCONNECT, autoconnect,
	              NULL);

	g_free (uuid);
	g_free (id);

	return connection;
}

CEPage *
ce_page_new (GType page_type,
             NMConnectionEditor *editor,
             NMConnection *connection,
             GtkWindow *parent_window,
             NMClient *client,
             const char *ui_file,
             const char *widget_name,
             const char *title)
{
	CEPage *self;
	GError *error = NULL;

	g_return_val_if_fail (title != NULL, NULL);
	if (ui_file)
		g_return_val_if_fail (widget_name != NULL, NULL);

	self = CE_PAGE (g_object_new (page_type,
	                              CE_PAGE_CONNECTION, connection,
	                              CE_PAGE_PARENT_WINDOW, parent_window,
	                              NULL));
	self->title = g_strdup (title);
	self->client = client;
	self->editor = editor;

	if (ui_file) {
		if (!gtk_builder_add_from_file (self->builder, ui_file, &error)) {
			g_warning ("Couldn't load builder file: %s", error->message);
			g_error_free (error);
			g_object_unref (self);
			return NULL;
		}

		self->page = GTK_WIDGET (gtk_builder_get_object (self->builder, widget_name));
		if (!self->page) {
			g_warning ("Couldn't load page widget '%s' from %s", widget_name, ui_file);
			g_object_unref (self);
			return NULL;
		}
		g_object_ref_sink (self->page);
	}
	return self;
}

