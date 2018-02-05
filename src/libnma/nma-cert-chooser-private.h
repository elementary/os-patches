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

#ifndef NMA_CERT_CHOOSER_PRIVATE_H
#define NMA_CERT_CHOOSER_PRIVATE_H

#include "nma-cert-chooser.h"

typedef struct _NMACertChooserVtable NMACertChooserVtable;

typedef struct {
	GtkWidget *key_button_label;
	GtkWidget *key_password_label;
	GtkWidget *cert_button_label;
	GtkWidget *key_button;
	GtkWidget *key_password;
	GtkWidget *cert_button;
	GtkWidget *show_password;
} NMAFileCertChooserPrivate;

typedef struct {
	GtkWidget *key_button_label;
	GtkWidget *key_password_label;
	GtkWidget *cert_button_label;
	GtkWidget *cert_password_label;
	GtkWidget *key_button;
	GtkWidget *key_password;
	GtkWidget *cert_button;
	GtkWidget *cert_password;
} NMAPkcs11CertChooserPrivate;

typedef struct {
	const NMACertChooserVtable *vtable;

	struct {
		union {
			NMAFileCertChooserPrivate file;
			NMAPkcs11CertChooserPrivate pkcs11;
		};
	} _sub;
} NMACertChooserPrivate;

struct _NMACertChooser {
	GtkGrid parent;
	NMACertChooserPrivate _priv;
};

struct _NMACertChooserClass {
	GtkGridClass parent_class;
};

/**
 * NMACertChooserVtable:
 * @init: called early to initialize the type.
 * @set_cert_uri: Set the certificate location for the chooser button.
 * @get_cert_uri: Get the real certificate location from the chooser button along
 *   with the scheme.
 * @set_cert_password: Set the password or a PIN that might be required to
 *   access the certificate.
 * @get_cert_password: Obtain the password or a PIN that was be required to
 *   access the certificate.
 * @set_key_uri: Set the key location for the chooser button.
 * @get_key_uri: Get the real key location from the chooser button along with the
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
 * @set_title: Setup the title property
 * @set_flags: Setup the flags property
 */
struct _NMACertChooserVtable {
	void                 (*init)                         (NMACertChooser *cert_chooser);
	void                 (*set_cert_uri)                 (NMACertChooser *cert_chooser,
	                                                      const gchar *uri);
	gchar               *(*get_cert_uri)                 (NMACertChooser *cert_chooser);
	void                 (*set_cert_password)            (NMACertChooser *cert_chooser,
	                                                      const gchar *password);
	const gchar         *(*get_cert_password)            (NMACertChooser *cert_chooser);
	void                 (*set_key_uri)                  (NMACertChooser *cert_chooser,
	                                                      const gchar *uri);
	gchar               *(*get_key_uri)                  (NMACertChooser *cert_chooser);
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

	void         (*set_title)                            (NMACertChooser *cert_chooser,
	                                                      const gchar *title);
	void         (*set_flags)                            (NMACertChooser *cert_chooser,
	                                                      NMACertChooserFlags flags);
};

extern const NMACertChooserVtable nma_cert_chooser_vtable_file;
#if LIBNM_BUILD && WITH_GCR
extern const NMACertChooserVtable nma_cert_chooser_vtable_pkcs11;
#endif

#endif /* NMA_CERT_CHOOSER_PRIVATE_H */
