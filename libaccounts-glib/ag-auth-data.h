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

#ifndef _AG_AUTH_DATA_H_
#define _AG_AUTH_DATA_H_

#include <glib-object.h>
#include <libaccounts-glib/ag-types.h>

G_BEGIN_DECLS

GType ag_auth_data_get_type (void) G_GNUC_CONST;

AgAuthData *ag_auth_data_ref (AgAuthData *self);
void ag_auth_data_unref (AgAuthData *self);

guint ag_auth_data_get_credentials_id (AgAuthData *self);
const gchar *ag_auth_data_get_method (AgAuthData *self);
const gchar *ag_auth_data_get_mechanism (AgAuthData *self);

#ifndef AG_DISABLE_DEPRECATED
AG_DEPRECATED_FOR(ag_auth_data_get_login_parameters)
GHashTable *ag_auth_data_get_parameters (AgAuthData *self);

AG_DEPRECATED_FOR(ag_auth_data_get_login_parameters)
void ag_auth_data_insert_parameters (AgAuthData *self, GHashTable *parameters);
#endif

GVariant *ag_auth_data_get_login_parameters (AgAuthData *self,
                                             GVariant *extra_parameters);

G_END_DECLS

#endif /* _AG_AUTH_DATA_H_ */
