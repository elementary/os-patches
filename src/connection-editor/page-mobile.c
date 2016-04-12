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

#include <string.h>

#include "page-mobile.h"
#include "nm-connection-editor.h"
#include "nma-mobile-wizard.h"

G_DEFINE_TYPE (CEPageMobile, ce_page_mobile, CE_TYPE_PAGE)

#define CE_PAGE_MOBILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_MOBILE, CEPageMobilePrivate))

typedef struct {
	NMSetting *setting;

	/* Common to GSM and CDMA */
	GtkEntry *number;
	GtkEntry *username;
	GtkEntry *password;

	/* GSM only */
	GtkEntry *apn;
	GtkButton *apn_button;
	GtkEntry *network_id;
	GtkToggleButton *roaming_allowed;
	GtkEntry *pin;

	GtkWindowGroup *window_group;
	gboolean window_added;
} CEPageMobilePrivate;

#define NET_TYPE_ANY         0
#define NET_TYPE_3G          1
#define NET_TYPE_2G          2
#define NET_TYPE_PREFER_3G   3
#define NET_TYPE_PREFER_2G   4
#define NET_TYPE_PREFER_4G   5
#define NET_TYPE_4G          6

static void
mobile_private_init (CEPageMobile *self)
{
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->number = GTK_ENTRY (gtk_builder_get_object (builder, "mobile_number"));
	priv->username = GTK_ENTRY (gtk_builder_get_object (builder, "mobile_username"));
	priv->password = GTK_ENTRY (gtk_builder_get_object (builder, "mobile_password"));

	priv->apn = GTK_ENTRY (gtk_builder_get_object (builder, "mobile_apn"));
	priv->apn_button = GTK_BUTTON (gtk_builder_get_object (builder, "mobile_apn_button"));
	priv->network_id = GTK_ENTRY (gtk_builder_get_object (builder, "mobile_network_id"));
	priv->roaming_allowed = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "mobile_roaming_allowed"));

	priv->pin = GTK_ENTRY (gtk_builder_get_object (builder, "mobile_pin"));

	priv->window_group = gtk_window_group_new ();
}

static void
populate_gsm_ui (CEPageMobile *self, NMConnection *connection)
{
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);
	NMSettingGsm *setting = NM_SETTING_GSM (priv->setting);
	const char *s;

	s = nm_setting_gsm_get_number (setting);
	if (s)
		gtk_entry_set_text (priv->number, s);

	s = nm_setting_gsm_get_username (setting);
	if (s)
		gtk_entry_set_text (priv->username, s);

	s = nm_setting_gsm_get_apn (setting);
	if (s)
		gtk_entry_set_text (priv->apn, s);

	s = nm_setting_gsm_get_network_id (setting);
	if (s)
		gtk_entry_set_text (priv->network_id, s);

	gtk_toggle_button_set_active (priv->roaming_allowed,
	                              !nm_setting_gsm_get_home_only (setting));

	s = nm_setting_gsm_get_password (setting);
	if (s)
		gtk_entry_set_text (priv->password, s);

	s = nm_setting_gsm_get_pin (setting);
	if (s)
		gtk_entry_set_text (priv->pin, s);
}

static void
populate_cdma_ui (CEPageMobile *self, NMConnection *connection)
{
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);
	NMSettingCdma *setting = NM_SETTING_CDMA (priv->setting);
	const char *s;

	s = nm_setting_cdma_get_number (setting);
	if (s)
		gtk_entry_set_text (priv->number, s);

	s = nm_setting_cdma_get_username (setting);
	if (s)
		gtk_entry_set_text (priv->username, s);

	s = nm_setting_cdma_get_password (setting);
	if (s)
		gtk_entry_set_text (priv->password, s);

	/* Hide GSM specific widgets */
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (CE_PAGE (self)->builder, "mobile_basic_label")));
	gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (CE_PAGE (self)->builder, "mobile_advanced_vbox")));
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
show_passwords (GtkToggleButton *button, gpointer user_data)
{
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (user_data);
	gboolean active;

	active = gtk_toggle_button_get_active (button);

	gtk_entry_set_visibility (priv->password, active);
	gtk_entry_set_visibility (priv->pin, active);
}

