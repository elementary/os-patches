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
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#include "nm-default.h"

#include <stdlib.h>

#include <NetworkManager.h>
#include <nm-setting-gsm.h>
#include <nm-setting-cdma.h>
#include <nm-client.h>
#include <nm-device-modem.h>

#include "nm-mobile-wizard.h"
#include "nm-mobile-providers.h"
#include "nm-ui-utils.h"
#include "utils.h"

#define DEVICE_TAG "device"
#define TYPE_TAG "setting-type"

static NMACountryInfo *get_selected_country (NMAMobileWizard *self);
static NMAMobileProvider *get_selected_provider (NMAMobileWizard *self);
static NMAMobileFamily get_provider_unlisted_type (NMAMobileWizard *self);
static NMAMobileAccessMethod *get_selected_method (NMAMobileWizard *self, gboolean *manual);

struct NMAMobileWizard {
	GtkWidget *assistant;
	NMAMobileWizardCallback callback;
	gpointer user_data;
	NMAMobileProvidersDatabase *mobile_providers_database;
	NMAMobileFamily family;
	gboolean initial_family;
	gboolean will_connect_after;

	/* Intro page */
	GtkWidget *dev_combo;
	GtkTreeStore *dev_store;
	char *dev_desc;
	NMClient *client;

	/* Country page */
	guint32 country_idx;
	NMACountryInfo *country;
	GtkWidget *country_page;
	GtkWidget *country_view;
	GtkTreeStore *country_store;
	GtkTreeModelSort *country_sort;
	guint32 country_focus_id;

	/* Providers page */
	guint32 providers_idx;
	GtkWidget *providers_page;
	GtkWidget *providers_view;
	GtkTreeStore *providers_store;
	GtkTreeModelSort *providers_sort;
	guint32 providers_focus_id;
	GtkWidget *providers_view_radio;

	GtkWidget *provider_unlisted_radio;
	GtkWidget *provider_unlisted_entry;
	GtkWidget *provider_unlisted_type_combo;

	gboolean provider_only_cdma;

	/* Plan page */
	guint32 plan_idx;
	GtkWidget *plan_page;
	GtkWidget *plan_combo;
	GtkTreeStore *plan_store;
	guint32 plan_focus_id;

	GtkWidget *plan_unlisted_entry;

	/* Confirm page */
	GtkWidget *confirm_page;
	GtkWidget *confirm_provider;
	GtkWidget *confirm_plan;
	GtkWidget *confirm_apn;
	GtkWidget *confirm_plan_label;
	GtkWidget *confirm_device;
	GtkWidget *confirm_device_label;
	guint32 confirm_idx;
};

static void
assistant_closed (GtkButton *button, gpointer user_data)
{
	NMAMobileWizard *self = user_data;
	NMAMobileProvider *provider;
	NMAMobileAccessMethod *method;
	NMAMobileWizardAccessMethod *wiz_method;
	NMAMobileFamily family = self->family;

	wiz_method = g_malloc0 (sizeof (NMAMobileWizardAccessMethod));

	provider = get_selected_provider (self);
	if (!provider) {
		if (family == NMA_MOBILE_FAMILY_UNKNOWN)
			family = get_provider_unlisted_type (self);

		wiz_method->provider_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->provider_unlisted_entry)));
		if (family == NMA_MOBILE_FAMILY_3GPP)
			wiz_method->gsm_apn = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->plan_unlisted_entry)));
	} else {
		gboolean manual = FALSE;

		wiz_method->provider_name = g_strdup (nma_mobile_provider_get_name (provider));
		method = get_selected_method (self, &manual);
		if (method) {
			family = nma_mobile_access_method_get_family (method);
			wiz_method->plan_name = g_strdup (nma_mobile_access_method_get_name (method));
			wiz_method->username = g_strdup (nma_mobile_access_method_get_username (method));
			wiz_method->password = g_strdup (nma_mobile_access_method_get_password (method));
			if (family == NMA_MOBILE_FAMILY_3GPP)
				wiz_method->gsm_apn = g_strdup (nma_mobile_access_method_get_3gpp_apn (method));
		} else {
			if (self->provider_only_cdma) {
				GSList *methods;

				family = NMA_MOBILE_FAMILY_CDMA;

				methods = nma_mobile_provider_get_methods (provider);
				/* Take username and password from the first (only) method for CDMA only provider */
				if (methods) {
					method = methods->data;
					wiz_method->username = g_strdup (nma_mobile_access_method_get_username (method));
					wiz_method->password = g_strdup (nma_mobile_access_method_get_password (method));
				}
			} else {
				family = NMA_MOBILE_FAMILY_3GPP;
				wiz_method->gsm_apn = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->plan_unlisted_entry)));
			}
		}
	}

	switch (family) {
	case NMA_MOBILE_FAMILY_3GPP:
		wiz_method->devtype = NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS;
		break;
	case NMA_MOBILE_FAMILY_CDMA:
		wiz_method->devtype = NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	(*(self->callback)) (self, FALSE, wiz_method, self->user_data);

	if (provider)
		nma_mobile_provider_unref (provider);
	g_free (wiz_method->provider_name);
	g_free (wiz_method->plan_name);
	g_free (wiz_method->username);
	g_free (wiz_method->password);
	g_free (wiz_method->gsm_apn);
	g_free (wiz_method);
}

static void
assistant_cancel (GtkButton *button, gpointer user_data)
{
	NMAMobileWizard *self = user_data;

	(*(self->callback)) (self, TRUE, NULL, self->user_data);
}

/**********************************************************/
/* Confirm page */
/**********************************************************/

