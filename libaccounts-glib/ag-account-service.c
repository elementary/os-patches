/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2011 Nokia Corporation.
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
 * SECTION:ag-account-service
 * @short_description: Account settings for a specific service
 * @include: libaccounts-glib/ag-account-service.h
 *
 * The #AgAccountService object provides access to the account settings for a
 * specific service type. It is meant to be easier to use than the #AgAccount
 * class because it hides the complexity of the account structure and gives
 * access to only the limited subset of account settings which are relevant to
 * a service.
 *
 * To get an #AgAccountService one can use the #AgManager methods
 * ag_manager_get_account_services() or
 * ag_manager_get_enabled_account_services(), which both return a #GList of
 * account services. Note that if the #AgManager was instantiated for a
 * specific service type, these lists will contain only those account services
 * matching that service type.
 * Another way to get an #AgAccountService is to instantiate one using
 * ag_account_service_new(): this is useful if one already has an #AgAccount
 * instance.
 *
 * This is intended to be a convenient wrapper over the accounts settings
 * specific for a service; as such, it doesn't offer all the editing
 * possibilities offered by the #AgAccount class, such as enabling the service
 * itself: these operations should ideally not be performed by consumer
 * applications, but by the account editing UI only.
 *
 * <example>
 * <title>Querying available e-mail services</title>
 *   <programlisting>
 * AgManager *manager;
 * GList *services, *list;
 *
 * // Instantiate an account manager interested in e-mail services only.
 * manager = ag_manager_new_for_service_type ("e-mail");
 *
 * // Get the list of enabled AgAccountService objects of type e-mail.
 * services = ag_manager_get_enabled_account_services (manager);
 *
 * // Loop through the account services and do something useful with them.
 * for (list = services; list != NULL; list = list->next)
 * {
 *     AgAccountService *service = AG_ACCOUNT_SERVICE (list->data);
 *     GVariant *v_server, *v_port, *v_username;
 *     gchar *server = NULL, *username = NULL;
 *     gint port;
 *     AgAccount *account;
 *
 *     v_server = ag_account_service_get_variant (service, "pop3/hostname", NULL);
 *     if (v_server != NULL)
 *         server = g_variant_dup_string (v_server, NULL);
 *
 *     v_port = ag_account_service_get_variant (service, "pop3/port", NULL);
 *     if (v_port != NULL)
 *         port = g_variant_get_int16 (&v_port);
 *
 *     // Suppose that the e-mail address is stored in the global account
 *     // settings; let's get it from there:
 *     account = ag_account_service_get_account (service);
 *     ag_account_select_service (NULL);
 *     v_username = ag_account_get_variant (account, "username", NULL);
 *     if (v_username != NULL)
 *         username = g_variant_dup_string (&v_username);
 *
 *     ...
 *
 *     g_free (username);
 *     g_free (server);
 * }
 *   </programlisting>
 * </example>
 *
 * <note>
 *   <para>
 * User applications (with the notable exception of the accounts editing
 * application) should never use account services which are not enabled, and
 * should stop using an account when the account service becomes disabled. The
 * latter can be done by connecting to the #AgAccountService::changed signal
 * and checking if ag_account_service_get_enabled() still returns %TRUE.
 * Note that if the account gets deleted, it will always get disabled first;
 * so, there is no need to connect to the #AgAccount::deleted signal; one can
 * just monitor the #AgAccountService::changed signal.
 *   </para>
 * </note>
 */

#define AG_DISABLE_DEPRECATION_WARNINGS

#include "ag-account-service.h"
#include "ag-errors.h"
#include "ag-internals.h"
#include "ag-manager.h"
#include "ag-service.h"
#include "ag-service-type.h"
#include "ag-util.h"

enum {
    PROP_0,

