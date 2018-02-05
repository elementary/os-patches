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
#include "nma-pkcs11-cert-chooser-dialog.h"
#include "nma-pkcs11-token-login-dialog.h"

#include <string.h>
#include <gck/gck.h>
#include <gcr/gcr.h>

/**
 * SECTION:nma-pkcs11-cert-chooser-dialog
 * @title: NMAPkcs11CertChooserDialog
 * @short_description: The PKCS\#11 Object Chooser Dialog
 *
 * #NMAPkcs11CertChooserDialog selects an object from a PKCS\#11 token,
 * optionally allowing the user to specify the PIN and log in.
 */

enum {
	COLUMN_LABEL,
	COLUMN_ISSUER,
	COLUMN_HAS_KEY,
	COLUMN_ATTRIBUTES,
	N_COLUMNS
};

struct _NMAPkcs11CertChooserDialogPrivate {
	GckSlot *slot;
	GtkListStore *cert_store;
	GtkListStore *key_store;
	GtkWidget *login_button;

	guchar *pin_value;
	gulong pin_length;
	gboolean remember_pin;

	GtkRevealer *error_revealer;
	GtkLabel *error_label;
	GtkTreeView *objects_view;
	GtkTreeViewColumn *list_name_column;
	GtkCellRenderer *list_name_renderer;
	GtkTreeViewColumn *list_issued_by_column;
	GtkCellRenderer *list_issued_by_renderer;
};

#define NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                                       NMA_TYPE_PKCS11_CERT_CHOOSER_DIALOG, \
                                                       NMAPkcs11CertChooserDialogPrivate))

G_DEFINE_TYPE_WITH_CODE (NMAPkcs11CertChooserDialog, nma_pkcs11_cert_chooser_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (NMAPkcs11CertChooserDialog))

#define NMA_RESPONSE_LOGIN 1

enum {
	PROP_0,
	PROP_SLOT,
};

typedef struct {
	GckAttributes *attrs;
	gboolean has_key;
} IdMatchData;

static gboolean
id_match (GtkTreeModel *model,
          GtkTreePath *path,
          GtkTreeIter *iter,
          gpointer user_data)
{
	IdMatchData *data = user_data;
	GckAttributes *attrs = NULL;
	const GckAttribute *attr1, *attr2;

	attr1 = gck_attributes_find (data->attrs, CKA_ID);
	if (!attr1 || !attr1->value || !attr1->length)
		goto out;

	gtk_tree_model_get (model, iter, COLUMN_ATTRIBUTES, &attrs, -1);
	attr2 = gck_attributes_find (attrs, CKA_ID);
	if (!attr2 || !attr2->value || !attr2->length)
		goto out;

	if (attr1->length != attr2->length)
		goto out;

	if (memcmp (attr1->value, attr2->value, attr1->length))
		goto out;

	data->has_key = TRUE;
	gtk_list_store_set (GTK_LIST_STORE (model), iter,
	                    COLUMN_HAS_KEY, TRUE, -1);

	if (attrs)
		gck_attributes_unref (attrs);
out:
	return data->has_key;
}

