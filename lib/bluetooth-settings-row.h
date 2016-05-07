/*
 *
 *  Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
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

#ifndef __BLUETOOTH_SETTINGS_ROW_H
#define __BLUETOOTH_SETTINGS_ROW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BLUETOOTH_TYPE_SETTINGS_ROW (bluetooth_settings_row_get_type())
#define BLUETOOTH_SETTINGS_ROW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
				BLUETOOTH_TYPE_SETTINGS_ROW, BluetoothSettingsRow))
#define BLUETOOTH_SETTINGS_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
				BLUETOOTH_TYPE_SETTINGS_ROW, BluetoothSettingsRowClass))
#define BLUETOOTH_IS_SETTINGS_ROW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						BLUETOOTH_TYPE_SETTINGS_ROW))
#define BLUETOOTH_IS_SETTINGS_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						BLUETOOTH_TYPE_SETTINGS_ROW))
#define BLUETOOTH_GET_SETTINGS_ROW_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
				BLUETOOTH_TYPE_SETTINGS_ROW, BluetoothSettingsRowClass))

/**
 * BluetoothSettingsRow:
 *
 * The <structname>BluetoothSettingsRow</structname> struct contains
 * only private fields and should not be directly accessed.
 */
typedef struct _BluetoothSettingsRow BluetoothSettingsRow;
typedef struct _BluetoothSettingsRowClass BluetoothSettingsRowClass;

struct _BluetoothSettingsRow {
	GtkListBoxRow parent;
};

struct _BluetoothSettingsRowClass {
	GtkListBoxRowClass parent_class;
};

GType bluetooth_settings_row_get_type (void);

GtkWidget *bluetooth_settings_row_new (void);

G_END_DECLS

#endif /* __BLUETOOTH_SETTINGS_ROW_H */