    PROP_ACCOUNT,
    PROP_SERVICE,
    PROP_ENABLED,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _AgAccountServicePrivate {
    AgAccount *account;
    AgService *service;
    gboolean enabled;
    AgAccountWatch watch;
    guint account_enabled_id;
};

enum
{
    CHANGED,
    ENABLED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (AgAccountService, ag_account_service, G_TYPE_OBJECT);

#define AG_ACCOUNT_SERVICE_PRIV(obj) (AG_ACCOUNT_SERVICE(obj)->priv)

static gboolean
check_enabled (AgAccountServicePrivate *priv)
{
    gboolean account_enabled;
    gboolean service_enabled;

    ag_account_select_service (priv->account, NULL);
    account_enabled = ag_account_get_enabled (priv->account);

    if (priv->service)
    {
        ag_account_select_service (priv->account, priv->service);
        service_enabled = ag_account_get_enabled (priv->account);
    }
    else
        service_enabled = TRUE;

    return service_enabled && account_enabled;
}

static void
account_watch_cb (G_GNUC_UNUSED AgAccount *account,
                  G_GNUC_UNUSED const gchar *key,
                  gpointer user_data)
{
    AgAccountService *self = (AgAccountService *)user_data;

    g_signal_emit (self, signals[CHANGED], 0);
}

static void
on_account_enabled (G_GNUC_UNUSED AgAccount *account,
                    const gchar *service_name,
                    gboolean service_enabled,
                    AgAccountService *self)
{
    AgAccountServicePrivate *priv = self->priv;
    gboolean enabled;

    DEBUG_INFO ("service: %s, enabled: %d", service_name, service_enabled);

    enabled = check_enabled (priv);
    if (enabled != priv->enabled)
    {
        priv->enabled = enabled;
        g_signal_emit (self, signals[ENABLED], 0, enabled);
        g_object_notify_by_pspec ((GObject *)self, properties[PROP_ENABLED]);
    }
}

static void
ag_account_service_get_property (GObject *object, guint property_id,
                                 GValue *value, GParamSpec *pspec)
{
    AgAccountService *self = AG_ACCOUNT_SERVICE (object);

    switch (property_id)
    {
    case PROP_ACCOUNT:
        g_value_set_object (value, self->priv->account);
        break;
    case PROP_SERVICE:
        g_value_set_boxed (value, self->priv->service);
        break;
    case PROP_ENABLED:
        g_value_set_boolean (value, self->priv->enabled);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
ag_account_service_set_property (GObject *object, guint property_id,
                                 const GValue *value, GParamSpec *pspec)
{
    AgAccountServicePrivate *priv = AG_ACCOUNT_SERVICE_PRIV (object);

    switch (property_id)
    {
    case PROP_ACCOUNT:
        g_assert (priv->account == NULL);
        priv->account = g_value_dup_object (value);
        break;
    case PROP_SERVICE:
        g_assert (priv->service == NULL);
        priv->service = g_value_dup_boxed (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }

}

static void
ag_account_service_constructed (GObject *object)
{
    AgAccountServicePrivate *priv = AG_ACCOUNT_SERVICE_PRIV (object);

    if (G_UNLIKELY (!priv->account))
    {
        g_warning ("AgAccountService constructed with no account!");
        return;
    }

    priv->account_enabled_id =
        g_signal_connect (priv->account, "enabled",
                          G_CALLBACK (on_account_enabled), object);

    ag_account_select_service (priv->account, priv->service);
    priv->watch = ag_account_watch_dir (priv->account, "",
                                        account_watch_cb, object);

    priv->enabled = check_enabled (priv);
}

static void
ag_account_service_dispose (GObject *object)
{
    AgAccountServicePrivate *priv = AG_ACCOUNT_SERVICE_PRIV (object);

    DEBUG_REFS ("Disposing account-service %p", object);

    if (priv->account)
    {
        ag_account_remove_watch (priv->account, priv->watch);
        g_signal_handler_disconnect (priv->account, priv->account_enabled_id);
        g_object_unref (priv->account);
        priv->account = NULL;
    }

    if (priv->service)
    {
        ag_service_unref (priv->service);
        priv->service = NULL;
    }

    G_OBJECT_CLASS (ag_account_service_parent_class)->dispose (object);
}

static void
ag_account_service_class_init(AgAccountServiceClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (AgAccountServicePrivate));

    object_class->constructed = ag_account_service_constructed;
    object_class->dispose = ag_account_service_dispose;
    object_class->get_property = ag_account_service_get_property;
    object_class->set_property = ag_account_service_set_property;

    /**
     * AgAccountService:account:
     *
     * The #AgAccount used by the account service.
     *
     * Since: 1.4
     */
    properties[PROP_ACCOUNT] =
        g_param_spec_object ("account", "account", "account",
                             AG_TYPE_ACCOUNT,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    /**
     * AgAccountService:service:
     *
     * The #AgService used by the account service.
     *
     * Since: 1.4
     */
    properties[PROP_SERVICE] =
        g_param_spec_boxed ("service", "service", "service",
                            ag_service_get_type(),
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    /**
     * AgAccountService:enabled:
     *
     * Whether the account service is currently enabled. The value of
     * this property is %TRUE if and only if the underlying #AgAccount
     * is enabled and the selected #AgService is enabled on it. If this
     * property is %FALSE, applications should not try to use this
     * object.
     *
     * Since: 1.4
     */
    properties[PROP_ENABLED] =
        g_param_spec_boolean ("enabled", "Enabled",
                              "Whether the account service is enabled",
                              FALSE,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class,
                                       N_PROPERTIES,
                                       properties);

    /**
     * AgAccountService::changed:
     * @self: the #AgAccountService.
     *
     * Emitted when some setting has changed on the account service. You can
     * use the ag_account_service_get_changed_fields() method to retrieve the
     * list of the settings which have changed.
     */
    signals[CHANGED] = g_signal_new ("changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);


    /**
     * AgAccountService::enabled:
     * @self: the #AgAccountService.
     * @enabled: whether the service is enabled.
     *
     * Emitted when the service enabled state changes.
     */
    signals[ENABLED] = g_signal_new ("enabled",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE,
        1, G_TYPE_BOOLEAN);
}

static void
ag_account_service_init(AgAccountService *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, AG_TYPE_ACCOUNT_SERVICE,
                                              AgAccountServicePrivate);
}

/**
 * ag_account_service_new:
 * @account: (transfer full): an #AgAccount.
 * @service: (transfer full) (allow-none): an #AgService supported by @account.
 *
 * Constructor. If @service is %NULL, the returned object will operate on the
 * global account settings.
 *
 * Returns: a new #AgAccountService; call g_object_unref() when you don't need
 * this object anymore.
 */
AgAccountService *
ag_account_service_new(AgAccount *account, AgService *service)
{
    g_return_val_if_fail (AG_IS_ACCOUNT (account), NULL);

    return g_object_new (AG_TYPE_ACCOUNT_SERVICE,
                         "account", account,
                         "service", service,
                         NULL);
}

/**
 * ag_account_service_get_account:
 * @self: the #AgAccountService.
 *
 * Get the #AgAccount associated with @self.
 *
 * Returns: (transfer none): the underlying #AgAccount. The reference count on
 * it is not incremented, so if you need to use it beyond the lifetime of
 * @self, you need to call g_object_ref() on it yourself.
 */
AgAccount *
ag_account_service_get_account (AgAccountService *self)
{
    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);

    return self->priv->account;
}

/**
 * ag_account_service_get_service:
 * @self: the #AgAccountService.
 *
 * Get the #AgService associated with @self.
 *
 * Returns: (transfer none): the underlying #AgService. The reference count on
 * it is not incremented, so if you need to use it beyond the lifetime of
 * @self, you need to call ag_service_ref() on it yourself.
 */
AgService *
ag_account_service_get_service (AgAccountService *self)
{
    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);

    return self->priv->service;
}

/**
 * ag_account_service_get_enabled:
 * @self: the #AgAccountService.
 *
 * Checks whether the underlying #AgAccount is enabled and the selected
 * #AgService is enabled on it. If this method returns %FALSE, applications
 * should not try to use this object.
 *
 * Returns: %TRUE if the service is enabled, %FALSE otherwise.
 */
gboolean
ag_account_service_get_enabled (AgAccountService *self)
{
    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), FALSE);

    return self->priv->enabled;
}

/**
 * ag_account_service_get_value:
 * @self: the #AgAccountService.
 * @key: the name of the setting to retrieve.
 * @value: (inout): an initialized #GValue to receive the setting's value.
 *
 * Gets the value of the configuration setting @key: @value must be a
 * #GValue initialized to the type of the setting.
 *
 * Returns: one of <type>#AgSettingSource</type>: %AG_SETTING_SOURCE_NONE if
 * the setting is not present, %AG_SETTING_SOURCE_ACCOUNT if the setting comes
 * from the account configuration, or %AG_SETTING_SOURCE_PROFILE if the value
 * comes as predefined in the profile.
 *
 * Deprecated: 1.4: Use ag_account_service_get_variant() instead.
 */
AgSettingSource
ag_account_service_get_value (AgAccountService *self, const gchar *key,
                              GValue *value)
{
    AgAccountServicePrivate *priv;

    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), AG_SETTING_SOURCE_NONE);
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    return ag_account_get_value (priv->account, key, value);
}

