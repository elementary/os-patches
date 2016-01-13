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
 * Copyright 2012 Red Hat, Inc.
 */

#include "config.h"

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nm-setting-connection.h>
#include <nm-setting-bond.h>
#include <nm-utils.h>

#include "page-bond.h"
#include "page-infiniband.h"
#include "nm-connection-editor.h"
#include "connection-helpers.h"

G_DEFINE_TYPE (CEPageBond, ce_page_bond, CE_TYPE_PAGE_MASTER)

#define CE_PAGE_BOND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_BOND, CEPageBondPrivate))

typedef struct {
	NMSettingBond *setting;
	NMSettingWired *wired;

	int slave_arptype;

	GtkWindow *toplevel;

	GtkComboBox *mode;
	GtkEntry *primary;
	GtkWidget *primary_label;
	GtkComboBox *monitoring;
	GtkSpinButton *frequency;
	GtkSpinButton *updelay;
	GtkWidget *updelay_label;
	GtkWidget *updelay_box;
	GtkSpinButton *downdelay;
	GtkWidget *downdelay_label;
	GtkWidget *downdelay_box;
	GtkEntry *arp_targets;
	GtkWidget *arp_targets_label;
	GtkSpinButton *mtu;
} CEPageBondPrivate;

#define MODE_BALANCE_RR    0
#define MODE_ACTIVE_BACKUP 1
#define MODE_BALANCE_XOR   2
#define MODE_BROADCAST     3
#define MODE_802_3AD       4
#define MODE_BALANCE_TLB   5
#define MODE_BALANCE_ALB   6

#define MONITORING_MII 0
#define MONITORING_ARP 1

static void
bond_private_init (CEPageBond *self)
{
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->mode = GTK_COMBO_BOX (gtk_builder_get_object (builder, "bond_mode"));
	priv->primary = GTK_ENTRY (gtk_builder_get_object (builder, "bond_primary"));
	priv->primary_label = GTK_WIDGET (gtk_builder_get_object (builder, "bond_primary_label"));
	priv->monitoring = GTK_COMBO_BOX (gtk_builder_get_object (builder, "bond_monitoring"));
	priv->frequency = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bond_frequency"));
	priv->updelay = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bond_updelay"));
	priv->updelay_label = GTK_WIDGET (gtk_builder_get_object (builder, "bond_updelay_label"));
	priv->updelay_box = GTK_WIDGET (gtk_builder_get_object (builder, "bond_updelay_box"));
	priv->downdelay = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bond_downdelay"));
	priv->downdelay_label = GTK_WIDGET (gtk_builder_get_object (builder, "bond_downdelay_label"));
	priv->downdelay_box = GTK_WIDGET (gtk_builder_get_object (builder, "bond_downdelay_box"));
	priv->arp_targets = GTK_ENTRY (gtk_builder_get_object (builder, "bond_arp_targets"));
	priv->arp_targets_label = GTK_WIDGET (gtk_builder_get_object (builder, "bond_arp_targets_label"));
	priv->mtu = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bond_mtu"));

	priv->toplevel = GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (priv->mode),
	                                                      GTK_TYPE_WINDOW));
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
connection_removed (CEPageMaster *master, NMConnection *connection)
{
	CEPageBond *self = CE_PAGE_BOND (master);
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);

	if (!ce_page_master_has_slaves (master))
		priv->slave_arptype = ARPHRD_VOID;
}

static void
connection_added (CEPageMaster *master, NMConnection *connection)
{
	CEPageBond *self = CE_PAGE_BOND (master);
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);

	if (nm_connection_is_type (connection, NM_SETTING_INFINIBAND_SETTING_NAME)) {
		priv->slave_arptype = ARPHRD_INFINIBAND;
		gtk_combo_box_set_active (priv->mode, MODE_ACTIVE_BACKUP);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->mode), FALSE);
	} else {
		priv->slave_arptype = ARPHRD_ETHER;
		gtk_widget_set_sensitive (GTK_WIDGET (priv->mode), TRUE);
	}
}

