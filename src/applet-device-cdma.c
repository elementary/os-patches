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
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <nm-device.h>
#include <nm-setting-connection.h>
#include <nm-setting-cdma.h>
#include <nm-setting-serial.h>
#include <nm-setting-ppp.h>
#include <nm-device-modem.h>
#include <nm-utils.h>
#include <nm-secret-agent.h>

#include "applet.h"
#include "applet-device-cdma.h"
#include "utils.h"
#include "applet-dialogs.h"
#include "nma-marshal.h"
#include "nm-mobile-providers.h"
#include "mb-menu-item.h"
#include "nm-ui-utils.h"
#include "nm-glib-compat.h"

typedef struct {
	NMApplet *applet;
	NMDevice *device;

	DBusGConnection *bus;
	DBusGProxy *props_proxy;
	DBusGProxy *cdma_proxy;
	gboolean quality_valid;
	guint32 quality;
	guint32 cdma1x_state;
	guint32 evdo_state;
	gboolean evdo_capable;
	guint32 sid;
	gboolean modem_enabled;

	NMAMobileProvidersDatabase *mobile_providers_database;
	char *provider_name;

	guint32 poll_id;
	gboolean skip_reg_poll;
	gboolean skip_signal_poll;
} CdmaDeviceInfo;

static void check_start_polling (CdmaDeviceInfo *info);

typedef struct {
	NMApplet *applet;
	NMDevice *device;
	NMConnection *connection;
} CdmaMenuItemInfo;

static void
cdma_menu_item_info_destroy (gpointer data)
{
	CdmaMenuItemInfo *info = data;

	g_object_unref (G_OBJECT (info->device));
	if (info->connection)
		g_object_unref (info->connection);

	g_slice_free (CdmaMenuItemInfo, data);
}

static gboolean
cdma_new_auto_connection (NMDevice *device,
                          gpointer dclass_data,
                          AppletNewAutoConnectionCallback callback,
                          gpointer callback_data)
{
	return mobile_helper_wizard (NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO,
	                             callback,
	                             callback_data);
}

static void
dbus_3g_add_and_activate_cb (NMClient *client,
                             NMActiveConnection *active,
                             const char *connection_path,
                             GError *error,
                             gpointer user_data)
{
	if (error)
		g_warning ("Failed to add/activate connection: (%d) %s", error->code, error->message);
}

typedef struct {
	NMApplet *applet;
	NMDevice *device;
} Dbus3gInfo;

static void
dbus_connect_3g_cb (NMConnection *connection,
                    gboolean auto_created,
                    gboolean canceled,
                    gpointer user_data)
{
	Dbus3gInfo *info = user_data;

	if (canceled == FALSE) {
		g_return_if_fail (connection != NULL);

		/* Ask NM to add the new connection and activate it; NM will fill in the
		 * missing details based on the specific object and the device.
		 */
		nm_client_add_and_activate_connection (info->applet->nm_client,
		                                       connection,
		                                       info->device,
		                                       "/",
		                                       dbus_3g_add_and_activate_cb,
		                                       info->applet);
	}

	g_object_unref (info->device);
	memset (info, 0, sizeof (*info));
	g_free (info);
}

void
applet_cdma_connect_network (NMApplet *applet, NMDevice *device)
{
	Dbus3gInfo *info;

	info = g_malloc0 (sizeof (*info));
	info->applet = applet;
	info->device = g_object_ref (device);

	if (!mobile_helper_wizard (NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO,
	                           dbus_connect_3g_cb,
	                           info)) {
		g_warning ("Couldn't run mobile wizard for CDMA device");
		g_object_unref (info->device);
		g_free (info);
	}
}

static void
cdma_menu_item_activate (GtkMenuItem *item, gpointer user_data)
{
	CdmaMenuItemInfo *info = (CdmaMenuItemInfo *) user_data;

	applet_menu_item_activate_helper (info->device,
	                                  info->connection,
	                                  "/",
	                                  info->applet,
	                                  user_data);
}

