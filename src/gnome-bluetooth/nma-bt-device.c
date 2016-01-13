/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 *  NetworkManager Applet
 *
 *  Copyright (C) 2009  Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2009 - 2010  Dan Williams <dcbw@redhat.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <net/ethernet.h>
#include <netinet/ether.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <nm-remote-settings.h>
#include <nm-remote-connection.h>

#include "nma-bt-device.h"
#include "nma-marshal.h"
#include "nm-mobile-wizard.h"
#include "nm-utils.h"
#include "utils.h"

#if WITH_MODEM_MANAGER_1
#include <libmm-glib.h>
#endif

G_DEFINE_TYPE (NmaBtDevice, nma_bt_device, G_TYPE_OBJECT)

#define NMA_BT_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NMA_TYPE_BT_DEVICE, NmaBtDevicePrivate))

typedef struct {
	DBusGConnection *bus;
	NMRemoteSettings *settings;

	char *bdaddr;
	GByteArray *bdaddr_array;
	char *alias;
	char *object_path;

	char *status;
	gboolean busy;

	gboolean has_pan;
	gboolean pan_enabled;
	gboolean has_dun;
	gboolean dun_enabled;

	/* DUN stuff */
	DBusGProxy *dun_proxy;
	DBusGProxy *mm_proxy;
	GSList *modem_proxies;
	char *rfcomm_iface;
	guint dun_timeout_id;

#if WITH_MODEM_MANAGER_1
	GDBusConnection *dbus_connection;
	MMManager *modem_manager_1;
#endif

	GtkWindow *parent_window;
	NMAMobileWizard *wizard;
	GtkWindowGroup *window_group;
} NmaBtDevicePrivate;


enum {
	PROP_0,
	PROP_BDADDR,
	PROP_ALIAS,
	PROP_OBJECT_PATH,
	PROP_HAS_PAN,
	PROP_PAN_ENABLED,
	PROP_HAS_DUN,
	PROP_DUN_ENABLED,
	PROP_BUSY,
	PROP_STATUS,

	LAST_PROP
};

static void _set_pan_enabled (NmaBtDevice *device, gboolean enabled);
static void _set_dun_enabled (NmaBtDevice *device, gboolean enabled);

#define DBUS_TYPE_G_MAP_OF_VARIANT (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

#define BLUEZ_SERVICE           "org.bluez"
#define BLUEZ_MANAGER_PATH      "/"
#define BLUEZ_MANAGER_INTERFACE "org.bluez.Manager"
#define BLUEZ_ADAPTER_INTERFACE "org.bluez.Adapter"
#define BLUEZ_DEVICE_INTERFACE  "org.bluez.Device"
#define BLUEZ_SERIAL_INTERFACE  "org.bluez.Serial"
#define BLUEZ_NETWORK_INTERFACE "org.bluez.Network"

#define MM_SERVICE         "org.freedesktop.ModemManager"
#define MM_PATH            "/org/freedesktop/ModemManager"
#define MM_INTERFACE       "org.freedesktop.ModemManager"
#define MM_MODEM_INTERFACE "org.freedesktop.ModemManager.Modem"

#if WITH_MODEM_MANAGER_1
#include <libmm-glib.h>
#endif

/*********************************************************************/

static gboolean
match_connection_bdaddr (NMConnection *connection, const GByteArray *bdaddr)
{
	NMSettingBluetooth *s_bt;
	const GByteArray *tmp;

	s_bt = nm_connection_get_setting_bluetooth (connection);
	if (s_bt) {
		tmp = nm_setting_bluetooth_get_bdaddr (s_bt);
		if (tmp && memcmp (tmp->data, bdaddr->data, tmp->len) == 0)
			return TRUE;
	}
	return FALSE;
}

static gboolean
match_connection_service (NMConnection *connection,
                          const GByteArray *bdaddr,
                          gboolean pan)
{
	NMSettingBluetooth *s_bt;
	const char *type;

	if (!match_connection_bdaddr (connection, bdaddr))
		return FALSE;

	s_bt = nm_connection_get_setting_bluetooth (connection);
	g_assert (s_bt);
	type = nm_setting_bluetooth_get_connection_type (s_bt);
	if (pan) {
		if (g_strcmp0 (type, NM_SETTING_BLUETOOTH_TYPE_PANU) != 0)
			return FALSE;
	} else {
		if (g_strcmp0 (type, NM_SETTING_BLUETOOTH_TYPE_DUN) != 0)
			return FALSE;
	}

	return TRUE;
}

static void
delete_cb (NMRemoteConnection *connection, GError *error, gpointer user_data)
{
	if (error) {
		g_warning ("Error deleting connection: (%d) %s",
		           error ? error->code : -1,
		           error && error->message ? error->message : "(unknown)");
	}
}

