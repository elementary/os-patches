/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2012 Canonical Ltd.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * SECTION:ag-auth-data
 * @short_description: information for account authentication.
 * @include: libaccounts-glib/ag-auth-data.h
 *
 * The #AgAuthData structure holds information on the authentication
 * parameters used by an account. It is created by
 * ag_account_service_get_auth_data(), and can be destroyed with
 * ag_auth_data_unref().
 */

#define AG_DISABLE_DEPRECATION_WARNINGS

#include "ag-auth-data.h"
#include "ag-internals.h"
#include "ag-util.h"

struct _AgAuthData {
    /*< private >*/
    gint ref_count;
    guint credentials_id;
    gchar *method;
    gchar *mechanism;
    GHashTable *parameters;
    /* For compatibility with the deprecated API */
    GHashTable *parameters_compat;
};

G_DEFINE_BOXED_TYPE (AgAuthData, ag_auth_data,
                     (GBoxedCopyFunc)ag_auth_data_ref,
                     (GBoxedFreeFunc)ag_auth_data_unref);

static GVariant *
get_value_with_fallback (AgAccount *account, AgService *service,
                         const gchar *key)
{
    GVariant *value;
    ag_account_select_service (account, service);
    value = ag_account_get_variant (account, key, NULL);
    if (value == NULL && service != NULL)
    {
        /* fallback to the global account */
        ag_account_select_service (account, NULL);
        value = ag_account_get_variant (account, key, NULL);
    }

    return value;
}

static gchar *
get_string_with_fallback (AgAccount *account, AgService *service,
                          const gchar *key)
{
    GVariant *value;

    value = get_value_with_fallback (account, service, key);
    if (value == NULL)
        return NULL;

    return g_variant_dup_string (value, NULL);
}

static guint32
get_uint_with_fallback (AgAccount *account, AgService *service,
                        const gchar *key)
{
    GVariant *value;

    value = get_value_with_fallback (account, service, key);
    if (value == NULL)
        return 0;

    return g_variant_get_uint32 (value);
}

static void
read_auth_settings (AgAccount *account, const gchar *key_prefix,
                    GHashTable *out)
{
    AgAccountSettingIter iter;
    const gchar *key;
    GVariant *value;

    ag_account_settings_iter_init (account, &iter, key_prefix);
    while (ag_account_settings_iter_get_next (&iter, &key, &value))
    {
        g_hash_table_insert (out, g_strdup (key), g_variant_ref (value));
    }
}

AgAuthData *
_ag_auth_data_new (AgAccount *account, AgService *service)
{
    guint32 credentials_id;
    gchar *method, *mechanism;
    gchar *key_prefix;
    GHashTable *parameters;
    AgAuthData *data = NULL;

    g_return_val_if_fail (account != NULL, NULL);

    credentials_id = get_uint_with_fallback (account, service, "CredentialsId");

    method = get_string_with_fallback (account, service, "auth/method");
    mechanism = get_string_with_fallback (account, service, "auth/mechanism");

    parameters = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free,
                                        (GDestroyNotify)g_variant_unref);
    key_prefix = g_strdup_printf ("auth/%s/%s/", method, mechanism);

    /* first, take the values from the global account */
    ag_account_select_service (account, NULL);
    read_auth_settings (account, key_prefix, parameters);

    /* next, the service-specific authentication settings */
    if (service != NULL)
    {
        ag_account_select_service (account, service);
        read_auth_settings (account, key_prefix, parameters);
    }

    g_free (key_prefix);

    data = g_slice_new (AgAuthData);
    data->credentials_id = credentials_id;
    data->method = method;
    data->mechanism = mechanism;
    data->parameters = parameters;
    data->parameters_compat = NULL;
    return data;
}

/**
 * ag_auth_data_ref:
 * @self: the #AgAuthData.
 *
 * Increment the reference count of @self.
 *
 * Returns: @self.
 */
AgAuthData *
ag_auth_data_ref (AgAuthData *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_atomic_int_inc (&self->ref_count);
    return self;
}

/**
 * ag_auth_data_unref:
 * @self: the #AgAuthData.
 *
 * Decrements the reference count of @self. The item is destroyed when the
 * count gets to 0.
 */
void
ag_auth_data_unref (AgAuthData *self)
{
    g_return_if_fail (self != NULL);
    if (g_atomic_int_dec_and_test (&self->ref_count))
    {
        g_free (self->method);
        g_free (self->mechanism);
        g_hash_table_unref (self->parameters);
        if (self->parameters_compat != NULL)
            g_hash_table_unref (self->parameters_compat);
        g_slice_free (AgAuthData, self);
    }
}

/**
 * ag_auth_data_get_credentials_id:
 * @self: the #AgAuthData.
 *
 * Gets the ID of the credentials associated with this account.
 *
 * Returns: the credentials ID.
 *
 * Since: 1.1
 */
guint
ag_auth_data_get_credentials_id (AgAuthData *self)
{
    g_return_val_if_fail (self != NULL, 0);
    return self->credentials_id;
}

