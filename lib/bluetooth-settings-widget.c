/*
 *
 *  Copyright (C) 2013  Bastien Nocera <hadess@hadess.net>
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

#include <gtk/gtk.h>

#include <glib/gi18n-lib.h>
#include <math.h>

#include "bluetooth-client.h"
#include "bluetooth-client-private.h"
#include "bluetooth-client-glue.h"
#include "bluetooth-agent.h"
#include "bluetooth-utils.h"
#include "bluetooth-settings-widget.h"
#include "bluetooth-settings-resources.h"
#include "bluetooth-settings-row.h"
#include "bluetooth-settings-obexpush.h"
#include "bluetooth-pairing-dialog.h"
#include "pin.h"

#define BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE(obj) \
	(bluetooth_settings_widget_get_instance_private (obj))

typedef struct _BluetoothSettingsWidgetPrivate BluetoothSettingsWidgetPrivate;

struct _BluetoothSettingsWidgetPrivate {
	GtkBuilder          *builder;
	GtkWidget           *child_box;
	BluetoothClient     *client;
	GtkTreeModel        *model;
	gboolean             debug;
	GCancellable        *cancellable;

	/* Pairing */
	BluetoothAgent      *agent;
	GtkWidget           *pairing_dialog;
	GHashTable          *pairing_devices; /* key=object-path, value=boolean */

	/* Properties */
	GtkWidget           *properties_dialog;
	char                *selected_bdaddr;
	char                *selected_name;
	char                *selected_object_path;

	/* Device section */
	GtkWidget           *device_list;
	GtkAdjustment       *focus_adjustment;
	GtkSizeGroup        *row_sizegroup;
	GtkWidget           *device_stack;
	GtkWidget           *device_spinner;
	GHashTable          *connecting_devices; /* key=bdaddr, value=boolean */

	/* Hack to work-around:
	 * http://thread.gmane.org/gmane.linux.bluez.kernel/41471 */
	GHashTable          *devices_type; /* key=bdaddr, value=guint32 */

	/* Sharing section */
	GtkWidget           *visible_label;
	gboolean             has_console;
	GDBusProxy          *session_proxy;
};

G_DEFINE_TYPE_WITH_PRIVATE(BluetoothSettingsWidget, bluetooth_settings_widget, GTK_TYPE_BOX)

enum {
	PANEL_CHANGED,
	ADAPTER_STATUS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define WID(s) GTK_WIDGET (gtk_builder_get_object (priv->builder, s))

#define KEYBOARD_PREFS		"keyboard"
#define MOUSE_PREFS		"mouse"
#define SOUND_PREFS		"sound"

#define ICON_SIZE 128

/* We'll try to connect to the device repeatedly for that
 * amount of time before we bail out */
#define CONNECT_TIMEOUT 3.0

#define BLUEZ_SERVICE	"org.bluez"
#define ADAPTER_IFACE	"org.bluez.Adapter1"

#define AGENT_PATH "/org/gnome/bluetooth/settings"

#define GNOME_SESSION_DBUS_NAME      "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_OBJECT    "/org/gnome/SessionManager"
#define GNOME_SESSION_DBUS_INTERFACE "org.gnome.SessionManager"

#define FILLER_PAGE                  "filler-page"
#define DEVICES_PAGE                 "devices-page"

enum {
	CONNECTING_NOTEBOOK_PAGE_SWITCH = 0,
	CONNECTING_NOTEBOOK_PAGE_SPINNER = 1
};

static void
set_connecting_page (BluetoothSettingsWidget *self,
		     int               page)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);

	if (page == CONNECTING_NOTEBOOK_PAGE_SPINNER)
		gtk_spinner_start (GTK_SPINNER (WID ("connecting_spinner")));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (WID ("connecting_notebook")), page);
	if (page == CONNECTING_NOTEBOOK_PAGE_SWITCH)
		gtk_spinner_start (GTK_SPINNER (WID ("connecting_spinner")));
}

static void
remove_connecting (BluetoothSettingsWidget *self,
		   const char       *bdaddr)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	g_hash_table_remove (priv->connecting_devices, bdaddr);
}

static void
add_connecting (BluetoothSettingsWidget *self,
		const char       *bdaddr)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	g_hash_table_insert (priv->connecting_devices,
			     g_strdup (bdaddr),
			     GINT_TO_POINTER (1));
}

static gboolean
is_connecting (BluetoothSettingsWidget *self,
	       const char       *bdaddr)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	return GPOINTER_TO_INT (g_hash_table_lookup (priv->connecting_devices,
						     bdaddr));
}

typedef struct {
	char             *bdaddr;
	BluetoothSettingsWidget *self;
} ConnectData;

static void
connect_done (GObject      *source_object,
	      GAsyncResult *res,
	      gpointer      user_data)
{
	BluetoothSettingsWidget *self;
	BluetoothSettingsWidgetPrivate *priv;
	gboolean success;
	GError *error = NULL;
	ConnectData *data = (ConnectData *) user_data;

	success = bluetooth_client_connect_service_finish (BLUETOOTH_CLIENT (source_object),
							   res, &error);
	if (!success && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		goto out;

	self = data->self;
	priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);

	/* Check whether the same device is now selected, and update the UI */
	if (g_strcmp0 (priv->selected_bdaddr, data->bdaddr) == 0) {
		GtkSwitch *button;

		button = GTK_SWITCH (WID ("switch_connection"));
		/* Reset the switch if it failed */
		if (success == FALSE) {
			g_debug ("Connection failed to %s: %s", data->bdaddr, error->message);
			gtk_switch_set_active (button, !gtk_switch_get_active (button));
		}
		set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SWITCH);
	}

	g_clear_error (&error);
	remove_connecting (self, data->bdaddr);

	//FIXME show an error if it failed?

out:
	g_clear_error (&error);
	g_free (data->bdaddr);
	g_free (data);
}

static void
add_device_type (BluetoothSettingsWidget *self,
		 const char              *bdaddr,
		 BluetoothType            type)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	BluetoothType t;

	t = GPOINTER_TO_UINT (g_hash_table_lookup (priv->devices_type, bdaddr));
	if (t == 0 || t == BLUETOOTH_TYPE_ANY) {
		g_hash_table_insert (priv->devices_type, g_strdup (bdaddr), GUINT_TO_POINTER (type));
		g_debug ("Saving device type %s for %s", bluetooth_type_to_string (type), bdaddr);
	}
}

static void
setup_pairing_dialog (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GtkWidget *toplevel;

	g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);
	priv->pairing_dialog = bluetooth_pairing_dialog_new ();
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	gtk_window_set_transient_for (GTK_WINDOW (priv->pairing_dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (priv->pairing_dialog), TRUE);
}

static gboolean
get_properties_for_device (BluetoothSettingsWidget  *self,
			   GDBusProxy               *device,
			   char                    **name,
			   char                    **ret_bdaddr,
			   BluetoothType            *type)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GVariant *value;
	char *bdaddr;

	g_return_val_if_fail (name != NULL, FALSE);

	value = g_dbus_proxy_get_cached_property (device, "Name");
	if (value == NULL) {
		/* What?! */
		return FALSE;
	}
	*name = g_variant_dup_string (value, NULL);
	g_variant_unref (value);

	value = g_dbus_proxy_get_cached_property (device, "Address");
	bdaddr = g_variant_dup_string (value, NULL);
	g_variant_unref (value);

	if (ret_bdaddr)
		*ret_bdaddr = g_strdup (bdaddr);

	if (type) {
		value = g_dbus_proxy_get_cached_property (device, "Class");
		if (value != NULL) {
			*type = bluetooth_class_to_type (g_variant_get_uint32 (value));
			g_variant_unref (value);
		} else {
			*type = GPOINTER_TO_UINT (g_hash_table_lookup (priv->devices_type, bdaddr));
			if (*type == 0)
				*type = BLUETOOTH_TYPE_ANY;
		}
	}

	g_free (bdaddr);

	return TRUE;
}