static void
delete_connections_of_type (NMRemoteSettings *settings,
                            const GByteArray *bdaddr,
                            gboolean pan)
{
	GSList *list, *iter;

	list = nm_remote_settings_list_connections (settings);
	for (iter = list; iter != NULL; iter = g_slist_next (iter)) {
		NMRemoteConnection *remote = iter->data;

		if (match_connection_service (NM_CONNECTION (remote), bdaddr, pan))
			nm_remote_connection_delete (remote, delete_cb, NULL);
	}
	g_slist_free (list);
}

static void
recheck_services_enabled (NmaBtDevice *self)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	GSList *list, *iter;
	gboolean pan = FALSE, dun = FALSE;

	/* Retrieve initial enabled state for both PAN and DUN; if there are any
	 * existing Bluetooth connections for the given device for either PAN
	 * or DUN, then we consider that service enabled.
	 */
	list = nm_remote_settings_list_connections (priv->settings);
	for (iter = list; iter != NULL; iter = g_slist_next (iter)) {
		NMConnection *connection = iter->data;

		if (match_connection_bdaddr (connection, priv->bdaddr_array)) {
			NMSettingBluetooth *s_bt;
			const char *type;

			s_bt = nm_connection_get_setting_bluetooth (connection);
			g_assert (s_bt);
			type = nm_setting_bluetooth_get_connection_type (s_bt);
			if (priv->has_pan && g_strcmp0 (type, NM_SETTING_BLUETOOTH_TYPE_PANU) == 0)
				pan = TRUE;
			else if (priv->has_dun && g_strcmp0 (type, NM_SETTING_BLUETOOTH_TYPE_DUN) == 0)
				dun =  TRUE;
		}
	}
	g_slist_free (list);

	_set_pan_enabled (self, pan);
	_set_dun_enabled (self, dun);
}

/*********************************************************************/

const char *
nma_bt_device_get_bdaddr (NmaBtDevice *device)
{
	return NMA_BT_DEVICE_GET_PRIVATE (device)->bdaddr;
}

gboolean
nma_bt_device_get_busy (NmaBtDevice *device)
{
	return NMA_BT_DEVICE_GET_PRIVATE (device)->busy;
}

static void
_set_busy (NmaBtDevice *device, gboolean busy)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (device);

	if (priv->busy != busy) {
		priv->busy = busy;
		g_object_notify (G_OBJECT (device), NMA_BT_DEVICE_BUSY);
	}
}

const char *
nma_bt_device_get_status (NmaBtDevice *device)
{
	return NMA_BT_DEVICE_GET_PRIVATE (device)->status;
}

static void
_set_status (NmaBtDevice *device, const char *fmt, ...)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (device);
	va_list args;

	g_free (priv->status);
	priv->status = NULL;

	if (fmt) {
		va_start (args, fmt);
		priv->status = g_strdup_vprintf (fmt, args);
		va_end (args);
		g_message ("%s", priv->status);
	}

	g_object_notify (G_OBJECT (device), NMA_BT_DEVICE_STATUS);
}

/*********************************************************************/

static void
dun_cleanup (NmaBtDevice *self)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	GSList *iter;

	/* ModemManager */
	for (iter = priv->modem_proxies; iter; iter = g_slist_next (iter))
		g_object_unref (DBUS_G_PROXY (iter->data));
	g_slist_free (priv->modem_proxies);
	priv->modem_proxies = NULL;
	g_clear_object (&priv->mm_proxy);

#if WITH_MODEM_MANAGER_1
	g_clear_object (&priv->dbus_connection);
	g_clear_object (&priv->modem_manager_1);
#endif

	if (priv->dun_proxy && priv->rfcomm_iface) {
		dbus_g_proxy_call_no_reply (priv->dun_proxy, "Disconnect",
		                            G_TYPE_STRING, priv->rfcomm_iface,
		                            G_TYPE_INVALID);
	}
	g_clear_object (&priv->dun_proxy);

	g_free (priv->rfcomm_iface);
	priv->rfcomm_iface = NULL;

	if (priv->dun_timeout_id) {
		g_source_remove (priv->dun_timeout_id);
		priv->dun_timeout_id = 0;
	}

	if (priv->wizard) {
		nma_mobile_wizard_destroy (priv->wizard);
		priv->wizard = NULL;
	}
}

static void
dun_error (NmaBtDevice *self, const char *func, GError *error, const char *fallback)
{
	g_warning ("%s: DUN error: %s", func, (error && error->message) ? error->message : fallback);
	_set_status (self, _("Error: %s"), (error && error->message) ? error->message : fallback);

	_set_busy (self, FALSE);
	dun_cleanup (self);
	recheck_services_enabled (self);
}

