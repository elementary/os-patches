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

#ifndef _AG_APPLICATION_H_
#define _AG_APPLICATION_H_

#include <gio/gdesktopappinfo.h>
#include <glib-object.h>
#include <libaccounts-glib/ag-types.h>

G_BEGIN_DECLS

GType ag_application_get_type (void) G_GNUC_CONST;

const gchar *ag_application_get_name (AgApplication *self);
const gchar *ag_application_get_description (AgApplication *self);
const gchar *ag_application_get_i18n_domain (AgApplication *self);

GDesktopAppInfo *ag_application_get_desktop_app_info (AgApplication *self);

const gchar *ag_application_get_service_usage(AgApplication *self,
                                              AgService *service);

AgApplication *ag_application_ref (AgApplication *self);
void ag_application_unref (AgApplication *self);

G_END_DECLS

#endif /* _AG_APPLICATION_H_ */