static char *
get_random_pincode (guint num_digits)
{
	if (num_digits == 0)
		num_digits = PIN_NUM_DIGITS;
	return g_strdup_printf ("%d", g_random_int_range (pow (10, num_digits - 1),
							  pow (10, num_digits)));
}

static char *
get_icade_pincode (char **pin_display_str)
{
	GString *pin, *pin_display;
	guint i;
	static char *arrows[] = {
		NULL,
		"⬆", /* up = 1    */
		"⬇", /* down = 2  */
		"⬅", /* left = 3  */
		"➡"  /* right = 4 */
	};

	pin = g_string_new (NULL);
	pin_display = g_string_new (NULL);

	for (i = 0; i < PIN_NUM_DIGITS; i++) {
		int r;
		char *c;

		r = g_random_int_range (1, 5);

		c = g_strdup_printf ("%d", r);
		g_string_append (pin, c);
		g_free (c);

		g_string_append (pin_display, arrows[r]);
	}
	g_string_append (pin_display, "❍");

	*pin_display_str = g_string_free (pin_display, FALSE);
	return g_string_free (pin, FALSE);
}

static void
display_cb (GtkDialog *dialog,
	    int        response,
	    gpointer   user_data)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);

	g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);
}

static void
enter_pin_cb (GtkDialog *dialog,
	      int        response,
	      gpointer   user_data)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (dialog), "invocation");

	if (response == GTK_RESPONSE_ACCEPT) {
		const char *name;
		char *pin;
		BluetoothPairingMode mode;

		mode = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (dialog), "mode"));
		name = g_object_get_data (G_OBJECT (dialog), "name");
		pin = bluetooth_pairing_dialog_get_pin (BLUETOOTH_PAIRING_DIALOG (dialog));
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", pin));

		if (bluetooth_pairing_dialog_get_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog)) == BLUETOOTH_PAIRING_MODE_PIN_QUERY) {
			g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);
			return;
		}
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
						   mode, pin, name);
		g_free (pin);
		g_signal_connect (G_OBJECT (priv->pairing_dialog), "response",
				  G_CALLBACK (display_cb), user_data);
	} else {
		g_dbus_method_invocation_return_dbus_error (invocation,
							    "org.bluez.Error.Canceled",
							    "User cancelled pairing");
		g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);
		return;
	}

	g_object_set_data (G_OBJECT (priv->pairing_dialog), "invocation", NULL);
	g_object_set_data (G_OBJECT (priv->pairing_dialog), "mode", NULL);
	g_object_set_data (G_OBJECT (priv->pairing_dialog), "name", NULL);
}

static void
confirm_remote_pin_cb (GtkDialog *dialog,
		       int        response,
		       gpointer   user_data)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (dialog), "invocation");

	if (response == GTK_RESPONSE_ACCEPT) {
		GDBusProxy *device;
		const char *pin;

		pin = g_object_get_data (G_OBJECT (invocation), "pin");
		device = g_object_get_data (G_OBJECT (invocation), "device");

		bluetooth_client_set_trusted (priv->client, g_dbus_proxy_get_object_path (device), TRUE);

		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", pin));
	} else {
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", "Pairing refused from settings panel");
	}

	g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);
}

static void
pincode_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy            *device,
		  gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	BluetoothType type;
	char *name, *bdaddr;
	guint max_digits;
	gboolean confirm_pin = TRUE;
	char *default_pin;
	char *display_pin = NULL;
	BluetoothPairingMode mode;
	gboolean remote_initiated;

	g_debug ("pincode_callback (%s)", g_dbus_proxy_get_object_path (device));

	if (!get_properties_for_device (self, device, &name, &bdaddr, &type)) {
		char *msg;

		msg = g_strdup_printf ("Missing information for %s", g_dbus_proxy_get_object_path (device));
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		g_free (msg);
		return;
	}

	remote_initiated = !GPOINTER_TO_UINT (g_hash_table_lookup (priv->pairing_devices,
								   g_dbus_proxy_get_object_path (device)));

	default_pin = get_pincode_for_device (type, bdaddr, name, &max_digits, &confirm_pin);
	if (g_strcmp0 (default_pin, "KEYBOARD") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD;
		g_free (default_pin);
		default_pin = get_random_pincode (max_digits);
		display_pin = g_strdup_printf ("%s⏎", default_pin);
	} else if (g_strcmp0 (default_pin, "ICADE") == 0) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_ICADE;
		confirm_pin = FALSE;
		g_free (default_pin);
		default_pin = get_icade_pincode (&display_pin);
	} else if (default_pin == NULL) {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL;
		confirm_pin = TRUE;
		default_pin = get_random_pincode (0);
	} else if (g_strcmp0 (default_pin, "NULL") == 0) {
		g_assert_not_reached ();
	} else {
		mode = BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_NORMAL;
		display_pin = g_strdup (default_pin);
	}

	setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));

	g_object_set_data_full (G_OBJECT (priv->pairing_dialog), "name", g_strdup (name), g_free);
	g_object_set_data (G_OBJECT (priv->pairing_dialog), "mode", GUINT_TO_POINTER (mode));

	if (confirm_pin) {
		g_object_set_data (G_OBJECT (priv->pairing_dialog), "invocation", invocation);
		if (remote_initiated) {
			bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
							   BLUETOOTH_PAIRING_MODE_PIN_QUERY,
							   default_pin,
							   name);
		} else {
			bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
							   BLUETOOTH_PAIRING_MODE_PIN_CONFIRMATION,
							   default_pin,
							   name);
		}
		g_signal_connect (G_OBJECT (priv->pairing_dialog), "response",
				  G_CALLBACK (enter_pin_cb), user_data);
	} else if (!remote_initiated) {
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
						   mode, display_pin, name);
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", default_pin));
		g_signal_connect (G_OBJECT (priv->pairing_dialog), "response",
				  G_CALLBACK (display_cb), user_data);
	} else {
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
						   BLUETOOTH_PAIRING_MODE_YES_NO,
						   default_pin, name);

		g_object_set_data_full (G_OBJECT (invocation), "pin", g_strdup (default_pin), g_free);
		g_object_set_data_full (G_OBJECT (invocation), "device", g_object_ref (device), g_object_unref);
		g_object_set_data (G_OBJECT (priv->pairing_dialog), "invocation", invocation);

		g_signal_connect (G_OBJECT (priv->pairing_dialog), "response",
				  G_CALLBACK (confirm_remote_pin_cb), user_data);
	}

	g_free (name);
	g_free (bdaddr);
	g_free (default_pin);
	g_free (display_pin);

	gtk_widget_show (priv->pairing_dialog);
}

static void
display_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy            *device,
		  guint                  pin,
		  guint                  entered,
		  gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	char *pin_str, *name;

	g_debug ("display_callback (%s, %i, %i)", g_dbus_proxy_get_object_path (device), pin, entered);

	if (priv->pairing_dialog == NULL ||
	    bluetooth_pairing_dialog_get_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog)) != BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD)
		setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));

	pin_str = g_strdup_printf ("%06d", pin);
	get_properties_for_device (self, device, &name, NULL, NULL);
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
					   BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD,
					   pin_str,
					   name);
	bluetooth_pairing_dialog_set_pin_entered (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
						  entered);
	g_free (pin_str);
	g_free (name);

	gtk_widget_show (priv->pairing_dialog);
}