/**
 * ag_account_service_set_value:
 * @self: the #AgAccountService.
 * @key: the name of the setting to change.
 * @value: (allow-none): a #GValue holding the new setting's value.
 *
 * Sets the value of the configuration setting @key to the value @value.
 * If @value is %NULL, then the setting is unset.
 *
 * Deprecated: 1.4: Use ag_account_service_set_variant() instead.
 */
void
ag_account_service_set_value (AgAccountService *self, const gchar *key,
                              const GValue *value)
{
    AgAccountServicePrivate *priv;

    g_return_if_fail (AG_IS_ACCOUNT_SERVICE (self));
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    ag_account_set_value (priv->account, key, value);
}

/**
 * ag_account_service_get_variant:
 * @self: the #AgAccountService.
 * @key: the name of the setting to retrieve.
 * @source: (allow-none) (out): a pointer to an
 * #AgSettingSource variable which will tell whether the setting was
 * retrieved from the accounts DB or from a service template.
 *
 * Gets the value of the configuration setting @key.
 *
 * Returns: (transfer none): a #GVariant holding the setting value, or
 * %NULL. The returned #GVariant is owned by the account, and no guarantees
 * are made about its lifetime. If the client wishes to keep it, it should
 * call g_variant_ref() on it.
 *
 * Since: 1.4
 */