static NMConnection *
dun_new_cdma (NMAMobileWizardAccessMethod *method)
{
	NMConnection *connection;
	NMSetting *setting;
	char *uuid, *id;

	connection = nm_connection_new ();

	setting = nm_setting_cdma_new ();
	g_object_set (setting,
	              NM_SETTING_CDMA_NUMBER, "#777",
	              NM_SETTING_CDMA_USERNAME, method->username,
	              NM_SETTING_CDMA_PASSWORD, method->password,
	              NULL);
	nm_connection_add_setting (connection, setting);

	/* Serial setting */
	setting = nm_setting_serial_new ();
	g_object_set (setting,
	              NM_SETTING_SERIAL_BAUD, 115200,
	              NM_SETTING_SERIAL_BITS, 8,
	              NM_SETTING_SERIAL_PARITY, 'n',
	              NM_SETTING_SERIAL_STOPBITS, 1,
	              NULL);
	nm_connection_add_setting (connection, setting);

	nm_connection_add_setting (connection, nm_setting_ppp_new ());

	setting = nm_setting_connection_new ();
	id = utils_create_mobile_connection_id (method->provider_name, method->plan_name);
	uuid = nm_utils_uuid_generate ();
	g_object_set (setting,
	              NM_SETTING_CONNECTION_ID, id,
	              NM_SETTING_CONNECTION_TYPE, NM_SETTING_BLUETOOTH_SETTING_NAME,
	              NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
	              NM_SETTING_CONNECTION_UUID, uuid,
	              NULL);
	g_free (uuid);
	g_free (id);
	nm_connection_add_setting (connection, setting);

	return connection;
}

static NMConnection *
dun_new_gsm (NMAMobileWizardAccessMethod *method)
{
	NMConnection *connection;
	NMSetting *setting;
	char *uuid, *id;

	connection = nm_connection_new ();

	setting = nm_setting_gsm_new ();
	g_object_set (setting,
	              NM_SETTING_GSM_NUMBER, "*99#",
	              NM_SETTING_GSM_USERNAME, method->username,
	              NM_SETTING_GSM_PASSWORD, method->password,
	              NM_SETTING_GSM_APN, method->gsm_apn,
	              NULL);
	nm_connection_add_setting (connection, setting);

	/* Serial setting */
	setting = nm_setting_serial_new ();
	g_object_set (setting,
	              NM_SETTING_SERIAL_BAUD, 115200,
	              NM_SETTING_SERIAL_BITS, 8,
	              NM_SETTING_SERIAL_PARITY, 'n',
	              NM_SETTING_SERIAL_STOPBITS, 1,
	              NULL);
	nm_connection_add_setting (connection, setting);

	nm_connection_add_setting (connection, nm_setting_ppp_new ());

	setting = nm_setting_connection_new ();
	id = utils_create_mobile_connection_id (method->provider_name, method->plan_name);
	uuid = nm_utils_uuid_generate ();
	g_object_set (setting,
	              NM_SETTING_CONNECTION_ID, id,
	              NM_SETTING_CONNECTION_TYPE, NM_SETTING_BLUETOOTH_SETTING_NAME,
	              NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
	              NM_SETTING_CONNECTION_UUID, uuid,
	              NULL);
	g_free (uuid);
	g_free (id);
	nm_connection_add_setting (connection, setting);

	return connection;
}

static void
dun_add_cb (NMRemoteSettings *settings,
            NMRemoteConnection *connection,
            GError *error,
            gpointer user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);

	if (error)
		_set_status (self, _("Failed to create DUN connection: %s"), error->message);
	else
		_set_status (self, _("Your phone is now ready to use!"));

	_set_busy (self, FALSE);
	dun_cleanup (self);
	recheck_services_enabled (self);
}

static void
wizard_done_cb (NMAMobileWizard *wizard,
                gboolean canceled,
                NMAMobileWizardAccessMethod *method,
                gpointer user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	NMConnection *connection = NULL;
	NMSetting *s_bt;

	g_return_if_fail (wizard == priv->wizard);

	g_message ("%s: mobile wizard done", __func__);

	if (canceled || !method) {
		dun_error (self, __func__, NULL, _("Mobile wizard was canceled"));
		return;
	}

	if (method->devtype == NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)
		connection = dun_new_cdma (method);
	else if (method->devtype == NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS)
		connection = dun_new_gsm (method);
	else {
		dun_error (self, __func__, NULL, _("Unknown phone device type (not GSM or CDMA)"));
		return;
	}

	nma_mobile_wizard_destroy (priv->wizard);
	priv->wizard = NULL;

	g_assert (connection);

	/* The Bluetooth settings */
	s_bt = nm_setting_bluetooth_new ();
	g_object_set (G_OBJECT (s_bt),
	              NM_SETTING_BLUETOOTH_BDADDR, priv->bdaddr_array,
	              NM_SETTING_BLUETOOTH_TYPE, NM_SETTING_BLUETOOTH_TYPE_DUN,
	              NULL);
	nm_connection_add_setting (connection, s_bt);

	g_message ("%s: adding new setting", __func__);

	/* Add the connection to the settings service */
	nm_remote_settings_add_connection (priv->settings,
	                                   connection,
	                                   dun_add_cb,
	                                   self);

	g_message ("%s: waiting for add connection result...", __func__);
}