static void
confirm_setup (NMAMobileWizard *self)
{
	GtkWidget *vbox, *label, *alignment, *pbox;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	label = gtk_label_new (_("Your mobile broadband connection is configured with the following settings:"));
	gtk_widget_set_size_request (label, 500, -1);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 6);

	/* Device */
	self->confirm_device_label = gtk_label_new (_("Your Device:"));
	gtk_misc_set_alignment (GTK_MISC (self->confirm_device_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), self->confirm_device_label, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 12, 25, 0);
	self->confirm_device = gtk_label_new (NULL);
	gtk_container_add (GTK_CONTAINER (alignment), self->confirm_device);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	/* Provider */
	label = gtk_label_new (_("Your Provider:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 12, 25, 0);
	self->confirm_provider = gtk_label_new (NULL);
	gtk_container_add (GTK_CONTAINER (alignment), self->confirm_provider);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	/* Plan and APN */
	self->confirm_plan_label = gtk_label_new (_("Your Plan:"));
	gtk_misc_set_alignment (GTK_MISC (self->confirm_plan_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), self->confirm_plan_label, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 25, 0);
	pbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (alignment), pbox);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	self->confirm_plan = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (self->confirm_plan), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (pbox), self->confirm_plan, FALSE, FALSE, 0);

	self->confirm_apn = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (self->confirm_apn), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (self->confirm_apn), 0, 6);
	gtk_box_pack_start (GTK_BOX (pbox), self->confirm_apn, FALSE, FALSE, 0);

	if (self->will_connect_after) {
		alignment = gtk_alignment_new (0, 0.5, 1, 0);
		label = gtk_label_new (_("A connection will now be made to your mobile broadband provider using the settings you selected.  If the connection fails or you cannot access network resources, double-check your settings.  To modify your mobile broadband connection settings, choose \"Network Connections\" from the System >> Preferences menu."));
		gtk_widget_set_size_request (label, 500, -1);
		gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
		gtk_misc_set_padding (GTK_MISC (label), 0, 6);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
		gtk_container_add (GTK_CONTAINER (alignment), label);
		gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 6);
	}

	gtk_widget_show_all (vbox);
	self->confirm_idx = gtk_assistant_append_page (GTK_ASSISTANT (self->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (self->assistant),
	                              vbox, _("Confirm Mobile Broadband Settings"));

	gtk_assistant_set_page_complete (GTK_ASSISTANT (self->assistant), vbox, TRUE);
	gtk_assistant_set_page_type (GTK_ASSISTANT (self->assistant), vbox, GTK_ASSISTANT_PAGE_CONFIRM);

	self->confirm_page = vbox;
}

static void
confirm_prepare (NMAMobileWizard *self)
{
	NMAMobileProvider *provider = NULL;
	NMAMobileAccessMethod *method = NULL;
	NMACountryInfo *country_info;
	gboolean manual = FALSE;
	GString *str;

	country_info = get_selected_country (self);
	provider = get_selected_provider (self);
	if (provider)
		method = get_selected_method (self, &manual);

	/* Provider */
	str = g_string_new (NULL);
	if (provider) {
		g_string_append (str, nma_mobile_provider_get_name (provider));
		nma_mobile_provider_unref (provider);
	} else {
		const char *unlisted_provider;

		unlisted_provider = gtk_entry_get_text (GTK_ENTRY (self->provider_unlisted_entry));
		g_string_append (str, unlisted_provider);
	}

	if (country_info) {
		g_string_append_printf (str, ", %s", nma_country_info_get_country_name (country_info));
		nma_country_info_unref (country_info);
	}
	gtk_label_set_text (GTK_LABEL (self->confirm_provider), str->str);
	g_string_free (str, TRUE);

	if (self->dev_desc)
		gtk_label_set_text (GTK_LABEL (self->confirm_device), self->dev_desc);
	else {
		gtk_widget_hide (self->confirm_device_label);
		gtk_widget_hide (self->confirm_device);
	}

	if (self->provider_only_cdma) {
		gtk_widget_hide (self->confirm_plan_label);
		gtk_widget_hide (self->confirm_plan);
		gtk_widget_hide (self->confirm_apn);
	} else {
		const char *apn = NULL;

		/* Plan */
		gtk_widget_show (self->confirm_plan_label);
		gtk_widget_show (self->confirm_plan);
		gtk_widget_show (self->confirm_apn);

		if (method) {
			gtk_label_set_text (GTK_LABEL (self->confirm_plan), nma_mobile_access_method_get_name (method));
			apn = nma_mobile_access_method_get_3gpp_apn (method);
		} else {
			gtk_label_set_text (GTK_LABEL (self->confirm_plan), _("Unlisted"));
			apn = gtk_entry_get_text (GTK_ENTRY (self->plan_unlisted_entry));
		}

		str = g_string_new (NULL);
		g_string_append_printf (str, "<span color=\"#999999\">APN: %s</span>", apn);
		gtk_label_set_markup (GTK_LABEL (self->confirm_apn), str->str);
		g_string_free (str, TRUE);
	}
}

/**********************************************************/
/* Plan page */
/**********************************************************/

#define PLAN_COL_NAME 0
#define PLAN_COL_METHOD 1
#define PLAN_COL_MANUAL 2

static NMAMobileAccessMethod *
get_selected_method (NMAMobileWizard *self, gboolean *manual)
{
	GtkTreeModel *model;
	NMAMobileAccessMethod *method = NULL;
	GtkTreeIter iter;
	gboolean is_manual = FALSE;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->plan_combo), &iter))
		return NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->plan_combo));
	if (!model)
		return NULL;

	gtk_tree_model_get (model, &iter,
	                    PLAN_COL_METHOD, &method,
	                    PLAN_COL_MANUAL, &is_manual,
	                    -1);
	if (is_manual) {
		if (manual)
			*manual = is_manual;
		if (method)
			nma_mobile_access_method_unref (method);
		method = NULL;
	}

	return method;
}

static void
plan_update_complete (NMAMobileWizard *self)
{
	GtkAssistant *assistant = GTK_ASSISTANT (self->assistant);
	gboolean is_manual = FALSE;
	NMAMobileAccessMethod *method;

	method = get_selected_method (self, &is_manual);
	if (method) {
		gtk_assistant_set_page_complete (assistant, self->plan_page, TRUE);
		nma_mobile_access_method_unref (method);
	} else {
		const char *manual_apn;

		manual_apn = gtk_entry_get_text (GTK_ENTRY (self->plan_unlisted_entry));
		gtk_assistant_set_page_complete (assistant, self->plan_page,
		                                 (manual_apn && strlen (manual_apn)));
	}
}

static void
plan_combo_changed (NMAMobileWizard *self)
{
	NMAMobileAccessMethod *method = NULL;
	gboolean is_manual = FALSE;

	method = get_selected_method (self, &is_manual);
	if (method) {
		gtk_entry_set_text (GTK_ENTRY (self->plan_unlisted_entry), nma_mobile_access_method_get_3gpp_apn (method));
		gtk_widget_set_sensitive (self->plan_unlisted_entry, FALSE);
	} else {
		gtk_entry_set_text (GTK_ENTRY (self->plan_unlisted_entry), "");
		gtk_widget_set_sensitive (self->plan_unlisted_entry, TRUE);
		gtk_widget_grab_focus (self->plan_unlisted_entry);
	}

	if (method)
		nma_mobile_access_method_unref (method);

	plan_update_complete (self);
}

