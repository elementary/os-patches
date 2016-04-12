/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Lubomir Rintel <lkundrak@v3.sk>
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
 * Copyright 2014 - 2015 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>

#include <NetworkManager.h>

#include "page-bluetooth.h"
#include "nm-connection-editor.h"
#include "nma-mobile-wizard.h"

G_DEFINE_TYPE (CEPageBluetooth, ce_page_bluetooth, CE_TYPE_PAGE)

#define CE_PAGE_BLUETOOTH_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_BLUETOOTH, CEPageBluetoothPrivate))

typedef struct {
	NMSettingBluetooth *setting;

	GtkComboBoxText *bdaddr;

	gboolean disposed;
} CEPageBluetoothPrivate;

static void
bluetooth_private_init (CEPageBluetooth *self)
{
	CEPageBluetoothPrivate *priv = CE_PAGE_BLUETOOTH_GET_PRIVATE (self);
	GtkBuilder *builder;
	GtkWidget *vbox;
	GtkLabel *label;

	builder = CE_PAGE (self)->builder;

	priv->bdaddr = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->bdaddr), 0);
	gtk_widget_set_tooltip_text (GTK_WIDGET (priv->bdaddr),
	                             _("MAC address of the Bluetooth device. Example: 00:11:22:33:44:55"));

	vbox = GTK_WIDGET (gtk_builder_get_object (builder, "bluetooth_device_vbox"));
	gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (priv->bdaddr));
	gtk_widget_set_halign (GTK_WIDGET (priv->bdaddr), GTK_ALIGN_FILL);
	gtk_widget_show_all (GTK_WIDGET (priv->bdaddr));

	/* Set mnemonic widget for Device label */
	label = GTK_LABEL (gtk_builder_get_object (builder, "bluetooth_device_label"));
	gtk_label_set_mnemonic_widget (label, GTK_WIDGET (priv->bdaddr));

}

static void
populate_ui (CEPageBluetooth *self, NMConnection *connection)
{
	CEPageBluetoothPrivate *priv = CE_PAGE_BLUETOOTH_GET_PRIVATE (self);
	NMSettingBluetooth *setting = priv->setting;
	const char *bdaddr;

	bdaddr = nm_setting_bluetooth_get_bdaddr (setting);
	ce_page_setup_device_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->bdaddr),
	                            NM_TYPE_DEVICE_BT, NULL,
	                            bdaddr, NM_DEVICE_BT_HW_ADDRESS, TRUE);
	g_signal_connect_swapped (priv->bdaddr, "changed", G_CALLBACK (ce_page_changed), self);
}

static void
stuff_changed (GtkEditable *editable, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
finish_setup (CEPageBluetooth *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPageBluetoothPrivate *priv = CE_PAGE_BLUETOOTH_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self, parent->connection);

	g_signal_connect (priv->bdaddr, "changed", G_CALLBACK (stuff_changed), self);
}

