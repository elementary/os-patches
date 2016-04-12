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

#ifndef __PAGE_VPN_H__
#define __PAGE_VPN_H__

#include <glib.h>
#include <glib-object.h>

#include "ce-page.h"

#define CE_TYPE_PAGE_VPN            (ce_page_vpn_get_type ())
#define CE_PAGE_VPN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CE_TYPE_PAGE_VPN, CEPageVpn))
#define CE_PAGE_VPN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CE_TYPE_PAGE_VPN, CEPageVpnClass))
#define CE_IS_PAGE_VPN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CE_TYPE_PAGE_VPN))
#define CE_IS_PAGE_VPN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CE_TYPE_PAGE_VPN))
#define CE_PAGE_VPN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CE_TYPE_PAGE_VPN, CEPageVpnClass))

typedef struct {
	CEPage parent;
} CEPageVpn;

typedef struct {
	CEPageClass parent;
} CEPageVpnClass;

GType ce_page_vpn_get_type (void);

CEPage *ce_page_vpn_new (NMConnectionEditor *editor,
                         NMConnection *connection,
                         GtkWindow *parent,
                         NMClient *client,
                         const char **out_secrets_setting_name,
                         GError **error);

gboolean ce_page_vpn_can_export (CEPageVpn *page);

void vpn_connection_new (GtkWindow *parent,
                         const char *detail,
                         NMClient *client,
                         PageNewConnectionResultFunc result_func,
                         gpointer user_data);

void vpn_connection_import (GtkWindow *parent,
                            const char *detail,
                            NMClient *client,
                            PageNewConnectionResultFunc result_func,
                            gpointer user_data);

#endif  /* __PAGE_VPN_H__ */