static void
bonding_mode_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageBond *self = user_data;
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);
	int mode;

	mode = gtk_combo_box_get_active (combo);

	if (mode == MODE_BALANCE_TLB || mode == MODE_BALANCE_ALB) {
		gtk_combo_box_set_active (priv->monitoring, MONITORING_MII);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->monitoring), FALSE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->monitoring), TRUE);
	}

	if (mode == MODE_ACTIVE_BACKUP) {
		gtk_widget_show (GTK_WIDGET (priv->primary));
		gtk_widget_show (GTK_WIDGET (priv->primary_label));
	} else {
		gtk_widget_hide (GTK_WIDGET (priv->primary));
		gtk_widget_hide (GTK_WIDGET (priv->primary_label));
	}
}

static void
monitoring_mode_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageBond *self = user_data;
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);

	if (gtk_combo_box_get_active (combo) == MONITORING_MII) {
		gtk_widget_show (GTK_WIDGET (priv->updelay));
		gtk_widget_show (priv->updelay_label);
		gtk_widget_show (priv->updelay_box);
		gtk_widget_show (GTK_WIDGET (priv->downdelay));
		gtk_widget_show (priv->downdelay_label);
		gtk_widget_show (priv->downdelay_box);
		gtk_widget_hide (GTK_WIDGET (priv->arp_targets));
		gtk_widget_hide (priv->arp_targets_label);
	} else {
		gtk_widget_hide (GTK_WIDGET (priv->updelay));
		gtk_widget_hide (priv->updelay_label);
		gtk_widget_hide (priv->updelay_box);
		gtk_widget_hide (GTK_WIDGET (priv->downdelay));
		gtk_widget_hide (priv->downdelay_label);
		gtk_widget_hide (priv->downdelay_box);
		gtk_widget_show (GTK_WIDGET (priv->arp_targets));
		gtk_widget_show (priv->arp_targets_label);
	}
}

static void
frequency_changed (GtkSpinButton *button, gpointer user_data)
{
	CEPageBond *self = user_data;
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);
	int frequency, delay;

	frequency = gtk_spin_button_get_value_as_int (priv->frequency);

	/* Round updelay and downdelay up to a multiple of frequency */

	delay = gtk_spin_button_get_value_as_int (priv->updelay);
	if (frequency == 0) {
		if (delay != 0)
			gtk_spin_button_set_value (priv->updelay, 0.0);
	} else if (delay % frequency) {
		delay += frequency - (delay % frequency);
		gtk_spin_button_set_value (priv->updelay, delay);
	}
	gtk_spin_button_set_increments (priv->updelay, frequency, frequency);

	delay = gtk_spin_button_get_value_as_int (priv->downdelay);
	if (frequency == 0) {
		if (delay != 0)
			gtk_spin_button_set_value (priv->downdelay, 0.0);
	} else if (delay % frequency) {
		delay += frequency - (delay % frequency);
		gtk_spin_button_set_value (priv->downdelay, (gdouble)delay);
	}
	gtk_spin_button_set_increments (priv->downdelay, frequency, frequency);
}

static void
delay_changed (GtkSpinButton *button, gpointer user_data)
{
	CEPageBond *self = user_data;
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);
	int frequency, delay;

	/* Clamp to nearest multiple of frequency */

	frequency = gtk_spin_button_get_value_as_int (priv->frequency);
	delay = gtk_spin_button_get_value_as_int (button);
	if (frequency == 0) {
		if (delay != 0)
			gtk_spin_button_set_value (button, 0.0);
	} else if (delay % frequency) {
		if (delay % frequency < frequency / 2)
			delay -= delay % frequency;
		else
			delay += frequency - (delay % frequency);
		gtk_spin_button_set_value (button, (gdouble)delay);
	}
}

static char *
prettify_targets (const char *text)
{
	char **addrs, *targets;

	if (!text || !*text)
		return NULL;

	addrs = g_strsplit (text, ",", -1);
	targets = g_strjoinv (", ", addrs);
	g_strfreev (addrs);

	return targets;
}

static char *
uglify_targets (const char *text)
{
	char **addrs, *targets;
	int i;

	if (!text || !*text)
		return NULL;

	addrs = g_strsplit (text, ",", -1);
	for (i = 0; addrs[i]; i++)
		g_strstrip (addrs[i]);
	targets = g_strjoinv (",", addrs);
	g_strfreev (addrs);

	return targets;
}

