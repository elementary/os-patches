/* -*- Mode: C; tab-width: 5; indent-tabs-mode: t; c-basic-offset: 5 -*- */

/* NetworkManager Applet -- allow user control over networking
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
 * Copyright 2007 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "eap-method.h"
#include "nm-utils.h"
#include "utils.h"
#include "helpers.h"

G_DEFINE_BOXED_TYPE (EAPMethod, eap_method, eap_method_ref, eap_method_unref)

GtkWidget *
eap_method_get_widget (EAPMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);

	return method->ui_widget;
}

gboolean
eap_method_validate (EAPMethod *method, GError **error)
{
	gboolean result;

	g_return_val_if_fail (method != NULL, FALSE);

	g_assert (method->validate);
	result = (*(method->validate)) (method, error);
	if (!result && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("undefined error in 802.1X security (wpa-eap)"));
	return result;
}

void
eap_method_add_to_size_group (EAPMethod *method, GtkSizeGroup *group)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (group != NULL);

	g_assert (method->add_to_size_group);
	return (*(method->add_to_size_group)) (method, group);
}

void
eap_method_fill_connection (EAPMethod *method,
                            NMConnection *connection)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (connection != NULL);

	g_assert (method->fill_connection);
	return (*(method->fill_connection)) (method, connection);
}

void
eap_method_update_secrets (EAPMethod *method, NMConnection *connection)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (connection != NULL);

	if (method->update_secrets)
		method->update_secrets (method, connection);
}

void
eap_method_phase2_update_secrets_helper (EAPMethod *method,
                                         NMConnection *connection,
                                         const char *combo_name,
                                         guint32 column)
{
	GtkWidget *combo;
	GtkTreeIter iter;
	GtkTreeModel *model;

	g_return_if_fail (method != NULL);
	g_return_if_fail (connection != NULL);
	g_return_if_fail (combo_name != NULL);

	combo = GTK_WIDGET (gtk_builder_get_object (method->builder, combo_name));
	g_assert (combo);

	/* Let each EAP phase2 method try to update its secrets */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			EAPMethod *eap = NULL;

			gtk_tree_model_get (model, &iter, column, &eap, -1);
			if (eap) {
				eap_method_update_secrets (eap, connection);
				eap_method_unref (eap);
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

EAPMethod *
eap_method_init (gsize obj_size,
                 EMValidateFunc validate,
                 EMAddToSizeGroupFunc add_to_size_group,
                 EMFillConnectionFunc fill_connection,
                 EMUpdateSecretsFunc update_secrets,
                 EMDestroyFunc destroy,
                 const char *ui_resource,
                 const char *ui_widget_name,
                 const char *default_field,
                 gboolean phase2)
{
	EAPMethod *method;
	GError *error = NULL;

	g_return_val_if_fail (obj_size > 0, NULL);
	g_return_val_if_fail (ui_resource != NULL, NULL);
	g_return_val_if_fail (ui_widget_name != NULL, NULL);

	method = g_slice_alloc0 (obj_size);
	g_assert (method);

	method->refcount = 1;
	method->obj_size = obj_size;
	method->validate = validate;
	method->add_to_size_group = add_to_size_group;
	method->fill_connection = fill_connection;
	method->update_secrets = update_secrets;
	method->default_field = default_field;
	method->phase2 = phase2;

	method->builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (method->builder, ui_resource, &error)) {
		g_warning ("Couldn't load UI builder resource %s: %s",
		           ui_resource, error->message);
		eap_method_unref (method);
		return NULL;
	}

	method->ui_widget = GTK_WIDGET (gtk_builder_get_object (method->builder, ui_widget_name));
	if (!method->ui_widget) {
		g_warning ("Couldn't load UI widget '%s' from UI file %s",
		           ui_widget_name, ui_resource);
		eap_method_unref (method);
		return NULL;
	}
	g_object_ref_sink (method->ui_widget);

	method->destroy = destroy;

	return method;
}


EAPMethod *
eap_method_ref (EAPMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);
	g_return_val_if_fail (method->refcount > 0, NULL);

	method->refcount++;
	return method;
}

void
eap_method_unref (EAPMethod *method)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (method->refcount > 0);

	method->refcount--;
	if (method->refcount == 0) {
		if (method->destroy)
			method->destroy (method);

		if (method->builder)
			g_object_unref (method->builder);
		if (method->ui_widget)
			g_object_unref (method->ui_widget);

		g_slice_free1 (method->obj_size, method);
	}
}