static void
display_pincode_callback (GDBusMethodInvocation *invocation,
			  GDBusProxy            *device,
			  const char            *pincode,
			  gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	BluetoothType type;
	char *display_pin = NULL;
	char *name, *bdaddr;
	char *db_pin;

	g_debug ("display_pincode_callback (%s, %s)", g_dbus_proxy_get_object_path (device), pincode);

	if (!get_properties_for_device (self, device, &name, &bdaddr, &type)) {
		char *msg;

		msg = g_strdup_printf ("Missing information for %s", g_dbus_proxy_get_object_path (device));
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		g_free (msg);
		return;
	}

	/* Verify PIN code validity */
	db_pin = get_pincode_for_device (type,
					 bdaddr,
					 name,
					 NULL,
					 NULL);
	if (g_strcmp0 (db_pin, "KEYBOARD") == 0) {
		/* Should work, follow through */
	} else if (g_strcmp0 (db_pin, "ICADE") == 0) {
		char *msg;

		msg = g_strdup_printf ("Generated pincode for %s when it shouldn't have", name);
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		g_free (msg);
		goto bail;
	} else if (g_strcmp0 (db_pin, "0000") == 0) {
		g_debug ("Ignoring generated keyboard PIN '%s', should get 0000 soon", pincode);
		g_dbus_method_invocation_return_value (invocation, NULL);
		goto bail;
	} else if (g_strcmp0 (db_pin, "NULL") == 0) {
		char *msg;

		msg = g_strdup_printf ("Attempting pairing for %s that doesn't support pairing", name);
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		g_free (msg);
		goto bail;
	}

	setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));

	display_pin = g_strdup_printf ("%s⏎", pincode);
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
					   BLUETOOTH_PAIRING_MODE_PIN_DISPLAY_KEYBOARD,
					   display_pin,
					   name);
	gtk_widget_show (priv->pairing_dialog);

	g_dbus_method_invocation_return_value (invocation, NULL);

bail:
	g_free (db_pin);
	g_free (display_pin);
	g_free (bdaddr);
	g_free (name);
}

static void
passkey_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy            *device,
		  gpointer               data)
{
	g_warning ("RequestPasskey(): not implemented");
	g_dbus_method_invocation_return_dbus_error (invocation,
						    "org.bluez.Error.Rejected",
						    "RequestPasskey not implemented");
}

static gboolean
cancel_callback (GDBusMethodInvocation *invocation,
		 gpointer               user_data)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	GList *l, *children;

	g_debug ("cancel_callback ()");

	g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);

	children = gtk_container_get_children (GTK_CONTAINER (priv->device_list));
	for (l = children; l != NULL; l = l->next)
		g_object_set (l->data, "pairing", FALSE, NULL);
	g_list_free (children);

	g_dbus_method_invocation_return_value (invocation, NULL);

	return TRUE;
}

static void
confirm_cb (GtkDialog *dialog,
	    int        response,
	    gpointer   user_data)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (dialog), "invocation");
	if (response == GTK_RESPONSE_ACCEPT) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		g_dbus_method_invocation_return_dbus_error (invocation,
							    "org.bluez.Error.Canceled",
							    "User cancelled pairing");
	}
	g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);
}

static void
confirm_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy            *device,
		  guint                  pin,
		  gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	char *name, *pin_str;

	g_debug ("confirm_callback (%s, %i)", g_dbus_proxy_get_object_path (device), pin);

	setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));

	pin_str = g_strdup_printf ("%06d", pin);
	get_properties_for_device (self, device, &name, NULL, NULL);
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
					   BLUETOOTH_PAIRING_MODE_PIN_MATCH,
					   pin_str, name);

	g_signal_connect (G_OBJECT (priv->pairing_dialog), "response",
			  G_CALLBACK (confirm_cb), user_data);
	g_object_set_data (G_OBJECT (priv->pairing_dialog), "invocation", invocation);

	gtk_widget_show (priv->pairing_dialog);

	g_free (pin_str);
	g_free (name);
}

static void
authorize_callback (GDBusMethodInvocation *invocation,
		    GDBusProxy            *device,
		    gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	char *name;

	g_debug ("authorize_callback (%s)", g_dbus_proxy_get_object_path (device));

	setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));
	get_properties_for_device (self, device, &name, NULL, NULL);
	bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
					   BLUETOOTH_PAIRING_MODE_YES_NO,
					   NULL, name);

	g_signal_connect (G_OBJECT (priv->pairing_dialog), "response",
			  G_CALLBACK (confirm_cb), user_data);
	g_object_set_data (G_OBJECT (priv->pairing_dialog), "invocation", invocation);

	gtk_widget_show (priv->pairing_dialog);

	g_free (name);
}

static void
authorize_service_cb (GtkDialog *dialog,
		      int        response,
		      gpointer   user_data)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (dialog), "invocation");

	if (response == GTK_RESPONSE_ACCEPT) {
		GDBusProxy *device;

		device = g_object_get_data (G_OBJECT (invocation), "device");
		bluetooth_client_set_trusted (priv->client, g_dbus_proxy_get_object_path (device), TRUE);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		char *msg;

		msg = g_strdup_printf ("Rejecting service auth (HID): not paired or trusted");
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		g_free (msg);
	}
	g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);
}

static void
authorize_service_callback (GDBusMethodInvocation *invocation,
			    GDBusProxy            *device,
			    const char            *uuid,
			    gpointer               user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	char *msg;
	GVariant *value;
	gboolean paired, trusted;

	g_debug ("authorize_service_callback (%s, %s)", g_dbus_proxy_get_object_path (device), uuid);

	value = g_dbus_proxy_get_cached_property (device, "Paired");
	paired = g_variant_get_boolean (value);
	g_variant_unref (value);

	value = g_dbus_proxy_get_cached_property (device, "Trusted");
	trusted = g_variant_get_boolean (value);
	g_variant_unref (value);

	/* Device was paired, initiated from the remote device,
	 * so we didn't get the opportunity to set the trusted bit */
	if (paired && !trusted) {
		bluetooth_client_set_trusted (priv->client,
					      g_dbus_proxy_get_object_path (device), TRUE);
		g_dbus_method_invocation_return_value (invocation, NULL);
		return;
	}

	if (g_strcmp0 (bluetooth_uuid_to_string (uuid), "HumanInterfaceDeviceService") != 0) {
		msg = g_strdup_printf ("Rejecting service auth (%s) for %s: not HID",
				       uuid, g_dbus_proxy_get_object_path (device));
		g_dbus_method_invocation_return_dbus_error (invocation, "org.bluez.Error.Rejected", msg);
		g_free (msg);
		return;
	}

	/* We shouldn't get asked, but shizzle happens */
	if (paired || trusted) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		char *name;

		setup_pairing_dialog (BLUETOOTH_SETTINGS_WIDGET (user_data));
		get_properties_for_device (self, device, &name, NULL, NULL);
		bluetooth_pairing_dialog_set_mode (BLUETOOTH_PAIRING_DIALOG (priv->pairing_dialog),
						   BLUETOOTH_PAIRING_MODE_CONFIRM_AUTH,
						   NULL, name);

		g_signal_connect (G_OBJECT (priv->pairing_dialog), "response",
				  G_CALLBACK (authorize_service_cb), user_data);
		g_object_set_data_full (G_OBJECT (invocation), "device", g_object_ref (device), g_object_unref);
		g_object_set_data (G_OBJECT (priv->pairing_dialog), "invocation", invocation);

		gtk_widget_show (priv->pairing_dialog);

		g_free (name);
	}
}