static void
populate_ui (CEPageBond *self)
{
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);
	NMSettingBond *setting = priv->setting;
	const char *mode, *primary, *frequency, *updelay, *downdelay, *raw_targets;
	char *targets;
	int mode_idx = MODE_BALANCE_RR;
	guint32 mtu_def, mtu_val;

	/* Mode */
	mode = nm_setting_bond_get_option_by_name (setting, NM_SETTING_BOND_OPTION_MODE);
	if (mode) {
		if (!strcmp (mode, "balance-rr"))
			mode_idx = MODE_BALANCE_RR;
		else if (!strcmp (mode, "active-backup"))
			mode_idx = MODE_ACTIVE_BACKUP;
		else if (!strcmp (mode, "balance-xor"))
			mode_idx = MODE_BALANCE_XOR;
		else if (!strcmp (mode, "broadcast"))
			mode_idx = MODE_BROADCAST;
		else if (!strcmp (mode, "802.3ad"))
			mode_idx = MODE_802_3AD;
		else if (!strcmp (mode, "balance-tlb"))
			mode_idx = MODE_BALANCE_TLB;
		else if (!strcmp (mode, "balance-alb"))
			mode_idx = MODE_BALANCE_ALB;
	}
	gtk_combo_box_set_active (priv->mode, mode_idx);
	g_signal_connect (priv->mode, "changed",
	                  G_CALLBACK (bonding_mode_changed),
	                  self);
	bonding_mode_changed (priv->mode, self);

	/* Primary */
	primary = nm_setting_bond_get_option_by_name (setting, NM_SETTING_BOND_OPTION_PRIMARY);
	gtk_entry_set_text (priv->primary, primary ? primary : "");

	/* Monitoring mode/frequency */
	frequency = nm_setting_bond_get_option_by_name (setting, NM_SETTING_BOND_OPTION_ARP_INTERVAL);
	if (frequency) {
		gtk_combo_box_set_active (priv->monitoring, MONITORING_ARP);
	} else {
		gtk_combo_box_set_active (priv->monitoring, MONITORING_MII);
		frequency = nm_setting_bond_get_option_by_name (setting, NM_SETTING_BOND_OPTION_MIIMON);
	}
	g_signal_connect (priv->monitoring, "changed",
	                  G_CALLBACK (monitoring_mode_changed),
	                  self);
	monitoring_mode_changed (priv->monitoring, self);

	if (frequency)
		gtk_spin_button_set_value (priv->frequency, (gdouble) atoi (frequency));
	else
		gtk_spin_button_set_value (priv->frequency, 0.0);
	g_signal_connect (priv->frequency, "value-changed",
	                  G_CALLBACK (frequency_changed),
	                  self);

	updelay = nm_setting_bond_get_option_by_name (setting, NM_SETTING_BOND_OPTION_UPDELAY);
	if (updelay)
		gtk_spin_button_set_value (priv->updelay, (gdouble) atoi (updelay));
	else
		gtk_spin_button_set_value (priv->updelay, 0.0);
	g_signal_connect (priv->updelay, "value-changed",
	                  G_CALLBACK (delay_changed),
	                  self);
	downdelay = nm_setting_bond_get_option_by_name (setting, NM_SETTING_BOND_OPTION_DOWNDELAY);
	if (downdelay)
		gtk_spin_button_set_value (priv->downdelay, (gdouble) atoi (downdelay));
	else
		gtk_spin_button_set_value (priv->downdelay, 0.0);
	g_signal_connect (priv->downdelay, "value-changed",
	                  G_CALLBACK (delay_changed),
	                  self);

	/* ARP targets */
	raw_targets = nm_setting_bond_get_option_by_name (setting, NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
	targets = prettify_targets (raw_targets);
	if (targets) {
		gtk_entry_set_text (priv->arp_targets, targets);
		g_free (targets);
	}

	/* MTU */
	if (priv->wired) {
		mtu_def = ce_get_property_default (NM_SETTING (priv->wired), NM_SETTING_WIRED_MTU);
		mtu_val = nm_setting_wired_get_mtu (priv->wired);
	} else {
		mtu_def = mtu_val = 0;
	}
	g_signal_connect (priv->mtu, "output",
	                  G_CALLBACK (ce_spin_output_with_automatic),
	                  GINT_TO_POINTER (mtu_def));
	gtk_spin_button_set_value (priv->mtu, (gdouble) mtu_val);
}

