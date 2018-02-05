/* NetworkManager Applet -- allow user control over networking
 *
 * Lubomir Rintel <lkundrak@v3.sk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2016,2017 Red Hat, Inc.
 */

#include "nm-default.h"
#include "nma-cert-chooser-button.h"
#include "nma-pkcs11-cert-chooser-dialog.h"
#include "utils.h"

#include <gck/gck.h>

/**
 * SECTION:nma-cert-chooser-button
 * @title: NMACertChooserButton
 * @short_description: The PKCS\#11 or file certificate chooser button
 *
 * #NMACertChooserButton is a button that provides a dropdown of
 * PKCS\#11 slots present in the system and allows choosing a certificate
 * from either of them or a file.
 */

enum {
	COLUMN_LABEL,
	COLUMN_SLOT,
	N_COLUMNS
};

typedef struct {
	gchar *title;
	gchar *uri;
	gchar *pin;
	gboolean remember_pin;
	NMACertChooserButtonFlags flags;
} NMACertChooserButtonPrivate;

G_DEFINE_TYPE (NMACertChooserButton, nma_cert_chooser_button, GTK_TYPE_COMBO_BOX);

#define NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                                NMA_TYPE_CERT_CHOOSER_BUTTON, \
                                                NMACertChooserButtonPrivate))

static gboolean
is_this_a_slot_nobody_loves (GckSlot *slot)
{
	GckSlotInfo *slot_info;
	gboolean ret_value = FALSE;

	slot_info = gck_slot_get_info (slot);

	/* The p11-kit CA trusts do use their filesystem paths for description. */
	if (g_str_has_prefix (slot_info->slot_description, "/"))
		ret_value = TRUE;
	else if (NM_IN_STRSET (slot_info->slot_description,
	                       "SSH Keys",
	                       "Secret Store",
	                       "User Key Storage"))
		ret_value = TRUE;

	gck_slot_info_free (slot_info);

	return ret_value;
}

static void
modules_initialized (GObject *object, GAsyncResult *res, gpointer user_data)
{
	NMACertChooserButton *self = NMA_CERT_CHOOSER_BUTTON (user_data);
	GList *slots;
	GList *list_iter;
	GError *error = NULL;
	GList *modules;
	GtkTreeIter iter;
	GtkListStore *model;
	GckTokenInfo *info;
	gchar *label;

	modules = gck_modules_initialize_registered_finish (res, &error);
	if (!modules) {
		/* The Front Fell Off. */
		g_critical ("Error getting registered modules: %s", error->message);
		g_error_free (error);
	}

	model = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (self)));

	/* A separator. */
	gtk_list_store_insert_with_values (model, &iter, 2,
	                                   COLUMN_LABEL, NULL,
	                                   COLUMN_SLOT, NULL, -1);

	slots = gck_modules_get_slots (modules, FALSE);
	for (list_iter = slots; list_iter; list_iter = list_iter->next) {
		GckSlot *slot = GCK_SLOT (list_iter->data);

		if (is_this_a_slot_nobody_loves (slot))
			continue;

		info = gck_slot_get_token_info (slot);
		if (!info) {
			/* This happens when the slot has no token inserted.
			 * Don't add this one to the list. The other widgets
			 * assume gck_slot_get_token_info() don't fail and a slot
			 * for which it does is essentially useless as it can't be
			 * used for crafting an URI. */
			continue;
		}

		if ((info->flags & CKF_TOKEN_INITIALIZED) == 0)
			continue;

		if (info->label && *info->label) {
			label = g_strdup_printf ("%s\342\200\246", info->label);
		} else if (info->model && *info->model) {
			g_warning ("The token doesn't have a valid label");
			label = g_strdup_printf ("%s\342\200\246", info->model);
		} else {
			g_warning ("The token has neither valid label nor model");
			label = g_strdup ("(Unknown)\342\200\246");
		}
		gtk_list_store_insert_with_values (model, &iter, 2,
		                                   COLUMN_LABEL, label,
		                                   COLUMN_SLOT, slot, -1);
		g_free (label);
		gck_token_info_free (info);
	}

	gck_list_unref_free (slots);
	gck_list_unref_free (modules);
}