static void
apn_button_mobile_wizard_done (NMAMobileWizard *wizard,
                               gboolean canceled,
                               NMAMobileWizardAccessMethod *method,
                               gpointer user_data)
{
	CEPageMobile *self = CE_PAGE_MOBILE (user_data);
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);

	if (canceled || !method) {
		nma_mobile_wizard_destroy (wizard);
		return;
	}

	if (!canceled && method) {
		switch (method->devtype) {
		case NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS:
			gtk_entry_set_text (GTK_ENTRY (priv->username),
			                    method->username ? method->username : "");
			gtk_entry_set_text (GTK_ENTRY (priv->password),
			                    method->password ? method->password : "");
			gtk_entry_set_text (GTK_ENTRY (priv->apn),
			                    method->gsm_apn ? method->gsm_apn : "");
			break;
		case NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO:
			gtk_entry_set_text (GTK_ENTRY (priv->username),
			                    method->username ? method->username : "");
			gtk_entry_set_text (GTK_ENTRY (priv->password),
			                    method->password ? method->password : "");
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}

	nma_mobile_wizard_destroy (wizard);
}

static void
apn_button_clicked (GtkButton *button, gpointer user_data)
{
	CEPageMobile *self = CE_PAGE_MOBILE (user_data);
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);
	NMAMobileWizard *wizard;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (CE_PAGE (self)->page);
	g_return_if_fail (gtk_widget_is_toplevel (toplevel));

	if (!priv->window_added) {
		gtk_window_group_add_window (priv->window_group, GTK_WINDOW (toplevel));
		priv->window_added = TRUE;
	}

	wizard = nma_mobile_wizard_new (GTK_WINDOW (toplevel),
									priv->window_group,
									NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS,
									FALSE,
									apn_button_mobile_wizard_done,
									self);
	if (wizard)
		nma_mobile_wizard_present (wizard);
}

static void
network_id_filter_cb (GtkEditable *editable,
                      gchar *text,
                      gint length,
                      gint *position,
                      gpointer user_data)
{
	utils_filter_editable_on_insert_text (editable,
	                                      text, length, position, user_data,
	                                      utils_char_is_ascii_digit,
	                                      network_id_filter_cb);
}

static void
apn_filter_cb (GtkEditable *editable,
               gchar *text,
               gint length,
               gint *position,
               gpointer user_data)
{
	utils_filter_editable_on_insert_text (editable,
	                                      text, length, position, user_data,
	                                      utils_char_is_ascii_apn,
	                                      apn_filter_cb);
}

static void
finish_setup (CEPageMobile *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);

	if (error)
		return;

	if (NM_IS_SETTING_GSM (priv->setting))
		populate_gsm_ui (self, parent->connection);
	else if (NM_IS_SETTING_CDMA (priv->setting))
		populate_cdma_ui (self, parent->connection);
	else
		g_assert_not_reached ();

	g_signal_connect (priv->number, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->username, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->password, "changed", G_CALLBACK (stuff_changed), self);

	g_signal_connect (priv->apn, "changed", G_CALLBACK (stuff_changed), self);
	gtk_entry_set_max_length (priv->apn, 64);  /* APNs are max 64 chars */
	g_signal_connect (priv->apn, "insert-text", G_CALLBACK (apn_filter_cb), self);
	g_signal_connect (priv->apn_button, "clicked", G_CALLBACK (apn_button_clicked), self);

	g_signal_connect (priv->network_id, "changed", G_CALLBACK (stuff_changed), self);
	gtk_entry_set_max_length (priv->network_id, 6);  /* MCC/MNCs are max 6 chars */
	g_signal_connect (priv->network_id, "insert-text", G_CALLBACK (network_id_filter_cb), self);

	g_signal_connect (priv->pin, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->roaming_allowed, "toggled", G_CALLBACK (stuff_changed), self);

	g_signal_connect (GTK_WIDGET (gtk_builder_get_object (parent->builder, "mobile_show_passwords")),
	                  "toggled",
	                  G_CALLBACK (show_passwords),
	                  self);
}