static void
turn_off_pairing (BluetoothSettingsWidget *self,
		  const char              *object_path)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GList *l, *children;

	children = gtk_container_get_children (GTK_CONTAINER (priv->device_list));
	for (l = children; l != NULL; l = l->next) {
		GDBusProxy *proxy;

		g_object_get (l->data, "proxy", &proxy, NULL);
		if (g_strcmp0 (g_dbus_proxy_get_object_path (proxy), object_path) == 0) {
			g_object_set (l->data, "pairing", FALSE, NULL);
			g_object_unref (proxy);
			break;
		}
		g_object_unref (proxy);
	}
	g_list_free (children);
}

typedef struct {
	BluetoothSettingsWidget *self;
	char *device;
	GTimer *timer;
	guint timeout_id;
} SetupConnectData;

static void connect_callback (GObject      *source_object,
			      GAsyncResult *res,
			      gpointer      user_data);

static gboolean
connect_timeout_cb (gpointer user_data)
{
	SetupConnectData *data = (SetupConnectData *) user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (data->self);

	bluetooth_client_connect_service (priv->client, data->device, TRUE, NULL, connect_callback, data);
	data->timeout_id = 0;

	return G_SOURCE_REMOVE;
}

static void
connect_callback (GObject      *source_object,
		  GAsyncResult *res,
		  gpointer      user_data)
{
	SetupConnectData *data = (SetupConnectData *) user_data;
	GError *error = NULL;
	gboolean success;

	success = bluetooth_client_connect_service_finish (BLUETOOTH_CLIENT (source_object), res, &error);

	if (success == FALSE) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_error_free (error);
			goto bail;
		} else if (g_timer_elapsed (data->timer, NULL) < CONNECT_TIMEOUT) {
			g_assert (data->timeout_id == 0);
			data->timeout_id = g_timeout_add (500, connect_timeout_cb, data);
			g_error_free (error);
			return;
		}
		g_debug ("Failed to connect to device %s", data->device);
	}

	turn_off_pairing (data->self, data->device);

bail:
	if (data->timeout_id > 0)
		g_source_remove (data->timeout_id);

	g_timer_destroy (data->timer);
	g_free (data);
}

static void
create_callback (GObject      *source_object,
		 GAsyncResult *res,
		 gpointer      user_data)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	SetupConnectData *data;
	GError *error = NULL;
	gboolean ret;
	char *path;

	ret = bluetooth_client_setup_device_finish (BLUETOOTH_CLIENT (source_object),
						    res, &path, &error);

	/* Create failed */
	if (ret == FALSE) {
		//char *text;
		char *dbus_error;

		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_error_free (error);
			g_free (path);
			return;
		}

		turn_off_pairing (user_data, path);

		dbus_error = g_dbus_error_get_remote_error (error);
		if (g_strcmp0 (dbus_error, "org.bluez.Error.AuthenticationCanceled") != 0) {
			//FIXME show an error?
			/* translators:
			 * The “%s” is the device name, for example:
			 * Setting up “Sony Bluetooth Headset” failed
			 */
			//text = g_strdup_printf(_("Setting up “%s” failed"), target_name);

			g_warning ("Setting up %s failed: %s", path, error->message);

			//gtk_label_set_markup(GTK_LABEL(label_summary), text);
			//g_free (text);
		}

		g_free (dbus_error);
		g_error_free (error);
		g_free (path);
		return;
	}

	priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);

	g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);

	g_hash_table_remove (priv->pairing_devices, path);

	bluetooth_client_set_trusted (BLUETOOTH_CLIENT (source_object), path, TRUE);

	data = g_new0 (SetupConnectData, 1);
	data->self = user_data;
	data->device = path;
	data->timer = g_timer_new ();

	bluetooth_client_connect_service (BLUETOOTH_CLIENT (source_object),
					  path, TRUE, priv->cancellable, connect_callback, data);
	//gtk_assistant_set_current_page (window_assistant, PAGE_FINISHING);
}

static void start_pairing (BluetoothSettingsWidget *self,
			   GtkListBoxRow           *row);

static void
device_name_appeared (GObject    *gobject,
		      GParamSpec *pspec,
		      gpointer    user_data)
{
	char *name;

	g_object_get (G_OBJECT (gobject),
		      "name", &name,
		      NULL);
	if (!name)
		return;

	g_debug ("Pairing device name is now '%s'", name);
	start_pairing (user_data, GTK_LIST_BOX_ROW (gobject));
	g_free (name);

	g_signal_handlers_disconnect_by_func (gobject, device_name_appeared, user_data);
}

static void
start_pairing (BluetoothSettingsWidget *self,
	       GtkListBoxRow           *row)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GDBusProxy *proxy;
	gboolean pair = TRUE;
	BluetoothType type;
	char *bdaddr, *name;
	gboolean legacy_pairing;
	const char *pincode;

	g_object_set (G_OBJECT (row), "pairing", TRUE, NULL);
	g_object_get (G_OBJECT (row),
		      "proxy", &proxy,
		      "type", &type,
		      "address", &bdaddr,
		      "name", &name,
		      "legacy-pairing", &legacy_pairing,
		      NULL);

	if (name == NULL) {
		g_debug ("No name yet, will start pairing later");
		g_signal_connect (G_OBJECT (row), "notify::name",
				  G_CALLBACK (device_name_appeared), self);
		g_object_unref (proxy);
		g_free (bdaddr);
		g_free (name);
		return;
	}

	g_debug ("Starting pairing for '%s'", name);

	/* Legacy pairing might not have been detected yet,
	 * so don't check for it */
	pincode = get_pincode_for_device (type,
					  bdaddr,
					  name,
					  NULL,
					  NULL);
	if (g_strcmp0 (pincode, "NULL") == 0)
		pair = FALSE;

	g_debug ("About to setup %s (legacy pairing: %d pair: %d)",
		 g_dbus_proxy_get_object_path (proxy),
		 legacy_pairing, pair);

	g_hash_table_insert (priv->pairing_devices,
			     g_strdup (g_dbus_proxy_get_object_path (proxy)),
			     GINT_TO_POINTER (1));

	bluetooth_client_setup_device (priv->client,
				       g_dbus_proxy_get_object_path (proxy),
				       pair,
				       priv->cancellable,
				       (GAsyncReadyCallback) create_callback,
				       self);
	g_object_unref (proxy);
}

static void
switch_connected_active_changed (GtkSwitch               *button,
				 GParamSpec              *spec,
				 BluetoothSettingsWidget *self)
{
	ConnectData *data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);

	if (is_connecting (self, priv->selected_bdaddr))
		return;

	data = g_new0 (ConnectData, 1);
	data->bdaddr = g_strdup (priv->selected_bdaddr);
	data->self = self;

	bluetooth_client_connect_service (priv->client,
					  priv->selected_object_path,
					  gtk_switch_get_active (button),
					  priv->cancellable,
					  connect_done,
					  data);

	add_connecting (self, data->bdaddr);
	set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SPINNER);
}