static void
start_wizard (NmaBtDevice *self,
              const gchar *path,
              NMDeviceModemCapabilities caps)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);

	if (priv->wizard) {
		g_message ("%s: (%s) oops! not starting Wizard as one is already in progress", __func__, path);
		return;
	}

	g_message ("%s: (%s) starting the mobile wizard", __func__, path);

	g_source_remove (priv->dun_timeout_id);
	priv->dun_timeout_id = 0;

	/* Start the mobile wizard */
	priv->wizard = nma_mobile_wizard_new (priv->parent_window,
	                                      priv->window_group,
	                                      caps,
	                                      FALSE,
	                                      wizard_done_cb,
	                                      self);
	nma_mobile_wizard_present (priv->wizard);
}

static void
modem_get_all_cb (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	const char *path;
	GHashTable *properties = NULL;
	GError *error = NULL;
	GValue *value;
	NMDeviceModemCapabilities caps = NM_DEVICE_MODEM_CAPABILITY_NONE;

	path = dbus_g_proxy_get_path (proxy);
	g_message ("%s: (%s) processing GetAll reply", __func__, path);

	if (!dbus_g_proxy_end_call (proxy, call, &error,
	                            DBUS_TYPE_G_MAP_OF_VARIANT, &properties,
	                            G_TYPE_INVALID)) {
		g_warning ("%s: (%s) Error getting modem properties: (%d) %s",
		           __func__,
		           path,
		           error ? error->code : -1,
		           (error && error->message) ? error->message : "(unknown)");
		g_error_free (error);
		goto out;
	}

	/* check whether this is the device we care about */
	value = g_hash_table_lookup (properties, "Device");
	if (value && G_VALUE_HOLDS_STRING (value) && g_value_get_string (value)) {
		char *iface_basename = g_path_get_basename (priv->rfcomm_iface);
		const char *modem_iface = g_value_get_string (value);

		if (strcmp (iface_basename, modem_iface) == 0) {
			/* yay, found it! */

			value = g_hash_table_lookup (properties, "Type");
			if (value && G_VALUE_HOLDS_UINT (value)) {
				switch (g_value_get_uint (value)) {
				case 1:
					caps = NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS;
					break;
				case 2:
					caps = NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO;
					break;
				default:
					g_message ("%s: (%s) unknown modem type", __func__, path);
					break;
				}
			}
		} else {
			g_message ("%s: (%s) (%s) not the modem we're looking for (%s)",
			           __func__, path, modem_iface, iface_basename);
		}

		g_free (iface_basename);
	} else
		g_message ("%s: (%s) modem had no 'Device' property", __func__, path);

	g_hash_table_unref (properties);

	/* Launch wizard! */
	start_wizard (self, path, caps);

out:
	g_message ("%s: finished", __func__);
}

static void
modem_added (DBusGProxy *proxy, const char *path, gpointer user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	DBusGProxy *props_proxy;

	g_return_if_fail (path != NULL);

	g_message ("%s: (%s) modem found", __func__, path);

	/* Create a proxy for the modem and get its properties */
	props_proxy = dbus_g_proxy_new_for_name (priv->bus,
	                                         MM_SERVICE,
	                                         path,
	                                         "org.freedesktop.DBus.Properties");
	g_assert (proxy);
	priv->modem_proxies = g_slist_append (priv->modem_proxies, props_proxy);

	g_message ("%s: (%s) calling GetAll...", __func__, path);

	dbus_g_proxy_begin_call (props_proxy, "GetAll",
	                         modem_get_all_cb,
	                         self,
	                         NULL,
	                         G_TYPE_STRING, MM_MODEM_INTERFACE,
	                         G_TYPE_INVALID);
}

static void
modem_removed (DBusGProxy *proxy, const char *path, gpointer user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	GSList *iter;

	g_return_if_fail (path != NULL);

	g_message ("%s: (%s) modem removed", __func__, path);

	/* Clean up if a modem gets removed */
	for (iter = priv->modem_proxies; iter; iter = g_slist_next (iter)) {
		if (!strcmp (path, dbus_g_proxy_get_path (DBUS_G_PROXY (iter->data)))) {
			priv->modem_proxies = g_slist_remove (priv->modem_proxies, iter->data);
			g_object_unref (iter->data);
			break;
		}
	}
}

#if WITH_MODEM_MANAGER_1