static gboolean
plan_row_separator_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	NMAMobileAccessMethod *method = NULL;
	gboolean is_manual = FALSE;
	gboolean draw_separator = FALSE;

	gtk_tree_model_get (model, iter,
	                    PLAN_COL_METHOD, &method,
	                    PLAN_COL_MANUAL, &is_manual,
	                    -1);
	if (!method && !is_manual)
		draw_separator = TRUE;
	if (method)
		nma_mobile_access_method_unref (method);
	return draw_separator;
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
plan_setup (NMAMobileWizard *self)
{
	GtkWidget *vbox, *label, *alignment, *hbox, *image;
	GtkCellRenderer *renderer;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	label = gtk_label_new_with_mnemonic (_("_Select your plan:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	self->plan_store = gtk_tree_store_new (3, G_TYPE_STRING, NMA_TYPE_MOBILE_ACCESS_METHOD, G_TYPE_BOOLEAN);

	self->plan_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (self->plan_store));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->plan_combo);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (self->plan_combo),
	                                      plan_row_separator_func,
	                                      NULL,
	                                      NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->plan_combo), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->plan_combo), renderer, "text", PLAN_COL_NAME);

	g_signal_connect_swapped (self->plan_combo, "changed", G_CALLBACK (plan_combo_changed), self);

	alignment = gtk_alignment_new (0, 0.5, 0.5, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 12, 0, 0);
	gtk_container_add (GTK_CONTAINER (alignment), self->plan_combo);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic (_("Selected plan _APN (Access Point Name):"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	self->plan_unlisted_entry = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->plan_unlisted_entry);
	gtk_entry_set_max_length (GTK_ENTRY (self->plan_unlisted_entry), 64);
	g_signal_connect (self->plan_unlisted_entry, "insert-text", G_CALLBACK (apn_filter_cb), self);
	g_signal_connect_swapped (self->plan_unlisted_entry, "changed", G_CALLBACK (plan_update_complete), self);

	alignment = gtk_alignment_new (0, 0.5, 0.5, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 24, 0, 0);
	gtk_container_add (GTK_CONTAINER (alignment), self->plan_unlisted_entry);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

	label = gtk_label_new (_("Warning: Selecting an incorrect plan may result in billing issues for your broadband account or may prevent connectivity.\n\nIf you are unsure of your plan please ask your provider for your plan's APN."));
	gtk_widget_set_size_request (label, 500, -1);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	self->plan_idx = gtk_assistant_append_page (GTK_ASSISTANT (self->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (self->assistant), vbox, _("Choose your Billing Plan"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self->assistant), vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_widget_show_all (vbox);

	self->plan_page = vbox;
}

static void
plan_prepare (NMAMobileWizard *self)
{
	NMAMobileProvider *provider;
	GtkTreeIter method_iter;

	gtk_tree_store_clear (self->plan_store);

	provider = get_selected_provider (self);
	if (provider) {
		GSList *iter;
		guint32 count = 0;

		for (iter = nma_mobile_provider_get_methods (provider); iter; iter = g_slist_next (iter)) {
			NMAMobileAccessMethod *method = iter->data;

			if (   (self->family != NMA_MOBILE_FAMILY_UNKNOWN)
			    && (nma_mobile_access_method_get_family (method) != self->family))
				continue;

			gtk_tree_store_append (GTK_TREE_STORE (self->plan_store), &method_iter, NULL);
			gtk_tree_store_set (GTK_TREE_STORE (self->plan_store),
			                    &method_iter,
			                    PLAN_COL_NAME,
			                    nma_mobile_access_method_get_name (method),
			                    PLAN_COL_METHOD,
			                    method,
			                    -1);
			count++;
		}
		nma_mobile_provider_unref (provider);

		/* Draw the separator */
		if (count)
			gtk_tree_store_append (GTK_TREE_STORE (self->plan_store), &method_iter, NULL);
	}

	/* Add the "My plan is not listed..." item */
	gtk_tree_store_append (GTK_TREE_STORE (self->plan_store), &method_iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (self->plan_store),
	                    &method_iter,
	                    PLAN_COL_NAME,
	                    _("My plan is not listed..."),
	                    PLAN_COL_MANUAL,
	                    TRUE,
	                    -1);

	/* Select the first item by default if nothing is yet selected */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self->plan_combo)) < 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->plan_combo), 0);

	plan_combo_changed (self);
}

/**********************************************************/
/* Providers page */
/**********************************************************/

#define PROVIDER_COL_NAME 0
#define PROVIDER_COL_PROVIDER 1

static gboolean
providers_search_func (GtkTreeModel *model,
                       gint column,
                       const char *key,
                       GtkTreeIter *iter,
                       gpointer search_data)
{
	gboolean unmatched = TRUE;
	char *provider = NULL;

	if (!key)
		return TRUE;

	gtk_tree_model_get (model, iter, column, &provider, -1);
	if (!provider)
		return TRUE;

	unmatched = !!g_ascii_strncasecmp (provider, key, strlen (key));
	g_free (provider);
	return unmatched;
}

static NMAMobileProvider *
get_selected_provider (NMAMobileWizard *self)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	NMAMobileProvider *provider = NULL;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->providers_view_radio)))
		return NULL;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->providers_view));
	g_assert (selection);

	if (!gtk_tree_selection_get_selected (GTK_TREE_SELECTION (selection), &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, PROVIDER_COL_PROVIDER, &provider, -1);
	return provider;
}

static void
providers_update_complete (NMAMobileWizard *self)
{
	GtkAssistant *assistant = GTK_ASSISTANT (self->assistant);
	gboolean use_view;

	use_view = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->providers_view_radio));
	if (use_view) {
		NMAMobileProvider *provider;

		provider = get_selected_provider (self);
		gtk_assistant_set_page_complete (assistant, self->providers_page, !!provider);
		if (provider)
			nma_mobile_provider_unref (provider);
	} else {
		const char *manual_provider;

		manual_provider = gtk_entry_get_text (GTK_ENTRY (self->provider_unlisted_entry));
		gtk_assistant_set_page_complete (assistant, self->providers_page,
		                                 (manual_provider && strlen (manual_provider)));
	}
}

