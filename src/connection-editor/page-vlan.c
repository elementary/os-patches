/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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

#include <stdlib.h>
#include <string.h>
#include <net/ethernet.h>

#include "page-vlan.h"
#include "connection-helpers.h"
#include "nm-connection-editor.h"

G_DEFINE_TYPE (CEPageVlan, ce_page_vlan, CE_TYPE_PAGE)

#define CE_PAGE_VLAN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_VLAN, CEPageVlanPrivate))

typedef struct {
	char *label;
	NMDevice *device;
	NMConnection *connection;
} VlanParent;

typedef struct {
	NMSettingVlan *setting;
	NMSetting *s_hw;

	VlanParent **parents;
	char **parent_labels;
	int parents_len;

	GtkWindow *toplevel;

	GtkComboBox *parent;
	GtkEntry *parent_entry;
	GtkSpinButton *id_entry;
	GtkEntry *name_entry;
	GtkEntry *cloned_mac;
	GtkSpinButton *mtu;
	GtkToggleButton *flag_reorder_hdr, *flag_gvrp, *flag_loose_binding, *flag_mvrp;

	char *last_parent;
	int last_id;
} CEPageVlanPrivate;

static void
vlan_private_init (CEPageVlan *self)
{
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	GtkBuilder *builder;
	GtkWidget *vbox;
	GtkLabel *label;

	builder = CE_PAGE (self)->builder;

	priv->parent = GTK_COMBO_BOX (gtk_combo_box_text_new_with_entry ());
	gtk_combo_box_set_entry_text_column (priv->parent, 0);
	priv->parent_entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->parent)));

	vbox = GTK_WIDGET (gtk_builder_get_object (builder, "vlan_parent_vbox"));
	gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (priv->parent));
	gtk_widget_show_all (GTK_WIDGET (priv->parent));

	/* Set mnemonic widget for parent label */
	label = GTK_LABEL (gtk_builder_get_object (builder, "vlan_parent_label"));
	gtk_label_set_mnemonic_widget (label, GTK_WIDGET (priv->parent));

	priv->id_entry = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "vlan_id_entry"));
	priv->name_entry = GTK_ENTRY (gtk_builder_get_object (builder, "vlan_name_entry"));
	priv->cloned_mac = GTK_ENTRY (gtk_builder_get_object (builder, "vlan_cloned_mac_entry"));
	priv->mtu = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "vlan_mtu"));
	priv->flag_reorder_hdr = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "reorder_hdr_flag"));
	priv->flag_gvrp = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "gvrp_flag"));
	priv->flag_loose_binding = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "loose_binding_flag"));
	priv->flag_mvrp = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "mvrp_flag"));

	priv->toplevel = GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (priv->mtu),
	                                                      GTK_TYPE_WINDOW));
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void name_changed (GtkWidget *widget, gpointer user_data);

static void
sync_iface (CEPageVlan *self, GtkEntry *changed_entry)
{
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	const char *iface, *iface_end, *parent_text;
	char *new_iface, *end;
	int iface_id, iface_len, parent_iface_len, id;
	gboolean vlan_style_name;

	iface = gtk_entry_get_text (priv->name_entry);
	if (!*iface)
		return;

	if (g_str_has_prefix (iface, "vlan")) {
		iface_end = iface + 4;
		iface_id = strtoul (iface_end, &end, 10);
		vlan_style_name = TRUE;
	} else if ((iface_end = strchr (iface, '.'))) {
		iface_id = strtoul (iface_end + 1, &end, 10);
		vlan_style_name = FALSE;
	} else
		return;
	if (*end)
		return;
	iface_len = iface_end - iface;

	parent_text = gtk_entry_get_text (priv->parent_entry);
	parent_iface_len = strcspn (parent_text, " ");
	id = gtk_spin_button_get_value_as_int (priv->id_entry);

	if (changed_entry == priv->name_entry) {
		/* The user changed the interface name. If it now matches
		 * parent and id, then update the last_* members, so we'll
		 * start keeping it in sync again.
		 */
		if (iface_id == id)
			priv->last_id = iface_id;
		else
			priv->last_id = -1;

		g_free (priv->last_parent);
		if (   iface_len == parent_iface_len
		    && !strncmp (iface, parent_text, iface_len))
			priv->last_parent = g_strndup (iface, iface_len);
		else
			priv->last_parent = NULL;
		return;
	}

	/* The user changed the parent or ID; if the previous parent and
	 * ID matched the interface name, then update the interface name
	 * to match the new one as well.
	 */
	if (iface_id != priv->last_id)
		return;
	if (   !vlan_style_name
	    && priv->last_parent
	    && strncmp (iface, priv->last_parent, iface_len) != 0)
		return;

	if (vlan_style_name) {
		new_iface = g_strdup_printf ("vlan%d", id);
	} else if (changed_entry == priv->parent_entry) {
		new_iface = g_strdup_printf ("%.*s.%d",
		                             parent_iface_len,
		                             parent_text, id);
	} else {
		new_iface = g_strdup_printf ("%.*s.%d", iface_len, iface, id);
	}

	g_signal_handlers_block_by_func (priv->name_entry, G_CALLBACK (name_changed), self);
	gtk_entry_set_text (priv->name_entry, new_iface);
	g_signal_handlers_unblock_by_func (priv->name_entry, G_CALLBACK (name_changed), self);

	g_free (new_iface);

	if (changed_entry == priv->parent_entry) {
		g_free (priv->last_parent);
		priv->last_parent = g_strndup (parent_text, parent_iface_len);
	} else if (changed_entry == GTK_ENTRY (priv->id_entry))
		priv->last_id = id;
}