static gboolean
check_modem (NmaBtDevice *self,
             MMObject *modem_object)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	NMDeviceModemCapabilities caps = NM_DEVICE_MODEM_CAPABILITY_NONE;
	MMModem *modem_iface;
	const gchar *path;
	const gchar *primary_port;
	const gchar *iface_basename;
	MMModemCapability mm_caps;

	path = mm_object_get_path (modem_object);
	g_message ("%s: (%s) modem found", __func__, path);

	/* Ensure we have the 'Modem' interface at least */
	modem_iface = mm_object_peek_modem (modem_object);
	g_return_val_if_fail (modem_iface != NULL, FALSE);

	/* Get modem's primary port */
	primary_port = mm_modem_get_primary_port (modem_iface);
	g_return_val_if_fail (primary_port != NULL, FALSE);

	/* Get rfcomm iface name */
	iface_basename = g_path_get_basename (priv->rfcomm_iface);

	/* If not matched, just return */
	if (!g_str_equal (primary_port, iface_basename)) {
		g_message ("%s: (%s) (%s) not the modem we're looking for (%s)",
		           __func__, path, primary_port, iface_basename);
		return FALSE;
	}

	/* This is the modem we were waiting for, so keep on */
	mm_caps = mm_modem_get_current_capabilities (modem_iface);
	/* CDMA-only? */
	if (mm_caps == MM_MODEM_CAPABILITY_CDMA_EVDO)
		caps = NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO;
	/* GSM/UMTS-only? */
	else if (mm_caps == MM_MODEM_CAPABILITY_GSM_UMTS)
		caps = NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS;
	/* LTE? */
	else if (mm_caps & MM_MODEM_CAPABILITY_LTE)
		caps = NM_DEVICE_MODEM_CAPABILITY_LTE;
	else
		g_message ("%s: (%s) unknown modem type", __func__, path);

	/* Launch wizard! */
	start_wizard (self, path, caps);

	return TRUE;
}

static void
modem_object_added (MMManager *modem_manager,
                    MMObject  *modem_object,
                    NmaBtDevice *self)
{
	check_modem (self, modem_object);
}

#endif /* WITH_MODEM_MANAGER_1 */

static void
dun_connect_cb (DBusGProxy *proxy,
                DBusGProxyCall *call,
                void *user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	GError *error = NULL;
	char *device;
#if WITH_MODEM_MANAGER_1
	GList *modems, *iter;
	gboolean matched = FALSE;
#endif

	g_message ("%s: processing Connect reply", __func__);

	if (!dbus_g_proxy_end_call (proxy, call, &error,
	                            G_TYPE_STRING, &device,
	                            G_TYPE_INVALID)) {
		dun_error (self, __func__, error, _("failed to connect to the phone."));
		g_clear_error (&error);
		goto out;
	}

	if (!device || !strlen (device)) {
		dun_error (self, __func__, NULL, _("failed to connect to the phone."));
		g_free (device);
		goto out;
	}

	g_free (priv->rfcomm_iface);
	priv->rfcomm_iface = device;
	g_message ("%s: new rfcomm interface '%s'", __func__, device);

#if WITH_MODEM_MANAGER_1
	/* ModemManager1 stuff */
	priv->dbus_connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!priv->dbus_connection) {
		dun_error (self, __func__, error, _("error getting bus connection"));
		g_error_free (error);
		goto out;
	}

	priv->modem_manager_1 = mm_manager_new_sync (priv->dbus_connection,
	                                             G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
	                                             NULL,
	                                             &error);
	if (!priv->modem_manager_1) {
		dun_error (self, __func__, error, "error creating modem manager");
		g_error_free (error);
		goto out;
	}

	modems = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (priv->modem_manager_1));
	for (iter = modems; iter; iter = iter->next) {
		if (check_modem (self, iter->data)) {
			matched = TRUE;
			break;
		}
	}
	g_list_free_full (modems, g_object_unref);

	if (!matched) {
		g_signal_connect (priv->modem_manager_1,
		                  "object-added",
		                  G_CALLBACK (modem_object_added),
		                  self);
	}
#endif

out:
	g_message ("%s: finished", __func__);
}

static void
dun_property_changed (DBusGProxy *proxy,
                      const char *property,
                      GValue *value,
                      gpointer user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);
	gboolean connected;

	if (strcmp (property, "Connected") == 0) {
		connected = g_value_get_boolean (value);
		g_message ("%s: device property Connected changed to %s",
			       __func__,
			       connected ? "TRUE" : "FALSE");

		if (connected) {
			/* Wait for MM here ? */
		} else
			dun_error (self, __func__, NULL, _("unexpectedly disconnected from the phone."));
	}
}

static gboolean
dun_timeout_cb (gpointer user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);

	NMA_BT_DEVICE_GET_PRIVATE (self)->dun_timeout_id = 0;
	dun_error (self, __func__, NULL, _("timed out detecting phone details."));
	return FALSE;
}

