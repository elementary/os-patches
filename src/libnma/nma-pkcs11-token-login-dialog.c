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
#include "nma-pkcs11-token-login-dialog.h"

#include <gck/gck.h>

/**
 * SECTION:nma-pkcs11-token-login-dialog
 * @title: NMAPkcs11TokenLoginDialog
 * @short_description: The PKCS\#11 PIN Dialog
 * @see_also: #GcrObjectChooserDialog
 *
 * #NMAPkcs11TokenLoginDialog asks for the PKCS\#11 login PIN.
 * It enforces the PIN constrains (maximum & minimum length).
 *
 * Used by the #GcrObjectChooserDialog when the #GcrTokensSidebar indicates
 * that the user requested the token to be logged in.
 */

struct _NMAPkcs11TokenLoginDialogPrivate
{
	GckSlot *slot;
	GckTokenInfo *info;

	GtkEntry *pin_entry;
	GtkCheckButton *remember;
};

G_DEFINE_TYPE_WITH_CODE (NMAPkcs11TokenLoginDialog, nma_pkcs11_token_login_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (NMAPkcs11TokenLoginDialog));

enum
{
	PROP_0,
	PROP_TOKEN_SLOT,
};

/**
 * nma_pkcs11_token_login_dialog_get_pin_value:
 * @self: The #NMAPkcs11TokenLoginDialog
 *
 * Returns: the entered PIN
 */

const guchar *
nma_pkcs11_token_login_dialog_get_pin_value (NMAPkcs11TokenLoginDialog *self)
{
	NMAPkcs11TokenLoginDialogPrivate *priv = self->priv;
	GtkEntryBuffer *buffer = gtk_entry_get_buffer (priv->pin_entry);

	return (guchar *) gtk_entry_buffer_get_text (buffer);
}

/**
 * nma_pkcs11_token_login_dialog_get_pin_length:
 * @self: The #NMAPkcs11TokenLoginDialog
 *
 * Returns: the PIN length
 */

gulong
nma_pkcs11_token_login_dialog_get_pin_length (NMAPkcs11TokenLoginDialog *self)
{
	NMAPkcs11TokenLoginDialogPrivate *priv = self->priv;
	GtkEntryBuffer *buffer = gtk_entry_get_buffer (priv->pin_entry);

	return gtk_entry_buffer_get_bytes (buffer);
}

/**
 * nma_pkcs11_token_login_dialog_get_remember_pin:
 * @self: The #NMAPkcs11TokenLoginDialog
 *
 * Returns: %TRUE if the "Remember PIN" checkbox was checked
 */

gboolean
nma_pkcs11_token_login_dialog_get_remember_pin (NMAPkcs11TokenLoginDialog *self)
{
	NMAPkcs11TokenLoginDialogPrivate *priv = self->priv;

	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->remember));
}

static void
get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	NMAPkcs11TokenLoginDialog *self = NMA_PKCS11_TOKEN_LOGIN_DIALOG (object);
	NMAPkcs11TokenLoginDialogPrivate *priv = self->priv;

	switch (prop_id) {
	case PROP_TOKEN_SLOT:
		if (priv->slot)
			g_value_set_object (value, priv->slot);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static gboolean
can_activate (NMAPkcs11TokenLoginDialog *self)
{
	NMAPkcs11TokenLoginDialogPrivate *priv = self->priv;
	GtkEntryBuffer *buffer = gtk_entry_get_buffer (priv->pin_entry);
	guint len = gtk_entry_buffer_get_length (buffer);

	return len <= priv->info->max_pin_len && len >= priv->info->min_pin_len;
}

static void
set_slot (NMAPkcs11TokenLoginDialog *self, GckSlot *slot)
{
	NMAPkcs11TokenLoginDialogPrivate *priv = self->priv;
	gchar *title;

	g_clear_object (&priv->slot);
	if (priv->info)
		gck_token_info_free (priv->info);

	priv->slot = slot;
	priv->info = gck_slot_get_token_info (slot);
	g_return_if_fail (priv->info);

	title = g_strdup_printf (_("Enter %s PIN"), priv->info->label);
	gtk_window_set_title (GTK_WINDOW (self), title);
	g_free (title);

	gtk_entry_set_max_length (priv->pin_entry, priv->info->max_pin_len);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT,
	                                   can_activate (self));
}


static void
set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	NMAPkcs11TokenLoginDialog *self = NMA_PKCS11_TOKEN_LOGIN_DIALOG (object);

	switch (prop_id) {
	case PROP_TOKEN_SLOT:
		set_slot (self, g_value_dup_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
finalize (GObject *object)
{
	NMAPkcs11TokenLoginDialog *self = NMA_PKCS11_TOKEN_LOGIN_DIALOG (object);
	NMAPkcs11TokenLoginDialogPrivate *priv = self->priv;

	g_clear_object (&priv->slot);
	if (priv->info) {
		gck_token_info_free (priv->info);
		priv->info = NULL;
	}

	G_OBJECT_CLASS (nma_pkcs11_token_login_dialog_parent_class)->finalize (object);
}

static void
pin_changed (GtkEditable *editable, gpointer user_data)
{
	gtk_dialog_set_response_sensitive (GTK_DIALOG (user_data), GTK_RESPONSE_ACCEPT,
	                                   can_activate (NMA_PKCS11_TOKEN_LOGIN_DIALOG (user_data)));
}


static void
pin_activate (GtkEditable *editable, gpointer user_data)
{
	if (can_activate (NMA_PKCS11_TOKEN_LOGIN_DIALOG (user_data)))
		gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_ACCEPT);
}

static void
nma_pkcs11_token_login_dialog_class_init (NMAPkcs11TokenLoginDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->finalize = finalize;

	/**
	 * NMAPkcs11TokenLoginDialog::token-slot:
	 *
	 * Slot that contains the pin for which the dialog requests
	 * the PIN.
	 */
	g_object_class_install_property (object_class, PROP_TOKEN_SLOT,
		g_param_spec_object ("token-slot", "Slot", "Slot containing the Token",
		                     GCK_TYPE_SLOT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gtk_widget_class_set_template_from_resource (widget_class, "/org/freedesktop/network-manager-applet/nma-pkcs11-token-login-dialog.ui");
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11TokenLoginDialog, pin_entry);
	gtk_widget_class_bind_template_child_private (widget_class, NMAPkcs11TokenLoginDialog, remember);
	gtk_widget_class_bind_template_callback (widget_class, pin_changed);
	gtk_widget_class_bind_template_callback (widget_class, pin_activate);
}

static void
nma_pkcs11_token_login_dialog_init (NMAPkcs11TokenLoginDialog *self)
{
	self->priv = nma_pkcs11_token_login_dialog_get_instance_private (self);

	gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * nma_pkcs11_token_login_dialog_new:
 * @slot: Slot that contains the pin for which the dialog requests the PIN
 *
 * Creates the new PKCS\#11 login dialog.
 *
 * Returns: the newly created #NMAPkcs11TokenLoginDialog
 */
GtkWidget *
nma_pkcs11_token_login_dialog_new (GckSlot *slot)
{
	return GTK_WIDGET (g_object_new (NMA_TYPE_PKCS11_TOKEN_LOGIN_DIALOG,
	                   "use-header-bar", TRUE,
	                   "token-slot", slot,
	                   NULL));
}
