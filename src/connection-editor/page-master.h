/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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
 * Copyright 2012 - 2014 Red Hat, Inc.
 */

#ifndef __PAGE_MASTER_H__
#define __PAGE_MASTER_H__

#include <glib.h>
#include <glib-object.h>

#include "ce-page.h"
#include "connection-helpers.h"

#define CE_TYPE_PAGE_MASTER            (ce_page_master_get_type ())
#define CE_PAGE_MASTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CE_TYPE_PAGE_MASTER, CEPageMaster))
#define CE_PAGE_MASTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CE_TYPE_PAGE_MASTER, CEPageMasterClass))
#define CE_IS_PAGE_MASTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CE_TYPE_PAGE_MASTER))
#define CE_IS_PAGE_MASTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CE_TYPE_PAGE_MASTER))
#define CE_PAGE_MASTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CE_TYPE_PAGE_MASTER, CEPageMasterClass))

typedef struct {
	CEPage parent;

	gboolean aggregating;
} CEPageMaster;

typedef struct {
	CEPageClass parent;

	/* signals */
	void (*create_connection)  (CEPageMaster *self, NMConnection *connection);
	void (*connection_added)   (CEPageMaster *self, NMConnection *connection);
	void (*connection_removed) (CEPageMaster *self, NMConnection *connection);

	/* methods */
	void (*add_slave) (CEPageMaster *self, NewConnectionResultFunc result_func);
} CEPageMasterClass;

GType ce_page_master_get_type (void);

gboolean ce_page_master_has_slaves (CEPageMaster *self);

#endif  /* __PAGE_MASTER_H__ */

