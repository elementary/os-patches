/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* inclusion guard */
#ifndef __BLUETOOTH_APPLET_H__
#define __BLUETOOTH_APPLET_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <bluetooth-enums.h>
#include <bluetooth-killswitch.h>

/**
 * BluetoothCapabilities:
 *
 * special actions that can be invoked on the object
 */

typedef enum {
  BLUETOOTH_CAPABILITIES_NONE = 0,
  BLUETOOTH_CAPABILITIES_OBEX_PUSH = 0x1,
  BLUETOOTH_CAPABILITIES_OBEX_FILE_TRANSFER = 0x2
} BluetoothCapabilities;

/**
 * BluetoothSimpleDevice:
 *
 * represents user visible properties of a device known to the default adapter
 */

typedef struct {
  char* bdaddr;
  char* device_path;
  char* alias;
  gboolean connected;
  gboolean can_connect;
  guint capabilities;
  BluetoothType type;
} BluetoothSimpleDevice;

#define BLUETOOTH_TYPE_SIMPLE_DEVICE	(bluetooth_simple_device_get_type ())

GType bluetooth_simple_device_get_type (void) G_GNUC_CONST;

/*
 * Type macros.
 */
#define BLUETOOTH_TYPE_APPLET                  (bluetooth_applet_get_type ())
#define BLUETOOTH_APPLET(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), BLUETOOTH_TYPE_APPLET, BluetoothApplet))
#define BLUETOOTH_IS_APPLET(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BLUETOOTH_TYPE_APPLET))
#define BLUETOOTH_APPLET_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), BLUETOOTH_TYPE_APPLET, BluetoothAppletClass))
#define BLUETOOTH_IS_APPLET_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), BLUETOOTH_TYPE_APPLET))
#define BLUETOOTH_APPLET_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), BLUETOOTH_TYPE_APPLET, BluetoothAppletClass))

/* These structs are fully opaque, type is not derivable */
typedef struct _BluetoothApplet        BluetoothApplet;
typedef struct _BluetoothAppletClass   BluetoothAppletClass;

/* used by BLUETOOTH_TYPE_APPLET */
GType bluetooth_applet_get_type (void) G_GNUC_CONST;

/*
 * Method definitions.
 */

GList* bluetooth_applet_get_devices(BluetoothApplet* self);

BluetoothKillswitchState bluetooth_applet_get_killswitch_state(BluetoothApplet* self);
gboolean bluetooth_applet_set_killswitch_state(BluetoothApplet* self, BluetoothKillswitchState state);

gboolean bluetooth_applet_get_discoverable(BluetoothApplet* self);
void bluetooth_applet_set_discoverable(BluetoothApplet* self, gboolean visible);

typedef void (*BluetoothAppletConnectFunc) (BluetoothApplet *applet,
					    gboolean success,
					    gpointer data);

gboolean bluetooth_applet_connect_device(BluetoothApplet *applet,
					  const char *device,
					  BluetoothAppletConnectFunc func,
					  gpointer data);
gboolean bluetooth_applet_disconnect_device (BluetoothApplet *applet,
					      const char *device,
					      BluetoothAppletConnectFunc func,
					      gpointer data);

void bluetooth_applet_send_to_address (BluetoothApplet *applet,
				       const char *address,
				       const char *alias);

gboolean bluetooth_applet_get_show_full_menu(BluetoothApplet* self);

void bluetooth_applet_agent_reply_pincode(BluetoothApplet* self, const gchar* request_key, const gchar* pincode);
void bluetooth_applet_agent_reply_passkey(BluetoothApplet* self, const gchar* request_key, gint passkey);
void bluetooth_applet_agent_reply_confirm(BluetoothApplet* self, const gchar* request_key, gboolean confirm);
void bluetooth_applet_agent_reply_auth(BluetoothApplet* self, const gchar* request_key, gboolean auth, gboolean trusted);

#endif /* __BLUETOOTH_APPLET_H__ */
