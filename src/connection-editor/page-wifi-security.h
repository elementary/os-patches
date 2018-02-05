/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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
 * Copyright 2008 - 2014 Red Hat, Inc.
 */

#ifndef __PAGE_WIFI_SECURITY_H__
#define __PAGE_WIFI_SECURITY_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "ce-page.h"

#define CE_TYPE_PAGE_WIFI_SECURITY            (ce_page_wifi_security_get_type ())
#define CE_PAGE_WIFI_SECURITY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CE_TYPE_PAGE_WIFI_SECURITY, CEPageWifiSecurity))
#define CE_PAGE_WIFI_SECURITY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CE_TYPE_PAGE_WIFI_SECURITY, CEPageWifiSecurityClass))
#define CE_IS_PAGE_WIFI_SECURITY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CE_TYPE_PAGE_WIFI_SECURITY))
#define CE_IS_PAGE_WIFI_SECURITY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CE_TYPE_PAGE_WIFI_SECURITY))
#define CE_PAGE_WIFI_SECURITY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CE_TYPE_PAGE_WIFI_SECURITY, CEPageWifiSecurityClass))

typedef struct {
	CEPage parent;
} CEPageWifiSecurity;

typedef struct {
	CEPageClass parent;
} CEPageWifiSecurityClass;

GType ce_page_wifi_security_get_type (void);

CEPage *ce_page_wifi_security_new (NMConnectionEditor *editor,
                                   NMConnection *connection,
                                   GtkWindow *parent,
                                   NMClient *client,
                                   const char **out_secrets_setting_name,
                                   GError **error);

#endif  /* __PAGE_WIFI_SECURITY_H__ */