static void
update_title (NMACertChooserButton *button)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (button);
	GckUriData *data;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gs_free char *label = NULL;
	GError *error = NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (button));

	if (!gtk_tree_model_get_iter_first (model, &iter))
		g_return_if_reached ();

	if (!priv->uri) {
		label = g_strdup (_("(None)"));
	} else if (g_str_has_prefix (priv->uri, "pkcs11:")) {
		data = gck_uri_parse (priv->uri, GCK_URI_FOR_ANY, &error);
		if (data) {
			if (!gck_attributes_find_string (data->attributes, CKA_LABEL, &label)) {
				if (data->token_info) {
					g_free (label);
					label = g_strdup_printf (  priv->flags & NMA_CERT_CHOOSER_BUTTON_FLAG_KEY
					                         ? _("Key in %s")
					                         : _("Certificate in %s"),
					                         data->token_info->label);
				}
			}
			gck_uri_data_free (data);
		} else {
			g_warning ("Bad URI '%s': %s\n", priv->uri, error->message);
			g_error_free (error);
		}
	} else {
		label = priv->uri;
		if (g_str_has_prefix (label, "file://"))
			label += 7;
		if (g_strrstr (label, "/"))
			label = g_strrstr (label, "/") + 1;
		label = g_strdup (label);
	}

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
	                    COLUMN_LABEL, label ?: _("(Unknown)"),
	                    -1);
}

static void
select_from_token (NMACertChooserButton *button, GckSlot *slot)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (button);
	GtkWidget *toplevel;
	GtkWidget *dialog;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	if (!gtk_widget_is_toplevel (toplevel) || !GTK_IS_WINDOW (toplevel))
		toplevel = NULL;

	dialog = nma_pkcs11_cert_chooser_dialog_new (slot,
	                                               priv->flags & NMA_CERT_CHOOSER_BUTTON_FLAG_KEY
	                                             ? CKO_PRIVATE_KEY
	                                             : CKO_CERTIFICATE,
	                                             priv->title,
	                                             GTK_WINDOW (toplevel),
	                                             GTK_FILE_CHOOSER_ACTION_OPEN | GTK_DIALOG_USE_HEADER_BAR,
	                                             _("Select"), GTK_RESPONSE_ACCEPT,
	                                             _("Cancel"), GTK_RESPONSE_CANCEL,
	                                             NULL);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		if (priv->uri)
			g_free (priv->uri);
		priv->uri = nma_pkcs11_cert_chooser_dialog_get_uri (NMA_PKCS11_CERT_CHOOSER_DIALOG (dialog));
		if (priv->pin)
			g_free (priv->pin);
		priv->pin = nma_pkcs11_cert_chooser_dialog_get_pin (NMA_PKCS11_CERT_CHOOSER_DIALOG (dialog));
		priv->remember_pin = nma_pkcs11_cert_chooser_dialog_get_remember_pin (NMA_PKCS11_CERT_CHOOSER_DIALOG (dialog));
		update_title (button);
	}
	gtk_widget_destroy (dialog);
}

static void
select_from_file (NMACertChooserButton *button)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (button);
	GtkWidget *toplevel;
	GtkWidget *dialog;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	if (!gtk_widget_is_toplevel (toplevel) || !GTK_IS_WINDOW (toplevel))
		toplevel = NULL;

	dialog = gtk_file_chooser_dialog_new (priv->title,
	                                      GTK_WINDOW (toplevel),
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      _("Select"), GTK_RESPONSE_ACCEPT,
	                                      _("Cancel"), GTK_RESPONSE_CANCEL,
	                                      NULL);

	if (priv->flags & NMA_CERT_CHOOSER_BUTTON_FLAG_KEY)
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), utils_key_filter ());
	else
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), utils_cert_filter ());

	if (priv->uri)
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dialog), priv->uri);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		if (priv->uri)
			g_free (priv->uri);
		priv->uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		if (priv->pin) {
			g_free (priv->pin);
			priv->pin = NULL;
		}
		priv->remember_pin = FALSE;
		update_title (button);
	}
	gtk_widget_destroy (dialog);
}

static void
dispose (GObject *object)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (object);

	nm_clear_g_free (&priv->title);
	nm_clear_g_free (&priv->uri);
	nm_clear_g_free (&priv->pin);
}

static void
changed (GtkComboBox *combo_box)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *label;
	GckSlot *slot;

	if (gtk_combo_box_get_active (combo_box) == 0)
		return;

	g_signal_stop_emission_by_name (combo_box, "changed");
	gtk_combo_box_get_active_iter (combo_box, &iter);

	model = gtk_combo_box_get_model (combo_box);
	gtk_tree_model_get (model, &iter,
	                    COLUMN_LABEL, &label,
	                    COLUMN_SLOT, &slot, -1);
	if (slot)
		select_from_token (NMA_CERT_CHOOSER_BUTTON (combo_box), slot);
	else
		select_from_file (NMA_CERT_CHOOSER_BUTTON (combo_box));

	g_free (label);
	g_clear_object (&slot);
	gtk_combo_box_set_active (combo_box, 0);
}