static gboolean
focus_providers_view (gpointer user_data)
{
	NMAMobileWizard *self = user_data;

	self->providers_focus_id = 0;
	gtk_widget_grab_focus (self->providers_view);
	return FALSE;
}

static gboolean
focus_provider_unlisted_entry (gpointer user_data)
{
	NMAMobileWizard *self = user_data;

	self->providers_focus_id = 0;
	gtk_widget_grab_focus (self->provider_unlisted_entry);
	return FALSE;
}

static void
providers_radio_toggled (GtkToggleButton *button, gpointer user_data)
{
	NMAMobileWizard *self = user_data;
	gboolean use_view;

	use_view = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->providers_view_radio));
	if (use_view) {
		if (!self->providers_focus_id)
			self->providers_focus_id = g_idle_add (focus_providers_view, self);
		gtk_widget_set_sensitive (self->providers_view, TRUE);
		gtk_widget_set_sensitive (self->provider_unlisted_entry, FALSE);
		gtk_widget_set_sensitive (self->provider_unlisted_type_combo, FALSE);
	} else {
		if (!self->providers_focus_id)
			self->providers_focus_id = g_idle_add (focus_provider_unlisted_entry, self);
		gtk_widget_set_sensitive (self->providers_view, FALSE);
		gtk_widget_set_sensitive (self->provider_unlisted_entry, TRUE);
		gtk_widget_set_sensitive (self->provider_unlisted_type_combo, TRUE);
	}

	providers_update_complete (self);
}

static NMAMobileFamily
get_provider_unlisted_type (NMAMobileWizard *self)
{
	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (self->provider_unlisted_type_combo))) {
	case 0:
		return NMA_MOBILE_FAMILY_3GPP;
	case 1:
		return NMA_MOBILE_FAMILY_CDMA;
	default:
		return NMA_MOBILE_FAMILY_UNKNOWN;
	}
}

static void
providers_setup (NMAMobileWizard *self)
{
	GtkWidget *vbox, *scroll, *alignment, *unlisted_grid, *label;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	self->providers_view_radio = gtk_radio_button_new_with_mnemonic (NULL, _("Select your provider from a _list:"));
	g_signal_connect (self->providers_view_radio, "toggled", G_CALLBACK (providers_radio_toggled), self);
	gtk_box_pack_start (GTK_BOX (vbox), self->providers_view_radio, FALSE, TRUE, 0);

	self->providers_store = gtk_tree_store_new (2, G_TYPE_STRING, NMA_TYPE_MOBILE_PROVIDER);

	self->providers_sort = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (self->providers_store)));
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->providers_sort),
	                                      PROVIDER_COL_NAME, GTK_SORT_ASCENDING);

	self->providers_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->providers_sort));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Provider"),
	                                                   renderer,
	                                                   "text", PROVIDER_COL_NAME,
	                                                   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->providers_view), column);
	gtk_tree_view_column_set_clickable (column, TRUE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->providers_view));
	g_assert (selection);
	g_signal_connect_swapped (selection, "changed", G_CALLBACK (providers_update_complete), self);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (scroll, -1, 140);
	gtk_container_add (GTK_CONTAINER (scroll), self->providers_view);

	alignment = gtk_alignment_new (0, 0, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 12, 25, 0);
	gtk_container_add (GTK_CONTAINER (alignment), scroll);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, TRUE, TRUE, 0);

	self->provider_unlisted_radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (self->providers_view_radio),
	                                            _("I can't find my provider and I wish to enter it _manually:"));
	g_signal_connect (self->providers_view_radio, "toggled", G_CALLBACK (providers_radio_toggled), self);
	gtk_box_pack_start (GTK_BOX (vbox), self->provider_unlisted_radio, FALSE, TRUE, 0);

	alignment = gtk_alignment_new (0, 0, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 15, 0);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	unlisted_grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (unlisted_grid), 12);
	gtk_grid_set_column_spacing (GTK_GRID (unlisted_grid), 12);
	gtk_container_add (GTK_CONTAINER (alignment), unlisted_grid);

	label = gtk_label_new (_("Provider:"));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_grid_attach (GTK_GRID (unlisted_grid), label, 0, 0, 1, 1);

	self->provider_unlisted_entry = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (self->provider_unlisted_entry), 40);
	g_signal_connect_swapped (self->provider_unlisted_entry, "changed", G_CALLBACK (providers_update_complete), self);

	alignment = gtk_alignment_new (0, 0.5, 0.66, 0);
	gtk_widget_set_hexpand (alignment, TRUE);
	gtk_container_add (GTK_CONTAINER (alignment), self->provider_unlisted_entry);
	gtk_grid_attach (GTK_GRID (unlisted_grid), alignment,
	                 1, 0, 1, 1);

	self->provider_unlisted_type_combo = gtk_combo_box_text_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->provider_unlisted_type_combo);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->provider_unlisted_type_combo),
	                           _("My provider uses GSM technology (GPRS, EDGE, UMTS, HSPA)"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->provider_unlisted_type_combo),
	                           _("My provider uses CDMA technology (1xRTT, EVDO)"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (self->provider_unlisted_type_combo), 0);

	gtk_grid_attach (GTK_GRID (unlisted_grid), self->provider_unlisted_type_combo,
	                 1, 1, 1, 1);

	/* Only show the CDMA/GSM combo if we don't know the device type */
	if (self->family != NMA_MOBILE_FAMILY_UNKNOWN)
		gtk_widget_hide (self->provider_unlisted_type_combo);

	self->providers_idx = gtk_assistant_append_page (GTK_ASSISTANT (self->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (self->assistant), vbox, _("Choose your Provider"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self->assistant), vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_widget_show_all (vbox);

	self->providers_page = vbox;
}

static void
providers_prepare (NMAMobileWizard *self)
{
	GtkTreeSelection *selection;
	NMACountryInfo *country_info;
	GSList *piter;

	gtk_tree_store_clear (self->providers_store);

	country_info = get_selected_country (self);
	if (!country_info) {
		/* Unlisted country */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->provider_unlisted_radio), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (self->providers_view_radio), FALSE);
		goto done;
	}
	gtk_widget_set_sensitive (GTK_WIDGET (self->providers_view_radio), TRUE);

	for (piter = nma_country_info_get_providers (country_info);
	     piter;
	     piter = g_slist_next (piter)) {
		NMAMobileProvider *provider = piter->data;
		GtkTreeIter provider_iter;

		/* Ignore providers that don't match the current device type */
		if (self->family != NMA_MOBILE_FAMILY_UNKNOWN) {
			GSList *miter;
			guint32 count = 0;

			for (miter = nma_mobile_provider_get_methods (provider); miter; miter = g_slist_next (miter)) {
				NMAMobileAccessMethod *method = miter->data;

				if (self->family == nma_mobile_access_method_get_family (method))
					count++;
			}

			if (!count)
				continue;
		}

		gtk_tree_store_append (GTK_TREE_STORE (self->providers_store), &provider_iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (self->providers_store),
		                    &provider_iter,
		                    PROVIDER_COL_NAME,
		                    nma_mobile_provider_get_name (provider),
		                    PROVIDER_COL_PROVIDER,
		                    provider,
		                    -1);
	}

	nma_country_info_unref (country_info);

	g_object_set (G_OBJECT (self->providers_view), "enable-search", TRUE, NULL);

	gtk_tree_view_set_search_column (GTK_TREE_VIEW (self->providers_view), PROVIDER_COL_NAME);
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (self->providers_view),
	                                     providers_search_func, self, NULL);

	/* If no row has focus yet, focus the first row so that the user can start
	 * incremental search without clicking.
	 */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->providers_view));
	g_assert (selection);
	if (!gtk_tree_selection_count_selected_rows (selection)) {
		GtkTreeIter first_iter;
		GtkTreePath *first_path;

		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->providers_sort), &first_iter)) {
			first_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->providers_sort), &first_iter);
			if (first_path) {
				gtk_tree_selection_select_path (selection, first_path);
				gtk_tree_path_free (first_path);
			}
		}
	}