CEPage *
ce_page_mobile_new (NMConnectionEditor *editor,
                    NMConnection *connection,
                    GtkWindow *parent_window,
                    NMClient *client,
                    const char **out_secrets_setting_name,
                    GError **error)
{
	CEPageMobile *self;
	CEPageMobilePrivate *priv;

	self = CE_PAGE_MOBILE (ce_page_new (CE_TYPE_PAGE_MOBILE,
	                                    editor,
	                                    connection,
	                                    parent_window,
	                                    client,
	                                    UIDIR "/ce-page-mobile.ui",
	                                    "MobilePage",
	                                    _("Mobile Broadband")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load mobile broadband user interface."));
		return NULL;
	}

	mobile_private_init (self);
	priv = CE_PAGE_MOBILE_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting (connection, NM_TYPE_SETTING_GSM);
	if (priv->setting)
		*out_secrets_setting_name = NM_SETTING_GSM_SETTING_NAME;
	else {
		priv->setting = nm_connection_get_setting (connection, NM_TYPE_SETTING_CDMA);
		if (priv->setting)
			*out_secrets_setting_name = NM_SETTING_CDMA_SETTING_NAME;
	}

	if (!priv->setting) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, "%s", _("Unsupported mobile broadband connection type."));
		g_object_unref (self);
		return NULL;
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static const char *
nm_entry_get_text (GtkEntry *entry)
{
	const char *txt;

	txt = gtk_entry_get_text (entry);
	if (txt && strlen (txt) > 0)
		return txt;

	return NULL;
}

static void
gsm_ui_to_setting (CEPageMobile *self)
{
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);
	gboolean roaming_allowed;

	roaming_allowed = gtk_toggle_button_get_active (priv->roaming_allowed);

	g_object_set (priv->setting,
	              NM_SETTING_GSM_NUMBER,       nm_entry_get_text (priv->number),
	              NM_SETTING_GSM_USERNAME,     nm_entry_get_text (priv->username),
	              NM_SETTING_GSM_PASSWORD,     nm_entry_get_text (priv->password),
	              NM_SETTING_GSM_APN,          nm_entry_get_text (priv->apn),
	              NM_SETTING_GSM_NETWORK_ID,   nm_entry_get_text (priv->network_id),
	              NM_SETTING_GSM_PIN,          nm_entry_get_text (priv->pin),
	              NM_SETTING_GSM_HOME_ONLY,    !roaming_allowed,
	              NULL);
}

static void
cdma_ui_to_setting (CEPageMobile *self)
{
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);

	g_object_set (priv->setting,
				  NM_SETTING_CDMA_NUMBER,   nm_entry_get_text (priv->number),
				  NM_SETTING_CDMA_USERNAME, nm_entry_get_text (priv->username),
				  NM_SETTING_CDMA_PASSWORD, nm_entry_get_text (priv->password),
				  NULL);
}

static void
ui_to_setting (CEPageMobile *self)
{
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);

	if (NM_IS_SETTING_GSM (priv->setting))
		gsm_ui_to_setting (self);
	else if (NM_IS_SETTING_CDMA (priv->setting))
		cdma_ui_to_setting (self);
	else
		g_error ("Invalid setting");
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageMobile *self = CE_PAGE_MOBILE (page);
	CEPageMobilePrivate *priv = CE_PAGE_MOBILE_GET_PRIVATE (self);

	ui_to_setting (self);
	return nm_setting_verify (priv->setting, NULL, error);
}

static void
dispose (GObject *object)
{
	g_clear_object (&CE_PAGE_MOBILE_GET_PRIVATE (object)->window_group);

	G_OBJECT_CLASS (ce_page_mobile_parent_class)->dispose (object);
}

static void
ce_page_mobile_init (CEPageMobile *self)
{
}

