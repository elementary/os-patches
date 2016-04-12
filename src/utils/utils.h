/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
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
 * Copyright 2007 - 2015 Red Hat, Inc.
 */

#ifndef UTILS_H
#define UTILS_H

#include <glib.h>
#include <gtk/gtk.h>

#include <net/ethernet.h>

#if defined (LIBNM_BUILD)
#include <NetworkManager.h>
#elif defined (LIBNM_GLIB_BUILD)
#include <nm-connection.h>
#include <nm-device.h>
#include <nm-access-point.h>
#else
#error neither LIBNM_BUILD nor LIBNM_GLIB_BUILD defined
#endif

gboolean utils_ether_addr_valid (const struct ether_addr *test_addr);

#ifdef LIBNM_BUILD
char *utils_hash_ap (GBytes *ssid,
                     NM80211Mode mode,
                     guint32 flags,
                     guint32 wpa_flags,
                     guint32 rsn_flags);
#else
char *utils_hash_ap (const GByteArray *ssid,
                     NM80211Mode mode,
                     guint32 flags,
                     guint32 wpa_flags,
                     guint32 rsn_flags);
#endif

char *utils_escape_notify_message (const char *src);

char *utils_create_mobile_connection_id (const char *provider,
                                         const char *plan_name);

void utils_show_error_dialog (const char *title,
                              const char *text1,
                              const char *text2,
                              gboolean modal,
                              GtkWindow *parent);

#define NMA_ERROR (g_quark_from_static_string ("nma-error-quark"))

typedef enum  {
	NMA_ERROR_GENERIC
} NMAError;


gboolean utils_char_is_ascii_print (char character);
gboolean utils_char_is_ascii_digit (char character);
gboolean utils_char_is_ascii_ip4_address (char character);
gboolean utils_char_is_ascii_ip6_address (char character);
gboolean utils_char_is_ascii_apn (char character);

typedef gboolean (*UtilsFilterGtkEditableFunc) (char character);
gboolean utils_filter_editable_on_insert_text (GtkEditable *editable,
                                               const gchar *text,
                                               gint length,
                                               gint *position,
                                               void *user_data,
                                               UtilsFilterGtkEditableFunc validate_character,
                                               gpointer block_func);

void utils_override_bg_color (GtkWidget *widget, GdkRGBA *rgba);
void utils_set_cell_background (GtkCellRenderer *cell,
                                const char *color,
                                const char *value);

void utils_fake_return_key (GdkEventKey *event);

#endif /* UTILS_H */

