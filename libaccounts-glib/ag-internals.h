/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 * Copyright (C) 2012-2013 Canonical Ltd.
 * Copyright (C) 2012 Intel Corporation.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 * Contact: Jussi Laako <jussi.laako@linux.intel.com>
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

#ifndef _AG_INTERNALS_H_
#define _AG_INTERNALS_H_

#include "ag-account.h"
#include "ag-auth-data.h"
#include "ag-debug.h"
#include "ag-manager.h"
#include <sqlite3.h>
#include <time.h>

G_BEGIN_DECLS

#define AG_DBUS_PATH_SERVICE "/ServiceType"
#define AG_DBUS_IFACE "com.google.code.AccountsSSO.Accounts"
#define AG_DBUS_SIG_CHANGED "AccountChanged"

#define SERVICE_GLOBAL_TYPE "global"
#define AG_DBUS_PATH_SERVICE_GLOBAL \
    AG_DBUS_PATH_SERVICE "/" SERVICE_GLOBAL_TYPE

#define MAX_SQLITE_BUSY_LOOP_TIME 5
#define MAX_SQLITE_BUSY_LOOP_TIME_MS (MAX_SQLITE_BUSY_LOOP_TIME * 1000)

#if GLIB_CHECK_VERSION (2, 30, 0)
#else
#define G_VALUE_INIT { 0, { { 0 } } }
#endif

typedef struct _AgAccountChanges AgAccountChanges;

struct _AgAccountChanges {
    gboolean deleted;
    gboolean created;

    /* The keys of the table are service names, and the values are
     * AgServiceChanges structures */
    GHashTable *services;
};

G_GNUC_INTERNAL
void _ag_account_store_completed (AgAccount *account,
                                  AgAccountChanges *changes);

G_GNUC_INTERNAL
void _ag_account_done_changes (AgAccount *account, AgAccountChanges *changes);

G_GNUC_INTERNAL
GVariant *_ag_account_build_signal (AgAccount *account,
                                    AgAccountChanges *changes,
                                    const struct timespec *ts);
G_GNUC_INTERNAL
AgAccountChanges *_ag_account_changes_from_dbus (AgManager *manager,
                                                 GVariant *v_services,
                                                 gboolean created,
                                                 gboolean deleted);

G_GNUC_INTERNAL
GHashTable *_ag_account_get_service_changes (AgAccount *account,
                                             AgService *service);

G_GNUC_INTERNAL
void _ag_manager_exec_transaction (AgManager *manager, const gchar *sql,
                                   AgAccountChanges *changes,
                                   AgAccount *account,
                                   GSimpleAsyncResult *async_result,
                                   GCancellable *cancellable);

typedef gboolean (*AgQueryCallback) (sqlite3_stmt *stmt, gpointer user_data);

G_GNUC_INTERNAL
void _ag_manager_exec_transaction_blocking (AgManager *manager,
                                            const gchar *sql,
                                            AgAccountChanges *changes,
                                            AgAccount *account,
                                            GError **error);
G_GNUC_INTERNAL
gint _ag_manager_exec_query (AgManager *manager,
                             AgQueryCallback callback, gpointer user_data,
                             const gchar *sql);
G_GNUC_INTERNAL
void _ag_manager_take_error (AgManager *manager, GError *error);
G_GNUC_INTERNAL
const GError *_ag_manager_get_last_error (AgManager *manager);

G_GNUC_INTERNAL
AgService *_ag_manager_get_service_lazy (AgManager *manager,
                                         const gchar *service_name,
                                         const gchar *service_type,
                                         const gint service_id);
G_GNUC_INTERNAL
guint _ag_manager_get_service_id (AgManager *manager, AgService *service);

struct _AgService {
    /*< private >*/
    gint ref_count;
    gchar *name;
    gchar *display_name;
    gchar *description;
    gchar *type;
    gchar *provider;
    gchar *icon_name;
    gchar *i18n_domain;
    gchar *file_data;
    gsize type_data_offset;
    gint id;
    GHashTable *default_settings;
    GHashTable *tags;
};

G_GNUC_INTERNAL
AgService *_ag_service_new_from_file (const gchar *service_name);
G_GNUC_INTERNAL
AgService *_ag_service_new_from_memory (const gchar *service_name,
                                        const gchar *service_type,
                                        const gint service_id);

G_GNUC_INTERNAL
GHashTable *_ag_service_load_default_settings (AgService *service);

G_GNUC_INTERNAL
GVariant *_ag_service_get_default_setting (AgService *service,
                                           const gchar *key);

G_GNUC_INTERNAL
AgService *_ag_service_new (void);

struct _AgProvider {
    /*< private >*/
    gint ref_count;
    gchar *i18n_domain;
    gchar *icon_name;
    gchar *name;
    gchar *display_name;
    gchar *description;
    gchar *domains;
    gchar *plugin_name;
    gchar *file_data;
    gboolean single_account;
    GHashTable *default_settings;
};

G_GNUC_INTERNAL
AgProvider *_ag_provider_new_from_file (const gchar *provider_name);

G_GNUC_INTERNAL
GHashTable *_ag_provider_load_default_settings (AgProvider *provider);

G_GNUC_INTERNAL
GVariant *_ag_provider_get_default_setting (AgProvider *provider,
                                            const gchar *key);

G_GNUC_INTERNAL
GPtrArray *_ag_account_changes_get_service_types (AgAccountChanges *changes);

G_GNUC_INTERNAL
gboolean _ag_account_changes_have_service_type (AgAccountChanges *changes,
                                                gchar *service_type);

G_GNUC_INTERNAL
gboolean _ag_account_changes_have_enabled (AgAccountChanges *changes);

G_GNUC_INTERNAL
GList *_ag_manager_list_all (AgManager *manager);

G_GNUC_INTERNAL
void _ag_account_changes_free (AgAccountChanges *change);

G_GNUC_INTERNAL
void _ag_account_settings_iter_init (AgAccount *account,
                                     AgAccountSettingIter *iter,
                                     const gchar *key_prefix,
                                     gboolean copy_string);

/* Service type functions */
G_GNUC_INTERNAL
AgServiceType *_ag_service_type_new_from_file (const gchar *service_type_name);

/* AgAuthData functions */
G_GNUC_INTERNAL
AgAuthData *_ag_auth_data_new (AgAccount *account, AgService *service);

/* Application functions */
G_GNUC_INTERNAL
AgApplication *_ag_application_new_from_file (const gchar *application_name);

/* Application functions */
G_GNUC_INTERNAL
gboolean _ag_application_supports_service (AgApplication *self,
                                           AgService *service);

#endif /* _AG_INTERNALS_H_ */