done:
	providers_radio_toggled (NULL, self);

	/* Initial completeness state */
	providers_update_complete (self);

	/* If there's already a selected device, hide the GSM/CDMA radios */
	if (self->family != NMA_MOBILE_FAMILY_UNKNOWN)
		gtk_widget_hide (self->provider_unlisted_type_combo);
	else
		gtk_widget_show (self->provider_unlisted_type_combo);
}

/**********************************************************/
/* Country page */
/**********************************************************/

#define COUNTRIES_COL_NAME 0
#define COUNTRIES_COL_INFO 1

static gboolean
country_search_func (GtkTreeModel *model,
                     gint column,
                     const char *key,
                     GtkTreeIter *iter,
                     gpointer search_data)
{
	gboolean unmatched = TRUE;
	char *country = NULL;

	if (!key)
		return TRUE;

	gtk_tree_model_get (model, iter, column, &country, -1);
	if (!country)
		return TRUE;

	unmatched = !!g_ascii_strncasecmp (country, key, strlen (key));
	g_free (country);
	return unmatched;
}

static void
add_one_country (gpointer key, gpointer value, gpointer user_data)
{
	NMAMobileWizard *self = user_data;
	NMACountryInfo *country_info = value;
	GtkTreeIter country_iter;
	GtkTreePath *country_path, *country_view_path;

	g_assert (key);

	gtk_tree_store_append (GTK_TREE_STORE (self->country_store), &country_iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (self->country_store),
	                    &country_iter,
	                    COUNTRIES_COL_NAME,
	                    nma_country_info_get_country_name (country_info),
	                    COUNTRIES_COL_INFO,
	                    country_info,
	                    -1);

	/* If this country is the same country as the user's current locale,
	 * select it by default.
	 */
	if (self->country != country_info)
		return;

	country_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->country_store), &country_iter);
	if (!country_path)
		return;

	country_view_path = gtk_tree_model_sort_convert_child_path_to_path (self->country_sort, country_path);
	if (country_view_path) {
		GtkTreeSelection *selection;

		gtk_tree_view_expand_row (GTK_TREE_VIEW (self->country_view), country_view_path, TRUE);

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->country_view));
		g_assert (selection);
		gtk_tree_selection_select_path (selection, country_view_path);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (self->country_view),
		                              country_view_path, NULL, TRUE, 0, 0);
		gtk_tree_path_free (country_view_path);
	}
	gtk_tree_path_free (country_path);
}

NMACountryInfo *
get_selected_country (NMAMobileWizard *self)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	NMACountryInfo *country_info = NULL;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->country_view));
	g_assert (selection);

	if (!gtk_tree_selection_get_selected (GTK_TREE_SELECTION (selection), &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COUNTRIES_COL_INFO, &country_info, -1);
	return country_info;
}

static void
country_update_complete (NMAMobileWizard *self)
{
	NMACountryInfo *country_info;

	country_info = get_selected_country (self);
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self->assistant),
	                                 self->country_page,
	                                 (!!country_info));
	if (country_info)
		nma_country_info_unref (country_info);
}

static void
country_update_continue (NMAMobileWizard *self)
{
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self->assistant),
	                                 self->country_page,
	                                 TRUE);

	gtk_assistant_next_page (GTK_ASSISTANT (self->assistant));
}

static gint
country_sort_func (GtkTreeModel *model,
                   GtkTreeIter *a,
                   GtkTreeIter *b,
                   gpointer user_data)
{
	char *a_str = NULL, *b_str = NULL;
	NMACountryInfo *a_country_info = NULL, *b_country_info = NULL;
	gint ret = 0;

	gtk_tree_model_get (model, a, COUNTRIES_COL_NAME, &a_str, COUNTRIES_COL_INFO, &a_country_info, -1);
	gtk_tree_model_get (model, b, COUNTRIES_COL_NAME, &b_str, COUNTRIES_COL_INFO, &b_country_info, -1);

	if (!a_country_info) {
		ret = -1;
		goto out;
	} else if (!b_country_info) {
		ret = 1;
		goto out;
	}

	if (a_str && !b_str)
		ret = -1;
	else if (!a_str && b_str)
		ret = 1;
	else if (!a_str && !b_str)
		ret = 0;
	else
		ret = g_utf8_collate (a_str, b_str);

out:
	if (a_country_info)
		nma_country_info_unref (a_country_info);
	if (b_country_info)
		nma_country_info_unref (b_country_info);
	g_free (a_str);
	g_free (b_str);
	return ret;
}