/**
 * ag_auth_data_get_method:
 * @self: the #AgAuthData.
 *
 * Gets the authentication method.
 *
 * Returns: the authentication method.
 */
const gchar *
ag_auth_data_get_method (AgAuthData *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->method;
}

/**
 * ag_auth_data_get_mechanism:
 * @self: the #AgAuthData.
 *
 * Gets the authentication mechanism.
 *
 * Returns: the authentication mechanism.
 */
const gchar *
ag_auth_data_get_mechanism (AgAuthData *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->mechanism;
}

/**
 * ag_auth_data_get_parameters:
 * @self: the #AgAuthData.
 *
 * Gets the authentication parameters.
 *
 * Returns: (transfer none) (element-type utf8 GValue): a #GHashTable
 * containing all the authentication parameters.
 *
 * Deprecated: 1.4: use ag_auth_data_get_login_parameters() instead.
 */
GHashTable *
ag_auth_data_get_parameters (AgAuthData *self)
{
    g_return_val_if_fail (self != NULL, NULL);

    if (self->parameters_compat == NULL)
    {
        GHashTableIter iter;
        const gchar *key;
        GVariant *variant;

        self->parameters_compat =
            g_hash_table_new_full (g_str_hash, g_str_equal,
                                   g_free,
                                   (GDestroyNotify)_ag_value_slice_free);

        /* Convert the GVariants into GValues */
        g_hash_table_iter_init (&iter, self->parameters);
        while (g_hash_table_iter_next (&iter,
                                       (gpointer)&key, (gpointer)&variant))
        {
            GValue *value = g_slice_new0 (GValue);
            _ag_value_from_variant (value, variant);
            g_hash_table_insert (self->parameters_compat,
                                 g_strdup (key), value);
        }

    }

    return self->parameters_compat;
}

/**
 * ag_auth_data_insert_parameters:
 * @self: the #AgAuthData.
 * @parameters: (transfer none) (element-type utf8 GValue): a #GHashTable
 * containing the authentication parameters to be added.
 *
 * Insert the given authentication parameters into the authentication data. If
 * some parameters were already present, the parameters passed with this method
 * take precedence.
 *
 * Deprecated: 1.4: use ag_auth_data_get_login_parameters() instead.
 */
void
ag_auth_data_insert_parameters (AgAuthData *self, GHashTable *parameters)
{
    GHashTable *self_parameters;
    GHashTableIter iter;
    const gchar *key;
    const GValue *value;

    g_return_if_fail (self != NULL);
    g_return_if_fail (parameters != NULL);

    self_parameters = ag_auth_data_get_parameters (self);
    g_hash_table_iter_init (&iter, parameters);
    while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value))
    {
        g_hash_table_insert (self_parameters,
                             g_strdup (key),
                             _ag_value_slice_dup (value));
    }
}

/**
 * ag_auth_data_get_login_parameters:
 * @self: the #AgAuthData.
 * @extra_parameters: (transfer floating): a #GVariant containing
 * client-specific authentication parameters to be added to the returned
 * dictionary.
 *
 * Gets the authentication parameters.
 *
 * Returns: (transfer none): a floating #GVariant of type
 * %G_VARIANT_TYPE_VARDICT containing all the authentication parameters.
 *
 * Since: 1.4
 */
GVariant *
ag_auth_data_get_login_parameters (AgAuthData *self, GVariant *extra_parameters)
{
    GVariantBuilder builder;
    GHashTableIter iter;
    gchar *key;
    GVariant *value;
    GSList *skip_keys = NULL;

    g_return_val_if_fail (self != NULL, NULL);

    g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

    /* Put the parameters from the client. */
    if (extra_parameters != NULL)
    {
        GVariantIter i_extra;

        g_variant_ref_sink (extra_parameters);
        g_variant_iter_init (&i_extra, extra_parameters);
        while (g_variant_iter_next (&i_extra, "{&sv}", &key, &value))
        {
            g_variant_builder_add (&builder, "{sv}", key, value);

            /* Make sure we are not going to add the same key later */
            if (g_hash_table_lookup (self->parameters, key))
                skip_keys = g_slist_prepend (skip_keys, g_strdup (key));
        }
        g_variant_unref (extra_parameters);
    }

    /* Put the parameters from the account first. */
    g_hash_table_iter_init (&iter, self->parameters);
    while (g_hash_table_iter_next (&iter,
                                   (gpointer)&key, (gpointer)&value))
    {
        /* If the key is also present in extra_parameters, then don't add it */
        if (!g_slist_find_custom (skip_keys, key, (GCompareFunc)g_strcmp0))
            g_variant_builder_add (&builder, "{sv}", key, value);
    }

    /* Free the skip_keys list */
    while (skip_keys != NULL)
    {
        g_free (skip_keys->data);
        skip_keys = g_slist_delete_link (skip_keys, skip_keys);
    }

    return g_variant_builder_end (&builder);
}