static void
dun_start (NmaBtDevice *self)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);

	g_message ("%s: starting DUN device discovery...", __func__);

	_set_status (self, _("Detecting phone configuration..."));

	/* ModemManager stuff */
	priv->mm_proxy = dbus_g_proxy_new_for_name (priv->bus,
	                                            MM_SERVICE,
	                                            MM_PATH,
	                                            MM_INTERFACE);
	g_assert (priv->mm_proxy);

	dbus_g_object_register_marshaller (g_cclosure_marshal_VOID__BOXED,
	                                   G_TYPE_NONE,
	                                   G_TYPE_BOXED,
	                                   G_TYPE_INVALID);
	dbus_g_proxy_add_signal (priv->mm_proxy, "DeviceAdded",
	                         DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (priv->mm_proxy, "DeviceAdded",
								 G_CALLBACK (modem_added), self,
								 NULL);

	dbus_g_proxy_add_signal (priv->mm_proxy, "DeviceRemoved",
	                         DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (priv->mm_proxy, "DeviceRemoved",
								 G_CALLBACK (modem_removed), self,
								 NULL);

	/* Bluez */
	priv->dun_proxy = dbus_g_proxy_new_for_name (priv->bus,
	                                             BLUEZ_SERVICE,
	                                             priv->object_path,
	                                             BLUEZ_SERIAL_INTERFACE);
	g_assert (priv->dun_proxy);

	priv->dun_timeout_id = g_timeout_add_seconds (45, dun_timeout_cb, self);

	g_message ("%s: calling Connect...", __func__);

	/* Watch for BT device property changes */
	dbus_g_object_register_marshaller (_nma_marshal_VOID__STRING_BOXED,
	                                   G_TYPE_NONE,
	                                   G_TYPE_STRING, G_TYPE_VALUE,
	                                   G_TYPE_INVALID);
	dbus_g_proxy_add_signal (priv->dun_proxy, "PropertyChanged",
	                         G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (priv->dun_proxy, "PropertyChanged",
	                             G_CALLBACK (dun_property_changed), self, NULL);

	/* Request a connection to the device and get the port */
	dbus_g_proxy_begin_call_with_timeout (priv->dun_proxy, "Connect",
	                                      dun_connect_cb,
	                                      self,
	                                      NULL,
	                                      20000,
	                                      G_TYPE_STRING, "dun",
	                                      G_TYPE_INVALID);

	g_message ("%s: waiting for Connect success...", __func__);
}

gboolean
nma_bt_device_get_has_dun (NmaBtDevice *device)
{
	return NMA_BT_DEVICE_GET_PRIVATE (device)->has_dun;
}

gboolean
nma_bt_device_get_dun_enabled (NmaBtDevice *device)
{
	return NMA_BT_DEVICE_GET_PRIVATE (device)->dun_enabled;
}

static void
_set_dun_enabled (NmaBtDevice *device, gboolean enabled)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (device);

	if (priv->dun_enabled != enabled) {
		priv->dun_enabled = enabled;
		g_object_notify (G_OBJECT (device), NMA_BT_DEVICE_DUN_ENABLED);
	}
}

void
nma_bt_device_set_dun_enabled (NmaBtDevice *device, gboolean enabled)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (device);

	_set_dun_enabled (device, enabled);

	if (enabled) {
		_set_busy (device, TRUE);
		dun_start (device);
	} else
		delete_connections_of_type (priv->settings, priv->bdaddr_array, FALSE);
}

void
nma_bt_device_cancel_dun (NmaBtDevice *device)
{
	dun_error (device, __func__, NULL, _("The default Bluetooth adapter must be enabled before setting up a Dial-Up-Networking connection."));
}

/*********************************************************************/

gboolean
nma_bt_device_get_has_pan (NmaBtDevice *device)
{
	return NMA_BT_DEVICE_GET_PRIVATE (device)->has_pan;
}

gboolean
nma_bt_device_get_pan_enabled (NmaBtDevice *device)
{
	return NMA_BT_DEVICE_GET_PRIVATE (device)->pan_enabled;
}

static void
_set_pan_enabled (NmaBtDevice *device, gboolean enabled)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (device);

	if (priv->pan_enabled != enabled) {
		priv->pan_enabled = enabled;
		g_object_notify (G_OBJECT (device), NMA_BT_DEVICE_PAN_ENABLED);
	}
}

static void
pan_add_cb (NMRemoteSettings *settings,
            NMRemoteConnection *connection,
            GError *error,
            gpointer user_data)
{
	NmaBtDevice *self = NMA_BT_DEVICE (user_data);

	if (error)
		_set_status (self, _("Failed to create PAN connection: %s"), error->message);
	else
		_set_status (self, _("Your phone is now ready to use!"));

	recheck_services_enabled (self);
	_set_busy (self, FALSE);
}

