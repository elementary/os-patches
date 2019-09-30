/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
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

#ifndef _AG_ACCOUNT_H_
#define _AG_ACCOUNT_H_

#include <gio/gio.h>
#include <glib-object.h>
#include <libaccounts-glib/ag-types.h>

G_BEGIN_DECLS

#define AG_TYPE_ACCOUNT             (ag_account_get_type ())
#define AG_ACCOUNT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), AG_TYPE_ACCOUNT, AgAccount))
#define AG_ACCOUNT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), AG_TYPE_ACCOUNT, AgAccountClass))
#define AG_IS_ACCOUNT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AG_TYPE_ACCOUNT))
#define AG_IS_ACCOUNT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), AG_TYPE_ACCOUNT))
#define AG_ACCOUNT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), AG_TYPE_ACCOUNT, AgAccountClass))

typedef struct _AgAccountClass AgAccountClass;
typedef struct _AgAccountPrivate AgAccountPrivate;

/**
 * AgAccountClass:
 *
 * Use the accessor functions below.
 */
struct _AgAccountClass
{
    GObjectClass parent_class;
    void (*_ag_reserved1) (void);
    void (*_ag_reserved2) (void);
    void (*_ag_reserved3) (void);
    void (*_ag_reserved4) (void);
    void (*_ag_reserved5) (void);
    void (*_ag_reserved6) (void);
    void (*_ag_reserved7) (void);
};

struct _AgAccount
{
    GObject parent_instance;
    AgAccountId id;

    /*< private >*/
    AgAccountPrivate *priv;
};

GType ag_account_get_type (void) G_GNUC_CONST;

gboolean ag_account_supports_service (AgAccount *account,
                                      const gchar *service_type);
GList *ag_account_list_services (AgAccount *account);
GList *ag_account_list_services_by_type (AgAccount *account,
                                         const gchar *service_type);
GList *ag_account_list_enabled_services (AgAccount *account);

AgManager *ag_account_get_manager (AgAccount *account);

const gchar *ag_account_get_provider_name (AgAccount *account);

const gchar *ag_account_get_display_name (AgAccount *account);
void ag_account_set_display_name (AgAccount *account,
                                  const gchar *display_name);

/* Account configuration */
void ag_account_select_service (AgAccount *account, AgService *service);
AgService *ag_account_get_selected_service (AgAccount *account);

gboolean ag_account_get_enabled (AgAccount *account);
void ag_account_set_enabled (AgAccount *account, gboolean enabled);

void ag_account_delete (AgAccount *account);

/**
 * AgSettingSource:
 * @AG_SETTING_SOURCE_NONE: the setting is not present
 * @AG_SETTING_SOURCE_ACCOUNT: the setting comes from the current account
 * configuration
 * @AG_SETTING_SOURCE_PROFILE: the setting comes from the predefined profile
 *
 * The source of a setting on a #AgAccount.
 */
typedef enum {
    AG_SETTING_SOURCE_NONE = 0,
    AG_SETTING_SOURCE_ACCOUNT,
    AG_SETTING_SOURCE_PROFILE,
} AgSettingSource;

#ifndef AG_DISABLE_DEPRECATED
AG_DEPRECATED_FOR(ag_account_get_variant)
AgSettingSource ag_account_get_value (AgAccount *account, const gchar *key,
                                      GValue *value);
AG_DEPRECATED_FOR(ag_account_set_variant)
void ag_account_set_value (AgAccount *account, const gchar *key,
                           const GValue *value);
#endif

GVariant *ag_account_get_variant (AgAccount *account, const gchar *key,
                                  AgSettingSource *source);
void ag_account_set_variant (AgAccount *account, const gchar *key,
                             GVariant *value);


typedef struct _AgAccountSettingIter AgAccountSettingIter;

/**
 * AgAccountSettingIter:
 * @account: the AgAccount to iterate over
 *
 * Iterator for account settings.
 */
struct _AgAccountSettingIter {
    AgAccount *account;
    /*< private >*/
    GHashTableIter iter1;
    gpointer ptr1;
    gpointer ptr2;
    gint idx1;
    gint idx2;
};

GType ag_account_settings_iter_get_type (void) G_GNUC_CONST;

void ag_account_settings_iter_free (AgAccountSettingIter *iter);

void ag_account_settings_iter_init (AgAccount *account,
                                    AgAccountSettingIter *iter,
                                    const gchar *key_prefix);
#ifndef AG_DISABLE_DEPRECATED
AG_DEPRECATED_FOR(ag_account_settings_iter_get_next)
gboolean ag_account_settings_iter_next (AgAccountSettingIter *iter,
                                        const gchar **key,
                                        const GValue **value);
#endif
gboolean ag_account_settings_iter_get_next (AgAccountSettingIter *iter,
                                            const gchar **key,
                                            GVariant **value);

AgAccountSettingIter *ag_account_get_settings_iter (AgAccount *account,
                                                    const gchar *key_prefix);

/**
 * AgAccountWatch:
 *
 * An opaque struct returned from ag_account_watch_dir() and
 * ag_account_watch_key().
 */
typedef struct _AgAccountWatch *AgAccountWatch;

typedef void (*AgAccountNotifyCb) (AgAccount *account, const gchar *key,
                                   gpointer user_data);
AgAccountWatch ag_account_watch_key (AgAccount *account,
                                     const gchar *key,
                                     AgAccountNotifyCb callback,
                                     gpointer user_data);
AgAccountWatch ag_account_watch_dir (AgAccount *account,
                                     const gchar *key_prefix,
                                     AgAccountNotifyCb callback,
                                     gpointer user_data);
void ag_account_remove_watch (AgAccount *account, AgAccountWatch watch);

#ifndef AG_DISABLE_DEPRECATED
typedef void (*AgAccountStoreCb) (AgAccount *account, const GError *error,
                                  gpointer user_data);
AG_DEPRECATED_FOR(ag_account_store_async)
void ag_account_store (AgAccount *account, AgAccountStoreCb callback,
                       gpointer user_data);
#endif
void ag_account_store_async (AgAccount *account,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data);
gboolean ag_account_store_finish (AgAccount *account,
                                  GAsyncResult *res,
                                  GError **error);

gboolean ag_account_store_blocking (AgAccount *account, GError **error);

void ag_account_sign (AgAccount *account, const gchar *key, const gchar *token);

gboolean ag_account_verify (AgAccount *account, const gchar *key, const gchar **token);

gboolean ag_account_verify_with_tokens (AgAccount *account, const gchar *key, const gchar **tokens);

/* Signon */
/* TODO: depends on signon-glib */

G_END_DECLS

#endif /* _AG_ACCOUNT_H_ */
