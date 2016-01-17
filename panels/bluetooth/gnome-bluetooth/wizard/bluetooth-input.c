/*
 *
 *  Copyright (C) 2009 Bastien Nocera <hadess@hadess.net>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "bluetooth-input.h"

#undef FAKE_RUN

enum {
	KEYBOARD_APPEARED,
	KEYBOARD_DISAPPEARED,
	MOUSE_APPEARED,
	MOUSE_DISAPPEARED,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

#define BLUETOOTH_INPUT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				BLUETOOTH_TYPE_INPUT, BluetoothInputPrivate))

typedef struct _BluetoothInputPrivate BluetoothInputPrivate;

struct _BluetoothInputPrivate {
	int has_mouse;
	int has_keyboard;
};

G_DEFINE_TYPE(BluetoothInput, bluetooth_input, G_TYPE_OBJECT)

static gboolean
bluetooth_input_ignore_device (const char *name)
{
	guint i;
	const char const *names[] = {
		"Virtual core XTEST pointer",
		"Macintosh mouse button emulation",
		"Virtual core XTEST keyboard",
		"Power Button",
		"Video Bus",
		"Sleep Button",
		"UVC Camera",
		"USB Audio",
		"Integrated Camera",
		"ThinkPad Extra Buttons"
	};

	for (i = 0 ; i < G_N_ELEMENTS (names); i++) {
		if (g_strcmp0 (name, names[i]) == 0)
			return TRUE;
	}
	return FALSE;
}

void
bluetooth_input_check_for_devices (BluetoothInput *input)
{
	BluetoothInputPrivate *priv = BLUETOOTH_INPUT_GET_PRIVATE(input);
	GdkDeviceManager *manager;
	GList *devices, *l;
	gboolean has_keyboard, has_mouse;

	has_keyboard = FALSE;
	has_mouse = FALSE;

	manager = gdk_display_get_device_manager (gdk_display_get_default ());
	devices = gdk_device_manager_list_devices (manager, GDK_DEVICE_TYPE_SLAVE);

	for (l = devices; l != NULL; l = l->next) {
		GdkDevice *device = l->data;
		GdkInputSource source;

#ifndef FAKE_RUN
		if (bluetooth_input_ignore_device (gdk_device_get_name (device)) != FALSE)
			continue;
		source = gdk_device_get_source (device);
		if (source == GDK_SOURCE_KEYBOARD && !has_keyboard) {
			g_debug ("has keyboard: %s", gdk_device_get_name (device));
			has_keyboard = TRUE;
		} else if (!has_mouse) {
			g_debug ("has mouse: %s", gdk_device_get_name (device));
			has_mouse = TRUE;
		}
#else
		/* No mouse, unless my Bluetooth mouse is there */
		has_keyboard = TRUE;

		if (g_str_equal ("hadess’s mouse", gdk_device_get_name (device)))
			has_mouse = TRUE;
#endif

		if (has_mouse && has_keyboard)
			break;
	}

	if (has_mouse != priv->has_mouse) {
		priv->has_mouse = has_mouse;
		if (has_mouse)
			g_signal_emit_by_name (input, "mouse-appeared");
		else
			g_signal_emit_by_name (input, "mouse-disappeared");
	}
	if (has_keyboard != priv->has_keyboard) {
		priv->has_keyboard = has_keyboard;
		if (has_keyboard)
			g_signal_emit_by_name (input, "keyboard-appeared");
		else
			g_signal_emit_by_name (input, "keyboard-disappeared");
	}

	g_list_free (devices);
}

static void
device_changed_cb (GdkDeviceManager *device_manager,
		   GdkDevice        *device,
		   BluetoothInput   *input)
{
	bluetooth_input_check_for_devices (input);
}

static void
set_devicepresence_handler (BluetoothInput *input)
{
	GdkDeviceManager *manager;

	manager = gdk_display_get_device_manager (gdk_display_get_default ());
	g_signal_connect (manager, "device-added",
			  G_CALLBACK (device_changed_cb), input);
	g_signal_connect (manager, "device-removed",
			  G_CALLBACK (device_changed_cb), input);
	g_signal_connect (manager, "device-changed",
			  G_CALLBACK (device_changed_cb), input);
}

static void bluetooth_input_init(BluetoothInput *input)
{
	BluetoothInputPrivate *priv = BLUETOOTH_INPUT_GET_PRIVATE(input);

	priv->has_mouse = -1;
	priv->has_keyboard = -1;
	set_devicepresence_handler (input);
}

static void
bluetooth_input_finalize (GObject *input)
{
	GdkDeviceManager *manager;

	manager = gdk_display_get_device_manager (gdk_display_get_default ());
	g_signal_handlers_disconnect_by_func (manager, device_changed_cb, input);

	G_OBJECT_CLASS(bluetooth_input_parent_class)->finalize(input);
}

static void
bluetooth_input_class_init (BluetoothInputClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	g_type_class_add_private(klass, sizeof(BluetoothInputPrivate));

	object_class->finalize = bluetooth_input_finalize;

	signals[KEYBOARD_APPEARED] = g_signal_new ("keyboard-appeared",
						   G_TYPE_FROM_CLASS (klass),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (BluetoothInputClass, keyboard_appeared),
						   NULL, NULL,
						   g_cclosure_marshal_VOID__VOID,
						   G_TYPE_NONE, 0, G_TYPE_NONE);
	signals[KEYBOARD_DISAPPEARED] = g_signal_new ("keyboard-disappeared",
						   G_TYPE_FROM_CLASS (klass),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (BluetoothInputClass, keyboard_disappeared),
						   NULL, NULL,
						   g_cclosure_marshal_VOID__VOID,
						   G_TYPE_NONE, 0, G_TYPE_NONE);
	signals[MOUSE_APPEARED] = g_signal_new ("mouse-appeared",
						G_TYPE_FROM_CLASS (klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET (BluetoothInputClass, mouse_appeared),
						NULL, NULL,
						g_cclosure_marshal_VOID__VOID,
						G_TYPE_NONE, 0, G_TYPE_NONE);
	signals[MOUSE_DISAPPEARED] = g_signal_new ("mouse-disappeared",
						   G_TYPE_FROM_CLASS (klass),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (BluetoothInputClass, mouse_disappeared),
						   NULL, NULL,
						   g_cclosure_marshal_VOID__VOID,
						   G_TYPE_NONE, 0, G_TYPE_NONE);
}

/**
 * bluetooth_input_new:
 *
 * Return value: a reference to the #BluetoothInput singleton or %NULL when XInput is not supported. Unref the object when done.
 **/
BluetoothInput *
bluetooth_input_new (void)
{
	static BluetoothInput *bluetooth_input = NULL;

	if (bluetooth_input != NULL)
		return g_object_ref (bluetooth_input);

	bluetooth_input = BLUETOOTH_INPUT (g_object_new (BLUETOOTH_TYPE_INPUT, NULL));
	g_object_add_weak_pointer (G_OBJECT (bluetooth_input),
				   (gpointer) &bluetooth_input);

	return bluetooth_input;
}