static void
add_pan_connection (NmaBtDevice *self)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
	NMConnection *connection;
	NMSetting *setting, *bt_setting, *ip_setting;
	char *id, *uuid;

	/* The connection */
	connection = nm_connection_new ();

	/* The connection settings */
	setting = nm_setting_connection_new ();
	id = g_strdup_printf (_("%s Network"), priv->alias ? priv->alias : priv->bdaddr);
	uuid = nm_utils_uuid_generate ();
	g_object_set (G_OBJECT (setting),
	              NM_SETTING_CONNECTION_ID, id,
	              NM_SETTING_CONNECTION_UUID, uuid,
	              NM_SETTING_CONNECTION_TYPE, NM_SETTING_BLUETOOTH_SETTING_NAME,
	              NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
	              NULL);
	g_free (id);
	g_free (uuid);
	nm_connection_add_setting (connection, setting);

	/* The Bluetooth settings */
	bt_setting = nm_setting_bluetooth_new ();
	g_object_set (G_OBJECT (bt_setting),
	              NM_SETTING_BLUETOOTH_BDADDR, priv->bdaddr_array,
	              NM_SETTING_BLUETOOTH_TYPE, NM_SETTING_BLUETOOTH_TYPE_PANU,
	              NULL);
	nm_connection_add_setting (connection, bt_setting);

	/* IPv4 */
	ip_setting = nm_setting_ip4_config_new ();
	g_object_set (G_OBJECT (ip_setting),
	              NM_SETTING_IP4_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO,
	              NM_SETTING_IP4_CONFIG_MAY_FAIL, FALSE,
	              NULL);
	nm_connection_add_setting (connection, ip_setting);

	/* IPv6 */
	ip_setting = nm_setting_ip6_config_new ();
	g_object_set (G_OBJECT (ip_setting),
	              NM_SETTING_IP6_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_AUTO,
	              NM_SETTING_IP6_CONFIG_MAY_FAIL, TRUE,
	              NULL);
	nm_connection_add_setting (connection, ip_setting);

	/* Add the connection to the settings service */
	nm_remote_settings_add_connection (priv->settings,
	                                   connection,
	                                   pan_add_cb,
	                                   self);
}

void
nma_bt_device_set_pan_enabled (NmaBtDevice *device, gboolean enabled)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (device);

	_set_pan_enabled (device, enabled);

	if (enabled) {
		_set_busy (device, TRUE);
		add_pan_connection (device);
	} else
		delete_connections_of_type (priv->settings, priv->bdaddr_array, TRUE);
}

/*********************************************************************/

void
nma_bt_device_set_parent_window (NmaBtDevice *device, GtkWindow *window)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (device);

	if (window == priv->parent_window)
		return;

	if (priv->parent_window) {
		gtk_window_group_remove_window (priv->window_group, priv->parent_window);
		g_object_unref (priv->parent_window);
	}
	priv->parent_window = g_object_ref (window);
	gtk_window_group_add_window (priv->window_group, window);
}

/*********************************************************************/

static void
connections_read (NMRemoteSettings *settings, gpointer user_data)
{
	recheck_services_enabled (NMA_BT_DEVICE (user_data));
}

NmaBtDevice *
nma_bt_device_new (const char *bdaddr,
                   const char *alias,
                   const char *object_path,
                   gboolean has_pan,
                   gboolean has_dun)
{
	NmaBtDevice *self;
	GError *error = NULL;

	g_return_val_if_fail (bdaddr != NULL, NULL);
	g_return_val_if_fail (object_path != NULL, NULL);

	self = (NmaBtDevice *) g_object_new (NMA_TYPE_BT_DEVICE,
	                                     NMA_BT_DEVICE_BDADDR, bdaddr,
	                                     NMA_BT_DEVICE_ALIAS, alias,
	                                     NMA_BT_DEVICE_OBJECT_PATH, object_path,
	                                     NMA_BT_DEVICE_HAS_PAN, has_pan,
	                                     NMA_BT_DEVICE_HAS_DUN, has_dun,
	                                     NULL);
	if (self) {
		NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (self);
		struct ether_addr *addr;

		g_assert (priv->bdaddr);
		g_assert (priv->object_path);

		addr = ether_aton (priv->bdaddr);
		if (!addr) {
			g_warning ("%s: invalid Bluetooth address '%s'", __func__, priv->bdaddr);
			g_object_unref (self);
			return NULL;
		}

		priv->bdaddr_array = g_byte_array_sized_new (ETH_ALEN);
		g_byte_array_append (priv->bdaddr_array, (const guint8 *) addr->ether_addr_octet, ETH_ALEN);

		priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
		if (error) {
			g_warning ("%s: failed to connect to D-Bus: %s", __func__, error->message);
			g_object_unref (self);
			self = NULL;
		}

		priv->window_group = gtk_window_group_new ();

		priv->settings = nm_remote_settings_new (priv->bus);
		g_signal_connect (priv->settings,
				          NM_REMOTE_SETTINGS_CONNECTIONS_READ,
				          G_CALLBACK (connections_read),
				          self);
	}
	return self;
}