static void
country_setup (NMAMobileWizard *self)
{
	GtkWidget *vbox, *label, *scroll, *alignment;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeIter unlisted_iter;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	label = gtk_label_new (_("Country or Region List:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

	self->country_store = gtk_tree_store_new (2, G_TYPE_STRING, NMA_TYPE_COUNTRY_INFO);

	self->country_sort = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (self->country_store)));
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->country_sort),
	                                      COUNTRIES_COL_NAME, GTK_SORT_ASCENDING);

	self->country_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->country_sort));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Country or region"),
	                                                   renderer,
	                                                   "text", COUNTRIES_COL_NAME,
	                                                   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->country_view), column);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* My country is not listed... */
	gtk_tree_store_append (GTK_TREE_STORE (self->country_store), &unlisted_iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (self->country_store), &unlisted_iter,
	                    PROVIDER_COL_NAME, _("My country is not listed"),
	                    PROVIDER_COL_PROVIDER, NULL,
	                    -1);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (self->country_sort),
	                                 COUNTRIES_COL_NAME,
	                                 country_sort_func,
	                                 NULL,
	                                 NULL);

	/* Add the rest of the providers */
	if (self->mobile_providers_database) {
		GHashTable *countries;

		countries = nma_mobile_providers_database_get_countries (self->mobile_providers_database);
		g_hash_table_foreach (countries, add_one_country, self);
	}
	g_object_set (G_OBJECT (self->country_view), "enable-search", TRUE, NULL);

	/* If no row has focus yet, focus the first row so that the user can start
	 * incremental search without clicking.
	 */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->country_view));
	g_assert (selection);
	if (!gtk_tree_selection_count_selected_rows (selection)) {
		GtkTreeIter first_iter;
		GtkTreePath *first_path;

		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->country_sort), &first_iter)) {
			first_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->country_sort), &first_iter);
			if (first_path) {
				gtk_tree_selection_select_path (selection, first_path);
				gtk_tree_path_free (first_path);
			}
		}
	}

	g_signal_connect_swapped (selection, "changed", G_CALLBACK (country_update_complete), self);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), self->country_view);

	alignment = gtk_alignment_new (0, 0, 1, 1);
	gtk_container_add (GTK_CONTAINER (alignment), scroll);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, TRUE, TRUE, 6);

	self->country_idx = gtk_assistant_append_page (GTK_ASSISTANT (self->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (self->assistant), vbox, _("Choose your Provider's Country or Region"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self->assistant), vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self->assistant), vbox, TRUE);
	gtk_widget_show_all (vbox);

	self->country_page = vbox;

	/* If the user presses the ENTER key after selecting the country, continue to the next page */
	g_signal_connect_swapped (self->country_view, "row-activated", G_CALLBACK (country_update_continue), self);

	/* Initial completeness state */
	country_update_complete (self);
}

static gboolean
focus_country_view (gpointer user_data)
{
	NMAMobileWizard *self = user_data;

	self->country_focus_id = 0;
	gtk_widget_grab_focus (self->country_view);
	return FALSE;
}

static void
country_prepare (NMAMobileWizard *self)
{
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (self->country_view), COUNTRIES_COL_NAME);
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (self->country_view), country_search_func, self, NULL);

	if (!self->country_focus_id)
		self->country_focus_id = g_idle_add (focus_country_view, self);

	country_update_complete (self);
}

/**********************************************************/
/* Intro page */
/**********************************************************/

#define INTRO_COL_NAME 0
#define INTRO_COL_DEVICE 1
#define INTRO_COL_SEPARATOR 2

static gboolean
__intro_device_added (NMAMobileWizard *self, NMDevice *device, gboolean select_it)
{
	GtkTreeIter iter;
	const char *desc = nma_utils_get_device_description (device);
	NMDeviceModemCapabilities caps;

	if (!NM_IS_DEVICE_MODEM (device))
		return FALSE;

	caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));
	if (caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) {
		if (!desc)
			desc = _("Installed GSM device");
	} else if (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) {
		if (!desc)
			desc = _("Installed CDMA device");
	} else
		return FALSE;

	gtk_tree_store_append (GTK_TREE_STORE (self->dev_store), &iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (self->dev_store),
	                    &iter,
	                    INTRO_COL_NAME, desc,
	                    INTRO_COL_DEVICE, device,
	                    -1);

	/* Select the device just added */
	if (select_it)
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self->dev_combo), &iter);

	gtk_widget_set_sensitive (self->dev_combo, TRUE);
	return TRUE;
}

static void
intro_device_added_cb (NMClient *client, NMDevice *device, NMAMobileWizard *self)
{
	__intro_device_added (self, device, TRUE);
}

static void
intro_device_removed_cb (NMClient *client, NMDevice *device, NMAMobileWizard *self)
{
	GtkTreeIter iter;
	gboolean have_device = FALSE, removed = FALSE;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->dev_store), &iter))
		return;

	do {
		NMDevice *candidate = NULL;

		gtk_tree_model_get (GTK_TREE_MODEL (self->dev_store), &iter,
		                    INTRO_COL_DEVICE, &candidate, -1);
		if (candidate) {
			if (candidate == device) {
				gtk_tree_store_remove (GTK_TREE_STORE (self->dev_store), &iter);
				removed = TRUE;
			}
			g_object_unref (candidate);
		}
	} while (!removed && gtk_tree_model_iter_next (GTK_TREE_MODEL (self->dev_store), &iter));

	/* There's already a selected device item; nothing more to do */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self->dev_combo)) > 1)
		return;

	/* If there are no more devices, select the "Any" item and disable the
	 * combo box.  If there is no selected item and there is at least one device
	 * item, select the first one.
	 */
	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->dev_store), &iter))
		return;

	do {
		NMDevice *candidate = NULL;

		gtk_tree_model_get (GTK_TREE_MODEL (self->dev_store), &iter,
		                    INTRO_COL_DEVICE, &candidate, -1);
		if (candidate) {
			have_device = TRUE;
			g_object_unref (candidate);
		}
	} while (!have_device && gtk_tree_model_iter_next (GTK_TREE_MODEL (self->dev_store), &iter));

	if (have_device) {
		/* Iter should point to the last device item in the combo */
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self->dev_combo), &iter);
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->dev_combo), 0);
		gtk_widget_set_sensitive (self->dev_combo, FALSE);
	}
}