static void
update_properties (BluetoothSettingsWidget *self,
		   GDBusProxy              *proxy)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GtkSwitch *button;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean ret;
	BluetoothType type;
	gboolean connected, paired;
	char **uuids, *bdaddr, *name, *icon;
	guint i;

	model = bluetooth_client_get_device_model (priv->client);
	g_assert (model);

	ret = gtk_tree_model_get_iter_first (model, &iter);
	while (ret) {
		GDBusProxy *p;

		gtk_tree_model_get (model, &iter,
				    BLUETOOTH_COLUMN_PROXY, &p,
				    -1);

		if (g_strcmp0 (g_dbus_proxy_get_object_path (proxy),
			       g_dbus_proxy_get_object_path (p)) == 0) {
			g_object_unref (p);
			break;
		}

		g_object_unref (p);

		ret = gtk_tree_model_iter_next (model, &iter);
	}

	/* This means we've found the device */
	g_assert (ret);

	gtk_tree_model_get (model, &iter,
			    BLUETOOTH_COLUMN_ADDRESS, &bdaddr,
			    BLUETOOTH_COLUMN_NAME, &name,
			    BLUETOOTH_COLUMN_ICON, &icon,
			    BLUETOOTH_COLUMN_PAIRED, &paired,
			    BLUETOOTH_COLUMN_CONNECTED, &connected,
			    BLUETOOTH_COLUMN_UUIDS, &uuids,
			    BLUETOOTH_COLUMN_TYPE, &type,
			    -1);
	if (priv->debug)
		bluetooth_client_dump_device (model, &iter);
	g_object_unref (model);

	g_free (priv->selected_object_path);
	priv->selected_object_path = g_strdup (g_dbus_proxy_get_object_path (proxy));

	/* Hide all the buttons now, and show them again if we need to */
	gtk_widget_hide (WID ("keyboard_button"));
	gtk_widget_hide (WID ("sound_button"));
	gtk_widget_hide (WID ("mouse_button"));
	gtk_widget_hide (WID ("send_button"));

	/* Name */
	gtk_window_set_title (GTK_WINDOW (priv->properties_dialog), name);
	g_free (priv->selected_name);
	priv->selected_name = name;

	/* Icon */
	gtk_image_set_from_icon_name (GTK_IMAGE (WID ("image")), icon, GTK_ICON_SIZE_DIALOG);

	/* Connection */
	button = GTK_SWITCH (WID ("switch_connection"));
	g_signal_handlers_block_by_func (button, switch_connected_active_changed, self);

	if (is_connecting (self, bdaddr)) {
		gtk_switch_set_active (button, TRUE);
		set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SPINNER);
	} else {
		gtk_switch_set_active (button, connected);
		set_connecting_page (self, CONNECTING_NOTEBOOK_PAGE_SWITCH);
	}

	g_signal_handlers_unblock_by_func (button, switch_connected_active_changed, self);

	/* Paired */
	gtk_label_set_text (GTK_LABEL (WID ("paired_label")),
			    paired ? _("Yes") : _("No"));

	/* UUIDs */
	gtk_widget_set_sensitive (GTK_WIDGET (button),
				  bluetooth_client_get_connectable ((const char **) uuids));
	for (i = 0; uuids && uuids[i] != NULL; i++) {
		if (g_str_equal (uuids[i], "OBEXObjectPush")) {
			gtk_widget_show (WID ("send_button"));
			break;
		}
	}

	/* Type */
	gtk_label_set_text (GTK_LABEL (WID ("type_label")), bluetooth_type_to_string (type));
	switch (type) {
	case BLUETOOTH_TYPE_KEYBOARD:
		gtk_widget_show (WID ("keyboard_button"));
		break;
	case BLUETOOTH_TYPE_MOUSE:
	case BLUETOOTH_TYPE_TABLET:
		gtk_widget_show (WID ("mouse_button"));
		break;
	case BLUETOOTH_TYPE_HEADSET:
	case BLUETOOTH_TYPE_HEADPHONES:
	case BLUETOOTH_TYPE_OTHER_AUDIO:
		gtk_widget_show (WID ("sound_button"));
	default:
		/* others? */
		;
	}

	/* Address */
	gtk_label_set_text (GTK_LABEL (WID ("address_label")), bdaddr);

	g_free (priv->selected_bdaddr);
	priv->selected_bdaddr = bdaddr;

	g_free (icon);
	g_strfreev (uuids);
}

static void
switch_panel (BluetoothSettingsWidget *self,
	      const char       *panel)
{
	g_signal_emit (G_OBJECT (self),
		       signals[PANEL_CHANGED],
		       0, panel);
}

static gboolean
keyboard_callback (GtkButton        *button,
		   BluetoothSettingsWidget *self)
{
	switch_panel (self, KEYBOARD_PREFS);
	return TRUE;
}

static gboolean
mouse_callback (GtkButton        *button,
		BluetoothSettingsWidget *self)
{
	switch_panel (self, MOUSE_PREFS);
	return TRUE;
}

static gboolean
sound_callback (GtkButton        *button,
		BluetoothSettingsWidget *self)
{
	switch_panel (self, SOUND_PREFS);
	return TRUE;
}

static void
send_callback (GtkButton        *button,
	       BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	bluetooth_send_to_address (priv->selected_bdaddr, priv->selected_name);
}

/* Visibility/Discoverable */
static void
update_visibility (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	char *name;

	g_object_get (G_OBJECT (priv->client), "default-adapter-name", &name, NULL);
	if (name != NULL) {
		char *label, *path, *uri;

		path = lookup_download_dir ();
		uri = g_filename_to_uri (path, NULL, NULL);
		g_free (path);

		/* translators: first %s is the name of the computer, for example:
		 * Visible as “Bastien Nocera’s Computer” followed by the
		 * location of the Downloads folder.*/
		label = g_strdup_printf (_("Visible as “%s” and available for Bluetooth file transfers. Transferred files are placed in the <a href=\"%s\">Downloads</a> folder."), name, uri);
		g_free (uri);
		g_free (name);
		gtk_label_set_markup (GTK_LABEL (priv->visible_label), label);
		g_free (label);
	}
	gtk_widget_set_visible (priv->visible_label, name != NULL);
}

static void
name_changed (BluetoothClient  *client,
	      GParamSpec       *spec,
	      BluetoothSettingsWidget *self)
{
	update_visibility (self);
}

static gboolean
show_confirm_dialog (BluetoothSettingsWidget *self,
		     const char *name)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new (GTK_WINDOW (priv->properties_dialog), GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("Remove “%s” from the list of devices?"), name);
	g_object_set (G_OBJECT (dialog), "secondary-text",
		      _("If you remove the device, you will have to set it up again before next use."),
		      NULL);

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Remove"), GTK_RESPONSE_ACCEPT);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_ACCEPT)
		return TRUE;

	return FALSE;
}

static gboolean
remove_selected_device (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GDBusProxy *adapter_proxy;
	GError *error = NULL;
	GVariant *ret;

	g_debug ("About to call RemoveDevice for %s", priv->selected_object_path);

	adapter_proxy = _bluetooth_client_get_default_adapter (priv->client);

	if (adapter_proxy == NULL) {
		g_warning ("Failed to get a GDBusProxy for the default adapter");
		return FALSE;
	}

	//FIXME use adapter1_call_remove_device_sync()
	ret = g_dbus_proxy_call_sync (G_DBUS_PROXY (adapter_proxy),
				      "RemoveDevice",
				      g_variant_new ("(o)", priv->selected_object_path),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      &error);

	if (ret == NULL) {
		g_warning ("Failed to remove device '%s': %s",
			   priv->selected_object_path, error->message);
		g_error_free (error);
	} else {
		g_variant_unref (ret);
	}

	g_object_unref (adapter_proxy);

	return (ret != NULL);
}

static void
delete_clicked (GtkButton        *button,
		BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	if (show_confirm_dialog (self, priv->selected_name) != FALSE) {
		remove_selected_device (self);
		gtk_widget_hide (priv->properties_dialog);
	}
}