static void
object_details (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GckObject *object = GCK_OBJECT (source_object);
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (user_data);
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);
	GckAttributes *attrs;
	GtkTreeIter iter;
	CK_OBJECT_CLASS cka_class;
	const GckAttribute *attr;
	GcrCertificate *cert;
	gchar *label, *issuer;
	GError *error = NULL;
	GtkListStore *store1, *store2;
	IdMatchData data;

	attrs = gck_object_get_finish (object, res, &error);
	if (!attrs) {
		/* No better idea than to just ignore the object. */
		g_warning ("Error getting attributes: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	if (!gck_attributes_find_ulong (attrs, CKA_CLASS, &cka_class)) {
		g_warning ("An object without CKA_CLASS\n");
		goto out;
	}

	switch (cka_class) {
	case CKO_CERTIFICATE:
		store1 = priv->cert_store;
		store2 = priv->key_store;
		break;
	case CKO_PRIVATE_KEY:
		store1 = priv->key_store;
		store2 = priv->cert_store;
		break;
	default:
		goto out;
	}

	/* See if there's a matching object in another store. */
	data.attrs = attrs;
	data.has_key = FALSE;
	gtk_tree_model_foreach (GTK_TREE_MODEL (store2),
	                        id_match,
	                        &data);

	attr = gck_attributes_find (attrs, CKA_VALUE);
	if (attr && attr->value && attr->length) {
		cert = gcr_simple_certificate_new (attr->value, attr->length);
		label = gcr_certificate_get_subject_name (cert);
		issuer = gcr_certificate_get_issuer_name (cert);
		g_object_unref (cert);
	} else {
		attr = gck_attributes_find (attrs, CKA_LABEL);
		if (attr && attr->value && attr->length) {
			label = g_malloc (attr->length + 1);
			memcpy (label, attr->value, attr->length);
			label[attr->length] = '\0';
		} else {
			label = g_strdup (_("(Unknown)"));
		}
		issuer = g_memdup ("", 1);
	}

	gtk_list_store_append (store1, &iter);
	gtk_list_store_set (store1, &iter,
	                    COLUMN_LABEL, label,
	                    COLUMN_ISSUER, issuer,
	                    COLUMN_HAS_KEY, data.has_key,
	                    COLUMN_ATTRIBUTES, attrs,
	                    -1);

	g_free (label);
	g_free (issuer);

out:
	if (attrs)
		gck_attributes_unref (attrs);
}

static void
next_object (GObject *obj, GAsyncResult *res, gpointer user_data)
{
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (user_data);
	GckEnumerator *enm = GCK_ENUMERATOR (obj);
	GList *objects;
	GList *iter;
	GError *error = NULL;

	objects = gck_enumerator_next_finish (enm, res, &error);
	if (error) {
		/* No better idea than to just ignore the object. */
		g_warning ("Error getting object: %s", error->message);
		g_error_free (error);
		return;
	}

	for (iter = objects; iter; iter = iter->next) {
		GckObject *object = GCK_OBJECT (iter->data);
		const gulong attr_types[] = { CKA_ID, CKA_LABEL, CKA_ISSUER,
		                              CKA_VALUE, CKA_CLASS };

		gck_object_get_async (object, attr_types,
		                      sizeof(attr_types) / sizeof(attr_types[0]),
		                      NULL, object_details, self);
	}

	gck_list_unref_free (objects);
}

static void
reload_slot (NMAPkcs11CertChooserDialog *self, GckSession *session)
{
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);
	GckEnumerator *enm;

	gtk_list_store_clear (priv->key_store);
	gtk_list_store_clear (priv->cert_store);
	enm = gck_session_enumerate_objects (session, gck_attributes_new_empty (GCK_INVALID));
	gck_enumerator_next_async (enm, -1, NULL, next_object, self);
}

static void
logged_in (GObject *obj, GAsyncResult *res, gpointer user_data)
{
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (user_data);
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);
	GckSession *session = GCK_SESSION (obj);
	GError *error = NULL;

	if (!gck_session_login_finish (session, res, &error)) {
		g_prefix_error (&error, _("Error logging in: "));
		gtk_label_set_label (priv->error_label, error->message);
		gtk_revealer_set_reveal_child (priv->error_revealer, TRUE);
		g_error_free (error);
	} else {
		gtk_revealer_set_reveal_child (priv->error_revealer, FALSE);
		gtk_widget_set_sensitive (priv->login_button, FALSE);
		reload_slot (self, session);
		g_clear_object (&session);
	}
}

static void
session_opened (GObject *obj, GAsyncResult *res, gpointer user_data)
{
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (user_data);
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);
	GckSession *session;
	GError *error = NULL;

	session = gck_slot_open_session_finish (priv->slot, res, &error);
	if (error) {
		g_prefix_error (&error, _("Error opening a session: "));
		gtk_label_set_label (priv->error_label, error->message);
		gtk_revealer_set_reveal_child (priv->error_revealer, TRUE);
		g_error_free (error);
		return;
	}

	if (priv->pin_value) {
		gck_session_login_async (session, CKU_USER,
		                         priv->pin_value, priv->pin_length,
		                         NULL, logged_in, self);
	} else {
		reload_slot (self, session);
		g_clear_object (&session);
	}
}

