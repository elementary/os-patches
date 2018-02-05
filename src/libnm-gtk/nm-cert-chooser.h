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
 * Copyright (C) 2015,2017 Red Hat, Inc.
 */

#ifndef NMA_CERT_CHOOSER_H
#define NMA_CERT_CHOOSER_H

#include <gtk/gtk.h>
#include <NetworkManager.h>
#include <nm-connection.h>

G_BEGIN_DECLS

#define NMA_TYPE_CERT_CHOOSER                   (nma_cert_chooser_get_type ())
#define NMA_CERT_CHOOSER(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), NMA_TYPE_CERT_CHOOSER, NMACertChooser))
#define NMA_IS_CERT_CHOOSER(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NMA_TYPE_CERT_CHOOSER))
#define NMA_CERT_CHOOSER_GET_INTERFACE(obj)     (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NMA_TYPE_CERT_CHOOSER, NMACertChooserInterface))

typedef struct _NMACertChooser NMACertChooser;

/**
 * NMACertChooserInterface:
 * @parent_iface: The parent interface.
 * @set_cert: Set the certificate location for the chooser button.
 * @get_cert: Get the real certificate location from the chooser button along
 *   with the scheme.
 * @set_cert_password: Set the password or a PIN that might be required to
 *   access the certificate.
 * @get_cert_password: Obtain the password or a PIN that was be required to
 *   access the certificate.
 * @set_key: Set the key location for the chooser button.
 * @get_key: Get the real key location from the chooser button along with the
 *   scheme.
 * @set_key_password: Set the password or a PIN that might be required to
 *   access the key.
 * @get_key_password: Obtain the password or a PIN that was be required to
 *   access the key.
 * @add_to_size_group: Add the labels to the specified size group so that they
 *   are aligned.
 * @validate: Validate whether the chosen values make sense.
 * @setup_cert_password_storage: Set up certificate password storage.
 * @update_cert_password_storage: Update certificate password storage.
 * @get_cert_password_flags: Return secret flags corresponding to the
 *   certificate password if one is present.
 * @setup_key_password_storage: Set up key password storage.
 * @update_key_password_storage: Update key password storage.
 * @get_key_password_flags: Returns secret flags corresponding to the key
 *   password if one is present.
 * @cert_validate: Emitted when the certificate needs validation.
 * @cert_password_validate: Emitted when the certificate password needs
 *   validation.
 * @key_validate: Emitted when the key needs validation.
 * @key_password_validate: Emitted when the key password needs validation.
 * @changed: Emitted when anything changes in the certificate chooser.
 *
 * Since: 1.8.0
 */
typedef struct {
	GTypeInterface parent_iface;

	/* virtual methods */
	void                 (*set_cert)                     (NMACertChooser *cert_chooser,
	                                                      const gchar *value,
	                                                      NMSetting8021xCKScheme scheme);
	gchar               *(*get_cert)                     (NMACertChooser *cert_chooser,
	                                                      NMSetting8021xCKScheme *scheme);
	void                 (*set_cert_password)            (NMACertChooser *cert_chooser,
	                                                      const gchar *password);
	const gchar         *(*get_cert_password)            (NMACertChooser *cert_chooser);
	void                 (*set_key)                      (NMACertChooser *cert_chooser,
	                                                      const gchar *value,
	                                                      NMSetting8021xCKScheme scheme);
	gchar               *(*get_key)                      (NMACertChooser *cert_chooser,
	                                                      NMSetting8021xCKScheme *scheme);
	void                 (*set_key_password)             (NMACertChooser *cert_chooser,
	                                                      const gchar *password);
	const gchar         *(*get_key_password)             (NMACertChooser *cert_chooser);

	void                 (*add_to_size_group)            (NMACertChooser *cert_chooser,
	                                                      GtkSizeGroup *group);
	gboolean             (*validate)                     (NMACertChooser *cert_chooser,
	                                                      GError **error);

	void                 (*setup_cert_password_storage)  (NMACertChooser *cert_chooser,
	                                                      NMSettingSecretFlags initial_flags,
	                                                      NMSetting *setting,
	                                                      const char *password_flags_name,
	                                                      gboolean with_not_required,
	                                                      gboolean ask_mode);
	void                 (*update_cert_password_storage) (NMACertChooser *cert_chooser,
	                                                      NMSettingSecretFlags secret_flags,
	                                                      NMSetting *setting,
	                                                      const char *password_flags_name);
	NMSettingSecretFlags (*get_cert_password_flags)      (NMACertChooser *cert_chooser);
	void                 (*setup_key_password_storage)   (NMACertChooser *cert_chooser,
	                                                      NMSettingSecretFlags initial_flags,
	                                                      NMSetting *setting,
	                                                      const char *password_flags_name,
	                                                      gboolean with_not_required,
	                                                      gboolean ask_mode);
	void                 (*update_key_password_storage)  (NMACertChooser *cert_chooser,
	                                                      NMSettingSecretFlags secret_flags,
	                                                      NMSetting *setting,
	                                                      const char *password_flags_name);
	NMSettingSecretFlags (*get_key_password_flags)       (NMACertChooser *cert_chooser);

	/* signals */
	GError      *(*cert_validate)                        (NMACertChooser *cert_chooser);
	GError      *(*cert_password_validate)               (NMACertChooser *cert_chooser);
	GError      *(*key_validate)                         (NMACertChooser *cert_chooser);
	GError      *(*key_password_validate)                (NMACertChooser *cert_chooser);
	void         (*changed)                              (NMACertChooser *cert_chooser);
} NMACertChooserInterface;