static gboolean
connection_type_filter (GType type, gpointer self)
{
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);

	if (!nm_utils_check_virtual_device_compatibility (NM_TYPE_SETTING_BOND, type))
		return FALSE;

	/* Can only have connections of a single arptype. Note that we don't
	 * need to check the reverse case here since we don't need to call
	 * new_connection_dialog() in the InfiniBand case.
	 */
	if (   priv->slave_arptype == ARPHRD_ETHER
	    && type == NM_TYPE_SETTING_INFINIBAND)
		return FALSE;

	return TRUE;
}

static void
add_slave (CEPageMaster *master, NewConnectionResultFunc result_func)
{
	CEPageBond *self = CE_PAGE_BOND (master);
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);

	if (priv->slave_arptype == ARPHRD_INFINIBAND) {
		new_connection_of_type (priv->toplevel,
		                        NULL,
		                        CE_PAGE (self)->settings,
		                        infiniband_connection_new,
		                        result_func,
		                        master);
	} else {
		new_connection_dialog (priv->toplevel,
		                       CE_PAGE (self)->settings,
		                       connection_type_filter,
		                       result_func,
		                       master);
	}
}

static void
finish_setup (CEPageBond *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self);

	g_signal_connect (priv->mode, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->primary, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->monitoring, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->frequency, "value-changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->updelay, "value-changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->downdelay, "value-changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->arp_targets, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->mtu, "value-changed", G_CALLBACK (stuff_changed), self);
}

