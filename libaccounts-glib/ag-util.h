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

#ifndef _AG_UTIL_H_
#define _AG_UTIL_H_

#include <glib.h>
#include <glib-object.h>
#include <libxml/xmlreader.h>
#include <sqlite3.h>

G_BEGIN_DECLS

GString *_ag_string_append_printf (GString *string,
                                   const gchar *format,
                                   ...) G_GNUC_INTERNAL;

G_GNUC_INTERNAL
GValue *_ag_value_slice_dup (const GValue *value);

G_GNUC_INTERNAL
void _ag_value_slice_free (GValue *value);

G_GNUC_INTERNAL
GVariant *_ag_value_to_variant (const GValue *value);
G_GNUC_INTERNAL
void _ag_value_from_variant (GValue *value, GVariant *variant);

G_GNUC_INTERNAL
gchar *_ag_value_to_db (GVariant *value, gboolean type_annotate);

G_GNUC_INTERNAL
GVariant *_ag_value_from_db (sqlite3_stmt *stmt, gint col_type, gint col_value);

G_GNUC_INTERNAL
const GVariantType *_ag_type_from_g_type (GType type);

G_GNUC_INTERNAL
gboolean _ag_xml_get_boolean (xmlTextReaderPtr reader, gboolean *dest_boolean);

G_GNUC_INTERNAL
gboolean _ag_xml_get_element_data (xmlTextReaderPtr reader,
                                   const gchar **dest_ptr);

G_GNUC_INTERNAL
gboolean _ag_xml_dup_element_data (xmlTextReaderPtr reader, gchar **dest_ptr);

G_GNUC_INTERNAL
gboolean _ag_xml_parse_settings (xmlTextReaderPtr reader, const gchar *group,
                                 GHashTable *settings);

G_GNUC_INTERNAL
gboolean _ag_xml_parse_element_list (xmlTextReaderPtr reader, const gchar *match,
                                     GHashTable **list);

G_GNUC_INTERNAL
gchar *_ag_dbus_escape_as_identifier (const gchar *name);

G_GNUC_INTERNAL
gchar *_ag_find_libaccounts_file (const gchar *file_id,
                                  const gchar *suffix,
                                  const gchar *env_var,
                                  const gchar *subdir);

G_END_DECLS

#endif /* _AG_UTIL_H_ */