static void
default_adapter_changed (BluetoothClient  *client,
			 GParamSpec       *spec,
			 BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	char *default_adapter;

	g_object_get (priv->client, "default-adapter", &default_adapter, NULL);

	g_debug ("Default adapter changed to: %s", default_adapter ? default_adapter : "(none)");

	g_object_set (G_OBJECT (client), "default-adapter-discovering", default_adapter != NULL, NULL);

	/* FIXME: This should turn off automatically when
	 * the settings panel goes away */
	g_object_set (G_OBJECT (client), "default-adapter-discoverable", default_adapter != NULL, NULL);

	g_signal_emit (G_OBJECT (self), signals[ADAPTER_STATUS_CHANGED], 0);
}

static gint
device_sort_func (gconstpointer a, gconstpointer b, gpointer data)
{
	GObject *row_a = (GObject*)a;
	GObject *row_b = (GObject*)b;
	gboolean setup_a, setup_b;
	gboolean paired_a, paired_b;
	gboolean trusted_a, trusted_b;
	gboolean connected_a, connected_b;
	char *name_a, *name_b;
	int ret;

	g_object_get (row_a,
		      "paired", &paired_a,
		      "trusted", &trusted_a,
		      "connected", &connected_a,
		      "name", &name_a,
		      NULL);
	g_object_get (row_b,
		      "paired", &paired_b,
		      "trusted", &trusted_b,
		      "connected", &connected_b,
		      "name", &name_b,
		      NULL);

	/* First, paired or trusted devices (setup devices) */
	setup_a = paired_a || trusted_a;
	setup_b = paired_b || trusted_b;
	if (setup_a != setup_b) {
		if (setup_a)
			ret = -1;
		else
			ret = 1;
		goto out;
	}

	/* Then connected ones */
	if (connected_a != connected_b) {
		if (connected_a)
			ret = -1;
		else
			ret = 1;
		goto out;
	}

	if (name_a == NULL) {
		ret = 1;
		goto out;
	}
	if (name_b == NULL) {
		ret = -1;
		goto out;
	}

	/* And all being equal, alphabetically */
	ret = g_utf8_collate (name_a, name_b);

out:
	g_free (name_a);
	g_free (name_b);
	return ret;
}

static void
update_header_func (GtkListBoxRow  *row,
		    GtkListBoxRow  *before,
		    gpointer    user_data)
{
	GtkWidget *current;

	if (before == NULL)
		return;

	current = gtk_list_box_row_get_header (row);
	if (current == NULL) {
		current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_show (current);
		gtk_list_box_row_set_header (row, current);
	}
}

static gboolean
keynav_failed (GtkWidget *list, GtkDirectionType direction, BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GList *children, *item;

	children = gtk_container_get_children (GTK_CONTAINER (priv->device_list));

	if (direction == GTK_DIR_DOWN) {
		item = children;
	} else {
		item = g_list_last (children);
	}

	gtk_widget_child_focus (item->data, direction);

	g_list_free (children);

	return TRUE;
}

static void
activate_row (BluetoothSettingsWidget *self,
              GtkListBoxRow    *row)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GtkWidget *w;
	GtkWidget *toplevel;
	gboolean paired, trusted, is_setup;

	g_object_get (G_OBJECT (row),
		      "paired", &paired,
		      "trusted", &trusted,
		      NULL);
	is_setup = paired || trusted;

	if (is_setup) {
		GDBusProxy *proxy;

		//FIXME pass the row
		//FIXME add UUIDs to the row
		//FIXME add icon to the row
		g_object_get (G_OBJECT (row), "proxy", &proxy, NULL);
		update_properties (self, proxy);
		g_object_unref (proxy);

		w = priv->properties_dialog;
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (toplevel));
		gtk_window_set_modal (GTK_WINDOW (w), TRUE);
		gtk_window_present (GTK_WINDOW (w));
	} else {
		start_pairing (self, row);
	}
}

static void
add_device_section (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GtkWidget *vbox;
	GtkWidget *widget, *box, *hbox, *spinner;
	GtkWidget *frame, *label;
	gchar *s;

	vbox = WID ("vbox_bluetooth");

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_top (box, 6);
	gtk_widget_set_margin_bottom (box, 24);
	gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);
	priv->child_box = box;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, TRUE, 0);

	s = g_markup_printf_escaped ("<b>%s</b>", _("Devices"));
	widget = gtk_label_new (s);
	g_free (s);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_widget_set_margin_end (widget, 6);
	gtk_widget_set_margin_bottom (widget, 12);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, TRUE, 0);

	/* Discoverable spinner */
	priv->device_spinner = spinner = gtk_spinner_new ();
	g_object_bind_property (G_OBJECT (priv->client), "default-adapter-discovering",
				G_OBJECT (priv->device_spinner), "active",
				G_BINDING_SYNC_CREATE);
	gtk_widget_set_margin_bottom (spinner, 12);
	gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, TRUE, 0);

	/* Discoverable label placeholder, the real name is set in update_visibility().
	 * If you ever see this string during normal use, please file a bug. */
	priv->visible_label = WID ("explanation-label");
	gtk_label_set_use_markup (GTK_LABEL (priv->visible_label), TRUE);
	update_visibility (self);

	priv->device_list = widget = gtk_list_box_new ();
	g_signal_connect (widget, "keynav-failed", G_CALLBACK (keynav_failed), self);
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_NONE);
	gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
				      update_header_func,
				      NULL, NULL);
	gtk_list_box_set_sort_func (GTK_LIST_BOX (widget),
				    (GtkListBoxSortFunc)device_sort_func, NULL, NULL);
	g_signal_connect_swapped (widget, "row-activated",
				  G_CALLBACK (activate_row), self);

	priv->device_stack = gtk_stack_new ();
	gtk_stack_set_homogeneous (GTK_STACK (priv->device_stack), FALSE);

	label = gtk_label_new (_("Searching for devices…"));
	gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
	gtk_stack_add_named (GTK_STACK (priv->device_stack), label, FILLER_PAGE);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (frame), widget);
	gtk_stack_add_named (GTK_STACK (priv->device_stack), frame, DEVICES_PAGE);
	gtk_box_pack_start (GTK_BOX (box), priv->device_stack, TRUE, TRUE, 0);

	gtk_widget_show_all (box);
}

static gboolean
is_interesting_device (GtkTreeModel *model,
		       GtkTreeIter  *iter)
{
	GtkTreeIter parent_iter;
	gboolean is_default;

	/* Not a child */
	if (gtk_tree_model_iter_parent (model, &parent_iter, iter) == FALSE)
		return FALSE;

	/* Not the default adapter */
	gtk_tree_model_get (model, &parent_iter,
			    BLUETOOTH_COLUMN_DEFAULT, &is_default,
			    -1);
	return is_default;
}

