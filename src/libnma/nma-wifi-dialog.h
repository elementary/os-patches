/* NetworkManager Wireless Applet -- Display wireless access points and allow user control
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
 * Copyright 2007 - 2014 Red Hat, Inc.
 */


/* WARNING: this file is private API between nm-applet and various GNOME
 * bits; it may change without notice and is not guaranteed to be stable.
 */

#ifndef NMA_WIFI_DIALOG_H
#define NMA_WIFI_DIALOG_H

#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>

#include <NetworkManager.h>

#define NMA_TYPE_WIFI_DIALOG            (nma_wifi_dialog_get_type ())
#define NMA_WIFI_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NMA_TYPE_WIFI_DIALOG, NMAWifiDialog))
#define NMA_WIFI_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NMA_TYPE_WIFI_DIALOG, NMAWifiDialogClass))
#define NMA_IS_WIFI_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NMA_TYPE_WIFI_DIALOG))
#define NMA_IS_WIFI_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NMA_TYPE_WIFI_DIALOG))
#define NMA_WIFI_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NMA_TYPE_WIFI_DIALOG, NMAWifiDialogClass))

typedef struct {
	GtkDialog parent;
} NMAWifiDialog;

typedef struct {
	GtkDialogClass parent;
} NMAWifiDialogClass;

GType nma_wifi_dialog_get_type (void);

GtkWidget *nma_wifi_dialog_new (NMClient *client,
                                NMConnection *connection,
                                NMDevice *device,
                                NMAccessPoint *ap,
                                gboolean secrets_only);

GtkWidget *nma_wifi_dialog_new_for_hidden (NMClient *client);

GtkWidget *nma_wifi_dialog_new_for_create (NMClient *client);

NMConnection * nma_wifi_dialog_get_connection (NMAWifiDialog *self,
                                               NMDevice **device,
                                               NMAccessPoint **ap);

GLIB_DEPRECATED
GtkWidget * nma_wifi_dialog_nag_user (NMAWifiDialog *self);

GLIB_DEPRECATED
void nma_wifi_dialog_set_nag_ignored (NMAWifiDialog *self, gboolean ignored);

GLIB_DEPRECATED
gboolean nma_wifi_dialog_get_nag_ignored (NMAWifiDialog *self);

GLIB_DEPRECATED_FOR(nma_wifi_dialog_new_for_hidden)
GtkWidget *nma_wifi_dialog_new_for_other (NMClient *client);

#endif	/* NMA_WIFI_DIALOG_H */