/* The first item in the combo box may be an arbitrary string not contained in parents array */
static int
get_parents_index (int parents_len, GtkComboBox *box, int combo_index)
{
	int size;
	GtkTreeModel *model;

	/* Get number of items in the combo box */
	model = gtk_combo_box_get_model (box);
	size = gtk_tree_model_iter_n_children (model, NULL);

	return combo_index - (size - parents_len);
}

static void
edit_parent_cb (NMConnectionEditor *editor, GtkResponseType response, gpointer user_data)
{
	CEPageVlan *self = user_data;
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	NMConnection *connection;
	NMConnection *parent;
	NMSettingConnection *s_con;

	if (response != GTK_RESPONSE_OK)
		goto finish;

	connection = nm_connection_editor_get_connection (editor);
	parent = (NMConnection *)nm_client_get_connection_by_uuid (CE_PAGE (self)->client,
	                                                           nm_connection_get_uuid (connection));

	s_con = nm_connection_get_setting_connection (parent);
	gtk_entry_set_text (priv->parent_entry, nm_setting_connection_get_interface_name (s_con));

finish:
	g_object_unref (editor);
}

static void
edit_parent (NMConnection *connection,
                           gpointer user_data)
{
	CEPageVlan *self = user_data;
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	NMConnectionEditor *editor;

	if (!connection)
		return;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_object_set (G_OBJECT (s_con),
		      NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
		      NULL);


	editor = nm_connection_editor_new (priv->toplevel,
					   connection,
					   CE_PAGE (self)->client);
	if (!editor) {
		g_object_unref (connection);
		return;
	}

	g_signal_connect (editor, "done", G_CALLBACK (edit_parent_cb), self);
	nm_connection_editor_run (editor);
}

static gboolean
connection_type_filter (GType type, gpointer user_data)
{
	return nm_utils_check_virtual_device_compatibility (NM_TYPE_SETTING_VLAN, type);
}

static void
parent_changed (GtkWidget *widget, gpointer user_data)
{
	CEPageVlan *self = user_data;
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	int active_id, parent_id;

	active_id = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->parent));
	parent_id = get_parents_index (priv->parents_len, GTK_COMBO_BOX (priv->parent), active_id);

	if (parent_id == priv->parents_len - 1) {
		gtk_entry_set_text (priv->parent_entry, "");
		new_connection_dialog (priv->toplevel,
				       CE_PAGE (self)->client,
				       connection_type_filter,
				       edit_parent,
				       self);
		return;
	}

	if (parent_id > -1 && priv->parents[parent_id]->device != NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->cloned_mac), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->mtu), TRUE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->cloned_mac), FALSE);
		gtk_entry_set_text (priv->cloned_mac, "");
		gtk_widget_set_sensitive (GTK_WIDGET (priv->mtu), FALSE);
		gtk_spin_button_set_value (priv->mtu, 1500);
	}

	sync_iface (self, priv->parent_entry);
	ce_page_changed (CE_PAGE (self));
}

