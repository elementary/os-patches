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

/*
 * The profile UUID list is provided by the Bluetooth SIG:
 * https://www.bluetooth.org/en-us/specification/assigned-numbers-overview/service-discovery
 */
#define BLUETOOTH_UUID_SPP		0x1101
#define BLUETOOTH_UUID_DUN		0x1103
#define BLUETOOTH_UUID_IRMC		0x1104
#define BLUETOOTH_UUID_OPP		0x1105
#define BLUETOOTH_UUID_FTP		0x1106
#define BLUETOOTH_UUID_HSP		0x1108
#define BLUETOOTH_UUID_A2DP_SOURCE	0x110A
#define BLUETOOTH_UUID_A2DP_SINK	0x110B
#define BLUETOOTH_UUID_AVRCP_TARGET	0x110C
#define BLUETOOTH_UUID_AVRCP_CONTROL	0x110E
#define BLUETOOTH_UUID_HSP_AG		0x1112
#define BLUETOOTH_UUID_PAN_PANU		0x1115
#define BLUETOOTH_UUID_PAN_NAP		0x1116
#define BLUETOOTH_UUID_PAN_GN		0x1117
#define BLUETOOTH_UUID_HFP_HF		0x111E
#define BLUETOOTH_UUID_HFP_AG		0x111F
#define BLUETOOTH_UUID_HID		0x1124
#define BLUETOOTH_UUID_SAP		0x112d
#define BLUETOOTH_UUID_PBAP		0x112F
#define BLUETOOTH_UUID_GENERIC_AUDIO	0x1203
#define BLUETOOTH_UUID_SDP		0x1000
#define BLUETOOTH_UUID_PNP		0x1200
#define BLUETOOTH_UUID_GENERIC_NET	0x1201
#define BLUETOOTH_UUID_VDP_SOURCE	0x1303

BluetoothType  bluetooth_class_to_type  (guint32 class);
const gchar   *bluetooth_type_to_string (guint type);
gboolean       bluetooth_verify_address (const char *bdaddr);
const char    *bluetooth_uuid_to_string (const char *uuid);

void bluetooth_send_to_address (const char *address,
				const char *alias);

G_END_DECLS

#endif /* __BLUETOOTH_UTILS_H */
