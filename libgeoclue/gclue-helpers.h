/* vim: set et ts=8 sw=8: */
/*
 * Geoclue convenience library.
 *
 * Copyright 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#ifndef __GCLUE_HELPERS_H__
#define __GCLUE_HELPERS_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <geoclue.h>

G_BEGIN_DECLS

void            gclue_client_proxy_create        (const char         *desktop_id,
                                                  GClueAccuracyLevel  accuracy_level,
                                                  GCancellable       *cancellable,
                                                  GAsyncReadyCallback callback,
                                                  gpointer            user_data);
GClueClient *   gclue_client_proxy_create_finish (GAsyncResult       *result,
                                                  GError            **error);
GClueClient *   gclue_client_proxy_create_sync   (const char         *desktop_id,
                                                  GClueAccuracyLevel  accuracy_level,
                                                  GCancellable       *cancellable,
                                                  GError            **error);

void            gclue_client_proxy_create_full        (const char                   *desktop_id,
                                                       GClueAccuracyLevel            accuracy_level,
                                                       GClueClientProxyCreateFlags   flags,
                                                       GCancellable                 *cancellable,
                                                       GAsyncReadyCallback           callback,
                                                       gpointer                      user_data);
GClueClient *   gclue_client_proxy_create_full_finish (GAsyncResult                 *result,
                                                       GError                      **error);
GClueClient *   gclue_client_proxy_create_full_sync   (const char                   *desktop_id,
                                                       GClueAccuracyLevel            accuracy_level,
                                                       GClueClientProxyCreateFlags  flags,
                                                       GCancellable                 *cancellable,
                                                       GError                      **error);

G_END_DECLS

#endif /* #ifndef __GCLUE_HELPERS_H__*/