static void
ce_page_mobile_class_init (CEPageMobileClass *mobile_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (mobile_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (mobile_class);

	g_type_class_add_private (object_class, sizeof (CEPageMobilePrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
	object_class->dispose = dispose;
}

typedef struct {
    NMClient *client;
    PageNewConnectionResultFunc result_func;
    gpointer user_data;
} WizardInfo;

static void
new_connection_mobile_wizard_done (NMAMobileWizard *wizard,
                                   gboolean canceled,
                                   NMAMobileWizardAccessMethod *method,
                                   gpointer user_data)
{
	WizardInfo *info = user_data;
	NMConnection *connection = NULL;

	if (!canceled && method) {
		NMSetting *type_setting;
		const char *ctype = NULL;
		char *detail = NULL;

		switch (method->devtype) {
		case NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS:
			ctype = NM_SETTING_GSM_SETTING_NAME;
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
			ctype = NM_SETTING_CDMA_SETTING_NAME;
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
		connection = ce_page_new_connection (detail, ctype, FALSE, info->client, info->user_data);
		g_free (detail);

		nm_connection_add_setting (connection, type_setting);
		nm_connection_add_setting (connection, nm_setting_ppp_new ());
	}

	(*info->result_func) (connection, canceled, NULL, info->user_data);

	if (wizard)
		nma_mobile_wizard_destroy (wizard);

	g_object_unref (info->client);
	g_free (info);
}

static void
cancel_dialog (GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
}

void
mobile_connection_new (GtkWindow *parent,
                       const char *detail,
                       NMClient *client,
                       PageNewConnectionResultFunc result_func,
                       gpointer user_data)
{
	NMAMobileWizard *wizard;
	WizardInfo *info;
	GtkWidget *dialog, *vbox, *gsm_radio, *cdma_radio, *label, *content, *alignment;
	GtkWidget *hbox, *image;
	gint response;
	NMAMobileWizardAccessMethod method;

	info = g_malloc0 (sizeof (WizardInfo));
	info->result_func = result_func;
	info->client = g_object_ref (client);
	info->user_data = user_data;

	wizard = nma_mobile_wizard_new (parent, NULL, NM_DEVICE_MODEM_CAPABILITY_NONE, FALSE,
									new_connection_mobile_wizard_done, info);
	if (wizard) {
		nma_mobile_wizard_present (wizard);
		return;
	}

	/* Fall back to just asking for GSM vs. CDMA */
	dialog = gtk_dialog_new_with_buttons (_("Select Mobile Broadband Provider Type"),
	                                      parent,
	                                      GTK_DIALOG_MODAL,
	                                      GTK_STOCK_CANCEL,
	                                      GTK_RESPONSE_CANCEL,
	                                      GTK_STOCK_OK,
	                                      GTK_RESPONSE_OK,
	                                      NULL);
	g_signal_connect (dialog, "delete-event", G_CALLBACK (cancel_dialog), NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "nm-device-wwan");

	content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	alignment = gtk_alignment_new (0, 0, 0.5, 0.5);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 12, 12, 12);
	gtk_box_pack_start (GTK_BOX (content), alignment, TRUE, FALSE, 6);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);

	image = gtk_image_new_from_icon_name ("nm-device-wwan", GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0);
	gtk_misc_set_padding (GTK_MISC (image), 0, 6);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 6);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, FALSE, 0);

	label = gtk_label_new (_("Select the technology your mobile broadband provider uses.  If you are unsure, ask your provider."));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 12);

	gsm_radio = gtk_radio_button_new_with_mnemonic (NULL, _("My provider uses _GSM-based technology (i.e. GPRS, EDGE, UMTS, HSDPA)"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gsm_radio), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), gsm_radio, FALSE, FALSE, 6);

	/* Translators: CDMA has 'D' accelerator key; 'C' collides with 'Cancel' button.
	                You may need to change it according to your language. */
	cdma_radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (gsm_radio),
                                           _("My provider uses C_DMA-based technology (i.e. 1xRTT, EVDO)"));
	gtk_box_pack_start (GTK_BOX (vbox), cdma_radio, FALSE, FALSE, 6);

	gtk_widget_show_all (dialog);

	memset (&method, 0, sizeof (method));
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response == GTK_RESPONSE_OK) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cdma_radio))) {
			method.devtype = NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO;
			method.provider_name = _("CDMA");
		} else {
			method.devtype = NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS;
			method.provider_name = _("GSM");
		}
	}
	gtk_widget_destroy (dialog);

	new_connection_mobile_wizard_done (NULL,
	                                   (response != GTK_RESPONSE_OK),
	                                   (response == GTK_RESPONSE_OK) ? &method : NULL,
	                                   info);
}

