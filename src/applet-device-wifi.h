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
 * (C) Copyright 2008 Red Hat, Inc.
 */

#ifndef __APPLET_DEVICE_WIFI_H__
#define __APPLET_DEVICE_WIFI_H__

#include <gtk/gtk.h>

#include "applet.h"

NMADeviceClass *applet_device_wifi_get_class (NMApplet *applet);

void nma_menu_add_hidden_network_item (GtkWidget *menu, NMApplet *applet);
void nma_menu_add_create_network_item (GtkWidget *menu, NMApplet *applet);

#endif /* __APPLET_DEVICE_WIFI_H__ */