CEPage *
ce_page_bluetooth_new (NMConnectionEditor *editor,
                       NMConnection *connection,
                       GtkWindow *parent_window,
                       NMClient *client,
                       const char **out_secrets_setting_name,
                       GError **error)
{
	CEPageBluetooth *self;
	CEPageBluetoothPrivate *priv;

	self = CE_PAGE_BLUETOOTH (ce_page_new (CE_TYPE_PAGE_BLUETOOTH,
	                          editor,
	                          connection,
	                          parent_window,
	                          client,
	                          UIDIR "/ce-page-bluetooth.ui",
	                          "BluetoothPage",
	                          _("Bluetooth")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load Bluetooth user interface."));
		return NULL;
	}

	bluetooth_private_init (self);
	priv = CE_PAGE_BLUETOOTH_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_bluetooth (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_BLUETOOTH (nm_setting_bluetooth_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	*out_secrets_setting_name = NM_SETTING_BLUETOOTH_SETTING_NAME;

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageBluetooth *self)
{
	CEPageBluetoothPrivate *priv = CE_PAGE_BLUETOOTH_GET_PRIVATE (self);
	GtkWidget *entry;
	char *bdaddr = NULL;

	entry = gtk_bin_get_child (GTK_BIN (priv->bdaddr));
	if (entry)
		ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, FALSE, NULL, &bdaddr, NULL, NULL);

	g_object_set (priv->setting,
	              NM_SETTING_BLUETOOTH_BDADDR, bdaddr && *bdaddr ? bdaddr : NULL,
	              NULL);
	g_free (bdaddr);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageBluetooth *self = CE_PAGE_BLUETOOTH (page);
	CEPageBluetoothPrivate *priv = CE_PAGE_BLUETOOTH_GET_PRIVATE (self);
	GtkWidget *entry;
	char *bdaddr = NULL;

	entry = gtk_bin_get_child (GTK_BIN (priv->bdaddr));
	if (entry) {
		ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, FALSE, NULL, &bdaddr, NULL, NULL);
		if (!bdaddr || !nm_utils_hwaddr_valid (bdaddr, nm_utils_hwaddr_len (ARPHRD_ETHER))) {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
			             _("invalid Bluetooth device (%s)"),
			             bdaddr ? bdaddr : "null");
			g_free (bdaddr);
			return FALSE;
		}
		g_free (bdaddr);
	}

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_bluetooth_init (CEPageBluetooth *self)
{
}

static void
ce_page_bluetooth_class_init (CEPageBluetoothClass *bluetooth_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (bluetooth_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (bluetooth_class);

	g_type_class_add_private (object_class, sizeof (CEPageBluetoothPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}

typedef struct {
	NMClient *client;
	PageNewConnectionResultFunc result_func;
	gpointer user_data;
	const gchar *type;
} WizardInfo;

static void
new_connection_mobile_wizard_done (NMAMobileWizard *wizard,
                                   gboolean canceled,
                                   NMAMobileWizardAccessMethod *method,
                                   gpointer user_data)
{
	WizardInfo *info = user_data;
	NMConnection *connection = NULL;
	char *detail = NULL;
	NMSetting *type_setting = NULL;

	if (canceled)
		goto out;

	if (method) {
		switch (method->devtype) {
		case NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS:
			type_setting = nm_setting_gsm_new ();
			/* De-facto standard for GSM */
			g_object_set (type_setting,
			              NM_SETTING_GSM_NUMBER, "*99#",
			              NM_SETTING_GSM_USERNAME, method->username,
			              NM_SETTING_GSM_PASSWORD, method->password,
			              NM_SETTING_GSM_APN, method->gsm_apn,
			              NULL);
			break;
		case NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO:
			type_setting = nm_setting_cdma_new ();
			/* De-facto standard for CDMA */
			g_object_set (type_setting,
			              NM_SETTING_CDMA_NUMBER, "#777",
			              NM_SETTING_GSM_USERNAME, method->username,
			              NM_SETTING_GSM_PASSWORD, method->password,
			              NULL);
			break;
		default:
			g_assert_not_reached ();
			break;
		}

		if (method->plan_name)
			detail = g_strdup_printf ("%s %s %%d", method->provider_name, method->plan_name);
		else
			detail = g_strdup_printf ("%s connection %%d", method->provider_name);
	}

	if (!detail)
		detail = g_strdup (_("Bluetooth connection %d"));
	connection = ce_page_new_connection (detail,
	                                     NM_SETTING_BLUETOOTH_SETTING_NAME,
	                                     FALSE,
	                                     info->client,
	                                     user_data);
	g_free (detail);
	nm_connection_add_setting (connection, nm_setting_bluetooth_new ());
	g_object_set (nm_connection_get_setting_bluetooth (connection),
	              NM_SETTING_BLUETOOTH_TYPE, info->type, NULL);

	if (type_setting) {
		nm_connection_add_setting (connection, type_setting);
		nm_connection_add_setting (connection, nm_setting_ppp_new ());
	}

out:
	(*info->result_func) (connection, canceled, NULL, info->user_data);

	if (wizard)
		nma_mobile_wizard_destroy (wizard);

	g_object_unref (info->client);
	g_free (info);
}

void
bluetooth_connection_new (GtkWindow *parent,
                          const char *detail,
                          NMClient *client,
                          PageNewConnectionResultFunc result_func,
                          gpointer user_data)
{
	gint response;
	NMAMobileWizard *wizard = NULL;
	WizardInfo *info;
	GtkWidget *dialog, *content, *alignment, *vbox, *label, *dun_radio, *panu_radio;

	info = g_malloc0 (sizeof (WizardInfo));
	info->result_func = result_func;
	info->client = g_object_ref (client);
	info->user_data = user_data;
	info->type = NM_SETTING_BLUETOOTH_TYPE_PANU;

	dialog = gtk_dialog_new_with_buttons (_("Bluetooth Type"),
	                                      parent,
	                                      GTK_DIALOG_MODAL,
	                                      GTK_STOCK_CANCEL,
	                                      GTK_RESPONSE_CANCEL,
	                                      GTK_STOCK_OK,
	                                      GTK_RESPONSE_OK,
	                                      NULL);

	content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	alignment = gtk_alignment_new (0, 0, 0.5, 0.5);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 12, 12, 12);
	gtk_box_pack_start (GTK_BOX (content), alignment, TRUE, FALSE, 6);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);

	label = gtk_label_new (_("Select the type of the Bluetooth connection profile."));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 12);

	panu_radio = gtk_radio_button_new_with_mnemonic (NULL, _("_Personal Area Network"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (panu_radio), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), panu_radio, FALSE, FALSE, 6);

	dun_radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (panu_radio),
	                                                            _("_Dial-Up Network"));
	gtk_box_pack_start (GTK_BOX (vbox), dun_radio, FALSE, FALSE, 6);

	gtk_widget_show_all (dialog);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response == GTK_RESPONSE_OK) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dun_radio))) {
			info->type = NM_SETTING_BLUETOOTH_TYPE_DUN;
			wizard = nma_mobile_wizard_new (parent, NULL, NM_DEVICE_MODEM_CAPABILITY_NONE, FALSE,
			                                new_connection_mobile_wizard_done, info);
		} else {
			info->type = NM_SETTING_BLUETOOTH_TYPE_PANU;
		}
	}
	gtk_widget_destroy (dialog);

	if (wizard)
		nma_mobile_wizard_present (wizard);
	else
		new_connection_mobile_wizard_done (NULL, (response != GTK_RESPONSE_OK), NULL, info);
}