/* Used as both GSettings keys and GObject data tags */
#define IGNORE_CA_CERT_TAG "ignore-ca-cert"
#define IGNORE_PHASE2_CA_CERT_TAG "ignore-phase2-ca-cert"

/**
 * eap_method_ca_cert_ignore_set:
 * @method: the #EAPMethod object
 * @connection: the #NMConnection
 * @filename: the certificate file, if any
 * @ca_cert_error: %TRUE if an error was encountered loading the given CA
 * certificate, %FALSE if not or if a CA certificate is not present
 *
 * Updates the connection's CA cert ignore value to %TRUE if the "CA certificate
 * not required" checkbox is checked.  If @ca_cert_error is %TRUE, then the
 * connection's CA cert ignore value will always be set to %FALSE, because it
 * means that the user selected an invalid certificate (thus he does not want to
 * ignore the CA cert)..
 */
void
eap_method_ca_cert_ignore_set (EAPMethod *method,
                               NMConnection *connection,
                               const char *filename,
                               gboolean ca_cert_error)
{
	NMSetting8021x *s_8021x;
	gboolean ignore;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x) {
		ignore = !ca_cert_error && filename == NULL;
		g_object_set_data (G_OBJECT (s_8021x),
		                   method->phase2 ? IGNORE_PHASE2_CA_CERT_TAG : IGNORE_CA_CERT_TAG,
		                   GUINT_TO_POINTER (ignore));
	}
}

/**
 * eap_method_ca_cert_ignore_get:
 * @method: the #EAPMethod object
 * @connection: the #NMConnection
 *
 * Returns: %TRUE if a missing CA certificate can be ignored, %FALSE if a CA
 * certificate should be required for the connection to be valid.
 */
gboolean
eap_method_ca_cert_ignore_get (EAPMethod *method, NMConnection *connection)
{
	NMSetting8021x *s_8021x;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x) {
		return !!g_object_get_data (G_OBJECT (s_8021x),
		                            method->phase2 ? IGNORE_PHASE2_CA_CERT_TAG : IGNORE_CA_CERT_TAG);
	}
	return FALSE;
}

static GSettings *
_get_ca_ignore_settings (NMConnection *connection)
{
	GSettings *settings;
	char *path = NULL;
	const char *uuid;

	g_return_val_if_fail (connection, NULL);

	uuid = nm_connection_get_uuid (connection);
	g_return_val_if_fail (uuid && *uuid, NULL);

	path = g_strdup_printf ("/org/gnome/nm-applet/eap/%s/", uuid);
	settings = g_settings_new_with_path ("org.gnome.nm-applet.eap", path);
	g_free (path);

	return settings;
}

/**
 * eap_method_ca_cert_ignore_save:
 * @connection: the connection for which to save CA cert ignore values to GSettings
 *
 * Reads the CA cert ignore tags from the 802.1x setting GObject data and saves
 * then to GSettings if present, using the connection UUID as the index.
 */
void
eap_method_ca_cert_ignore_save (NMConnection *connection)
{
	NMSetting8021x *s_8021x;
	GSettings *settings;
	gboolean ignore = FALSE, phase2_ignore = FALSE;

	g_return_if_fail (connection);

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x) {
		ignore = !!g_object_get_data (G_OBJECT (s_8021x), IGNORE_CA_CERT_TAG);
		phase2_ignore = !!g_object_get_data (G_OBJECT (s_8021x), IGNORE_PHASE2_CA_CERT_TAG);
	}

	settings = _get_ca_ignore_settings (connection);
	if (!settings)
		return;

	g_settings_set_boolean (settings, IGNORE_CA_CERT_TAG, ignore);
	g_settings_set_boolean (settings, IGNORE_PHASE2_CA_CERT_TAG, phase2_ignore);
	g_object_unref (settings);
}

/**
 * eap_method_ca_cert_ignore_load:
 * @connection: the connection for which to load CA cert ignore values to GSettings
 *
 * Reads the CA cert ignore tags from the 802.1x setting GObject data and saves
 * then to GSettings if present, using the connection UUID as the index.
 */
