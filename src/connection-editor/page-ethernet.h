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
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#ifndef __PAGE_ETHERNET_H__
#define __PAGE_ETHERNET_H__

#include <nm-connection.h>

#include <glib.h>
#include <glib-object.h>

#include "ce-page.h"

#define CE_TYPE_PAGE_ETHERNET            (ce_page_ethernet_get_type ())
#define CE_PAGE_ETHERNET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CE_TYPE_PAGE_ETHERNET, CEPageEthernet))
#define CE_PAGE_ETHERNET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CE_TYPE_PAGE_ETHERNET, CEPageEthernetClass))
#define CE_IS_PAGE_ETHERNET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CE_TYPE_PAGE_ETHERNET))
#define CE_IS_PAGE_ETHERNET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CE_TYPE_PAGE_ETHERNET))
#define CE_PAGE_ETHERNET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CE_TYPE_PAGE_ETHERNET, CEPageEthernetClass))

typedef struct {
	CEPage parent;
} CEPageEthernet;

typedef struct {
	CEPageClass parent;
} CEPageEthernetClass;

GType ce_page_ethernet_get_type (void);

CEPage *ce_page_ethernet_new (NMConnectionEditor *editor,
                              NMConnection *connection,
                              GtkWindow *parent,
                              NMClient *client,
                              NMRemoteSettings *settings,
                              const char **out_secrets_setting_name,
                              GError **error);

void ethernet_connection_new (GtkWindow *parent,
                              const char *detail,
                              NMRemoteSettings *settings,
                              PageNewConnectionResultFunc result_func,
                              gpointer user_data);

#endif  /* __PAGE_ETHERNET_H__ */

