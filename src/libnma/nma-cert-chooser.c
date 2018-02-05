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
 * Copyright (C) 2017 Red Hat, Inc.
 */

#include "nm-default.h"
#include "nma-cert-chooser.h"

#include "nma-cert-chooser-private.h"

/**
 * SECTION:nma-cert-chooser
 * @title: NMACertChooser
 *
 * Certificate chooser allows for selection of a certificate or
 * various schemes optionally accompanied with a key and passwords
 * or PIN.
 *
 * The widgets that implement this interface may allow selecting
 * the certificates from various sources such as files or cryptographic
 * tokens.
 */

enum {
	PROP_0,
	PROP_TITLE,
	PROP_FLAGS,
	LAST_PROP,
};

static GParamSpec *properties[LAST_PROP];

enum {
	CERT_VALIDATE,
	CERT_PASSWORD_VALIDATE,
	KEY_VALIDATE,
	KEY_PASSWORD_VALIDATE,
	CHANGED,
	LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NMACertChooser, nma_cert_chooser, GTK_TYPE_GRID)

#define NMA_CERT_CHOOSER_GET_PRIVATE(self) _NM_GET_PRIVATE (self, NMACertChooser, NMA_IS_CERT_CHOOSER)
#define NMA_CERT_CHOOSER_GET_VTABLE(o) (NMA_CERT_CHOOSER_GET_PRIVATE (o)->vtable)

static gboolean
accu_validation_error (GSignalInvocationHint *ihint,
                       GValue *return_accu,
                       const GValue *handler_return,
                       gpointer data)
{
	if (g_value_get_boxed (handler_return)) {
		g_value_copy (handler_return, return_accu);
		return FALSE;
	}

	return TRUE;
}

#if LIBNM_BUILD

static gchar *
value_with_scheme_to_uri (const gchar *value, NMSetting8021xCKScheme scheme)
{
	switch (scheme) {
	case NM_SETTING_802_1X_CK_SCHEME_PATH:
		return g_strdup_printf (NM_SETTING_802_1X_CERT_SCHEME_PREFIX_PATH "%s", value);
	case NM_SETTING_802_1X_CK_SCHEME_PKCS11:
		return g_strdup (value);
	default:
		g_warning ("The key '%s' uses an unknown scheme %d\n", value, scheme);
		return NULL;
	}
}

static gchar *
uri_to_value_with_scheme (const gchar *uri, NMSetting8021xCKScheme *scheme)
{
	if (!uri) {
		*scheme = NM_SETTING_802_1X_CK_SCHEME_UNKNOWN;
		return NULL;
	} else if (g_str_has_prefix (uri, NM_SETTING_802_1X_CERT_SCHEME_PREFIX_PATH)) {
		*scheme = NM_SETTING_802_1X_CK_SCHEME_PATH;
		return g_strdup (uri + sizeof (NM_SETTING_802_1X_CERT_SCHEME_PREFIX_PATH) - 1);
	} else if (g_str_has_prefix (uri, NM_SETTING_802_1X_CERT_SCHEME_PREFIX_PKCS11)) {
		*scheme = NM_SETTING_802_1X_CK_SCHEME_PKCS11;
		return g_strdup (uri);
	} else {
		g_warning ("The dialog returned URI of unknown scheme: '%s'\n", uri);
		return NULL;
	}
}

#else

/* libnm-glib only supports certificates in files. */

static gchar *
value_with_scheme_to_uri (const gchar *value, NMSetting8021xCKScheme scheme)
{
	g_return_val_if_fail (scheme == NM_SETTING_802_1X_CK_SCHEME_PATH, NULL);
	return g_strdup_printf ("file://%s", value);
}

static gchar *
uri_to_value_with_scheme (const gchar *uri, NMSetting8021xCKScheme *scheme)
{
	if (!uri) {
		*scheme = NM_SETTING_802_1X_CK_SCHEME_UNKNOWN;
		return NULL;
	}

	g_return_val_if_fail (g_str_has_prefix (uri, "file://"), NULL);
	return g_strdup (uri + 7);
}