static void
name_changed (GtkWidget *w, gpointer user_data)
{
	CEPageVlan *self = user_data;
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);

	sync_iface (self, priv->name_entry);
	ce_page_changed (CE_PAGE (self));
}

static void
id_changed (GtkWidget *w, gpointer user_data)
{
	CEPageVlan *self = user_data;
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);

	sync_iface (self, GTK_ENTRY (priv->id_entry));
	ce_page_changed (CE_PAGE (self));
}

static int
sort_parents (gconstpointer a, gconstpointer b)
{
	VlanParent *pa = *(VlanParent **)a;
	VlanParent *pb = *(VlanParent **)b;

	if (pa->connection && !pb->connection)
		return 1;
	else if (pb->connection && !pa->connection)
		return -1;
	return strcmp (pa->label, pb->label);
}

static GSList *
get_vlan_devices (CEPageVlan *self)
{
	const GPtrArray *devices_array;
	GSList *devices;
	NMDevice *device;
	int i;
	
	devices_array = nm_client_get_devices (CE_PAGE (self)->client);
	devices = NULL;
	for (i = 0; i < devices_array->len; i++) {
		device = devices_array->pdata[i];

		if (!nm_utils_check_virtual_device_compatibility (NM_TYPE_SETTING_VLAN,
		                                                  nm_device_get_setting_type (device)))
			continue;

		devices = g_slist_prepend (devices, device);
	}

	return devices;
}

static void
build_vlan_parent_list (CEPageVlan *self, GSList *devices)
{
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	const GPtrArray *connections;
	GSList *d_iter;
	GPtrArray *parents;
	VlanParent *parent;
	NMDevice *device;
	const char *iface, *mac, *id;
	int i;

	parents = g_ptr_array_new ();

	/* Devices with no interesting L2 configuration can spawn VLANs directly. At the
	 * moment, this means just Ethernet.
	 */
	for (d_iter = devices; d_iter; d_iter = d_iter->next) {
		device = d_iter->data;

		if (!NM_IS_DEVICE_ETHERNET (device))
			continue;

		parent = g_slice_new (VlanParent);
		parent->device = device;
		parent->connection = NULL;

		iface = nm_device_get_iface (device);
		mac = nm_device_ethernet_get_permanent_hw_address (NM_DEVICE_ETHERNET (device));
		parent->label = g_strdup_printf ("%s (%s)", iface, mac);

		g_ptr_array_add (parents, parent);
	}

	/* Otherwise, VLANs have to be built on top of configured connections */
	connections = nm_client_get_connections (CE_PAGE (self)->client);
	for (i = 0; i < connections->len; i++) {
		NMConnection *candidate = connections->pdata[i];
		NMSettingConnection *s_con = nm_connection_get_setting_connection (candidate);
		GType connection_gtype;

		if (nm_setting_connection_get_master (s_con))
			continue;

		connection_gtype = nm_setting_lookup_type (nm_setting_connection_get_connection_type (s_con));
		if (!nm_utils_check_virtual_device_compatibility (NM_TYPE_SETTING_VLAN, connection_gtype))
			continue;

		for (d_iter = devices; d_iter; d_iter = d_iter->next) {
			device = d_iter->data;

			if (nm_device_connection_valid (device, candidate)) {
				parent = g_slice_new (VlanParent);
				parent->device = device;
				parent->connection = candidate;

				iface = nm_device_get_iface (device);
				id = nm_setting_connection_get_id (s_con);

				/* Translators: the first %s is a device name (eg, "em1"), the
				 * second is a connection name (eg, "Auto Ethernet").
				 */
				parent->label = g_strdup_printf (_("%s (via \"%s\")"), iface, id);
				g_ptr_array_add (parents, parent);
				/* no break here; the connection may apply to multiple devices */
			}
		}
	}

	g_ptr_array_sort (parents, sort_parents);

	parent = g_slice_new (VlanParent);
	parent->device = NULL;
	parent->connection = NULL;
	parent->label = g_strdup_printf (_("New connection..."));
	g_ptr_array_add (parents, parent);

	g_ptr_array_add (parents, NULL);

	priv->parent_labels = g_new (char *, parents->len);
	priv->parents = (VlanParent **)g_ptr_array_free (parents, FALSE);

	for (i = 0; priv->parents[i]; i++)
		priv->parent_labels[i] = priv->parents[i]->label;
	priv->parent_labels[i] = NULL;
	priv->parents_len = i;
}

