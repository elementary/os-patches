/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2011 Nokia Corporation.
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

#ifndef _AG_ACCOUNT_SERVICE_H_
#define _AG_ACCOUNT_SERVICE_H_

#include <glib-object.h>
#include <libaccounts-glib/ag-account.h>
#include <libaccounts-glib/ag-types.h>

G_BEGIN_DECLS

#define AG_TYPE_ACCOUNT_SERVICE (ag_account_service_get_type ())
#define AG_ACCOUNT_SERVICE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                 AG_TYPE_ACCOUNT_SERVICE, AgAccountService))
#define AG_ACCOUNT_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                         AG_TYPE_ACCOUNT_SERVICE, \
                                         AgAccountServiceClass))
#define AG_IS_ACCOUNT_SERVICE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                    AG_TYPE_ACCOUNT_SERVICE))
#define AG_IS_ACCOUNT_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                            AG_TYPE_ACCOUNT_SERVICE))
#define AG_ACCOUNT_SERVICE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                           AG_TYPE_ACCOUNT_SERVICE, \
                                           AgAccountServiceClass))

typedef struct _AgAccountServiceClass AgAccountServiceClass;
typedef struct _AgAccountServicePrivate AgAccountServicePrivate;

/**
 * AgAccountServiceClass:
 *
 * Use the accessor functions below.
 */
struct _AgAccountServiceClass
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

struct _AgAccountService
{
    GObject parent_instance;
    AgAccountServicePrivate *priv;
};

GType ag_account_service_get_type (void) G_GNUC_CONST;

AgAccountService *ag_account_service_new (AgAccount *account,
                                          AgService *service);

AgAccount *ag_account_service_get_account (AgAccountService *self);

AgService *ag_account_service_get_service (AgAccountService *self);

gboolean ag_account_service_get_enabled (AgAccountService *self);

#ifndef AG_DISABLE_DEPRECATED
AG_DEPRECATED_FOR(ag_account_service_get_variant)
AgSettingSource ag_account_service_get_value (AgAccountService *self,
                                              const gchar *key,
                                              GValue *value);

AG_DEPRECATED_FOR(ag_account_service_set_variant)
void ag_account_service_set_value (AgAccountService *self, const gchar *key,
                                   const GValue *value);
#endif
GVariant *ag_account_service_get_variant (AgAccountService *self,
                                          const gchar *key,
                                          AgSettingSource *source);
void ag_account_service_set_variant (AgAccountService *self,
                                     const gchar *key,
                                     GVariant *value);

AgAccountSettingIter *
ag_account_service_get_settings_iter (AgAccountService *self,
                                      const gchar *key_prefix);

void ag_account_service_settings_iter_init (AgAccountService *self,
                                            AgAccountSettingIter *iter,
                                            const gchar *key_prefix);

#ifndef AG_DISABLE_DEPRECATED
AG_DEPRECATED_FOR(ag_account_settings_iter_get_next)
gboolean ag_account_service_settings_iter_next (AgAccountSettingIter *iter,
                                                const gchar **key,
                                                const GValue **value);
#endif

AgAuthData *ag_account_service_get_auth_data (AgAccountService *self);

gchar **ag_account_service_get_changed_fields (AgAccountService *self);

G_END_DECLS

#endif /* _AG_ACCOUNT_SERVICE_H_ */