static void
nma_bt_device_init (NmaBtDevice *self)
{
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_BDADDR:
		g_value_set_string (value, priv->bdaddr);
		break;
	case PROP_ALIAS:
		g_value_set_string (value, priv->alias);
		break;
	case PROP_OBJECT_PATH:
		g_value_set_string (value, priv->object_path);
		break;
	case PROP_HAS_PAN:
		g_value_set_boolean (value, priv->has_pan);
		break;
	case PROP_PAN_ENABLED:
		g_value_set_boolean (value, priv->pan_enabled);
		break;
	case PROP_HAS_DUN:
		g_value_set_boolean (value, priv->has_dun);
		break;
	case PROP_DUN_ENABLED:
		g_value_set_boolean (value, priv->dun_enabled);
		break;
	case PROP_BUSY:
		g_value_set_boolean (value, priv->busy);
		break;
	case PROP_STATUS:
		g_value_set_string (value, priv->status);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_BDADDR:
		priv->bdaddr = g_value_dup_string (value);
		break;
	case PROP_ALIAS:
		priv->alias = g_value_dup_string (value);
		break;
	case PROP_OBJECT_PATH:
		priv->object_path = g_value_dup_string (value);
		break;
	case PROP_HAS_PAN:
		priv->has_pan = g_value_get_boolean (value);
		break;
	case PROP_HAS_DUN:
		priv->has_dun = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
dispose (GObject *object)
{
	NmaBtDevicePrivate *priv = NMA_BT_DEVICE_GET_PRIVATE (object);

	dun_cleanup (NMA_BT_DEVICE (object));

	g_free (priv->bdaddr);
	priv->bdaddr = NULL;
	g_free (priv->alias);
	priv->alias = NULL;
	g_free (priv->object_path);
	priv->object_path = NULL;
	g_free (priv->status);
	priv->status = NULL;

	g_clear_object (&priv->window_group);
	g_clear_object (&priv->parent_window);

	if (priv->bdaddr_array) {
		g_byte_array_free (priv->bdaddr_array, TRUE);
		priv->bdaddr_array = NULL;
	}

	G_OBJECT_CLASS (nma_bt_device_parent_class)->dispose (object);
}

static void
nma_bt_device_class_init (NmaBtDeviceClass *btdevice_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (btdevice_class);

	g_type_class_add_private (btdevice_class, sizeof (NmaBtDevicePrivate));

	/* virtual methods */
	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->dispose = dispose;

	/* properties */
	g_object_class_install_property (object_class, PROP_BDADDR,
		 g_param_spec_string (NMA_BT_DEVICE_BDADDR,
		                      "Bluetooth address",
		                      "Bluetooth address",
		                      NULL,
		                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_ALIAS,
		 g_param_spec_string (NMA_BT_DEVICE_ALIAS,
		                      "Bluetooth alias",
		                      "Bluetooth alias",
		                      NULL,
		                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_OBJECT_PATH,
		 g_param_spec_string (NMA_BT_DEVICE_OBJECT_PATH,
		                      "Bluez object path",
		                      "Bluez object path",
		                      NULL,
		                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_HAS_PAN,
		 g_param_spec_boolean (NMA_BT_DEVICE_HAS_PAN,
		                       "PAN capable",
		                       "PAN capable",
		                       FALSE,
		                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_PAN_ENABLED,
		 g_param_spec_boolean (NMA_BT_DEVICE_PAN_ENABLED,
		                       "PAN enabled",
		                       "PAN enabled",
		                       FALSE,
		                       G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_HAS_DUN,
		 g_param_spec_boolean (NMA_BT_DEVICE_HAS_DUN,
		                       "DUN capable",
		                       "DUN capable",
		                       FALSE,
		                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_DUN_ENABLED,
		 g_param_spec_boolean (NMA_BT_DEVICE_DUN_ENABLED,
		                       "DUN enabled",
		                       "DUN enabled",
		                       FALSE,
		                       G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_BUSY,
		 g_param_spec_boolean (NMA_BT_DEVICE_BUSY,
		                       "Busy",
		                       "Busy",
		                       FALSE,
		                       G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_STATUS,
		 g_param_spec_string (NMA_BT_DEVICE_STATUS,
		                      "Status",
		                      "Status",
		                      NULL,
		                      G_PARAM_READABLE));
}