static void
populate_ui (CEPageVlan *self)
{
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	GSList *devices, *d_iter;
	NMConnection *parent_connection = NULL;
	NMDevice *device, *parent_device = NULL;
	const char *parent, *iface, *current_parent;
	int i, mtu_def, mtu_val;
	guint32 flags;

	devices = get_vlan_devices (self);

	/* Parent */
	build_vlan_parent_list (self, devices);

	parent = nm_setting_vlan_get_parent (priv->setting);
	if (parent) {
		/* UUID? */
		parent_connection = (NMConnection *)nm_client_get_connection_by_uuid (CE_PAGE (self)->client, parent);
		if (!parent_connection) {
			/* Interface name? */
			for (d_iter = devices; d_iter; d_iter = d_iter->next) {
				device = d_iter->data;

				if (!g_strcmp0 (parent, nm_device_get_iface (device))) {
					parent_device = device;
					break;
				}
			}
		}
	}

	/* If NMSettingVlan:parent didn't indicate a device, but we have a
	 * wired setting, figure out the device from that.
	 */
	if (priv->s_hw && !parent_device) {
		const char *device_mac;
		const char *mac;

		if (NM_IS_SETTING_WIRED (priv->s_hw))
			mac = nm_setting_wired_get_mac_address (NM_SETTING_WIRED (priv->s_hw));
		else
			mac = NULL;

		if (mac) {
			for (d_iter = devices; d_iter; d_iter = d_iter->next) {
				device = d_iter->data;

				if (NM_IS_DEVICE_ETHERNET (device))
					device_mac = nm_device_ethernet_get_permanent_hw_address (NM_DEVICE_ETHERNET (device));
				else
					device_mac = NULL;

				if (device_mac && nm_utils_hwaddr_matches (mac, -1, device_mac, -1)) {
					parent_device = device;
					break;
				}
			}
		}
	}

	current_parent = parent;
	if (parent_device || parent_connection) {
		for (i = 0; priv->parents[i]; i++) {
			if (parent_device && parent_device != priv->parents[i]->device)
				continue;
			if (parent_connection != priv->parents[i]->connection)
				continue;

			current_parent = priv->parents[i]->label;
			break;
		}
	}
	g_signal_connect (priv->parent, "changed", G_CALLBACK (parent_changed), self);
	ce_page_setup_data_combo (CE_PAGE (self), priv->parent, current_parent, priv->parent_labels);

	if (current_parent)
		priv->last_parent = g_strndup (current_parent, strcspn (current_parent, " "));

	/* Name */
	iface = nm_connection_get_interface_name (CE_PAGE (self)->connection);
	if (iface)
		gtk_entry_set_text (priv->name_entry, iface);
	g_signal_connect (priv->name_entry, "changed", G_CALLBACK (name_changed), self);

	/* ID */
	priv->last_id = nm_setting_vlan_get_id (priv->setting);
	gtk_spin_button_set_value (priv->id_entry, priv->last_id);
	g_signal_connect (priv->id_entry, "value-changed", G_CALLBACK (id_changed), self);

	/* Cloned MAC address */
	if (NM_IS_SETTING_WIRED (priv->s_hw)) {
		const char *mac = nm_setting_wired_get_cloned_mac_address (NM_SETTING_WIRED (priv->s_hw));
		if (mac)
			gtk_entry_set_text (priv->cloned_mac, mac);
	}
	g_signal_connect (priv->cloned_mac, "changed", G_CALLBACK (stuff_changed), self);

	/* MTU */
	if (NM_IS_SETTING_WIRED (priv->s_hw)) {
		mtu_def = ce_get_property_default (priv->s_hw, NM_SETTING_WIRED_MTU);
		mtu_val = nm_setting_wired_get_mtu (NM_SETTING_WIRED (priv->s_hw));
	} else {
		mtu_def = mtu_val = 1500;
	}
	g_signal_connect (priv->mtu, "output",
	                  G_CALLBACK (ce_spin_output_with_automatic),
	                  GINT_TO_POINTER (mtu_def));

	gtk_spin_button_set_value (priv->mtu, (gdouble) mtu_val);
	g_signal_connect (priv->mtu, "value-changed", G_CALLBACK (stuff_changed), self);

	/* Flags */
	flags = nm_setting_vlan_get_flags (priv->setting);
	if (flags & NM_VLAN_FLAG_REORDER_HEADERS)
		gtk_toggle_button_set_active (priv->flag_reorder_hdr, TRUE);
	if (flags & NM_VLAN_FLAG_GVRP)
		gtk_toggle_button_set_active (priv->flag_gvrp, TRUE);
	if (flags & NM_VLAN_FLAG_LOOSE_BINDING)
		gtk_toggle_button_set_active (priv->flag_loose_binding, TRUE);
	if (flags & NM_VLAN_FLAG_MVRP)
		gtk_toggle_button_set_active (priv->flag_mvrp, TRUE);

	g_slist_free (devices);
}