#endif

/**
 * nma_cert_chooser_set_cert_uri:
 * @cert_chooser: certificate chooser button instance
 * @uri: the path or URI of a certificate
 *
 * Sets the certificate URI for the chooser button.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_set_cert_uri (NMACertChooser *cert_chooser,
                               const gchar *uri)
{
	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->set_cert_uri (cert_chooser, uri);
}

/**
 * nma_cert_chooser_set_cert:
 * @cert_chooser: certificate chooser button instance
 * @value: the path or URI of a certificate
 * @scheme: the scheme of the certificate path
 *
 * Sets the certificate location for the chooser button.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_set_cert (NMACertChooser *cert_chooser,
                           const gchar *value,
                           NMSetting8021xCKScheme scheme)
{
	gs_free gchar *uri = NULL;

	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	if (!value)
		return;

	uri = value_with_scheme_to_uri (value, scheme);
	nma_cert_chooser_set_cert_uri (cert_chooser, uri);
}

/**
 * nma_cert_chooser_get_cert_uri:
 * @cert_chooser: certificate chooser button instance
 *
 * Gets the real certificate URI from the chooser button along with the scheme.
 *
 * Returns: the certificate URI
 *
 * Since: 1.8.0
 */
gchar *
nma_cert_chooser_get_cert_uri (NMACertChooser *cert_chooser)
{
	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser), NULL);

	return NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->get_cert_uri (cert_chooser);
}

/**
 * nma_cert_chooser_get_cert:
 * @cert_chooser: certificate chooser button instance
 * @scheme: (out): the scheme of the returned certificate path
 *
 * Gets the real certificate location from the chooser button along with the scheme.
 *
 * Returns: the certificate path
 *
 * Since: 1.8.0
 */
gchar *
nma_cert_chooser_get_cert (NMACertChooser *cert_chooser, NMSetting8021xCKScheme *scheme)
{
	gs_free gchar *uri = NULL;

	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser), NULL);

	uri = nma_cert_chooser_get_cert_uri (cert_chooser);
	return uri_to_value_with_scheme (uri, scheme);
}

/**
 * nma_cert_chooser_set_cert_password:
 * @cert_chooser: certificate chooser button instance
 * @password: the certificate PIN or password
 *
 * Sets the password or a PIN that might be required to access the certificate.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_set_cert_password (NMACertChooser *cert_chooser, const gchar *password)
{
	const NMACertChooserVtable *vtable;

	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	vtable = NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser);
	if (vtable->set_cert_password)
		vtable->set_cert_password (cert_chooser, password);
	else
		g_warning ("Can't set certificate password");
}

/**
 * nma_cert_chooser_get_cert_password:
 * @cert_chooser: certificate chooser button instance
 *
 * Obtains the password or a PIN that was be required to access the certificate.
 *
 * Returns: the certificate PIN or password
 *
 * Since: 1.8.0
 */
const gchar *
nma_cert_chooser_get_cert_password (NMACertChooser *cert_chooser)
{
	const NMACertChooserVtable *vtable;

	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser), NULL);

	vtable = NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser);
	if (!vtable->get_cert_password)
		return NULL;
	return vtable->get_cert_password (cert_chooser);
}

/**
 * nma_cert_chooser_set_key_uri:
 * @cert_chooser: certificate chooser button instance
 * @uri: the URI of a key
 *
 * Sets the key URI for the chooser button.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_set_key_uri (NMACertChooser *cert_chooser,
                              const gchar *uri)
{
	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	return NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->set_key_uri (cert_chooser, uri);
}

/**
 * nma_cert_chooser_set_key:
 * @cert_chooser: certificate chooser button instance
 * @value: the path or URI of a key
 * @scheme: the scheme of the key path
 *
 * Sets the key location for the chooser button.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_set_key (NMACertChooser *cert_chooser,
                          const gchar *value,
                          NMSetting8021xCKScheme scheme)
{
	gs_free gchar *uri = NULL;

	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	if (!value)
		return;

	uri = value_with_scheme_to_uri (value, scheme);
	nma_cert_chooser_set_key_uri (cert_chooser, uri);
}

/**
 * nma_cert_chooser_get_key:
 * @cert_chooser: certificate chooser button instance
 * @scheme: (out): the scheme of the returned key path
 *
 * Gets the real key location from the chooser button along with the scheme.
 *
 * Returns: the key path
 *
 * Since: 1.8.0
 */
