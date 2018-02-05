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
 * Copyright 2017 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>

#include "page-macsec.h"
#include "nm-connection-editor.h"
#include "nma-ui-utils.h"

G_DEFINE_TYPE (CEPageMacsec, ce_page_macsec, CE_TYPE_PAGE)

#define CE_PAGE_MACSEC_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_MACSEC, CEPageMacsecPrivate))

typedef struct {
	NMSettingMacsec *setting;

	GtkEntry *name;
	GtkComboBoxText *parent;
	GtkComboBox *mode;
	GtkEntry *cak;
	GtkEntry *ckn;
	GtkToggleButton *encryption;
	GtkComboBox *validation;
	GtkSpinButton *sci_port;
} CEPageMacsecPrivate;

static void
macsec_private_init (CEPageMacsec *self)
{
	CEPageMacsecPrivate *priv = CE_PAGE_MACSEC_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->name = GTK_ENTRY (gtk_builder_get_object (builder, "macsec_name"));
	priv->parent = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder, "macsec_parent"));
	priv->mode = GTK_COMBO_BOX (gtk_builder_get_object (builder, "macsec_mode"));
	priv->cak = GTK_ENTRY (gtk_builder_get_object (builder, "macsec_cak"));
	priv->ckn = GTK_ENTRY (gtk_builder_get_object (builder, "macsec_ckn"));
	priv->encryption = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "macsec_encryption"));
	priv->validation = GTK_COMBO_BOX (gtk_builder_get_object (builder, "macsec_validation"));
	priv->sci_port = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "macsec_sci_port"));
}

static void
mode_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageMacsec *self = user_data;
	CEPageMacsecPrivate *priv = CE_PAGE_MACSEC_GET_PRIVATE (self);
	NMSettingMacsecMode mode;
	NMConnection *connection;
	gboolean mode_psk;

	mode = gtk_combo_box_get_active (combo);

	mode_psk = mode == NM_SETTING_MACSEC_MODE_PSK;

	gtk_widget_set_sensitive (GTK_WIDGET (priv->cak), mode_psk);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->ckn), mode_psk);

	if (!mode_psk) {
		gtk_entry_set_text (priv->cak, "");
		gtk_entry_set_text (priv->ckn, "");

		connection = CE_PAGE (self)->connection;
		if (!nm_connection_get_setting_802_1x (connection))
			nm_connection_add_setting (connection, nm_setting_802_1x_new ());
	}

	nm_connection_editor_inter_page_set_value (CE_PAGE (self)->editor,
	                                           INTER_PAGE_CHANGE_MACSEC_MODE,
	                                           GUINT_TO_POINTER (mode));
	ce_page_changed (CE_PAGE (user_data));
}

static void
populate_ui (CEPageMacsec *self, NMConnection *connection)
{
	CEPageMacsecPrivate *priv = CE_PAGE_MACSEC_GET_PRIVATE (self);
	NMSettingMacsec *setting = priv->setting;
	NMSettingMacsecMode mode;
	NMSettingMacsecValidation validation;
	const char *cak = "", *ckn = "", *str;

	str = nm_connection_get_interface_name (CE_PAGE (self)->connection);
	if (str)
		gtk_entry_set_text (priv->name, str);

	str = nm_setting_macsec_get_parent (setting);
	ce_page_setup_device_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->parent),
	                            G_TYPE_NONE, str,
	                            NULL, NULL);

	mode = nm_setting_macsec_get_mode (setting);
	if (mode >= NM_SETTING_MACSEC_MODE_PSK && mode <= NM_SETTING_MACSEC_MODE_EAP)
		gtk_combo_box_set_active (priv->mode, mode);

	if (mode == NM_SETTING_MACSEC_MODE_PSK) {
		cak = nm_setting_macsec_get_mka_cak (setting);
		ckn = nm_setting_macsec_get_mka_ckn (setting);
	}

	gtk_entry_set_text (priv->cak, cak ?: "");
	gtk_entry_set_text (priv->ckn, ckn ?: "");

	nma_utils_setup_password_storage ((GtkWidget *) priv->cak, 0,
	                                  (NMSetting *) priv->setting,
	                                  NM_SETTING_MACSEC_MKA_CAK,
	                                  FALSE, FALSE);

	gtk_toggle_button_set_active (priv->encryption,
	                              nm_setting_macsec_get_encrypt (setting));

	validation = nm_setting_macsec_get_validation (setting);
	if (   validation >= NM_SETTING_MACSEC_VALIDATION_DISABLE
	    && validation <= NM_SETTING_MACSEC_VALIDATION_STRICT)
		gtk_combo_box_set_active (priv->validation, validation);

	gtk_spin_button_set_value (priv->sci_port, nm_setting_macsec_get_port (setting));

	mode_changed (priv->mode, self);
}

