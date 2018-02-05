/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
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
 * Copyright 2007 - 2017 Red Hat, Inc.
 */

#include "nm-default.h"

#include <ctype.h>
#include <string.h>

#include "eap-method.h"
#include "wireless-security.h"
#include "helpers.h"
#include "nma-ui-utils.h"
#include "nma-cert-chooser.h"
#include "utils.h"

struct _EAPMethodTLS {
	EAPMethod parent;

#if LIBNM_BUILD
/* libnm-glib doesn't support these. */
	const char *ca_cert_password_flags_name;
	const char *client_cert_password_flags_name;
#endif
	const char *client_key_password_flags_name;

	gboolean editing_connection;
	GtkWidget *ca_cert_chooser;
	GtkWidget *client_cert_chooser;
};


static gboolean
validate (EAPMethod *parent, GError **error)
{
	EAPMethodTLS *method = (EAPMethodTLS *) parent;
	GtkWidget *widget;
	const char *identity;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_entry"));
	g_assert (widget);
	identity = gtk_entry_get_text (GTK_ENTRY (widget));
	if (!identity || !strlen (identity)) {
		widget_set_error (widget);
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP-TLS identity"));
		return FALSE;
	} else {
		widget_unset_error (widget);
	}

	if (   gtk_widget_get_sensitive (method->ca_cert_chooser)
	    && !nma_cert_chooser_validate (NMA_CERT_CHOOSER (method->ca_cert_chooser), error))
		return FALSE;

	if (!nma_cert_chooser_validate (NMA_CERT_CHOOSER (method->client_cert_chooser), error))
		return FALSE;

	return TRUE;
}

static void
ca_cert_not_required_toggled (GtkWidget *ignored, gpointer user_data)
{
	EAPMethodTLS *method = (EAPMethodTLS *) user_data;

	gtk_widget_set_sensitive (method->ca_cert_chooser,
	                          !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ignored)));
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodTLS *method = (EAPMethodTLS *) parent;
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_domain_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	nma_cert_chooser_add_to_size_group (NMA_CERT_CHOOSER (method->client_cert_chooser), group);
	nma_cert_chooser_add_to_size_group (NMA_CERT_CHOOSER (method->ca_cert_chooser), group);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodTLS *method = (EAPMethodTLS *) parent;
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	NMSetting8021x *s_8021x;
	NMSettingSecretFlags secret_flags;
	GtkWidget *widget;
	char *value = NULL;
	const char *password = NULL;
	GError *error = NULL;
	gboolean ca_cert_error = FALSE;
	NMSetting8021xCKScheme scheme;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	if (parent->phase2)
		g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, "tls", NULL);
	else
		nm_setting_802_1x_add_eap_method (s_8021x, "tls");

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_entry"));
	g_assert (widget);
	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (GTK_ENTRY (widget)), NULL);

#if LIBNM_BUILD
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_domain_entry"));
	g_assert (widget);
	g_object_set (s_8021x,
	              parent->phase2 ? NM_SETTING_802_1X_PHASE2_DOMAIN_SUFFIX_MATCH : NM_SETTING_802_1X_DOMAIN_SUFFIX_MATCH,
	              gtk_entry_get_text (GTK_ENTRY (widget)), NULL);
#endif

	/* TLS private key */
	password = nma_cert_chooser_get_key_password (NMA_CERT_CHOOSER (method->client_cert_chooser));
	value = nma_cert_chooser_get_key (NMA_CERT_CHOOSER (method->client_cert_chooser), &scheme);

	if (parent->phase2) {
		if (!nm_setting_802_1x_set_phase2_private_key (s_8021x, value, password, scheme, &format, &error)) {
			g_warning ("Couldn't read phase2 private key '%s': %s", value, error ? error->message : "(unknown)");
			g_clear_error (&error);
		}
	} else {
		if (!nm_setting_802_1x_set_private_key (s_8021x, value, password, scheme, &format, &error)) {
			g_warning ("Couldn't read private key '%s': %s", value, error ? error->message : "(unknown)");
			g_clear_error (&error);
		}
	}
	g_free (value);

