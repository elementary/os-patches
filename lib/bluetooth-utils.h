/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2009-2011  Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2010       Giovanni Campagna <scampa.giovanni@gmail.com>
 *
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
 */

#ifndef __BLUETOOTH_UTILS_H
#define __BLUETOOTH_UTILS_H

#include <gio/gio.h>
#include <bluetooth-enums.h>

G_BEGIN_DECLS

BluetoothType  bluetooth_class_to_type  (guint32 class);
const gchar   *bluetooth_type_to_string (guint type);
gboolean       bluetooth_verify_address (const char *bdaddr);
const char    *bluetooth_uuid_to_string (const char *uuid);

void bluetooth_send_to_address (const char *address,
				const char *alias);

G_END_DECLS

#endif /* __BLUETOOTH_UTILS_H */