GVariant *
ag_account_service_get_variant (AgAccountService *self, const gchar *key,
                                AgSettingSource *source)
{
    AgAccountServicePrivate *priv;

    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    return ag_account_get_variant (priv->account, key, source);
}

/**
 * ag_account_service_set_variant:
 * @self: the #AgAccountService.
 * @key: the name of the setting to change.
 * @value: (allow-none): a #GVariant holding the new setting's value.
 *
 * Sets the value of the configuration setting @key to the value @value.
 * If @value has a floating reference, the @account will take ownership
 * of it.
 * If @value is %NULL, then the setting is unset.
 *
 * Since: 1.4
 */
void
ag_account_service_set_variant (AgAccountService *self, const gchar *key,
                                GVariant *value)
{
    AgAccountServicePrivate *priv;

    g_return_if_fail (AG_IS_ACCOUNT_SERVICE (self));
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    ag_account_set_variant (priv->account, key, value);
}

/**
 * ag_account_service_settings_iter_init:
 * @self: the #AgAccountService.
 * @iter: an uninitialized #AgAccountSettingIter structure.
 * @key_prefix: (allow-none): enumerate only the settings whose key starts with
 * @key_prefix.
 *
 * Initializes @iter to iterate over the account settings. If @key_prefix is
 * not %NULL, only keys whose names start with @key_prefix will be iterated
 * over.
 * After calling this method, one would typically call
 * ag_account_settings_iter_get_next() to read the settings one by one.
 */