static void
row_activated (GtkTreeView *tree_view, GtkTreePath *path,
               GtkTreeViewColumn *column, gpointer user_data)
{
	if (gtk_window_activate_default (GTK_WINDOW (user_data)))
		return;
}

static void
cursor_changed (GtkTreeView *tree_view, gpointer user_data)
{
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (user_data);
	gchar *uri;

	uri = nma_pkcs11_cert_chooser_dialog_get_uri (self);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT, uri != NULL);
	g_free (uri);
}

static void
error_close (GtkInfoBar *bar, gint response_id, gpointer user_data)
{
	NMAPkcs11CertChooserDialog *self = user_data;
	NMAPkcs11CertChooserDialogPrivate *priv = self->priv;

	gtk_revealer_set_reveal_child (priv->error_revealer, FALSE);
}

static void
login_clicked (GtkButton *button, gpointer user_data)
{
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (user_data);
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);
	GtkWidget *dialog;
	GckTokenInfo *token_info;
	gboolean has_pin_pad = FALSE;

	/* See if the token has a PIN pad. */
	token_info = gck_slot_get_token_info (priv->slot);
	g_return_if_fail (token_info);
	if (token_info->flags & CKF_PROTECTED_AUTHENTICATION_PATH)
		has_pin_pad = TRUE;
	gck_token_info_free (token_info);

	if (priv->pin_value)
		g_free (priv->pin_value);

	if (has_pin_pad) {
		/* Login with empty credentials makes the token
		 * log in on its PIN pad. */
		priv->pin_length = 0;
		priv->pin_value =  g_memdup ("", 1);
		priv->remember_pin = TRUE;
		gck_slot_open_session_async (priv->slot, GCK_SESSION_READ_ONLY, NULL, session_opened, self);
		return;
	}

	/* The token doesn't have a PIN pad. Ask for PIN. */
	dialog = nma_pkcs11_token_login_dialog_new (priv->slot);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (self));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		priv->pin_length = nma_pkcs11_token_login_dialog_get_pin_length (NMA_PKCS11_TOKEN_LOGIN_DIALOG (dialog));
		priv->pin_value = g_memdup (nma_pkcs11_token_login_dialog_get_pin_value (NMA_PKCS11_TOKEN_LOGIN_DIALOG (dialog)),
		                            priv->pin_length + 1);
		priv->remember_pin = nma_pkcs11_token_login_dialog_get_remember_pin (NMA_PKCS11_TOKEN_LOGIN_DIALOG (dialog));
		gck_slot_open_session_async (priv->slot, GCK_SESSION_READ_ONLY, NULL, session_opened, self);
	}

	gtk_widget_destroy (dialog);
}

static void
get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (object);
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);

	switch (prop_id) {
	case PROP_SLOT:
		if (priv->slot)
			g_value_set_object (value, priv->slot);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (object);
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);
	GckTokenInfo *token_info;

	switch (prop_id) {
	case PROP_SLOT:
		priv->slot = g_value_dup_object (value);
		token_info = gck_slot_get_token_info (priv->slot);
		g_return_if_fail (token_info);
		if ((token_info->flags & CKF_LOGIN_REQUIRED) == 0)
			gtk_widget_set_sensitive (priv->login_button, FALSE);
		gck_token_info_free (token_info);
		gck_slot_open_session_async (priv->slot, GCK_SESSION_READ_ONLY, NULL, session_opened, self);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
finalize (GObject *object)
{
	NMAPkcs11CertChooserDialog *self = NMA_PKCS11_CERT_CHOOSER_DIALOG (object);
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);

	g_clear_object (&priv->cert_store);
	g_clear_object (&priv->key_store);
	g_clear_object (&priv->slot);

	if (priv->pin_value) {
		g_free (priv->pin_value);
		priv->pin_value = NULL;
	}

	G_OBJECT_CLASS (nma_pkcs11_cert_chooser_dialog_parent_class)->finalize (object);
}