gchar *
nma_cert_chooser_get_key (NMACertChooser *cert_chooser, NMSetting8021xCKScheme *scheme)
{
	gs_free gchar *uri = NULL;

	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser), NULL);

	uri = nma_cert_chooser_get_key_uri (cert_chooser);
	return uri_to_value_with_scheme (uri, scheme);
}

/**
 * nma_cert_chooser_get_key_uri:
 * @cert_chooser: certificate chooser button instance
 *
 * Gets the real key URI from the chooser button along with the scheme.
 *
 * Returns: the key URI
 *
 * Since: 1.8.0
 */
gchar *
nma_cert_chooser_get_key_uri (NMACertChooser *cert_chooser)
{
	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser), NULL);

	return NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->get_key_uri (cert_chooser);
}

/**
 * nma_cert_chooser_set_key_password:
 * @cert_chooser: certificate chooser button instance
 * @password: the key PIN or password
 *
 * Sets the password or a PIN that might be required to access the key.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_set_key_password (NMACertChooser *cert_chooser, const gchar *password)
{
	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->set_key_password (cert_chooser, password);
}

/**
 * nma_cert_chooser_get_key_password:
 * @cert_chooser: certificate chooser button instance
 *
 * Obtains the password or a PIN that was be required to access the key.
 *
 * Returns: the key PIN or password
 *
 * Since: 1.8.0
 */
const gchar *
nma_cert_chooser_get_key_password (NMACertChooser *cert_chooser)
{
	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser), NULL);

	return NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->get_key_password (cert_chooser);
}

/**
 * nma_cert_chooser_add_to_size_group:
 * @cert_chooser: certificate chooser button instance
 * @group: a size group
 *
 * Adds the labels to the specified size group so that they are aligned
 * nicely with other entries in a form.
 *
 * It is expected that the NMACertChooser is a GtkGrid with two columns
 * with the labels in the first one.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_add_to_size_group (NMACertChooser *cert_chooser, GtkSizeGroup *group)
{
	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->add_to_size_group (cert_chooser, group);
}

/**
 * nma_cert_chooser_validate:
 * @cert_chooser: certificate chooser button instance
 * @error: error return location
 *
 * Validates whether the chosen values make sense. The users can do further
 * validation by subscribing to the "*-changed" signals and returning an
 * error themselves.
 *
 * Returns: %TRUE if validation passes, %FALSE otherwise
 *
 * Since: 1.8.0
 */
gboolean
nma_cert_chooser_validate (NMACertChooser *cert_chooser, GError **error)
{
	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser), TRUE);

	return NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->validate (cert_chooser, error);
}