static void
nma_cert_chooser_button_class_init (NMACertChooserButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkComboBoxClass *combo_box_class = GTK_COMBO_BOX_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (NMACertChooserButtonPrivate));

	object_class->dispose = dispose;
	combo_box_class->changed = changed;
}

static void
nma_cert_chooser_button_init (NMACertChooserButton *self)
{
	gck_modules_initialize_registered_async (NULL, modules_initialized, self);
}

static gboolean
row_separator (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gchar *label;
	GckSlot *slot;

	gtk_tree_model_get (model, iter, 0, &label, 1, &slot, -1);
	if (label == NULL && slot == NULL)
		return TRUE;
	g_free (label);
	g_clear_object (&slot);

	return FALSE;
}

/**
 * nma_cert_chooser_button_set_title:
 * @button: the #NMACertChooserButton instance
 * @title: the title of the token or file chooser dialogs
 *
 * Set the title of file or PKCS\#11 object chooser dialogs.
 */
void
nma_cert_chooser_button_set_title (NMACertChooserButton *button, const gchar *title)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (button);

	if (priv->title)
		g_free (priv->title);
	priv->title = g_strdup (title);
}

/**
 * nma_cert_chooser_button_get_uri:
 * @button: the #NMACertChooserButton instance
 *
 * Obtain the URI of the selected obejct -- either of
 * "pkcs11" or "file" scheme.
 *
 * Returns: the URI or %NULL if none was selected.
 */
const gchar *
nma_cert_chooser_button_get_uri (NMACertChooserButton *button)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (button);

	return priv->uri;
}

/**
 * nma_cert_chooser_button_set_uri:
 * @button: the #NMACertChooserButton instance
 * @uri: the URI
 *
 * Set the chosen URI to given string.
 */
void
nma_cert_chooser_button_set_uri (NMACertChooserButton *button, const gchar *uri)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (button);

	if (priv->uri)
		g_free (priv->uri);
	priv->uri = g_strdup (uri);
	update_title (button);
}

/**
 * nma_cert_chooser_button_get_pin:
 * @button: the #NMACertChooserButton instance
 *
 * Obtain the PIN that was used to unlock the token.
 *
 * Returns: the PIN, %NULL if the token was not logged into or an emtpy
 *   string ("") if the protected authentication path was used.
 */
gchar *
nma_cert_chooser_button_get_pin (NMACertChooserButton *button)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (button);

	return g_strdup (priv->pin);
}

/**
 * nma_cert_chooser_button_get_remember_pin:
 * @button: the #NMACertChooserButton instance
 *
 * Obtain the value of the "Remember PIN" checkbox during the token login.
 *
 * Returns: TRUE if the user chose to remember the PIN, FALSE
 *   if not or if the tokin was not logged into at all.
 */
gboolean
nma_cert_chooser_button_get_remember_pin (NMACertChooserButton *button)
{
	NMACertChooserButtonPrivate *priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (button);

	return priv->remember_pin;
}

/**
 * nma_cert_chooser_button_new:
 * @flags: the flags configuring the behavior of the chooser dialogs
 *
 * Creates the new button that can select certificates from
 * files or PKCS\#11 tokens.
 *
 * Returns: the newly created #NMACertChooserButton
 */
GtkWidget *
nma_cert_chooser_button_new (NMACertChooserButtonFlags flags)
{
	GtkWidget *self;
	GtkListStore *model;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	NMACertChooserButtonPrivate *priv;

	model = gtk_list_store_new (2, G_TYPE_STRING, GCK_TYPE_SLOT);
	self = g_object_new (NMA_TYPE_CERT_CHOOSER_BUTTON,
	                     "model", model,
	                     "popup-fixed-width", TRUE,
	                     NULL);
	g_object_unref (model);

	priv = NMA_CERT_CHOOSER_BUTTON_GET_PRIVATE (self);
	priv->flags = flags;

	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (self),
	                                      row_separator,
	                                      NULL,
	                                      NULL);

	/* The first entry with current object name. */
	gtk_list_store_insert_with_values (model, &iter, 0,
	                                   COLUMN_LABEL, NULL,
	                                   COLUMN_SLOT, NULL, -1);
	update_title (NMA_CERT_CHOOSER_BUTTON (self));

	/* The separator and the last entry. The tokens will be added in between. */
	gtk_list_store_insert_with_values (model, &iter, 1,
	                                   COLUMN_LABEL, NULL,
	                                   COLUMN_SLOT, NULL, -1);
	gtk_list_store_insert_with_values (model, &iter, 2,
	                                   COLUMN_LABEL, _("Select from file\342\200\246"),
	                                   COLUMN_SLOT, NULL, -1);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), cell, FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell, "text", 0);
	gtk_combo_box_set_active (GTK_COMBO_BOX (self), 0);

	return self;
}