static void
nma_pkcs11_cert_chooser_dialog_class_init (NMAPkcs11CertChooserDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->finalize = finalize;

	g_object_class_install_property (object_class, PROP_SLOT,
		g_param_spec_object ("slot", "PKCS#11 Slot", "PKCS#11 Slot",
		                     GCK_TYPE_SLOT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/freedesktop/network-manager-applet/nma-pkcs11-cert-chooser-dialog.ui");

	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11CertChooserDialog, objects_view);
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11CertChooserDialog, list_name_column);
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11CertChooserDialog, list_name_renderer);
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11CertChooserDialog, list_issued_by_column);
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11CertChooserDialog, list_issued_by_renderer);
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11CertChooserDialog, error_revealer);
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11CertChooserDialog, error_label);
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11CertChooserDialog, login_button);

	gtk_widget_class_bind_template_callback (widget_class, row_activated);
	gtk_widget_class_bind_template_callback (widget_class, cursor_changed);
	gtk_widget_class_bind_template_callback (widget_class, error_close);
	gtk_widget_class_bind_template_callback (widget_class, login_clicked);
}

static void
nma_pkcs11_cert_chooser_dialog_init (NMAPkcs11CertChooserDialog *self)
{
	NMAPkcs11CertChooserDialogPrivate *priv;

	self->priv = nma_pkcs11_cert_chooser_dialog_get_instance_private (self);
	priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);

	gtk_widget_init_template (GTK_WIDGET (self));

	gtk_tree_view_column_set_attributes (priv->list_name_column,
	                                     priv->list_name_renderer,
	                                     "text", 0, NULL);
	gtk_tree_view_column_set_attributes (priv->list_issued_by_column,
	                                     priv->list_issued_by_renderer,
	                                     "text", 1, NULL);

	priv->cert_store = gtk_list_store_new (N_COLUMNS,
	                                       G_TYPE_STRING,
	                                       G_TYPE_STRING,
	                                       G_TYPE_BOOLEAN,
	                                       GCK_TYPE_ATTRIBUTES);
	priv->key_store = gtk_list_store_new (N_COLUMNS,
	                                      G_TYPE_STRING,
	                                      G_TYPE_STRING,
	                                      G_TYPE_BOOLEAN,
	                                      GCK_TYPE_ATTRIBUTES);
}

static GtkWidget *
nma_pkcs11_cert_chooser_dialog_new_valist (GckSlot *slot,
                                           CK_OBJECT_CLASS object_class,
                                           const gchar *title, GtkWindow *parent,
                                           GtkDialogFlags flags,
                                           const gchar *first_button_text,
                                           va_list varargs)
{
	NMAPkcs11CertChooserDialogPrivate *priv;
	GtkWidget *self;
	const char *button_text = first_button_text;
	gint response_id;

	self = g_object_new (NMA_TYPE_PKCS11_CERT_CHOOSER_DIALOG,
	                     "use-header-bar", !!(flags & GTK_DIALOG_USE_HEADER_BAR),
	                     "title", title,
	                     "slot", slot,
	                     NULL);

	priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (self);
	switch (object_class) {
	case CKO_CERTIFICATE:
		gtk_tree_view_set_model (GTK_TREE_VIEW (priv->objects_view),
		                         GTK_TREE_MODEL (priv->cert_store));
		break;
	case CKO_PRIVATE_KEY:
		gtk_tree_view_set_model (GTK_TREE_VIEW (priv->objects_view),
		                         GTK_TREE_MODEL (priv->key_store));
		break;
	default:
		g_warn_if_reached ();
	}

	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (self), parent);

	while (button_text) {
		response_id = va_arg (varargs, gint);
		gtk_dialog_add_button (GTK_DIALOG (self), button_text, response_id);
		button_text = va_arg (varargs, const gchar *);
	}

	gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT, FALSE);

	return self;
}

/**
 * nma_pkcs11_cert_chooser_dialog_get_uri:
 * @dialog: the #NMAPkcs11CertChooserDialog instance
 *
 * Obtain the URI of the selected obejct.
 *
 * Returns: the URI or %NULL if none was selected.
 */