/**
 * nma_cert_chooser_setup_cert_password_storage:
 * @cert_chooser: certificate chooser button instance
 * @initial_flags: initial secret flags to setup password menu from
 * @setting: #NMSetting containing the password, or NULL
 * @password_flags_name: name of the secret flags (like psk-flags), or NULL
 * @with_not_required: whether to include "Not required" menu item
 * @ask_mode: %TRUE if the entry is shown in ASK mode
 *
 * This method basically calls nma_utils_setup_password_storage()
 * on the certificate password entry, in case one is present.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_setup_cert_password_storage (NMACertChooser *cert_chooser,
                                              NMSettingSecretFlags initial_flags,
                                              NMSetting *setting,
                                              const char *password_flags_name,
                                              gboolean with_not_required,
                                              gboolean ask_mode)
{
	const NMACertChooserVtable *vtable;

	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	vtable = NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser);
	if (vtable->setup_cert_password_storage) {
		vtable->setup_cert_password_storage (cert_chooser,
		                                     initial_flags,
		                                     setting,
		                                     password_flags_name,
		                                     with_not_required,
		                                     ask_mode);
	}
}

/**
 * nma_cert_chooser_update_cert_password_storage:
 * @cert_chooser: certificate chooser button instance
 * @secret_flags: secret flags to set
 * @setting: #NMSetting containing the password, or NULL
 * @password_flags_name: name of the secret flags (like psk-flags), or NULL
 *
 * This method basically calls nma_utils_update_password_storage()
 * on the certificate password entry, in case one is present.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_update_cert_password_storage (NMACertChooser *cert_chooser,
                                               NMSettingSecretFlags secret_flags,
                                               NMSetting *setting,
                                               const char *password_flags_name)
{
	const NMACertChooserVtable *vtable;

	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	vtable = NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser);
	if (vtable->update_cert_password_storage) {
		vtable->update_cert_password_storage (cert_chooser,
		                                      secret_flags,
		                                      setting,
		                                      password_flags_name);
	}
}

/**
 * nma_cert_chooser_get_cert_password_flags:
 * @cert_chooser: certificate chooser button instance
 *
 * Returns secret flags corresponding to the certificate password
 * if one is present. The chooser would typically call into
 * nma_utils_menu_to_secret_flags() for the certificate password
 * entry.
 *
 * Returns: secret flags corresponding to the certificate password
 *
 * Since: 1.8.0
 */
NMSettingSecretFlags
nma_cert_chooser_get_cert_password_flags (NMACertChooser *cert_chooser)
{
	const NMACertChooserVtable *vtable;

	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser),
	                      NM_SETTING_SECRET_FLAG_NONE);

	vtable = NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser);
	if (!vtable->get_cert_password_flags)
		return NM_SETTING_SECRET_FLAG_NONE;
	return vtable->get_cert_password_flags (cert_chooser);
}


/**
 * nma_cert_chooser_setup_key_password_storage:
 * @cert_chooser: certificate chooser button instance
 * @initial_flags: initial secret flags to setup password menu from
 * @setting: #NMSetting containing the password, or NULL
 * @password_flags_name: name of the secret flags (like psk-flags), or NULL
 * @with_not_required: whether to include "Not required" menu item
 * @ask_mode: %TRUE if the entry is shown in ASK mode
 *
 * This method basically calls nma_utils_setup_password_storage()
 * on the key password entry, in case one is present.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_setup_key_password_storage (NMACertChooser *cert_chooser,
                                             NMSettingSecretFlags initial_flags,
                                             NMSetting *setting,
                                             const char *password_flags_name,
                                             gboolean with_not_required,
                                             gboolean ask_mode)
{
	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->setup_key_password_storage (cert_chooser,
	                                                                        initial_flags,
	                                                                        setting,
	                                                                        password_flags_name,
	                                                                        with_not_required,
	                                                                        ask_mode);
}

/**
 * nma_cert_chooser_update_key_password_storage:
 * @cert_chooser: certificate chooser button instance
 * @secret_flags: secret flags to set
 * @setting: #NMSetting containing the password, or NULL
 * @password_flags_name: name of the secret flags (like psk-flags), or NULL
 *
 * This method basically calls nma_utils_update_password_storage()
 * on the key password entry, in case one is present.
 *
 * Since: 1.8.0
 */
void
nma_cert_chooser_update_key_password_storage (NMACertChooser *cert_chooser,
                                               NMSettingSecretFlags secret_flags,
                                               NMSetting *setting,
                                               const char *password_flags_name)
{
	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->update_key_password_storage (cert_chooser,
	                                                                         secret_flags,
	                                                                         setting,
	                                                                         password_flags_name);
}