static void
row_inserted_cb (GtkTreeModel *tree_model,
		 GtkTreePath  *path,
		 GtkTreeIter  *iter,
		 gpointer      user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	GDBusProxy *proxy;
	char *name, *bdaddr;
	BluetoothType type;
	gboolean paired, trusted, connected, legacy_pairing;
	GtkWidget *row;

	if (is_interesting_device (tree_model, iter) == FALSE) {
		gtk_tree_model_get (tree_model, iter,
				    BLUETOOTH_COLUMN_NAME, &name,
				    -1);
		g_debug ("Not adding device '%s'", name);
		g_free (name);
		return;
	}

	gtk_tree_model_get (tree_model, iter,
			    BLUETOOTH_COLUMN_PROXY, &proxy,
			    BLUETOOTH_COLUMN_NAME, &name,
			    BLUETOOTH_COLUMN_PAIRED, &paired,
			    BLUETOOTH_COLUMN_TRUSTED, &trusted,
			    BLUETOOTH_COLUMN_CONNECTED, &connected,
			    BLUETOOTH_COLUMN_ADDRESS, &bdaddr,
			    BLUETOOTH_COLUMN_TYPE, &type,
			    BLUETOOTH_COLUMN_LEGACYPAIRING, &legacy_pairing,
			    -1);

	g_debug ("Adding device %s (%s)", name, g_dbus_proxy_get_object_path (proxy));

	add_device_type (self, bdaddr, type);

	row = g_object_new (BLUETOOTH_TYPE_SETTINGS_ROW,
			    "proxy", proxy,
			    "paired", paired,
			    "trusted", trusted,
			    "type", type,
			    "connected", connected,
			    "name", name,
			    "address", bdaddr,
			    "legacy-pairing", legacy_pairing,
			    NULL);
	g_object_set_data_full (G_OBJECT (row), "object-path", g_strdup (g_dbus_proxy_get_object_path (proxy)), g_free);

	gtk_container_add (GTK_CONTAINER (priv->device_list), row);
	gtk_size_group_add_widget (priv->row_sizegroup, row);

	g_object_unref (proxy);
	g_free (name);
	g_free (bdaddr);

	gtk_stack_set_transition_type (GTK_STACK (priv->device_stack),
				       GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN);
	gtk_container_child_set (GTK_CONTAINER (WID ("vbox_bluetooth")),
				 priv->child_box, "expand", FALSE, NULL);
	gtk_stack_set_visible_child_name (GTK_STACK (priv->device_stack), DEVICES_PAGE);
}

static void
row_changed_cb (GtkTreeModel *tree_model,
		GtkTreePath  *path,
		GtkTreeIter  *iter,
		gpointer      user_data)
{
	BluetoothSettingsWidget *self = user_data;
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	GDBusProxy *proxy;
	GList *l, *children;
	const char *object_path;

	if (is_interesting_device (tree_model, iter) == FALSE) {
		char *name;

		gtk_tree_model_get (tree_model, iter,
				    BLUETOOTH_COLUMN_NAME, &name,
				    -1);
		g_debug ("Not interested in device '%s'", name);
		g_free (name);
		return;
	}

	gtk_tree_model_get (tree_model, iter,
			    BLUETOOTH_COLUMN_PROXY, &proxy,
			    -1);
	object_path = g_dbus_proxy_get_object_path (proxy);

	children = gtk_container_get_children (GTK_CONTAINER (priv->device_list));
	for (l = children; l != NULL; l = l->next) {
		const char *path;

		path = g_object_get_data (G_OBJECT (l->data), "object-path");
		if (g_str_equal (object_path, path)) {
			char *name, *bdaddr;
			BluetoothType type;
			gboolean paired, trusted, connected, legacy_pairing;

			gtk_tree_model_get (tree_model, iter,
					    BLUETOOTH_COLUMN_NAME, &name,
					    BLUETOOTH_COLUMN_PAIRED, &paired,
					    BLUETOOTH_COLUMN_TRUSTED, &trusted,
					    BLUETOOTH_COLUMN_CONNECTED, &connected,
					    BLUETOOTH_COLUMN_ADDRESS, &bdaddr,
					    BLUETOOTH_COLUMN_TYPE, &type,
					    BLUETOOTH_COLUMN_LEGACYPAIRING, &legacy_pairing,
					    -1);

			add_device_type (self, bdaddr, type);

			g_object_set (G_OBJECT (l->data),
				      "paired", paired,
				      "trusted", trusted,
				      "type", type,
				      "connected", connected,
				      "name", name,
				      "legacy-pairing", legacy_pairing,
				      NULL);

			/* Update the properties if necessary */
			if (g_strcmp0 (priv->selected_object_path, object_path) == 0)
				update_properties (user_data, proxy);
			break;
		}
	}
	g_list_free (children);
	g_object_unref (proxy);
}

static void
device_removed_cb (BluetoothClient *client,
		   const char      *object_path,
		   gpointer         user_data)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (user_data);
	GList *children, *l;
	gboolean found = FALSE;

	children = gtk_container_get_children (GTK_CONTAINER (priv->device_list));
	for (l = children; l != NULL; l = l->next) {
		const char *path;

		path = g_object_get_data (G_OBJECT (l->data), "object-path");
		if (g_str_equal (path, object_path)) {
			char *name;

			g_object_get (G_OBJECT (l->data), "name", &name, NULL);
			g_debug ("Removing device '%s'", name);
			g_free (name);

			gtk_widget_destroy (GTK_WIDGET (l->data));
			found = TRUE;
			break;
		}
	}

	if (found) {
		if (gtk_container_get_children (GTK_CONTAINER (priv->device_list)) == NULL) {
			gtk_stack_set_transition_type (GTK_STACK (priv->device_stack),
						       GTK_STACK_TRANSITION_TYPE_NONE);
			gtk_container_child_set (GTK_CONTAINER (WID ("vbox_bluetooth")),
						 priv->child_box, "expand", TRUE, NULL);
			gtk_stack_set_visible_child_name (GTK_STACK (priv->device_stack), FILLER_PAGE);
		}
	} else {
		g_debug ("Didn't find a row to remove for tree path %s", object_path);
	}
}

static void
setup_properties_dialog (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GtkWidget *container;
	GtkStyleContext *context;

	priv->properties_dialog = g_object_new (GTK_TYPE_DIALOG, "use-header-bar", TRUE, NULL);
	gtk_widget_set_size_request (priv->properties_dialog, 380, -1);
	gtk_window_set_resizable (GTK_WINDOW (priv->properties_dialog), FALSE);
	container = gtk_dialog_get_content_area (GTK_DIALOG (priv->properties_dialog));
	gtk_container_add (GTK_CONTAINER (container), WID ("properties_vbox"));

	g_signal_connect (G_OBJECT (priv->properties_dialog), "delete-event",
			  G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	g_signal_connect (G_OBJECT (WID ("delete_button")), "clicked",
			  G_CALLBACK (delete_clicked), self);
	g_signal_connect (G_OBJECT (WID ("mouse_button")), "clicked",
			  G_CALLBACK (mouse_callback), self);
	g_signal_connect (G_OBJECT (WID ("keyboard_button")), "clicked",
			  G_CALLBACK (keyboard_callback), self);
	g_signal_connect (G_OBJECT (WID ("sound_button")), "clicked",
			  G_CALLBACK (sound_callback), self);
	g_signal_connect (G_OBJECT (WID ("send_button")), "clicked",
			  G_CALLBACK (send_callback), self);
	g_signal_connect (G_OBJECT (WID ("switch_connection")), "notify::active",
			  G_CALLBACK (switch_connected_active_changed), self);

	/* Styling */
	gtk_image_set_pixel_size (GTK_IMAGE (WID ("image")), ICON_SIZE);

	context = gtk_widget_get_style_context (WID ("delete_button"));
	gtk_style_context_add_class (context, "destructive-action");
}

static void
setup_pairing_agent (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);

	priv->agent = bluetooth_agent_new (AGENT_PATH);
	if (bluetooth_agent_register (priv->agent) == FALSE) {
		g_clear_object (&priv->agent);
		return;
	}

	bluetooth_agent_set_pincode_func (priv->agent, pincode_callback, self);
	bluetooth_agent_set_passkey_func (priv->agent, passkey_callback, self);
	bluetooth_agent_set_display_func (priv->agent, display_callback, self);
	bluetooth_agent_set_display_pincode_func (priv->agent, display_pincode_callback, self);
	bluetooth_agent_set_cancel_func (priv->agent, cancel_callback, self);
	bluetooth_agent_set_confirm_func (priv->agent, confirm_callback, self);
	bluetooth_agent_set_authorize_func (priv->agent, authorize_callback, self);
	bluetooth_agent_set_authorize_service_func (priv->agent, authorize_service_callback, self);
}