#if LIBNM_BUILD
/* libnm-glib doesn't support these. */
	/* Save CA certificate PIN and its flags to the connection */
	secret_flags = nma_cert_chooser_get_cert_password_flags (NMA_CERT_CHOOSER (method->ca_cert_chooser));
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), method->ca_cert_password_flags_name,
	                             secret_flags, NULL);
	if (method->editing_connection) {
		/* Update secret flags and popup when editing the connection */
		nma_cert_chooser_update_cert_password_storage (NMA_CERT_CHOOSER (method->ca_cert_chooser),
		                                               secret_flags, NM_SETTING (s_8021x),
		                                               method->ca_cert_password_flags_name);
		g_object_set (s_8021x, method->ca_cert_password_flags_name,
		              nma_cert_chooser_get_cert_password (NMA_CERT_CHOOSER (method->ca_cert_chooser)),
		              NULL);
	}

	/* Save user certificate PIN and its flags flags to the connection */
	secret_flags = nma_cert_chooser_get_cert_password_flags (NMA_CERT_CHOOSER (method->client_cert_chooser));
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), method->client_cert_password_flags_name,
	                             secret_flags, NULL);
	if (method->editing_connection) {
		nma_cert_chooser_update_cert_password_storage (NMA_CERT_CHOOSER (method->client_cert_chooser),
		                                               secret_flags, NM_SETTING (s_8021x),
		                                               method->client_cert_password_flags_name);
		g_object_set (s_8021x, method->client_cert_password_flags_name,
		              nma_cert_chooser_get_cert_password (NMA_CERT_CHOOSER (method->client_cert_chooser)),
		              NULL);
	}
#endif

	/* Save user private key password flags to the connection */
	secret_flags = nma_cert_chooser_get_key_password_flags (NMA_CERT_CHOOSER (method->client_cert_chooser));
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), method->client_key_password_flags_name,
	                             secret_flags, NULL);
	if (method->editing_connection) {
		nma_cert_chooser_update_key_password_storage (NMA_CERT_CHOOSER (method->client_cert_chooser),
		                                              secret_flags, NM_SETTING (s_8021x),
		                                              method->client_key_password_flags_name);
	}

	/* TLS client certificate */
	if (format != NM_SETTING_802_1X_CK_FORMAT_PKCS12) {
		/* If the key is pkcs#12 nm_setting_802_1x_set_private_key() already
		 * set the client certificate for us.
		 */
		value = nma_cert_chooser_get_cert (NMA_CERT_CHOOSER (method->client_cert_chooser), &scheme);
		format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
		if (parent->phase2) {
			if (!nm_setting_802_1x_set_phase2_client_cert (s_8021x, value, scheme, &format, &error)) {
				g_warning ("Couldn't read phase2 client certificate '%s': %s", value, error ? error->message : "(unknown)");
				g_clear_error (&error);
			}
		} else {
			if (!nm_setting_802_1x_set_client_cert (s_8021x, value, scheme, &format, &error)) {
				g_warning ("Couldn't read client certificate '%s': %s", value, error ? error->message : "(unknown)");
				g_clear_error (&error);
			}
		}
		g_free (value);
	}

	/* TLS CA certificate */
	if (gtk_widget_get_sensitive (method->ca_cert_chooser))
		value = nma_cert_chooser_get_cert (NMA_CERT_CHOOSER (method->ca_cert_chooser), &scheme);
	else
		value = NULL;
	format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	if (parent->phase2) {
		if (!nm_setting_802_1x_set_phase2_ca_cert (s_8021x, value, scheme, &format, &error)) {
			g_warning ("Couldn't read phase2 CA certificate '%s': %s", value, error ? error->message : "(unknown)");
			g_clear_error (&error);
			ca_cert_error = TRUE;
		}
	} else {
		if (!nm_setting_802_1x_set_ca_cert (s_8021x, value, scheme, &format, &error)) {
			g_warning ("Couldn't read CA certificate '%s': %s", value, error ? error->message : "(unknown)");
			g_clear_error (&error);
			ca_cert_error = TRUE;
		}
	}
	eap_method_ca_cert_ignore_set (parent, connection, value, ca_cert_error);
	g_free (value);
}