gchar *
nma_pkcs11_cert_chooser_dialog_get_uri (NMAPkcs11CertChooserDialog *dialog)
{
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (dialog);
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GckAttributes *attrs;
	gboolean has_key;
	GckBuilder *builder;
	GckUriData uri_data = { 0, };
	gchar *uri;

	gtk_tree_view_get_cursor (priv->objects_view, &path, NULL);
	if (path == NULL)
		return NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->objects_view));
	if (!gtk_tree_model_get_iter (model, &iter, path))
		g_return_val_if_reached (NULL);

	gtk_tree_model_get (model, &iter,
	                    COLUMN_HAS_KEY, &has_key,
	                    COLUMN_ATTRIBUTES, &attrs, -1);

	builder = gck_builder_new (GCK_BUILDER_NONE);
	if (has_key) {
		/* We do have a object with matching id in the other store (a key)
		 * but its other properties (label) may be unset or missing.
		 * Still, we want an URI that matches both. */
		gck_builder_add_only (builder, attrs, CKA_ID, GCK_INVALID);
	} else {
		gck_builder_add_all (builder, attrs);
	}

	uri_data.attributes = gck_builder_end (builder);
	uri_data.token_info = gck_slot_get_token_info (priv->slot);
	uri = gck_uri_build (&uri_data, GCK_URI_FOR_OBJECT_ON_TOKEN);

	gck_attributes_unref (uri_data.attributes);
	gck_attributes_unref (attrs);

	return uri;
}

/**
 * nma_pkcs11_cert_chooser_dialog_get_pin:
 * @dialog: the #NMAPkcs11CertChooserDialog instance
 *
 * Obtain the PIN that was used to unlock the token.
 *
 * Returns: the PIN, %NULL if the token was not logged into or an emtpy
 *   string ("") if the protected authentication path was used.
 */
gchar *
nma_pkcs11_cert_chooser_dialog_get_pin (NMAPkcs11CertChooserDialog *dialog)
{
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (dialog);

	return g_strdup ((gchar *) priv->pin_value);
}

/**
 * nma_pkcs11_cert_chooser_dialog_get_remember_pin:
 * @dialog: the #NMAPkcs11CertChooserDialog instance
 *
 * Obtain the value of the "Remember PIN" checkbox during the token login.
 *
 * Returns: TRUE if the user chose to remember the PIN, FALSE
 *   if not or if the tokin was not logged into at all.
 */
gboolean
nma_pkcs11_cert_chooser_dialog_get_remember_pin (NMAPkcs11CertChooserDialog *dialog)
{
	NMAPkcs11CertChooserDialogPrivate *priv = NMA_PKCS11_CERT_CHOOSER_DIALOG_GET_PRIVATE (dialog);

	return priv->remember_pin;
}

/**
 * nma_pkcs11_cert_chooser_dialog_new:
 * @slot: the PKCS\#11 slot the token is in
 * @object_class: CKA_CLASS of object to be selected
 * @title: The dialog window title
 * @parent: (allow-none): The parent window or %NULL
 * @flags: The dialog flags
 * @first_button_text: (allow-none): The text of the first button
 * @...: response ID for the first button, texts and response ids for other buttons, terminated with %NULL
 *
 * Creates the new #NMAPkcs11CertChooserDialog.
 *
 * Returns: newly created #NMAPkcs11CertChooserDialog
 */

GtkWidget *
nma_pkcs11_cert_chooser_dialog_new (GckSlot *slot,
                                    CK_OBJECT_CLASS object_class,
                                    const gchar *title,
                                    GtkWindow *parent,
                                    GtkDialogFlags flags,
                                    const gchar *first_button_text,
                                    ...)
{
	GtkWidget *result;
	va_list varargs;

	va_start (varargs, first_button_text);
	result = nma_pkcs11_cert_chooser_dialog_new_valist (slot,
	                                                    object_class,
	                                                    title,
	                                                    parent,
	                                                    flags,
	                                                    first_button_text,
	                                                    varargs);
	va_end (varargs);

	return result;
}