static void
finish_setup (CEPageVlan *self, gpointer unused, GError *error, gpointer user_data)
{
	if (error)
		return;

	populate_ui (self);
}

CEPage *
ce_page_vlan_new (NMConnectionEditor *editor,
                  NMConnection *connection,
                  GtkWindow *parent_window,
                  NMClient *client,
                  const char **out_secrets_setting_name,
                  GError **error)
{
	CEPageVlan *self;
	CEPageVlanPrivate *priv;

	self = CE_PAGE_VLAN (ce_page_new (CE_TYPE_PAGE_VLAN,
	                                  editor,
	                                  connection,
	                                  parent_window,
	                                  client,
	                                  UIDIR "/ce-page-vlan.ui",
	                                  "VlanPage",
	                                  _("VLAN")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load vlan user interface."));
		return NULL;
	}

	vlan_private_init (self);
	priv = CE_PAGE_VLAN_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_vlan (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_VLAN (nm_setting_vlan_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}
	priv->s_hw = nm_connection_get_setting (connection, NM_TYPE_SETTING_WIRED);

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageVlan *self)
{
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	NMConnection *connection = CE_PAGE (self)->connection;
	NMSettingConnection *s_con = nm_connection_get_setting_connection (connection);
	const char *cloned_mac;
	VlanParent *parent = NULL;
	int active_id, parent_id, vid;
	const char *parent_iface = NULL, *parent_uuid = NULL;
	const char *slave_type;
	const char *iface;
	char *tmp_parent_iface = NULL;
	GType hwtype;
	gboolean mtu_set;
	int mtu;
	guint32 flags = 0;

	active_id = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->parent));
	parent_id = get_parents_index (priv->parents_len, GTK_COMBO_BOX (priv->parent), active_id);
	if (parent_id < 0) {
		parent_iface = gtk_entry_get_text (priv->parent_entry);
		tmp_parent_iface = g_strndup (parent_iface, strcspn (parent_iface, " "));
		parent_iface = tmp_parent_iface;
	} else {
		parent = priv->parents[parent_id];
		if (parent->connection)
			parent_uuid = nm_connection_get_uuid (parent->connection);
		if (parent->device)
			parent_iface = nm_device_get_iface (parent->device);
	}

	g_assert (parent_uuid != NULL || parent_iface != NULL);

	slave_type = nm_setting_connection_get_slave_type (s_con);
	if (parent_uuid) {
		/* Update NMSettingConnection:master if it's set, but don't
		 * set it if it's not.
		 */
		if (!g_strcmp0 (slave_type, NM_SETTING_VLAN_SETTING_NAME)) {
			g_object_set (s_con,
			              NM_SETTING_CONNECTION_MASTER, parent_uuid,
			              NULL);
		}
	} else if (!g_strcmp0 (slave_type, NM_SETTING_VLAN_SETTING_NAME)) {
		g_object_set (s_con,
		              NM_SETTING_CONNECTION_MASTER, NULL,
		              NM_SETTING_CONNECTION_SLAVE_TYPE, NULL,
		              NULL);
	}

	if (parent && NM_IS_DEVICE_ETHERNET (parent->device))
		hwtype = NM_TYPE_SETTING_WIRED;
	else
		hwtype = G_TYPE_NONE;

	if (priv->s_hw && G_OBJECT_TYPE (priv->s_hw) != hwtype) {
		nm_connection_remove_setting (connection, G_OBJECT_TYPE (priv->s_hw));
		priv->s_hw = NULL;
	}

	iface = gtk_entry_get_text (priv->name_entry);
	vid = gtk_spin_button_get_value_as_int (priv->id_entry);

	/* Flags */
	if (gtk_toggle_button_get_active (priv->flag_reorder_hdr))
		flags |= NM_VLAN_FLAG_REORDER_HEADERS;
	if (gtk_toggle_button_get_active (priv->flag_gvrp))
		flags |= NM_VLAN_FLAG_GVRP;
	if (gtk_toggle_button_get_active (priv->flag_loose_binding))
		flags |= NM_VLAN_FLAG_LOOSE_BINDING;
	if (gtk_toggle_button_get_active (priv->flag_mvrp))
		flags |= NM_VLAN_FLAG_MVRP;

	g_object_set (s_con, NM_SETTING_CONNECTION_INTERFACE_NAME, *iface ? iface : NULL, NULL);
	g_object_set (priv->setting,
	              NM_SETTING_VLAN_PARENT, parent_uuid ? parent_uuid : parent_iface,
	              NM_SETTING_VLAN_ID, vid,
	              NM_SETTING_VLAN_FLAGS, flags,
	              NULL);

	if (hwtype != G_TYPE_NONE) {
		cloned_mac = gtk_entry_get_text (priv->cloned_mac);
		if (cloned_mac && !*cloned_mac)
			cloned_mac = NULL;
		mtu_set = g_ascii_isdigit (*gtk_entry_get_text (GTK_ENTRY (priv->mtu)));
		mtu = gtk_spin_button_get_value_as_int (priv->mtu);

		if (cloned_mac || mtu_set) {
			if (!priv->s_hw) {
				priv->s_hw = g_object_new (hwtype, NULL);
				nm_connection_add_setting (connection, priv->s_hw);
			}

			g_object_set (priv->s_hw,
			              NM_SETTING_WIRED_CLONED_MAC_ADDRESS, cloned_mac,
			              NM_SETTING_WIRED_MTU, (guint32) mtu,
			              NULL);

		} else if (priv->s_hw) {
			nm_connection_remove_setting (connection, G_OBJECT_TYPE (priv->s_hw));
			priv->s_hw = NULL;
		}
	}

	g_free (tmp_parent_iface);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageVlan *self = CE_PAGE_VLAN (page);
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	const char *parent;
	char *parent_iface;

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->parent)) == -1) {
		parent = gtk_entry_get_text (priv->parent_entry);
		parent_iface = g_strndup (parent, strcspn (parent, " "));
		if (!ce_page_interface_name_valid (parent_iface, _("vlan parent"), error)) {
			g_free (parent_iface);
			return FALSE;
		}
		g_free (parent_iface);
	}

	if (!ce_page_mac_entry_valid (priv->cloned_mac, ARPHRD_ETHER, _("cloned MAC"), error))
		return FALSE;

	ui_to_setting (self);

	if (   priv->s_hw
	    && !nm_setting_verify (priv->s_hw, NULL, error))
		return FALSE;

	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