static GError *
client_cert_validate_cb (NMACertChooser *cert_chooser, gpointer user_data)
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
		                            _("no user certificate selected"));
	}
	if (scheme == NM_SETTING_802_1X_CK_SCHEME_PATH) {
		if (!g_file_test (value, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			return g_error_new_literal (NMA_ERROR, NMA_ERROR_GENERIC,
			                            _("selected user certificate file does not exist"));
		}
	}

	if (!nm_setting_802_1x_set_client_cert (setting, value, scheme, &format, &local))
		return local;

	return NULL;
}

static GError *
client_key_validate_cb (NMACertChooser *cert_chooser, gpointer user_data)
{
	NMSetting8021xCKScheme scheme;
	gs_free char *value = NULL;


	value = nma_cert_chooser_get_key (cert_chooser, &scheme);
	if (!value) {
		return g_error_new_literal (NMA_ERROR, NMA_ERROR_GENERIC,
		                            _("no key selected"));
	}
	if (scheme == NM_SETTING_802_1X_CK_SCHEME_PATH) {
		if (!g_file_test (value, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			return g_error_new_literal (NMA_ERROR, NMA_ERROR_GENERIC,
			                            _("selected key file does not exist"));
		}
	}

	return NULL;
}

static GError *
client_key_password_validate_cb (NMACertChooser *cert_chooser, gpointer user_data)
{
	NMSetting8021xCKScheme scheme;
	NMSettingSecretFlags secret_flags;
	gs_unref_object NMSetting8021x *setting = NULL;
	gs_free char *value = NULL;
	const char *password = NULL;
	GError *local = NULL;

	secret_flags = nma_cert_chooser_get_key_password_flags (cert_chooser);
	if (   secret_flags & NM_SETTING_SECRET_FLAG_NOT_SAVED
	    || secret_flags & NM_SETTING_SECRET_FLAG_NOT_REQUIRED)
		return NULL;

	setting = (NMSetting8021x *) nm_setting_802_1x_new ();

	value = nma_cert_chooser_get_key (cert_chooser, &scheme);
	password = nma_cert_chooser_get_key_password (cert_chooser);
	if (!nm_setting_802_1x_set_private_key (setting, value, password, scheme, NULL, &local))
		return local;

	return NULL;
}

static void
client_cert_fixup_pkcs12 (NMACertChooser *cert_chooser, gpointer user_data)
{
	NMSetting8021xCKScheme cert_scheme, key_scheme;
        NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	gs_free char *cert_value = NULL;
	gs_free char *key_value = NULL;
	gs_unref_object NMSetting8021x *setting = NULL;

	setting = (NMSetting8021x *) nm_setting_802_1x_new ();

	cert_value = nma_cert_chooser_get_cert (cert_chooser, &cert_scheme);
	key_value = nma_cert_chooser_get_key (cert_chooser, &key_scheme);

	if (   !cert_value || key_value
	    || !nm_setting_802_1x_set_client_cert (setting, cert_value, cert_scheme, &format, NULL))
		return;

	if (format == NM_SETTING_802_1X_CK_FORMAT_PKCS12)
		nma_cert_chooser_set_key (cert_chooser, cert_value, cert_scheme);
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodTLS *method = (EAPMethodTLS *) parent;

	eap_method_setup_cert_chooser (NMA_CERT_CHOOSER (method->client_cert_chooser),
	                               nm_connection_get_setting_802_1x (connection),
	                               NULL,
	                               NULL,
	                               NULL,
	                               parent->phase2 ? nm_setting_802_1x_get_phase2_client_cert_password : nm_setting_802_1x_get_client_cert_password,
	                               parent->phase2 ? nm_setting_802_1x_get_phase2_private_key_scheme : nm_setting_802_1x_get_private_key_scheme,
	                               parent->phase2 ? nm_setting_802_1x_get_phase2_private_key_path : nm_setting_802_1x_get_private_key_path,
	                               parent->phase2 ? nm_setting_802_1x_get_phase2_private_key_uri : nm_setting_802_1x_get_private_key_uri,
	                               parent->phase2 ? nm_setting_802_1x_get_phase2_private_key_password : nm_setting_802_1x_get_private_key_password);
}

EAPMethodTLS *
eap_method_tls_new (WirelessSecurity *ws_parent,
                    NMConnection *connection,
                    gboolean phase2,
                    gboolean secrets_only)
{
	EAPMethodTLS *method;
	EAPMethod *parent;
	GtkWidget *widget;
	NMSetting8021x *s_8021x = NULL;
	gboolean ca_not_required = FALSE;

	parent = eap_method_init (sizeof (EAPMethodTLS),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          NULL,
	                          "/org/freedesktop/network-manager-applet/eap-method-tls.ui",
	                          "eap_tls_notebook",
	                          "eap_tls_identity_entry",
	                          phase2);
	if (!parent)
		return NULL;

	method = (EAPMethodTLS *) parent;
#if LIBNM_BUILD
/* libnm-glib doesn't support these. */
	method->ca_cert_password_flags_name = phase2
	                                      ? NM_SETTING_802_1X_PHASE2_CA_CERT_PASSWORD
	                                      : NM_SETTING_802_1X_CA_CERT_PASSWORD;
	method->client_cert_password_flags_name = phase2
	                                          ? NM_SETTING_802_1X_PHASE2_CLIENT_CERT_PASSWORD
	                                          : NM_SETTING_802_1X_CLIENT_CERT_PASSWORD;
#endif
	method->client_key_password_flags_name = phase2
	                                         ? NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD
	                                         : NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD;
	method->editing_connection = secrets_only ? FALSE : TRUE;

	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_ca_cert_not_required_checkbox"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) ca_cert_not_required_toggled,
	                  parent);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);
	if (s_8021x && nm_setting_802_1x_get_identity (s_8021x))
		gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_802_1x_get_identity (s_8021x));

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_domain_entry"));
	g_assert (widget);
