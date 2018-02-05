/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

#ifndef EAP_METHOD_H
#define EAP_METHOD_H

typedef struct _EAPMethod EAPMethod;

typedef void        (*EMAddToSizeGroupFunc) (EAPMethod *method, GtkSizeGroup *group);
typedef void        (*EMFillConnectionFunc) (EAPMethod *method, NMConnection *connection);
typedef void        (*EMUpdateSecretsFunc)  (EAPMethod *method, NMConnection *connection);
typedef void        (*EMDestroyFunc)        (EAPMethod *method);
typedef gboolean    (*EMValidateFunc)       (EAPMethod *method, GError **error);

struct _EAPMethod {
	guint32 refcount;
	gsize obj_size;

	GtkBuilder *builder;
	GtkWidget *ui_widget;

	const char *default_field;

	gboolean phase2;
	gboolean secrets_only;

	EMAddToSizeGroupFunc add_to_size_group;
	EMFillConnectionFunc fill_connection;
	EMUpdateSecretsFunc update_secrets;
	EMValidateFunc validate;
	EMDestroyFunc destroy;
};

#define EAP_METHOD(x) ((EAPMethod *) x)


GtkWidget *eap_method_get_widget (EAPMethod *method);

gboolean eap_method_validate (EAPMethod *method, GError **error);

void eap_method_add_to_size_group (EAPMethod *method, GtkSizeGroup *group);

void eap_method_fill_connection (EAPMethod *method,
                                 NMConnection *connection);

void eap_method_update_secrets (EAPMethod *method, NMConnection *connection);

EAPMethod *eap_method_ref (EAPMethod *method);

void eap_method_unref (EAPMethod *method);

GType eap_method_get_type (void);

/* Below for internal use only */

#include "nma-cert-chooser.h"
#include "eap-method-tls.h"
#include "eap-method-leap.h"
#include "eap-method-fast.h"
#include "eap-method-ttls.h"
#include "eap-method-peap.h"
#include "eap-method-simple.h"

EAPMethod *eap_method_init (gsize obj_size,
                            EMValidateFunc validate,
                            EMAddToSizeGroupFunc add_to_size_group,
                            EMFillConnectionFunc fill_connection,
                            EMUpdateSecretsFunc update_secrets,
                            EMDestroyFunc destroy,
                            const char *ui_resource,
                            const char *ui_widget_name,
                            const char *default_field,
                            gboolean phase2);

void eap_method_phase2_update_secrets_helper (EAPMethod *method,
                                              NMConnection *connection,
                                              const char *combo_name,
                                              guint32 column);

void eap_method_ca_cert_ignore_set (EAPMethod *method,
                                    NMConnection *connection,
                                    const char *filename,
                                    gboolean ca_cert_error);
gboolean eap_method_ca_cert_ignore_get (EAPMethod *method, NMConnection *connection);

void eap_method_ca_cert_ignore_save (NMConnection *connection);
void eap_method_ca_cert_ignore_load (NMConnection *connection);

GError *eap_method_ca_cert_validate_cb (NMACertChooser *cert_chooser, gpointer user_data);

#if !LIBNM_BUILD
/* For mere convenience. These are not available in libnm-glib. */
#define nm_setting_802_1x_get_ca_cert_password             NULL
#define nm_setting_802_1x_get_ca_cert_uri                  NULL
#define nm_setting_802_1x_get_client_cert_password         NULL
#define nm_setting_802_1x_get_client_cert_uri              NULL
#define nm_setting_802_1x_get_private_key_uri              NULL
#define nm_setting_802_1x_get_phase2_ca_cert_password      NULL
#define nm_setting_802_1x_get_phase2_ca_cert_uri           NULL
#define nm_setting_802_1x_get_phase2_client_cert_password  NULL
#define nm_setting_802_1x_get_phase2_client_cert_uri       NULL
#define nm_setting_802_1x_get_phase2_private_key_uri       NULL
#endif

void eap_method_setup_cert_chooser (NMACertChooser *cert_chooser,
                                    NMSetting8021x *s_8021x,
                                    NMSetting8021xCKScheme (*cert_scheme_func) (NMSetting8021x *setting),
                                    const char *(*cert_path_func) (NMSetting8021x *setting),
                                    const char *(*cert_uri_func) (NMSetting8021x *setting),
                                    const char *(*cert_password_func) (NMSetting8021x *setting),
                                    NMSetting8021xCKScheme (*key_scheme_func) (NMSetting8021x *setting),
                                    const char *(*key_path_func) (NMSetting8021x *setting),
                                    const char *(*key_uri_func) (NMSetting8021x *setting),
                                    const char *(*key_password_func) (NMSetting8021x *setting));

#endif /* EAP_METHOD_H */