void
ag_account_service_settings_iter_init (AgAccountService *self,
                                       AgAccountSettingIter *iter,
                                       const gchar *key_prefix)
{
    AgAccountServicePrivate *priv;

    g_return_if_fail (AG_IS_ACCOUNT_SERVICE (self));
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    ag_account_settings_iter_init (priv->account, iter, key_prefix);
}

/**
 * ag_account_service_get_settings_iter:
 * @self: the #AgAccountService.
 * @key_prefix: (allow-none): enumerate only the settings whose key starts with
 * @key_prefix.
 *
 * Creates a new iterator. This method is useful for language bindings only.
 *
 * Returns: (transfer full): an #AgAccountSettingIter.
 */
AgAccountSettingIter *
ag_account_service_get_settings_iter (AgAccountService *self,
                                      const gchar *key_prefix)
{
    AgAccountSettingIter *iter;
    AgAccountServicePrivate *priv;

    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);
    priv = self->priv;

    ag_account_select_service (priv->account, priv->service);
    iter = g_slice_new (AgAccountSettingIter);
    _ag_account_settings_iter_init (priv->account, iter, key_prefix, TRUE);
    return iter;
}

/**
 * ag_account_service_settings_iter_next:
 * @iter: an initialized #AgAccountSettingIter structure.
 * @key: (out callee-allocates) (transfer none): a pointer to a string
 * receiving the key name.
 * @value: (out callee-allocates) (transfer none): a pointer to a pointer to a
 * #GValue, to receive the key value.
 *
 * Iterates over the account keys. @iter must be an iterator previously
 * initialized with ag_account_service_settings_iter_init().
 *
 * Returns: %TRUE if @key and @value have been set, %FALSE if we there are no
 * more account settings to iterate over.
 *
 * Deprecated: 1.4: Use ag_account_settings_iter_get_next() instead.
 */
gboolean
ag_account_service_settings_iter_next (AgAccountSettingIter *iter,
                                       const gchar **key,
                                       const GValue **value)
{
    return ag_account_settings_iter_next (iter, key, value);
}

/**
 * ag_account_service_get_auth_data:
 * @self: the #AgAccountService.
 *
 * Reads the authentication data stored in the account (merging the
 * service-specific settings with the global account settings) and returns an
 * #AgAuthData structure.
 * The method and mechanism are read from the "auth/method" and
 * "auth/mechanism" keys, respectively. The authentication parameters are
 * found under the "auth/&lt;method&gt;/&lt;mechanism&gt;/" group.
 *
 * Returns: (transfer full): a newly allocated #AgAuthData structure.
 */
AgAuthData *
ag_account_service_get_auth_data (AgAccountService *self)
{
    AgAccountServicePrivate *priv;

    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);
    priv = self->priv;

    return _ag_auth_data_new (priv->account, priv->service);
}

/**
 * ag_account_service_get_changed_fields:
 * @self: the #AgAccountService.
 *
 * This method should be called only in the context of a handler of the
 * #AgAccountService::changed signal, and can be used to retrieve the set of
 * changes.
 *
 * Returns: (transfer full): a newly allocated array of strings describing the
 * keys of the fields which have been altered. It must be free'd with
 * g_strfreev().
 */
gchar **
ag_account_service_get_changed_fields (AgAccountService *self)
{
    AgAccountServicePrivate *priv;
    GHashTable *settings;
    GList *keys, *list;
    gchar **fields;
    gint i;

    g_return_val_if_fail (AG_IS_ACCOUNT_SERVICE (self), NULL);
    priv = self->priv;

    settings = _ag_account_get_service_changes (priv->account, priv->service);
    keys = g_hash_table_get_keys (settings);
    fields = g_malloc ((g_hash_table_size (settings) + 1) *
                       sizeof(gchar *));

    i = 0;
    for (list = keys; list != NULL; list = list->next)
    {
        fields[i++] = g_strdup ((gchar *)(list->data));
    }
    fields[i] = NULL;
    g_list_free (keys);

    return fields;
}