finalize (GObject *object)
{
	CEPageVlan *self = CE_PAGE_VLAN (object);
	CEPageVlanPrivate *priv = CE_PAGE_VLAN_GET_PRIVATE (self);
	int i;

	g_free (priv->last_parent);

	for (i = 0; priv->parents[i]; i++)
		g_slice_free (VlanParent, priv->parents[i]);
	g_free (priv->parents);

	G_OBJECT_CLASS (ce_page_vlan_parent_class)->finalize (object);
}

static void
ce_page_vlan_init (CEPageVlan *self)
{
}

static void
ce_page_vlan_class_init (CEPageVlanClass *vlan_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (vlan_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (vlan_class);

	g_type_class_add_private (object_class, sizeof (CEPageVlanPrivate));

	/* virtual methods */
	object_class->finalize = finalize;
	parent_class->ce_page_validate_v = ce_page_validate_v;
}


void
vlan_connection_new (GtkWindow *parent,
                     const char *detail,
                     NMClient *client,
                     PageNewConnectionResultFunc result_func,
                     gpointer user_data)
{
	NMConnection *connection;

	connection = ce_page_new_connection (_("VLAN connection %d"),
	                                     NM_SETTING_VLAN_SETTING_NAME,
	                                     TRUE,
	                                     client,
	                                     user_data);
	nm_connection_add_setting (connection, nm_setting_vlan_new ());

	(*result_func) (connection, FALSE, NULL, user_data);
}