/**
 * nma_cert_chooser_get_key_password_flags:
 * @cert_chooser: certificate chooser button instance
 *
 * Returns secret flags corresponding to the key password
 * if one is present. The chooser would typically call into
 * nma_utils_menu_to_secret_flags() for the key password
 * entry.
 *
 * Returns: secret flags corresponding to the key password
 *
 * Since: 1.8.0
 */
NMSettingSecretFlags
nma_cert_chooser_get_key_password_flags (NMACertChooser *cert_chooser)
{
	g_return_val_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser),
	                      NM_SETTING_SECRET_FLAG_NONE);

	return NMA_CERT_CHOOSER_GET_VTABLE (cert_chooser)->get_key_password_flags (cert_chooser);
}

static GObject *
constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
	GObject *object;
	NMACertChooser *cert_chooser;
	NMACertChooserFlags flags = NMA_CERT_CHOOSER_FLAG_NONE;
	NMACertChooserPrivate *priv;
	int i;
	const gchar *title = NULL;

	object = G_OBJECT_CLASS (nma_cert_chooser_parent_class)->constructor (type,
	                                                                      n_construct_properties,
	                                                                      construct_properties);
	cert_chooser = NMA_CERT_CHOOSER (object);
	priv = NMA_CERT_CHOOSER_GET_PRIVATE (cert_chooser);

	for (i = 0; i < n_construct_properties; i++) {
		if (strcmp (construct_properties[i].pspec->name, "title") == 0)
			title = g_value_get_string (construct_properties[i].value);
		if (strcmp (construct_properties[i].pspec->name, "flags") == 0)
			flags |= g_value_get_uint (construct_properties[i].value);
	}
	priv->vtable = &nma_cert_chooser_vtable_file;
#if LIBNM_BUILD && WITH_GCR
	if ((flags & NMA_CERT_CHOOSER_FLAG_PEM) == 0)
		priv->vtable = &nma_cert_chooser_vtable_pkcs11;
#endif

	/* Initialize the vtable and construct-time properties */
	priv->vtable->init (cert_chooser);
	priv->vtable->set_flags (cert_chooser, flags);
	priv->vtable->set_title (cert_chooser, title);

	return object;
}

