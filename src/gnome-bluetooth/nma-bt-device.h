/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 *  NetworkManager Applet
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * (C) Copyright 2009 - 2012 Red Hat, Inc.
 *
 */

#ifndef NMA_BT_DEVICE_H
#define NMA_BT_DEVICE_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>

#define NMA_TYPE_BT_DEVICE            (nma_bt_device_get_type ())
#define NMA_BT_DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NMA_TYPE_BT_DEVICE, NmaBtDevice))
#define NMA_BT_DEVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NMA_TYPE_BT_DEVICE, NmaBtDeviceClass))
#define NMA_IS_BT_DEVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NMA_TYPE_BT_DEVICE))
#define NMA_IS_BT_DEVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NMA_TYPE_BT_DEVICE))
#define NMA_BT_DEVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NMA_TYPE_BT_DEVICE, NmaBtDeviceClass))

#define NMA_BT_DEVICE_BDADDR      "bdaddr"
#define NMA_BT_DEVICE_ALIAS       "alias"
#define NMA_BT_DEVICE_OBJECT_PATH "object-path"
#define NMA_BT_DEVICE_HAS_PAN     "has-pan"
#define NMA_BT_DEVICE_PAN_ENABLED "pan-enabled"
#define NMA_BT_DEVICE_HAS_DUN     "has-dun"
#define NMA_BT_DEVICE_DUN_ENABLED "dun-enabled"
#define NMA_BT_DEVICE_BUSY        "busy"
#define NMA_BT_DEVICE_STATUS      "status"

typedef struct {
	GObject parent;
} NmaBtDevice;

typedef struct {
	GObjectClass parent;
} NmaBtDeviceClass;

GType nma_bt_device_get_type (void);

NmaBtDevice *nma_bt_device_new (const char *bdaddr,
                                const char *alias,
                                const char *object_path,
                                gboolean has_pan,
                                gboolean has_dun);

void nma_bt_device_set_parent_window   (NmaBtDevice *device,
                                        GtkWindow *window);

const char *nma_bt_device_get_bdaddr   (NmaBtDevice *device);

gboolean nma_bt_device_get_has_dun     (NmaBtDevice *device);
gboolean nma_bt_device_get_dun_enabled (NmaBtDevice *device);
void     nma_bt_device_set_dun_enabled (NmaBtDevice *device, gboolean enabled);

void     nma_bt_device_cancel_dun      (NmaBtDevice *device);

gboolean nma_bt_device_get_has_pan     (NmaBtDevice *device);
gboolean nma_bt_device_get_pan_enabled (NmaBtDevice *device);
void     nma_bt_device_set_pan_enabled (NmaBtDevice *device, gboolean enabled);

gboolean nma_bt_device_get_busy (NmaBtDevice *device);

const char *nma_bt_device_get_status (NmaBtDevice *device);

#endif /* NMA_BT_DEVICE_H */