void
eap_method_ca_cert_ignore_load (NMConnection *connection)
{
	GSettings *settings;
	NMSetting8021x *s_8021x;
	gboolean ignore, phase2_ignore;

	g_return_if_fail (connection);

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (!s_8021x)
		return;

	settings = _get_ca_ignore_settings (connection);
	if (!settings)
		return;

	ignore = g_settings_get_boolean (settings, IGNORE_CA_CERT_TAG);
	phase2_ignore = g_settings_get_boolean (settings, IGNORE_PHASE2_CA_CERT_TAG);

	g_object_set_data (G_OBJECT (s_8021x),
	                   IGNORE_CA_CERT_TAG,
	                   GUINT_TO_POINTER (ignore));
	g_object_set_data (G_OBJECT (s_8021x),
	                   IGNORE_PHASE2_CA_CERT_TAG,
	                   GUINT_TO_POINTER (phase2_ignore));
	g_object_unref (settings);
}

GError *
eap_method_ca_cert_validate_cb (NMACertChooser *cert_chooser, gpointer user_data)
{
	NMSetting8021xCKScheme scheme;
        NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	gs_unref_object NMSetting8021x *setting = NULL;
	gs_free char *value = NULL;
	GError *local = NULL;

	setting = (NMSetting8021x *) nm_setting_802_1x_new ();

	value = nma_cert_chooser_get_cert (cert_chooser, &scheme);
	if (!value) {
		return g_error_new_literal (NMA_ERROR, NMA_ERROR_GENERIC,
		                            _("no CA certificate selected"));
	}
	if (scheme == NM_SETTING_802_1X_CK_SCHEME_PATH) {
		if (!g_file_test (value, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			return g_error_new_literal (NMA_ERROR, NMA_ERROR_GENERIC,
			                            _("selected CA certificate file does not exist"));
		}
	}

	if (!nm_setting_802_1x_set_ca_cert (setting, value, scheme, &format, &local))
		return local;

	return NULL;
}

void
eap_method_setup_cert_chooser (NMACertChooser *cert_chooser,
                               NMSetting8021x *s_8021x,
                               NMSetting8021xCKScheme (*cert_scheme_func) (NMSetting8021x *setting),
                               const char *(*cert_path_func) (NMSetting8021x *setting),
                               const char *(*cert_uri_func) (NMSetting8021x *setting),
                               const char *(*cert_password_func) (NMSetting8021x *setting),
                               NMSetting8021xCKScheme (*key_scheme_func) (NMSetting8021x *setting),
                               const char *(*key_path_func) (NMSetting8021x *setting),
                               const char *(*key_uri_func) (NMSetting8021x *setting),
                               const char *(*key_password_func) (NMSetting8021x *setting))
{
	NMSetting8021xCKScheme scheme = NM_SETTING_802_1X_CK_SCHEME_UNKNOWN;
	const char *value = NULL;


	if (s_8021x && cert_path_func && cert_uri_func && cert_scheme_func) {
		scheme = cert_scheme_func (s_8021x);
		switch (scheme) {
		case NM_SETTING_802_1X_CK_SCHEME_PATH:
			value = cert_path_func (s_8021x);
			break;
#if LIBNM_BUILD
/* Not available in libnm-glib */
		case NM_SETTING_802_1X_CK_SCHEME_PKCS11:
			value = cert_uri_func (s_8021x);
			if (cert_password_func)
				nma_cert_chooser_set_cert_password (cert_chooser, cert_password_func (s_8021x));
			break;
#endif
		case NM_SETTING_802_1X_CK_SCHEME_UNKNOWN:
			/* No CA set. */
			break;
		default:
			g_warning ("unhandled certificate scheme %d", scheme);
		}

	}
	nma_cert_chooser_set_cert (cert_chooser, value, scheme);

	if (s_8021x && key_path_func && key_uri_func && key_scheme_func) {
		scheme = key_scheme_func (s_8021x);
		switch (scheme) {
		case NM_SETTING_802_1X_CK_SCHEME_PATH:
			value = key_path_func (s_8021x);
			break;
#if LIBNM_BUILD
/* Not available in libnm-glib */
		case NM_SETTING_802_1X_CK_SCHEME_PKCS11:
			value = key_uri_func (s_8021x);
			break;
#endif
		case NM_SETTING_802_1X_CK_SCHEME_UNKNOWN:
			/* No certificate set. */
			break;
		default:
			g_warning ("unhandled key scheme %d", scheme);
		}

		nma_cert_chooser_set_key (cert_chooser, value, scheme);
	}

	if (s_8021x && key_password_func)
		nma_cert_chooser_set_key_password (cert_chooser, key_password_func (s_8021x));
}