static void
add_connection_item (NMDevice *device,
                     NMConnection *connection,
                     GtkWidget *item,
                     GtkWidget *menu,
                     NMApplet *applet)
{
	CdmaMenuItemInfo *info;

	info = g_slice_new0 (CdmaMenuItemInfo);
	info->applet = applet;
	info->device = g_object_ref (G_OBJECT (device));
	info->connection = connection ? g_object_ref (connection) : NULL;

	g_signal_connect_data (item, "activate",
	                       G_CALLBACK (cdma_menu_item_activate),
	                       info,
	                       (GClosureNotify) cdma_menu_item_info_destroy, 0);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static guint32
cdma_state_to_mb_state (CdmaDeviceInfo *info)
{
	if (!info->modem_enabled)
		return MB_STATE_UNKNOWN;

	/* EVDO state overrides 1X state for now */
	if (info->evdo_state) {
		if (info->evdo_state == 3)
			return MB_STATE_ROAMING;
		return MB_STATE_HOME;
	} else if (info->cdma1x_state) {
		if (info->cdma1x_state == 3)
			return MB_STATE_ROAMING;
		return MB_STATE_HOME;
	}

	return MB_STATE_UNKNOWN;
}

static guint32
cdma_act_to_mb_act (CdmaDeviceInfo *info)
{
	if (info->evdo_state)
		return MB_TECH_EVDO;
	else if (info->cdma1x_state)
		return MB_TECH_1XRTT;
	return MB_TECH_UNKNOWN;
}

static void
cdma_add_menu_item (NMDevice *device,
                    gboolean multiple_devices,
                    GSList *connections,
                    NMConnection *active,
                    GtkWidget *menu,
                    NMApplet *applet)
{
	CdmaDeviceInfo *info;
	char *text;
	GtkWidget *item;
	GSList *iter;

	info = g_object_get_data (G_OBJECT (device), "devinfo");

	if (multiple_devices) {
		const char *desc;

		desc = nma_utils_get_device_description (device);
		text = g_strdup_printf (_("Mobile Broadband (%s)"), desc);
	} else {
		text = g_strdup (_("Mobile Broadband"));
	}

	item = applet_menu_item_create_device_item_helper (device, applet, text);
	gtk_widget_set_sensitive (item, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	g_free (text);

	/* Add the active connection */
	if (active) {
		NMSettingConnection *s_con;

		s_con = nm_connection_get_setting_connection (active);
		g_assert (s_con);

		item = nm_mb_menu_item_new (nm_setting_connection_get_id (s_con),
		                            info->quality_valid ? info->quality : 0,
		                            info->provider_name,
		                            TRUE,
		                            cdma_act_to_mb_act (info),
		                            cdma_state_to_mb_state (info),
		                            info->modem_enabled,
		                            applet);
		gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
		add_connection_item (device, active, item, menu, applet);
	}

	/* Get the "disconnect" item if connected */
	if (nm_device_get_state (device) > NM_DEVICE_STATE_DISCONNECTED) {
		item = nma_menu_device_get_menu_item (device, applet, NULL);
		if (item) {
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			gtk_widget_show (item);
		}
	} else {
		/* Otherwise show idle registration state or disabled */
		item = nm_mb_menu_item_new (NULL,
		                            info->quality_valid ? info->quality : 0,
		                            info->provider_name,
		                            FALSE,
		                            cdma_act_to_mb_act (info),
		                            cdma_state_to_mb_state (info),
		                            info->modem_enabled,
		                            applet);
		gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	/* Add the default / inactive connection items */
	if (!nma_menu_device_check_unusable (device)) {
		if ((!active && g_slist_length (connections)) || (active && g_slist_length (connections) > 1))
			applet_menu_item_add_complex_separator_helper (menu, applet, _("Available"));

		if (g_slist_length (connections)) {
			for (iter = connections; iter; iter = g_slist_next (iter)) {
				NMConnection *connection = NM_CONNECTION (iter->data);

				if (connection != active) {
					item = applet_new_menu_item_helper (connection, NULL, FALSE);
					add_connection_item (device, connection, item, menu, applet);
				}
			}
		} else {
			/* Default connection item */
			item = gtk_check_menu_item_new_with_label (_("New Mobile Broadband (CDMA) connection..."));
			add_connection_item (device, NULL, item, menu, applet);
		}
	}
}

static void
cdma_device_state_changed (NMDevice *device,
                           NMDeviceState new_state,
                           NMDeviceState old_state,
                           NMDeviceStateReason reason,
                           NMApplet *applet)
{
	CdmaDeviceInfo *info;

	/* Start/stop polling of quality and registration when device state changes */
	info = g_object_get_data (G_OBJECT (device), "devinfo");
	check_start_polling (info);
}

static void
cdma_notify_connected (NMDevice *device,
                       const char *msg,
                       NMApplet *applet)
{
	applet_do_notify_with_pref (applet,
	                            _("Connection Established"),
	                            msg ? msg : _("You are now connected to the CDMA network."),
	                            "nm-device-wwan",
	                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
}

static void
cdma_get_icon (NMDevice *device,
               NMDeviceState state,
               NMConnection *connection,
               GdkPixbuf **out_pixbuf,
               const char **out_icon_name,
               char **tip,
               NMApplet *applet)
{
	CdmaDeviceInfo *info;

	info = g_object_get_data (G_OBJECT (device), "devinfo");
	g_assert (info);

	mobile_helper_get_icon (device,
	                        state,
	                        connection,
	                        out_pixbuf,
	                        out_icon_name,
	                        tip,
	                        applet,
	                        cdma_state_to_mb_state (info),
	                        cdma_act_to_mb_act (info),
	                        info->quality,
	                        info->quality_valid);
}

static gboolean
cdma_get_secrets (SecretsRequest *req, GError **error)
{
	return mobile_helper_get_secrets (NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO, req, error);
}

static void
cdma_device_info_free (gpointer data)
{
	CdmaDeviceInfo *info = data;

	if (info->props_proxy)
		g_object_unref (info->props_proxy);
	if (info->cdma_proxy)
		g_object_unref (info->cdma_proxy);
	if (info->bus)
		dbus_g_connection_unref (info->bus);
	if (info->poll_id)
		g_source_remove (info->poll_id);
	if (info->mobile_providers_database)
		g_object_unref (info->mobile_providers_database);
	g_free (info->provider_name);
	memset (info, 0, sizeof (CdmaDeviceInfo));
	g_free (info);
}

static void
notify_user_of_cdma_reg_change (CdmaDeviceInfo *info)
{
	guint32 mb_state = cdma_state_to_mb_state (info);

	if (mb_state == MB_STATE_HOME) {
		applet_do_notify_with_pref (info->applet,
		                            _("CDMA network."),
		                            _("You are now registered on the home network."),
		                            "nm-signal-100",
		                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
	} else if (mb_state == MB_STATE_ROAMING) {
		applet_do_notify_with_pref (info->applet,
		                            _("CDMA network."),
		                            _("You are now registered on a roaming network."),
		                            "nm-signal-100",
		                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
	}
}

static void
update_registration_state (CdmaDeviceInfo *info,
                           guint32 new_cdma1x_state,
                           guint32 new_evdo_state)
{
	guint32 old_mb_state = cdma_state_to_mb_state (info);

	if (   (info->cdma1x_state != new_cdma1x_state)
	    || (info->evdo_state != new_evdo_state)) {
		info->cdma1x_state = new_cdma1x_state;
		info->evdo_state = new_evdo_state;
	}

	/* Use the composite state to notify of home/roaming changes */
	if (cdma_state_to_mb_state (info) != old_mb_state)
		notify_user_of_cdma_reg_change (info);
}

static void
reg_state_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	CdmaDeviceInfo *info = user_data;
	GError *error = NULL;
	guint32 cdma1x_state = 0, evdo_state = 0;

	if (dbus_g_proxy_end_call (proxy, call, &error,
	                           G_TYPE_UINT, &cdma1x_state,
	                           G_TYPE_UINT, &evdo_state,
	                           G_TYPE_INVALID)) {
		update_registration_state (info, cdma1x_state, evdo_state);
		applet_schedule_update_icon (info->applet);
	}

	g_clear_error (&error);
}

static void
signal_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	CdmaDeviceInfo *info = user_data;
	GError *error = NULL;
	guint32 quality = 0;

	if (dbus_g_proxy_end_call (proxy, call, &error,
	                           G_TYPE_UINT, &quality,
	                           G_TYPE_INVALID)) {
		info->quality = quality;
		info->quality_valid = TRUE;
		applet_schedule_update_icon (info->applet);
	}

	g_clear_error (&error);
}

#define SERVING_SYSTEM_TYPE (dbus_g_type_get_struct ("GValueArray", G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INVALID))

static void
serving_system_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	CdmaDeviceInfo *info = user_data;
	GError *error = NULL;
	GValueArray *array = NULL;
	guint32 new_sid = 0;
	GValue *value;

	if (dbus_g_proxy_end_call (proxy, call, &error,
	                           SERVING_SYSTEM_TYPE, &array,
	                           G_TYPE_INVALID)) {
		if (array->n_values == 3) {
			value = g_value_array_get_nth (array, 2);
			if (G_VALUE_HOLDS_UINT (value))
				new_sid = g_value_get_uint (value);
		}

		g_value_array_free (array);
	}

	if (new_sid != info->sid) {
		info->sid = new_sid;
		g_free (info->provider_name);
		info->provider_name = mobile_helper_parse_3gpp2_operator_name (&(info->mobile_providers_database), info->sid);
	}

	g_clear_error (&error);
}

static void
enabled_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	CdmaDeviceInfo *info = user_data;
	GError *error = NULL;
	GValue value = { 0 };

	if (dbus_g_proxy_end_call (proxy, call, &error,
	                           G_TYPE_VALUE, &value,
	                           G_TYPE_INVALID)) {
		if (G_VALUE_HOLDS_BOOLEAN (&value))
			info->modem_enabled = g_value_get_boolean (&value);
		g_value_unset (&value);
	}

	g_clear_error (&error);
	check_start_polling (info);
}

static gboolean
cdma_poll_cb (gpointer user_data)
{
	CdmaDeviceInfo *info = user_data;

	/* Kick off calls to get registration state and signal quality */
	if (!info->skip_reg_poll) {
		dbus_g_proxy_begin_call (info->cdma_proxy, "GetRegistrationState",
		                         reg_state_reply, info, NULL,
		                         G_TYPE_INVALID);
		info->skip_reg_poll = FALSE;
	}

	if (!info->skip_signal_poll) {
		dbus_g_proxy_begin_call (info->cdma_proxy, "GetSignalQuality",
		                         signal_reply, info, NULL,
		                         G_TYPE_INVALID);
		info->skip_signal_poll = FALSE;
	}

	dbus_g_proxy_begin_call (info->cdma_proxy, "GetServingSystem",
	                         serving_system_reply, info, NULL,
	                         G_TYPE_INVALID);

	return TRUE;  /* keep running until we're told to stop */
}

static void
check_start_polling (CdmaDeviceInfo *info)
{
	NMDeviceState state;
	gboolean poll = TRUE;

	g_return_if_fail (info != NULL);

	/* Don't poll if any of the following are true:
	 *
	 * 1) NM says the device is not available
	 * 3) the modem isn't enabled
	 */

	state = nm_device_get_state (info->device);
	if (   (state <= NM_DEVICE_STATE_UNAVAILABLE)
	    || (info->modem_enabled == FALSE))
		poll = FALSE;

	if (poll) {
		if (!info->poll_id) {
			/* 33 seconds to be just a bit more than MM's poll interval, so
			 * that if we get an unsolicited update from MM between polls we'll
			 * skip the next poll.
			 */
			info->poll_id = g_timeout_add_seconds (33, cdma_poll_cb, info);
		}
		cdma_poll_cb (info);
	} else {
		if (info->poll_id)
			g_source_remove (info->poll_id);
		info->poll_id = 0;
		info->skip_reg_poll = FALSE;
		info->skip_signal_poll = FALSE;
	}
}

static void
reg_state_changed_cb (DBusGProxy *proxy,
                      guint32 cdma1x_state,
                      guint32 evdo_state,
                      gpointer user_data)
{
	CdmaDeviceInfo *info = user_data;

	update_registration_state (info, cdma1x_state, evdo_state);
	info->skip_reg_poll = TRUE;
	applet_schedule_update_icon (info->applet);
}

static void
signal_quality_changed_cb (DBusGProxy *proxy,
                           guint32 quality,
                           gpointer user_data)
{
	CdmaDeviceInfo *info = user_data;

	info->quality = quality;
	info->quality_valid = TRUE;
	info->skip_signal_poll = TRUE;

	applet_schedule_update_icon (info->applet);
}

#define MM_OLD_DBUS_INTERFACE_MODEM "org.freedesktop.ModemManager.Modem"
#define DBUS_TYPE_G_MAP_OF_VARIANT (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

static void
modem_properties_changed (DBusGProxy *proxy,
                          const char *interface,
                          GHashTable *props,
                          gpointer user_data)
{
	CdmaDeviceInfo *info = user_data;
	GValue *value;

	if (!strcmp (interface, MM_OLD_DBUS_INTERFACE_MODEM)) {
		value = g_hash_table_lookup (props, "Enabled");
		if (value && G_VALUE_HOLDS_BOOLEAN (value)) {
			info->modem_enabled = g_value_get_boolean (value);
			if (!info->modem_enabled) {
				info->quality = 0;
				info->quality_valid = 0;
				info->cdma1x_state = 0;
				info->evdo_state = 0;
				info->sid = 0;
				g_free (info->provider_name);
				info->provider_name = NULL;
			}
			check_start_polling (info);
		}
	}
}

static void
cdma_device_added (NMDevice *device, NMApplet *applet)
{
	NMDeviceModem *modem = NM_DEVICE_MODEM (device);
	CdmaDeviceInfo *info;
	DBusGConnection *bus;
	const char *udi;
	GError *error = NULL;

	udi = nm_device_get_udi (device);
	if (!udi)
		return;

	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!bus) {
		g_warning ("%s: failed to connect to D-Bus: (%d) %s", __func__, error->code, error->message);
		g_clear_error (&error);
		return;
	}

	info = g_malloc0 (sizeof (CdmaDeviceInfo));
	info->applet = applet;
	info->device = device;
	info->bus = bus;
	info->quality_valid = FALSE;

	info->props_proxy = dbus_g_proxy_new_for_name (bus,
	                                               "org.freedesktop.ModemManager",
	                                               udi,
	                                               "org.freedesktop.DBus.Properties");
	if (!info->props_proxy) {
		g_message ("%s: failed to create D-Bus properties proxy.", __func__);
		cdma_device_info_free (info);
		return;
	}

	info->cdma_proxy = dbus_g_proxy_new_for_name (bus,
	                                              "org.freedesktop.ModemManager",
	                                              udi,
	                                              "org.freedesktop.ModemManager.Modem.Cdma");
	if (!info->cdma_proxy) {
		g_message ("%s: failed to create CDMA proxy.", __func__);
		cdma_device_info_free (info);
		return;
	}

	g_object_set_data_full (G_OBJECT (modem), "devinfo", info, cdma_device_info_free);

	/* Registration state change signal */
	dbus_g_object_register_marshaller (_nma_marshal_VOID__UINT_UINT,
	                                   G_TYPE_NONE,
	                                   G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (info->cdma_proxy, "RegistrationStateChanged",
	                         G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (info->cdma_proxy, "RegistrationStateChanged",
	                             G_CALLBACK (reg_state_changed_cb), info, NULL);

	/* Signal quality change signal */
	dbus_g_object_register_marshaller (g_cclosure_marshal_VOID__UINT,
	                                   G_TYPE_NONE, G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (info->cdma_proxy, "SignalQuality", G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (info->cdma_proxy, "SignalQuality",
	                             G_CALLBACK (signal_quality_changed_cb), info, NULL);

	/* Modem property change signal */
	dbus_g_object_register_marshaller (_nma_marshal_VOID__STRING_BOXED,
	                                   G_TYPE_NONE, G_TYPE_STRING, DBUS_TYPE_G_MAP_OF_VARIANT,
	                                   G_TYPE_INVALID);
	dbus_g_proxy_add_signal (info->props_proxy, "MmPropertiesChanged",
	                         G_TYPE_STRING, DBUS_TYPE_G_MAP_OF_VARIANT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (info->props_proxy, "MmPropertiesChanged",
	                             G_CALLBACK (modem_properties_changed),
	                             info, NULL);

	/* Ask whether the device is enabled */
	dbus_g_proxy_begin_call (info->props_proxy, "Get",
	                         enabled_reply, info, NULL,
	                         G_TYPE_STRING, MM_OLD_DBUS_INTERFACE_MODEM,
	                         G_TYPE_STRING, "Enabled",
	                         G_TYPE_INVALID);
}

NMADeviceClass *
applet_device_cdma_get_class (NMApplet *applet)
{
	NMADeviceClass *dclass;

	dclass = g_slice_new0 (NMADeviceClass);
	if (!dclass)
		return NULL;

	dclass->new_auto_connection = cdma_new_auto_connection;
	dclass->add_menu_item = cdma_add_menu_item;
	dclass->device_state_changed = cdma_device_state_changed;
	dclass->notify_connected = cdma_notify_connected;
	dclass->get_icon = cdma_get_icon;
	dclass->get_secrets = cdma_get_secrets;
	dclass->secrets_request_size = sizeof (MobileHelperSecretsInfo);
	dclass->device_added = cdma_device_added;

	return dclass;
}
