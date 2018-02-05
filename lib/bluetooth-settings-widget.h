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

#ifndef __BLUETOOTH_SETTINGS_WIDGET_H
#define __BLUETOOTH_SETTINGS_WIDGET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BLUETOOTH_TYPE_SETTINGS_WIDGET (bluetooth_settings_widget_get_type())
#define BLUETOOTH_SETTINGS_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
				BLUETOOTH_TYPE_SETTINGS_WIDGET, BluetoothSettingsWidget))
#define BLUETOOTH_SETTINGS_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
				BLUETOOTH_TYPE_SETTINGS_WIDGET, BluetoothSettingsWidgetClass))
#define BLUETOOTH_IS_SETTINGS_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						BLUETOOTH_TYPE_SETTINGS_WIDGET))
#define BLUETOOTH_IS_SETTINGS_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						BLUETOOTH_TYPE_SETTINGS_WIDGET))
#define BLUETOOTH_GET_SETTINGS_WIDGET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
				BLUETOOTH_TYPE_SETTINGS_WIDGET, BluetoothSettingsWidgetClass))

/**
 * BluetoothSettingsWidget:
 *
 * The <structname>BluetoothSettingsWidget</structname> struct contains
 * only private fields and should not be directly accessed.
 */
typedef struct _BluetoothSettingsWidget BluetoothSettingsWidget;
typedef struct _BluetoothSettingsWidgetClass BluetoothSettingsWidgetClass;

struct _BluetoothSettingsWidget {
	GtkBox parent;
};

struct _BluetoothSettingsWidgetClass {
	GtkBoxClass parent_class;
};

GType bluetooth_settings_widget_get_type (void);

GtkWidget *bluetooth_settings_widget_new (void);

gboolean bluetooth_settings_widget_get_default_adapter_powered (BluetoothSettingsWidget *widget);

G_END_DECLS

#endif /* __BLUETOOTH_SETTINGS_WIDGET_H */