/**
 * NMACertChooserFlags:
 * @NMA_CERT_CHOOSER_FLAG_CERT: Only pick a certificate, not a key
 * @NMA_CERT_CHOOSER_FLAG_PASSWORDS: Hide all controls but the secrets entries
 * @NMA_CERT_CHOOSER_FLAG_PEM: Ensure the chooser only selects regular PEM files
 *
 * Flags that controls what is the certificate chooser button able to pick.
 * Currently only local files are supported, but might be extended to use URIs,
 * such as PKCS\#11 certificate URIs in future as well.
 *
 * Since: 1.8.0
 */
typedef enum {
	NMA_CERT_CHOOSER_FLAG_CERT      = 0x1,
	NMA_CERT_CHOOSER_FLAG_PASSWORDS = 0x2,
	NMA_CERT_CHOOSER_FLAG_PEM       = 0x4,
} NMACertChooserFlags;

GType                nma_cert_chooser_get_type                     (void);

void                 nma_cert_chooser_set_cert                     (NMACertChooser *cert_chooser,
                                                                    const gchar *value,
                                                                    NMSetting8021xCKScheme scheme);

gchar               *nma_cert_chooser_get_cert                     (NMACertChooser *cert_chooser,
                                                                    NMSetting8021xCKScheme *scheme);

void                 nma_cert_chooser_set_cert_password            (NMACertChooser *cert_chooser,
                                                                    const gchar *password);

const gchar         *nma_cert_chooser_get_cert_password            (NMACertChooser *cert_chooser);

void                 nma_cert_chooser_set_key                      (NMACertChooser *cert_chooser,
                                                                    const gchar *value,
                                                                    NMSetting8021xCKScheme scheme);

gchar               *nma_cert_chooser_get_key                      (NMACertChooser *cert_chooser,
                                                                    NMSetting8021xCKScheme *scheme);

void                 nma_cert_chooser_set_key_password             (NMACertChooser *cert_chooser,
                                                                    const gchar *password);

const gchar         *nma_cert_chooser_get_key_password             (NMACertChooser *cert_chooser);

GtkWidget           *nma_cert_chooser_new                          (const gchar *title,
                                                                    NMACertChooserFlags flags);


void                 nma_cert_chooser_add_to_size_group            (NMACertChooser *cert_chooser,
                                                                    GtkSizeGroup *group);

gboolean             nma_cert_chooser_validate                     (NMACertChooser *cert_chooser,
                                                                    GError **error);

void                 nma_cert_chooser_setup_cert_password_storage  (NMACertChooser *cert_chooser,
                                                                    NMSettingSecretFlags initial_flags,
                                                                    NMSetting *setting,
                                                                    const char *password_flags_name,
                                                                    gboolean with_not_required,
                                                                    gboolean ask_mode);

void                 nma_cert_chooser_update_cert_password_storage (NMACertChooser *cert_chooser,
                                                                    NMSettingSecretFlags secret_flags,
                                                                    NMSetting *setting,
                                                                    const char *password_flags_name);

NMSettingSecretFlags nma_cert_chooser_get_cert_password_flags      (NMACertChooser *cert_chooser);

void                 nma_cert_chooser_setup_key_password_storage   (NMACertChooser *cert_chooser,
                                                                    NMSettingSecretFlags initial_flags,
                                                                    NMSetting *setting,
                                                                    const char *password_flags_name,
                                                                    gboolean with_not_required,
                                                                    gboolean ask_mode);

void                 nma_cert_chooser_update_key_password_storage  (NMACertChooser *cert_chooser,
                                                                    NMSettingSecretFlags secret_flags,
                                                                    NMSetting *setting,
                                                                    const char *password_flags_name);

NMSettingSecretFlags nma_cert_chooser_get_key_password_flags       (NMACertChooser *cert_chooser);

G_END_DECLS

#endif /* NMA_CERT_CHOOSER_H */