#if LIBNM_BUILD
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);
	if (phase2) {
		if (s_8021x && nm_setting_802_1x_get_phase2_domain_suffix_match (s_8021x))
			gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_802_1x_get_phase2_domain_suffix_match (s_8021x));
	} else {
		if (s_8021x && nm_setting_802_1x_get_domain_suffix_match (s_8021x))
			gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_802_1x_get_domain_suffix_match (s_8021x));
	}
#else
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_domain_label"));
	gtk_widget_hide (widget);
#endif

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_grid"));
	g_assert (widget);

	method->ca_cert_chooser = nma_cert_chooser_new ("CA",
	                                                  NMA_CERT_CHOOSER_FLAG_CERT
	                                                | (secrets_only ? NMA_CERT_CHOOSER_FLAG_PASSWORDS : 0));
	gtk_grid_attach (GTK_GRID (widget), method->ca_cert_chooser, 0, 2, 2, 1);
	gtk_widget_show (method->ca_cert_chooser);

	g_signal_connect (method->ca_cert_chooser,
	                  "cert-validate",
	                  G_CALLBACK (eap_method_ca_cert_validate_cb),
	                  NULL);
	g_signal_connect (method->ca_cert_chooser,
	                  "changed",
	                  G_CALLBACK (wireless_security_changed_cb),
	                  ws_parent);

	eap_method_setup_cert_chooser (NMA_CERT_CHOOSER (method->ca_cert_chooser), s_8021x,
	                               phase2 ? nm_setting_802_1x_get_phase2_ca_cert_scheme : nm_setting_802_1x_get_ca_cert_scheme,
	                               phase2 ? nm_setting_802_1x_get_phase2_ca_cert_path : nm_setting_802_1x_get_ca_cert_path,
	                               phase2 ? nm_setting_802_1x_get_phase2_ca_cert_uri : nm_setting_802_1x_get_ca_cert_uri,
	                               phase2 ? nm_setting_802_1x_get_phase2_ca_cert_password : nm_setting_802_1x_get_ca_cert_password,
	                               NULL,
	                               NULL,
	                               NULL,
	                               NULL);

	if (connection && eap_method_ca_cert_ignore_get (parent, connection)) {
		gchar *ca_cert;
		NMSetting8021xCKScheme scheme;

		ca_cert = nma_cert_chooser_get_cert (NMA_CERT_CHOOSER (method->ca_cert_chooser), &scheme);
		if (ca_cert)
			g_free (ca_cert);
		else
			ca_not_required = TRUE;
	}

	method->client_cert_chooser = nma_cert_chooser_new ("User",
	                                                    secrets_only ? NMA_CERT_CHOOSER_FLAG_PASSWORDS : 0);
	gtk_grid_attach (GTK_GRID (widget), method->client_cert_chooser, 0, 4, 2, 1);
	gtk_widget_show (method->client_cert_chooser);

	g_signal_connect (method->client_cert_chooser, "cert-validate",
	                  G_CALLBACK (client_cert_validate_cb),
	                  NULL);
	g_signal_connect (method->client_cert_chooser,
	                  "key-validate",
	                  G_CALLBACK (client_key_validate_cb),
	                  NULL);
	g_signal_connect (method->client_cert_chooser,
	                  "key-password-validate",
	                  G_CALLBACK (client_key_password_validate_cb),
	                  NULL);
	g_signal_connect (method->client_cert_chooser,
	                  "changed",
	                  G_CALLBACK (client_cert_fixup_pkcs12),
	                  ws_parent);
	g_signal_connect (method->client_cert_chooser,
	                  "changed",
	                  G_CALLBACK (wireless_security_changed_cb),
	                  ws_parent);

	eap_method_setup_cert_chooser (NMA_CERT_CHOOSER (method->client_cert_chooser), s_8021x,
	                               phase2 ? nm_setting_802_1x_get_phase2_client_cert_scheme : nm_setting_802_1x_get_client_cert_scheme,
	                               phase2 ? nm_setting_802_1x_get_phase2_client_cert_path : nm_setting_802_1x_get_client_cert_path,
	                               phase2 ? nm_setting_802_1x_get_phase2_client_cert_uri : nm_setting_802_1x_get_client_cert_uri,
	                               phase2 ? nm_setting_802_1x_get_phase2_client_cert_password : nm_setting_802_1x_get_client_cert_password,
	                               phase2 ? nm_setting_802_1x_get_phase2_private_key_scheme : nm_setting_802_1x_get_private_key_scheme,
	                               phase2 ? nm_setting_802_1x_get_phase2_private_key_path : nm_setting_802_1x_get_private_key_path,
	                               phase2 ? nm_setting_802_1x_get_phase2_private_key_uri : nm_setting_802_1x_get_private_key_uri,
	                               phase2 ? nm_setting_802_1x_get_phase2_private_key_password : nm_setting_802_1x_get_private_key_password);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_ca_cert_not_required_checkbox"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), ca_not_required);

	/* Create password-storage popup menus for password entries under their secondary icon */
#if LIBNM_BUILD
/* libnm-glib doesn't support these. */
	nma_cert_chooser_setup_cert_password_storage (NMA_CERT_CHOOSER (method->ca_cert_chooser),
	                                              0, (NMSetting *) s_8021x, method->ca_cert_password_flags_name,
	                                              FALSE, secrets_only);
	nma_cert_chooser_setup_cert_password_storage (NMA_CERT_CHOOSER (method->client_cert_chooser),
	                                              0, (NMSetting *) s_8021x, method->client_cert_password_flags_name,
	                                              FALSE, secrets_only);
#endif
	nma_cert_chooser_setup_key_password_storage (NMA_CERT_CHOOSER (method->client_cert_chooser),
	                                             0, (NMSetting *) s_8021x, method->client_key_password_flags_name,
	                                             FALSE, secrets_only);

	return method;
}
