/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 * Copyright (C) 2012 Canonical Ltd.
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

#ifndef _AG_SERVICE_H_
#define _AG_SERVICE_H_

#include <glib.h>
#include <glib-object.h>
#include <libaccounts-glib/ag-types.h>

G_BEGIN_DECLS


GType ag_service_get_type (void) G_GNUC_CONST;

const gchar *ag_service_get_name (AgService *service);
const gchar *ag_service_get_display_name (AgService *service);
const gchar *ag_service_get_description (AgService *service);
const gchar *ag_service_get_service_type (AgService *service);
const gchar *ag_service_get_provider (AgService *service);
const gchar *ag_service_get_icon_name (AgService *service);
const gchar *ag_service_get_i18n_domain (AgService *service);
gboolean ag_service_has_tag (AgService *service, const gchar *tag);
GList *ag_service_get_tags (AgService *service);
void ag_service_get_file_contents (AgService *service,
                                   const gchar **contents,
                                   gsize *data_offset);
AgService *ag_service_ref (AgService *service);
void ag_service_unref (AgService *service);
void ag_service_list_free (GList *list);


G_END_DECLS

#endif /* _AG_SERVICE_H_ */