CEPage *
ce_page_bond_new (NMConnectionEditor *editor,
                  NMConnection *connection,
                  GtkWindow *parent_window,
                  NMClient *client,
                  NMRemoteSettings *settings,
                  const char **out_secrets_setting_name,
                  GError **error)
{
	CEPageBond *self;
	CEPageBondPrivate *priv;

	self = CE_PAGE_BOND (ce_page_new (CE_TYPE_PAGE_BOND,
	                                  editor,
	                                  connection,
	                                  parent_window,
	                                  client,
	                                  settings,
	                                  UIDIR "/ce-page-bond.ui",
	                                  "BondPage",
	                                  _("Bond")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC,
		                     _("Could not load bond user interface."));
		return NULL;
	}

	bond_private_init (self);
	priv = CE_PAGE_BOND_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_bond (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_BOND (nm_setting_bond_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}
	priv->wired = nm_connection_get_setting_wired (connection);

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageBond *self)
{
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);
	NMConnection *connection = CE_PAGE (self)->connection;
	const char *mode;
	const char *frequency;
	const char *updelay;
	const char *downdelay;
	const char *primary = NULL;
	char *targets;
	guint32 mtu;

	/* Mode */
	switch (gtk_combo_box_get_active (priv->mode)) {
	case MODE_BALANCE_RR:
		mode = "balance-rr";
		break;
	case MODE_ACTIVE_BACKUP:
		mode = "active-backup";
		primary = gtk_entry_get_text (priv->primary);
		break;
	case MODE_BALANCE_XOR:
		mode = "balance-xor";
		break;
	case MODE_BROADCAST:
		mode = "broadcast";
		break;
	case MODE_802_3AD:
		mode = "802.3ad";
		break;
	case MODE_BALANCE_TLB:
		mode = "balance-tlb";
		break;
	case MODE_BALANCE_ALB:
		mode = "balance-alb";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	/* Set bond mode and primary */
	nm_setting_bond_add_option (priv->setting, NM_SETTING_BOND_OPTION_MODE, mode);

	if (primary && *primary)
		nm_setting_bond_add_option (priv->setting, NM_SETTING_BOND_OPTION_PRIMARY, primary);
	else
		nm_setting_bond_remove_option (priv->setting, NM_SETTING_BOND_OPTION_PRIMARY);

	/* Monitoring mode/frequency */
	frequency = gtk_entry_get_text (GTK_ENTRY (priv->frequency));
	updelay = gtk_entry_get_text (GTK_ENTRY (priv->updelay));
	downdelay = gtk_entry_get_text (GTK_ENTRY (priv->downdelay));
	targets = uglify_targets (gtk_entry_get_text (priv->arp_targets));

	switch (gtk_combo_box_get_active (priv->monitoring)) {
	case MONITORING_MII:
		nm_setting_bond_add_option (priv->setting, NM_SETTING_BOND_OPTION_MIIMON, frequency);
		nm_setting_bond_add_option (priv->setting, NM_SETTING_BOND_OPTION_UPDELAY, updelay);
		nm_setting_bond_add_option (priv->setting, NM_SETTING_BOND_OPTION_DOWNDELAY, downdelay);
		nm_setting_bond_remove_option (priv->setting, NM_SETTING_BOND_OPTION_ARP_INTERVAL);
		nm_setting_bond_remove_option (priv->setting, NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
		break;
	case MONITORING_ARP:
		nm_setting_bond_add_option (priv->setting, NM_SETTING_BOND_OPTION_ARP_INTERVAL, frequency);
		if (targets)
			nm_setting_bond_add_option (priv->setting, NM_SETTING_BOND_OPTION_ARP_IP_TARGET, targets);
		else
			nm_setting_bond_remove_option (priv->setting, NM_SETTING_BOND_OPTION_ARP_IP_TARGET);
		nm_setting_bond_remove_option (priv->setting, NM_SETTING_BOND_OPTION_MIIMON);
		nm_setting_bond_remove_option (priv->setting, NM_SETTING_BOND_OPTION_UPDELAY);
		nm_setting_bond_remove_option (priv->setting, NM_SETTING_BOND_OPTION_DOWNDELAY);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	g_free (targets);

	mtu = gtk_spin_button_get_value_as_int (priv->mtu);
	if (mtu && !priv->wired) {
		priv->wired = NM_SETTING_WIRED (nm_setting_wired_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->wired));
	}
	if (priv->wired)
		g_object_set (priv->wired, NM_SETTING_WIRED_MTU, mtu, NULL);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageBond *self = CE_PAGE_BOND (page);
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);

	if (!CE_PAGE_CLASS (ce_page_bond_parent_class)->ce_page_validate_v (page, connection, error))
		return FALSE;

	if (!ce_page_interface_name_valid (gtk_entry_get_text (priv->primary),
	                                   _("primary"), error))
		return FALSE;

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_bond_init (CEPageBond *self)
{
	CEPageBondPrivate *priv = CE_PAGE_BOND_GET_PRIVATE (self);
	CEPageMaster *master = CE_PAGE_MASTER (self);

	priv->slave_arptype = ARPHRD_VOID;
	master->aggregating = TRUE;
}

static void
ce_page_bond_class_init (CEPageBondClass *bond_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (bond_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (bond_class);
	CEPageMasterClass *master_class = CE_PAGE_MASTER_CLASS (bond_class);

	g_type_class_add_private (object_class, sizeof (CEPageBondPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;

	master_class->connection_added = connection_added;
	master_class->connection_removed = connection_removed;
	master_class->add_slave = add_slave;
}


void
bond_connection_new (GtkWindow *parent,
                     const char *detail,
                     NMRemoteSettings *settings,
                     PageNewConnectionResultFunc result_func,
                     gpointer user_data)
{
	NMConnection *connection;
	int bond_num = 0, num;
	GSList *connections, *iter;
	NMConnection *conn2;
	NMSettingBond *s_bond;
	const char *iface;
	char *my_iface;

	connection = ce_page_new_connection (_("Bond connection %d"),
	                                     NM_SETTING_BOND_SETTING_NAME,
	                                     TRUE,
	                                     settings,
	                                     user_data);
	nm_connection_add_setting (connection, nm_setting_bond_new ());

	/* Find an available interface name */
	connections = nm_remote_settings_list_connections (settings);
	for (iter = connections; iter; iter = iter->next) {
		conn2 = iter->data;

		if (!nm_connection_is_type (conn2, NM_SETTING_BOND_SETTING_NAME))
			continue;
		s_bond = nm_connection_get_setting_bond (conn2);
		if (!s_bond)
			continue;
		iface = nm_setting_bond_get_interface_name (s_bond);
		if (!iface || strncmp (iface, "bond", 4) != 0 || !g_ascii_isdigit (iface[4]))
			continue;

		num = atoi (iface + 4);
		if (bond_num <= num)
			bond_num = num + 1;
	}
	g_slist_free (connections);

	my_iface = g_strdup_printf ("bond%d", bond_num);
	s_bond = nm_connection_get_setting_bond (connection);
	g_object_set (G_OBJECT (s_bond),
	              NM_SETTING_BOND_INTERFACE_NAME, my_iface,
	              NULL);
	g_free (my_iface);

	(*result_func) (connection, FALSE, NULL, user_data);
}