static void
session_properties_changed_cb (GDBusProxy               *session,
			       GVariant                 *changed,
			       char                    **invalidated,
			       BluetoothSettingsWidget  *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GVariant *v;

	v = g_variant_lookup_value (changed, "SessionIsActive", G_VARIANT_TYPE_BOOLEAN);
	if (v) {
		priv->has_console = g_variant_get_boolean (v);
		g_debug ("Received session is active change: now %s", priv->has_console ? "active" : "inactive");
		g_variant_unref (v);

		if (priv->has_console)
			obex_agent_up ();
		else
			obex_agent_down ();
	}
}

static gboolean
is_session_active (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GVariant *variant;
	gboolean is_session_active = FALSE;

	variant = g_dbus_proxy_get_cached_property (priv->session_proxy,
						    "SessionIsActive");
	if (variant) {
		is_session_active = g_variant_get_boolean (variant);
		g_variant_unref (variant);
	}

	return is_session_active;
}

static void
setup_obex (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GError *error = NULL;

	priv->session_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							     G_DBUS_PROXY_FLAGS_NONE,
							     NULL,
							     GNOME_SESSION_DBUS_NAME,
							     GNOME_SESSION_DBUS_OBJECT,
							     GNOME_SESSION_DBUS_INTERFACE,
							     NULL,
							     &error);

	if (priv->session_proxy == NULL) {
		g_warning ("Failed to get session proxy: %s", error->message);
		g_error_free (error);
		return;
	}
	g_signal_connect (priv->session_proxy, "g-properties-changed",
			  G_CALLBACK (session_properties_changed_cb), self);
	priv->has_console = is_session_active (self);

	if (priv->has_console)
		obex_agent_up ();
}

static void
bluetooth_settings_widget_init (BluetoothSettingsWidget *self)
{
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (self);
	GtkWidget *widget;
	GError *error = NULL;

	priv->cancellable = g_cancellable_new ();
	priv->debug = g_getenv ("BLUETOOTH_DEBUG") != NULL;

	g_resources_register (bluetooth_settings_get_resource ());
	priv->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (priv->builder,
                                       "/org/gnome/bluetooth/settings.ui",
                                       &error);
	if (error != NULL) {
		g_warning ("Could not load ui: %s", error->message);
		g_error_free (error);
		return;
	}

	widget = WID ("scrolledwindow1");

	priv->connecting_devices = g_hash_table_new_full (g_str_hash,
								g_str_equal,
								(GDestroyNotify) g_free,
								NULL);
	priv->pairing_devices = g_hash_table_new_full (g_str_hash,
						       g_str_equal,
						       (GDestroyNotify) g_free,
						       NULL);
	priv->devices_type = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    (GDestroyNotify) g_free,
						    NULL);

	setup_pairing_agent (self);
	priv->client = bluetooth_client_new ();
	g_signal_connect (G_OBJECT (priv->client), "notify::default-adapter-name",
			  G_CALLBACK (name_changed), self);
	priv->model = bluetooth_client_get_model (priv->client);
	g_signal_connect (priv->model, "row-changed",
			  G_CALLBACK (row_changed_cb), self);
	g_signal_connect (priv->model, "row-inserted",
			  G_CALLBACK (row_inserted_cb), self);
	g_signal_connect (priv->client, "device-removed",
			  G_CALLBACK (device_removed_cb), self);
	g_signal_connect (G_OBJECT (priv->client), "notify::default-adapter",
			  G_CALLBACK (default_adapter_changed), self);
	g_signal_connect (G_OBJECT (priv->client), "notify::default-adapter-powered",
			  G_CALLBACK (default_adapter_changed), self);
	default_adapter_changed (priv->client, NULL, self);

	priv->row_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	add_device_section (self);

	gtk_widget_set_hexpand (widget, TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (self), widget);

	setup_properties_dialog (self);

	gtk_widget_show_all (GTK_WIDGET (self));

	setup_obex (self);
}

static void
bluetooth_settings_widget_finalize (GObject *object)
{
	BluetoothSettingsWidget *widget = BLUETOOTH_SETTINGS_WIDGET(object);
	BluetoothSettingsWidgetPrivate *priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (widget);

	g_clear_object (&priv->agent);
	g_clear_pointer (&priv->properties_dialog, gtk_widget_destroy);
	g_clear_pointer (&priv->pairing_dialog, gtk_widget_destroy);
	g_clear_object (&priv->session_proxy);

	obex_agent_down ();

	/* See default_adapter_changed () */
	/* FIXME: This is blocking */
	if (priv->client)
		g_object_set (G_OBJECT (priv->client), "default-adapter-discoverable", FALSE, NULL);

	g_cancellable_cancel (priv->cancellable);
	g_clear_object (&priv->cancellable);

	g_clear_object (&priv->model);
	g_clear_object (&priv->client);
	g_clear_object (&priv->builder);

	g_clear_pointer (&priv->devices_type, g_hash_table_destroy);
	g_clear_pointer (&priv->connecting_devices, g_hash_table_destroy);
	g_clear_pointer (&priv->pairing_devices, g_hash_table_destroy);
	g_clear_pointer (&priv->selected_name, g_free);
	g_clear_pointer (&priv->selected_object_path, g_free);

	G_OBJECT_CLASS(bluetooth_settings_widget_parent_class)->finalize(object);
}

static void
bluetooth_settings_widget_class_init (BluetoothSettingsWidgetClass *klass)
{
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);

	G_OBJECT_CLASS (klass)->finalize = bluetooth_settings_widget_finalize;

	/**
	 * BluetoothSettingsWidget::panel-changed:
	 * @chooser: a #BluetoothSettingsWidget widget which received the signal
	 * @panel: the new panel that the Settings application should now open
	 *
	 * The #BluetoothChooser::panel-changed signal is launched when a
	 * link to another settings panel is clicked.
	 **/
	signals[PANEL_CHANGED] =
		g_signal_new ("panel-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * BluetoothSettingsWidget::adapter-status-changed:
	 * @chooser: a #BluetoothSettingsWidget widget which received the signal
	 *
	 * The #BluetoothChooser::adapter-status-changed signal is launched when the status
	 * of the adapter changes (powered, available, etc.).
	 **/
	signals[ADAPTER_STATUS_CHANGED] =
		g_signal_new ("adapter-status-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0, G_TYPE_NONE);
}

/**
 * bluetooth_settings_widget_new:
 *
 * Returns a new #BluetoothSettingsWidget widget.
 *
 * Return value: A #BluetoothSettingsWidget widget
 **/
GtkWidget *
bluetooth_settings_widget_new (void)
{
	return g_object_new (BLUETOOTH_TYPE_SETTINGS_WIDGET, NULL);
}

/**
 * bluetooth_settings_widget_get_default_adapter_powered:
 * @widget: a #BluetoothSettingsWidget widget.
 *
 * Return value: Whether the default Bluetooth adapter is powered.
 **/
gboolean
bluetooth_settings_widget_get_default_adapter_powered (BluetoothSettingsWidget *widget)
{
	BluetoothSettingsWidgetPrivate *priv;
	gboolean ret;

	g_return_val_if_fail (BLUETOOTH_IS_SETTINGS_WIDGET (widget), FALSE);

	priv = BLUETOOTH_SETTINGS_WIDGET_GET_PRIVATE (widget);
	g_object_get (G_OBJECT (priv->client),
		      "default-adapter-powered", &ret,
		      NULL);

	return ret;
}