static void
intro_add_initial_devices (NMAMobileWizard *self)
{
	const GPtrArray *devices;
	gboolean selected_first = FALSE;
	int i;

	devices = self->client ? nm_client_get_devices (self->client) : NULL;
	for (i = 0; devices && (i < devices->len); i++) {
		if (__intro_device_added (self, g_ptr_array_index (devices, i), !selected_first)) {
			if (selected_first == FALSE)
				selected_first = TRUE;
		}
	}

	/* Otherwise the "Any device" item */
	if (!selected_first) {
		/* Select the first device item by default */
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->dev_combo), 0);
		gtk_widget_set_sensitive (self->dev_combo, FALSE);
	}
}

static void
intro_remove_all_devices (NMAMobileWizard *self)
{
	gtk_tree_store_clear (self->dev_store);

	/* Select the "Any device" item */
	gtk_combo_box_set_active (GTK_COMBO_BOX (self->dev_combo), 0);
	gtk_widget_set_sensitive (self->dev_combo, FALSE);
}

static void
intro_manager_running_cb (NMClient *client, GParamSpec *pspec, NMAMobileWizard *self)
{
	if (nm_client_get_manager_running (client))
		intro_add_initial_devices (self);
	else
		intro_remove_all_devices (self);
}

static gboolean
intro_row_separator_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean separator = FALSE;
	gtk_tree_model_get (model, iter, INTRO_COL_SEPARATOR, &separator, -1);
	return separator;
}

static void
intro_combo_changed (NMAMobileWizard *self)
{
	GtkTreeIter iter;
	NMDevice *selected = NULL;
	NMDeviceModemCapabilities caps;

	g_free (self->dev_desc);
	self->dev_desc = NULL;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->dev_combo), &iter))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (self->dev_store), &iter,
	                    INTRO_COL_DEVICE, &selected, -1);
	if (selected) {
		self->dev_desc = g_strdup (nma_utils_get_device_description (selected));
		caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (selected));
		if (caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS)
			self->family = NMA_MOBILE_FAMILY_3GPP;
		else if (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)
			self->family = NMA_MOBILE_FAMILY_CDMA;
		else
			g_warning ("%s: unknown modem capabilities 0x%X", __func__, caps);

		g_object_unref (selected);
	}
}

static void
intro_setup (NMAMobileWizard *self)
{
	GtkWidget *vbox, *label, *alignment, *info_vbox;
	GtkCellRenderer *renderer;
	char *s;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	label = gtk_label_new (_("This assistant helps you easily set up a mobile broadband connection to a cellular (3G) network."));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 6);

	label = gtk_label_new (_("You will need the following information:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 6);

	alignment = gtk_alignment_new (0, 0, 1, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 25, 25, 0);
	info_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add (GTK_CONTAINER (alignment), info_vbox);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 6);

	s = g_strdup_printf ("• %s", _("Your broadband provider's name"));
	label = gtk_label_new (s);
	g_free (s);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (info_vbox), label, FALSE, TRUE, 0);

	s = g_strdup_printf ("• %s", _("Your broadband billing plan name"));
	label = gtk_label_new (s);
	g_free (s);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (info_vbox), label, FALSE, TRUE, 0);

	s = g_strdup_printf ("• %s", _("(in some cases) Your broadband billing plan APN (Access Point Name)"));
	label = gtk_label_new (s);
	g_free (s);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (info_vbox), label, FALSE, TRUE, 0);

	/* Device combo; only built if the wizard's caller didn't pass one in */
	if (!self->initial_family) {
		GtkTreeIter iter;

		self->client = nm_client_new ();
		if (self->client) {
			g_signal_connect (self->client, "device-added",
			                  G_CALLBACK (intro_device_added_cb), self);
			g_signal_connect (self->client, "device-removed",
			                  G_CALLBACK (intro_device_removed_cb), self);
			g_signal_connect (self->client, "notify::manager-running",
			                  G_CALLBACK (intro_manager_running_cb), self);
		}

		self->dev_store = gtk_tree_store_new (3, G_TYPE_STRING, NM_TYPE_DEVICE, G_TYPE_BOOLEAN);
		self->dev_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (self->dev_store));
		gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (self->dev_combo),
		                                      intro_row_separator_func, NULL, NULL);

		renderer = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->dev_combo), renderer, TRUE);
		gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->dev_combo), renderer, "text", INTRO_COL_NAME);

		label = gtk_label_new_with_mnemonic (_("Create a connection for _this mobile broadband device:"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->dev_combo);
		gtk_misc_set_alignment (GTK_MISC (label), 0, 1);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		alignment = gtk_alignment_new (0, 0, 0.5, 0);
		gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 25, 0);
		gtk_container_add (GTK_CONTAINER (alignment), self->dev_combo);
		gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

		g_signal_connect_swapped (self->dev_combo, "changed", G_CALLBACK (intro_combo_changed), self);

		/* Any device */
		gtk_tree_store_append (GTK_TREE_STORE (self->dev_store), &iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (self->dev_store), &iter,
		                    INTRO_COL_NAME, _("Any device"), -1);

		/* Separator */
		gtk_tree_store_append (GTK_TREE_STORE (self->dev_store), &iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (self->dev_store), &iter,
		                    INTRO_COL_SEPARATOR, TRUE, -1);

		intro_add_initial_devices (self);
	}

	gtk_widget_show_all (vbox);
	gtk_assistant_append_page (GTK_ASSISTANT (self->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (self->assistant),
	                              vbox, _("Set up a Mobile Broadband Connection"));

	gtk_assistant_set_page_complete (GTK_ASSISTANT (self->assistant), vbox, TRUE);
	gtk_assistant_set_page_type (GTK_ASSISTANT (self->assistant), vbox, GTK_ASSISTANT_PAGE_INTRO);
}

/**********************************************************/
/* General assistant stuff */
/**********************************************************/

static void
remove_provider_focus_idle (NMAMobileWizard *self)
{
	if (self->providers_focus_id) {
		g_source_remove (self->providers_focus_id);
		self->providers_focus_id = 0;
	}
}