static void
set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	NMACertChooser *cert_chooser = NMA_CERT_CHOOSER (object);

	g_return_if_fail (NMA_IS_CERT_CHOOSER (cert_chooser));

	switch (property_id) {
	case PROP_TITLE:
	case PROP_FLAGS:
		/* Just ignore these, should be set at construct time */
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nma_cert_chooser_class_init (NMACertChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (NMACertChooserPrivate));

	object_class->constructor = constructor;
	object_class->set_property = set_property;

	/**
	 * NMACertChooser::title:
	 *
	 * Name of the certificate or certificate/key pair to be chosen.
	 * Used in labels and chooser dialog titles.
	 *
	 * Since: 1.8.0
	 */
	properties[PROP_TITLE] = g_param_spec_string ("title",
	                                             "Title",
	                                             "Certificate Chooser Title",
	                                             NULL,
	                                               G_PARAM_WRITABLE
	                                             | G_PARAM_CONSTRUCT_ONLY
	                                             | G_PARAM_STATIC_STRINGS);

	/**
	 * NMACertChooser::flags:
	 *
	 * The #NMACertChooserFlags flags that influnce which chooser
	 * implementation is used and configure its behavior.
	 *
	 * Since: 1.8.0
	 */
	properties[PROP_FLAGS] = g_param_spec_uint ("flags",
	                                            "Flags",
	                                            "Certificate Chooser Flags",
	                                            NMA_CERT_CHOOSER_FLAG_NONE,
	                                              NMA_CERT_CHOOSER_FLAG_CERT
	                                            | NMA_CERT_CHOOSER_FLAG_PASSWORDS
	                                            | NMA_CERT_CHOOSER_FLAG_PEM,
	                                            NMA_CERT_CHOOSER_FLAG_NONE,
	                                              G_PARAM_WRITABLE
	                                            | G_PARAM_CONSTRUCT_ONLY
	                                            | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, LAST_PROP, properties);

	/**
	 * NMACertChooser::cert-validate:
	 *
	 * Emitted when the certificate needs validation. The handlers can indicate that
	 * the certificate is invalid by returning an error, which blocks further
	 * signal processing and causes a call to nma_cert_chooser_validate()
	 * to fail.
	 *
	 * Since: 1.8.0
	 */
	signals[CERT_VALIDATE] = g_signal_new ("cert-validate",
	                                       NMA_TYPE_CERT_CHOOSER,
	                                       G_SIGNAL_RUN_LAST,
	                                       0,
	                                       accu_validation_error, NULL, NULL,
	                                       G_TYPE_ERROR, 0);

	/**
	 * NMACertChooser::cert-password-validate:
	 *
	 * Emitted when the certificate password needs validation. The handlers
	 * can indicate that the password is invalid by returning an error, which blocks further
	 * signal processing and causes a call to nma_cert_chooser_validate()
	 * to fail.
	 *
	 * Since: 1.8.0
	 */
	signals[CERT_PASSWORD_VALIDATE] = g_signal_new ("cert-password-validate",
	                                                NMA_TYPE_CERT_CHOOSER,
	                                                G_SIGNAL_RUN_LAST,
	                                                0,
	                                                accu_validation_error, NULL, NULL,
	                                                G_TYPE_ERROR, 0);

	/**
	 * NMACertChooser::key-validate:
	 *
	 * Emitted when the key needs validation. The handlers can indicate that
	 * the key is invalid by returning an error, which blocks further
	 * signal processing and causes a call to nma_cert_chooser_validate()
	 * to fail.
	 *
	 * Since: 1.8.0
	 */
	signals[KEY_VALIDATE] = g_signal_new ("key-validate",
	                                      NMA_TYPE_CERT_CHOOSER,
	                                      G_SIGNAL_RUN_LAST,
	                                      0,
	                                      accu_validation_error, NULL, NULL,
	                                      G_TYPE_ERROR, 0);

	/**
	 * NMACertChooser::key-password-validate:
	 *
	 * Emitted when the key password needs validation. The handlers can indicate
	 * that the password is invalid by returning an error, which blocks further
	 * signal processing and causes a call to nma_cert_chooser_validate()
	 * to fail.
	 *
	 * Since: 1.8.0
	 */
	signals[KEY_PASSWORD_VALIDATE] = g_signal_new ("key-password-validate",
	                                               NMA_TYPE_CERT_CHOOSER,
	                                               G_SIGNAL_RUN_LAST,
	                                               0,
	                                               accu_validation_error, NULL, NULL,
	                                               G_TYPE_ERROR, 0);

	/**
	 * NMACertChooser::changed:
	 *
	 * Emitted when anything changes in the certificate chooser, be it a certificate,
	 * a key or associated passwords.
	 *
	 * Since: 1.8.0
	 */
	signals[CHANGED] = g_signal_new ("changed",
	                                 NMA_TYPE_CERT_CHOOSER,
	                                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
	                                 0,
	                                 NULL, NULL, NULL,
	                                 G_TYPE_NONE, 0);

}

static void
nma_cert_chooser_init (NMACertChooser *file_cert_chooser)
{
}

/**
 * nma_cert_chooser_new:
 * @title: title of the certificate chooser dialog
 * @flags: the flags that configure the capabilities of the button
 *
 * Constructs the button that is capable of selecting a certificate
 * and a key.
 *
 * Returns: (transfer full): the certificate chooser button instance
 *
 * Since: 1.8.0
 */
GtkWidget *
nma_cert_chooser_new (const gchar *title, NMACertChooserFlags flags)
{
	return g_object_new (NMA_TYPE_CERT_CHOOSER,
	                     "title", title,
	                     "flags", flags,
	                     NULL);
}