static void
stuff_changed (GtkEditable *editable, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
finish_setup (CEPageMacsec *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPageMacsecPrivate *priv = CE_PAGE_MACSEC_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self, parent->connection);

	g_signal_connect (priv->name, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->parent, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->mode, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->cak, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->ckn, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->sci_port, "value-changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->validation, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->encryption, "toggled", G_CALLBACK (stuff_changed), self);

	g_signal_connect (priv->mode, "changed", G_CALLBACK (mode_changed), self);
}

CEPage *
ce_page_macsec_new (NMConnectionEditor *editor,
                    NMConnection *connection,
                    GtkWindow *parent_window,
                    NMClient *client,
                    const char **out_secrets_setting_name,
                    GError **error)
{
	CEPageMacsec *self;
	CEPageMacsecPrivate *priv;

	self = CE_PAGE_MACSEC (ce_page_new (CE_TYPE_PAGE_MACSEC,
	                                    editor,
	                                    connection,
	                                    parent_window,
	                                    client,
	                                    "/org/freedesktop/network-manager-applet/ce-page-macsec.ui",
	                                    "MacsecPage",
	                                   _("MACsec")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load MACsec user interface."));
		return NULL;
	}

	macsec_private_init (self);
	priv = CE_PAGE_MACSEC_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_macsec (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_MACSEC (nm_setting_macsec_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	*out_secrets_setting_name = NM_SETTING_MACSEC_SETTING_NAME;

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageMacsec *self)
{
	CEPageMacsecPrivate *priv = CE_PAGE_MACSEC_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	const char *parent = NULL;
	const char *cak = NULL;
	const char *ckn = NULL;
	NMSettingMacsecMode mode;
	gboolean encryption;
	NMSettingMacsecValidation validation;
	gint sci_port;
	GtkWidget *entry;
	NMSettingSecretFlags secret_flags;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_return_if_fail (s_con != NULL);
	g_object_set (s_con,
	              NM_SETTING_CONNECTION_INTERFACE_NAME, gtk_entry_get_text (priv->name),
	              NULL);

	entry = gtk_bin_get_child (GTK_BIN (priv->parent));
	if (entry) {
		ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE,
		                          (char **) &parent, NULL, NULL, NULL);
	}

	mode = gtk_combo_box_get_active (priv->mode);

	if (mode == NM_SETTING_MACSEC_MODE_PSK) {
		cak = gtk_entry_get_text (priv->cak);
		ckn = gtk_entry_get_text (priv->ckn);
	}

	encryption = gtk_toggle_button_get_active (priv->encryption);
	validation = gtk_combo_box_get_active (priv->validation);
	sci_port = gtk_spin_button_get_value_as_int (priv->sci_port);

	g_object_set (priv->setting,
	              NM_SETTING_MACSEC_PARENT, parent,
	              NM_SETTING_MACSEC_MODE, mode,
	              NM_SETTING_MACSEC_MKA_CAK, cak,
	              NM_SETTING_MACSEC_MKA_CKN, ckn,
	              NM_SETTING_MACSEC_ENCRYPT, encryption,
	              NM_SETTING_MACSEC_VALIDATION, validation,
	              NM_SETTING_MACSEC_PORT, sci_port,
	              NULL);

	/* Save CAK flags to the connection */
	secret_flags = nma_utils_menu_to_secret_flags ((GtkWidget *) priv->cak);
	nm_setting_set_secret_flags (NM_SETTING (priv->setting), NM_SETTING_MACSEC_MKA_CAK,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	nma_utils_update_password_storage ((GtkWidget *) priv->cak, secret_flags,
	                                   NM_SETTING (priv->setting), NM_SETTING_MACSEC_MKA_CAK);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageMacsec *self = CE_PAGE_MACSEC (page);
	CEPageMacsecPrivate *priv = CE_PAGE_MACSEC_GET_PRIVATE (self);

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), connection, error);
}

static gboolean
inter_page_change (CEPage *page)
{
	CEPageMacsecPrivate *priv = CE_PAGE_MACSEC_GET_PRIVATE (page);
	gpointer enable;

	if (nm_connection_editor_inter_page_get_value (page->editor,
	                                               INTER_PAGE_CHANGE_802_1X_ENABLE,
	                                               &enable)) {
		gtk_combo_box_set_active (priv->mode,
		                          GPOINTER_TO_INT (enable) ?
		                              NM_SETTING_MACSEC_MODE_EAP :
		                              NM_SETTING_MACSEC_MODE_PSK);
		ce_page_changed (page);
	}

	return TRUE;
}

static void
ce_page_macsec_init (CEPageMacsec *self)
{
}

static void
ce_page_macsec_class_init (CEPageMacsecClass *macsec_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (macsec_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (macsec_class);

	g_type_class_add_private (object_class, sizeof (CEPageMacsecPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
	parent_class->inter_page_change = inter_page_change;
}

void
macsec_connection_new (FUNC_TAG_PAGE_NEW_CONNECTION_IMPL,
                       GtkWindow *parent,
                       const char *detail,
                       gpointer detail_data,
                       NMConnection *connection,
                       NMClient *client,
                       PageNewConnectionResultFunc result_func,
                       gpointer user_data)
{
	gs_unref_object NMConnection *connection_tmp = NULL;

	connection = _ensure_connection_other (connection, &connection_tmp);
	ce_page_complete_connection (connection,
	                             _("MACSEC connection %d"),
	                             NM_SETTING_MACSEC_SETTING_NAME,
	                             FALSE,
	                             client);
	nm_connection_add_setting (connection, nm_setting_macsec_new ());
	nm_connection_add_setting (connection, nm_setting_wired_new ());

	(*result_func) (FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_CALL, connection, FALSE, NULL, user_data);
}