static void
remove_country_focus_idle (NMAMobileWizard *self)
{
	if (self->country_focus_id) {
		g_source_remove (self->country_focus_id);
		self->country_focus_id = 0;
	}
}

static void
assistant_prepare (GtkAssistant *assistant, GtkWidget *page, gpointer user_data)
{
	NMAMobileWizard *self = user_data;

	if (page != self->providers_page)
		remove_provider_focus_idle (self);
	if (page != self->country_page)
		remove_country_focus_idle (self);

	if (page == self->country_page)
		country_prepare (self);
	else if (page == self->providers_page)
		providers_prepare (self);
	else if (page == self->plan_page)
		plan_prepare (self);
	else if (page == self->confirm_page)
		confirm_prepare (self);
}

static gint
forward_func (gint current_page, gpointer user_data)
{
	NMAMobileWizard *self = user_data;

	if (current_page == self->providers_idx) {
		NMAMobileFamily family = self->family;

		/* If the provider is unlisted, we can skip ahead of the user's
		 * access technology is CDMA.
		 */
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->provider_unlisted_radio))) {
			if (family == NMA_MOBILE_FAMILY_UNKNOWN)
				family = get_provider_unlisted_type (self);
		} else {
			/* Or, if the provider is only CDMA, then we can also skip ahead */
			NMAMobileProvider *provider;
			GSList *iter;
			gboolean gsm = FALSE, cdma = FALSE;

			provider = get_selected_provider (self);
			if (provider) {
				for (iter = nma_mobile_provider_get_methods (provider); iter; iter = g_slist_next (iter)) {
					NMAMobileAccessMethod *method = iter->data;

					if (nma_mobile_access_method_get_family (method) == NMA_MOBILE_FAMILY_CDMA)
						cdma = TRUE;
					else if (nma_mobile_access_method_get_family (method) == NMA_MOBILE_FAMILY_3GPP)
						gsm = TRUE;
				}
				nma_mobile_provider_unref (provider);

				if (cdma && !gsm)
					family = NMA_MOBILE_FAMILY_CDMA;
			}
		}

		/* Skip to the confirm page if we know its CDMA */
		if (family == NMA_MOBILE_FAMILY_CDMA) {
			self->provider_only_cdma = TRUE;
			return self->confirm_idx;
		} else
			self->provider_only_cdma = FALSE;
	}

	return current_page + 1;
}

static char *
get_country_from_locale (void)
{
	char *p, *m, *cc, *lang;

	lang = getenv ("LC_ALL");
	if (!lang)
		lang = getenv ("LANG");
	if (!lang)
		return NULL;

	p = strchr (lang, '_');
	if (!p || !strlen (p)) {
		g_free (p);
		return NULL;
	}

	p = cc = g_strdup (++p);
	m = strchr (cc, '.');
	if (m)
		*m = '\0';

	while (*p) {
		*p = g_ascii_toupper (*p);
		p++;
	}

	return cc;
}

/**
 * nma_mobile_wizard_new: (skip)
 * @cb: (scope async):
 */
NMAMobileWizard *
nma_mobile_wizard_new (GtkWindow *parent,
                       GtkWindowGroup *window_group,
                       NMDeviceModemCapabilities modem_caps,
                       gboolean will_connect_after,
                       NMAMobileWizardCallback cb,
                       gpointer user_data)
{
	NMAMobileWizard *self;
	char *cc;
	GError *error = NULL;

	self = g_malloc0 (sizeof (NMAMobileWizard));
	g_return_val_if_fail (self != NULL, NULL);

	self->mobile_providers_database = nma_mobile_providers_database_new_sync (NULL, NULL, NULL, &error);
	if (!self->mobile_providers_database) {
		g_warning ("Cannot create mobile providers database: %s",
		           error->message);
		g_error_free (error);
		nma_mobile_wizard_destroy (self);
		return NULL;
	}

	cc = get_country_from_locale ();
	if (cc) {
		self->country = nma_mobile_providers_database_lookup_country (self->mobile_providers_database, cc);
		g_free (cc);
	}

	self->will_connect_after = will_connect_after;
	self->callback = cb;
	self->user_data = user_data;
	if (modem_caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS)
		self->family = NMA_MOBILE_FAMILY_3GPP;
	else if (modem_caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)
		self->family = NMA_MOBILE_FAMILY_CDMA;
	if (self->family)
		self->initial_family = TRUE;  /* Skip device selection */

	self->assistant = gtk_assistant_new ();
	gtk_assistant_set_forward_page_func (GTK_ASSISTANT (self->assistant),
	                                     forward_func, self, NULL);
	gtk_window_set_title (GTK_WINDOW (self->assistant), _("New Mobile Broadband Connection"));
	gtk_window_set_position (GTK_WINDOW (self->assistant), GTK_WIN_POS_CENTER_ALWAYS);

	intro_setup (self);
	country_setup (self);
	providers_setup (self);
	plan_setup (self);
	confirm_setup (self);

	g_signal_connect (self->assistant, "close", G_CALLBACK (assistant_closed), self);
	g_signal_connect (self->assistant, "cancel", G_CALLBACK (assistant_cancel), self);
	g_signal_connect (self->assistant, "prepare", G_CALLBACK (assistant_prepare), self);

	/* Run the wizard */
	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (self->assistant), parent);
	gtk_window_set_modal (GTK_WINDOW (self->assistant), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (self->assistant), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (self->assistant), GDK_WINDOW_TYPE_HINT_DIALOG);

	if (window_group)
		gtk_window_group_add_window (window_group, GTK_WINDOW (self->assistant));

	return self;
}

void
nma_mobile_wizard_present (NMAMobileWizard *self)
{
	g_return_if_fail (self != NULL);

	gtk_window_present (GTK_WINDOW (self->assistant));
	gtk_widget_show_all (self->assistant);
}

void
nma_mobile_wizard_destroy (NMAMobileWizard *self)
{
	g_return_if_fail (self != NULL);

	g_free (self->dev_desc);

	if (self->assistant) {
		gtk_widget_hide (self->assistant);
		gtk_widget_destroy (self->assistant);
	}

	if (self->client)
		g_object_unref (self->client);

	remove_provider_focus_idle (self);
	remove_country_focus_idle (self);

	if (self->mobile_providers_database)
		g_object_unref (self->mobile_providers_database);

	g_free (self);
}
