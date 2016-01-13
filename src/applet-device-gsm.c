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
 * (C) Copyright 2008 Novell, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ctype.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define SECRET_API_SUBJECT_TO_CHANGE
#include <libsecret/secret.h>

#include <nm-device.h>
#include <nm-setting-connection.h>
#include <nm-setting-gsm.h>
#include <nm-setting-serial.h>
#include <nm-setting-ppp.h>
#include <nm-device-modem.h>
#include <nm-utils.h>
#include <nm-secret-agent.h>

#include "applet.h"
#include "applet-device-gsm.h"
#include "utils.h"
#include "applet-dialogs.h"
#include "mb-menu-item.h"
#include "nma-marshal.h"
#include "nm-mobile-providers.h"
#include "nm-ui-utils.h"
#include "nm-glib-compat.h"

typedef enum {
    MM_MODEM_GSM_ACCESS_TECH_UNKNOWN     = 0,
    MM_MODEM_GSM_ACCESS_TECH_GSM         = 1,
    MM_MODEM_GSM_ACCESS_TECH_GSM_COMPACT = 2,
    MM_MODEM_GSM_ACCESS_TECH_GPRS        = 3,
    MM_MODEM_GSM_ACCESS_TECH_EDGE        = 4,  /* GSM w/EGPRS */
    MM_MODEM_GSM_ACCESS_TECH_UMTS        = 5,  /* UTRAN */
    MM_MODEM_GSM_ACCESS_TECH_HSDPA       = 6,  /* UTRAN w/HSDPA */
    MM_MODEM_GSM_ACCESS_TECH_HSUPA       = 7,  /* UTRAN w/HSUPA */
    MM_MODEM_GSM_ACCESS_TECH_HSPA        = 8,  /* UTRAN w/HSDPA and HSUPA */
    MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS   = 9,
    MM_MODEM_GSM_ACCESS_TECH_LTE         = 10,

    MM_MODEM_GSM_ACCESS_TECH_LAST = MM_MODEM_GSM_ACCESS_TECH_LTE
} MMModemGsmAccessTech;

typedef struct {
	NMApplet *applet;
	NMDevice *device;

	DBusGConnection *bus;
	DBusGProxy *props_proxy;
	DBusGProxy *card_proxy;
	DBusGProxy *net_proxy;

	gboolean quality_valid;
	guint32 quality;
	char *unlock_required;
	char *devid;
	char *simid;
	gboolean modem_enabled;
	MMModemGsmAccessTech act;

	/* reg_state is (1 + MM reg state) so that 0 means we haven't gotten a
	 * value from MM yet.  0 is a valid MM GSM reg state.
	 */
	guint reg_state;
	char *op_code;
	char *op_name;
	NMAMobileProvidersDatabase *mobile_providers_database;

	guint32 poll_id;
	gboolean skip_reg_poll;
	gboolean skip_signal_poll;

	/* Unlock dialog stuff */
	GtkWidget *dialog;
	GCancellable *cancellable;
} GsmDeviceInfo;

static void unlock_dialog_destroy (GsmDeviceInfo *info);
static void check_start_polling (GsmDeviceInfo *info);

typedef struct {
	NMApplet *applet;
	NMDevice *device;
	NMConnection *connection;
} GSMMenuItemInfo;

static void
gsm_menu_item_info_destroy (gpointer data)
{
	GSMMenuItemInfo *info = data;

	g_object_unref (G_OBJECT (info->device));
	if (info->connection)
		g_object_unref (info->connection);

	g_slice_free (GSMMenuItemInfo, data);
}

static gboolean
gsm_new_auto_connection (NMDevice *device,
                         gpointer dclass_data,
                         AppletNewAutoConnectionCallback callback,
                         gpointer callback_data)
{
	return mobile_helper_wizard (NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS,
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
applet_gsm_connect_network (NMApplet *applet, NMDevice *device)
{
	Dbus3gInfo *info;

	info = g_malloc0 (sizeof (*info));
	info->applet = applet;
	info->device = g_object_ref (device);


	if (!mobile_helper_wizard (NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS,
	                           dbus_connect_3g_cb,
	                           info)) {
		g_warning ("Couldn't run mobile wizard for CDMA device");
		g_object_unref (info->device);
		g_free (info);
	}
}

static void
gsm_menu_item_activate (GtkMenuItem *item, gpointer user_data)
{
	GSMMenuItemInfo *info = (GSMMenuItemInfo *) user_data;

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
	GSMMenuItemInfo *info;

	info = g_slice_new0 (GSMMenuItemInfo);
	info->applet = applet;
	info->device = g_object_ref (G_OBJECT (device));
	info->connection = connection ? g_object_ref (connection) : NULL;

	g_signal_connect_data (item, "activate",
	                       G_CALLBACK (gsm_menu_item_activate),
	                       info,
	                       (GClosureNotify) gsm_menu_item_info_destroy, 0);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static guint32
gsm_state_to_mb_state (GsmDeviceInfo *info)
{
	if (!info->modem_enabled)
		return MB_STATE_UNKNOWN;

	switch (info->reg_state) {
	case 1:  /* IDLE */
		return MB_STATE_IDLE;
	case 2:  /* HOME */
		return MB_STATE_HOME;
	case 3:  /* SEARCHING */
		return MB_STATE_SEARCHING;
	case 4:  /* DENIED */
		return MB_STATE_DENIED;
	case 6:  /* ROAMING */
		return MB_STATE_ROAMING;
	case 5:  /* UNKNOWN */
	default:
		break;
	}

	return MB_STATE_UNKNOWN;
}

static guint32
gsm_act_to_mb_act (GsmDeviceInfo *info)
{
	switch (info->act) {
	case MM_MODEM_GSM_ACCESS_TECH_GPRS:
		return MB_TECH_GPRS;
	case MM_MODEM_GSM_ACCESS_TECH_EDGE:
		return MB_TECH_EDGE;
	case MM_MODEM_GSM_ACCESS_TECH_UMTS:
		return MB_TECH_UMTS;
	case MM_MODEM_GSM_ACCESS_TECH_HSDPA:
		return MB_TECH_HSDPA;
	case MM_MODEM_GSM_ACCESS_TECH_HSUPA:
		return MB_TECH_HSUPA;
	case MM_MODEM_GSM_ACCESS_TECH_HSPA:
		return MB_TECH_HSPA;
	case MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS:
		return MB_TECH_HSPA_PLUS;
	case MM_MODEM_GSM_ACCESS_TECH_LTE:
		return MB_TECH_LTE;
	default:
		break;
	}

	return MB_TECH_GSM;
}

static void
gsm_add_menu_item (NMDevice *device,
                   gboolean multiple_devices,
                   GSList *connections,
                   NMConnection *active,
                   GtkWidget *menu,
                   NMApplet *applet)
{
	GsmDeviceInfo *info;
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
		                            info->op_name,
		                            TRUE,
		                            gsm_act_to_mb_act (info),
		                            gsm_state_to_mb_state (info),
		                            info->modem_enabled,
		                            applet);
		gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
		add_connection_item (device, active, item, menu, applet);
	}

	/* Notify user of unmanaged or unavailable device */
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
		                            info->op_name,
		                            FALSE,
		                            gsm_act_to_mb_act (info),
		                            gsm_state_to_mb_state (info),
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
			item = gtk_check_menu_item_new_with_label (_("New Mobile Broadband (GSM) connection..."));
			add_connection_item (device, NULL, item, menu, applet);
		}
	}
}

static void
gsm_device_state_changed (NMDevice *device,
                          NMDeviceState new_state,
                          NMDeviceState old_state,
                          NMDeviceStateReason reason,
                          NMApplet *applet)
{
	GsmDeviceInfo *info;

	/* Start/stop polling of quality and registration when device state changes */
	info = g_object_get_data (G_OBJECT (device), "devinfo");
	check_start_polling (info);
}

static void
gsm_notify_connected (NMDevice *device,
                      const char *msg,
                      NMApplet *applet)
{
	applet_do_notify_with_pref (applet,
	                            _("Connection Established"),
	                            msg ? msg : _("You are now connected to the GSM network."),
	                            "nm-device-wwan",
	                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
}

static void
gsm_get_icon (NMDevice *device,
              NMDeviceState state,
              NMConnection *connection,
              GdkPixbuf **out_pixbuf,
              const char **out_icon_name,
              char **tip,
              NMApplet *applet)
{
	GsmDeviceInfo *info;

	info = g_object_get_data (G_OBJECT (device), "devinfo");
	g_assert (info);

	mobile_helper_get_icon (device,
	                        state,
	                        connection,
	                        out_pixbuf,
	                        out_icon_name,
	                        tip,
	                        applet,
	                        gsm_state_to_mb_state (info),
	                        gsm_act_to_mb_act (info),
	                        info->quality,
	                        info->quality_valid);
}

static gboolean
gsm_get_secrets (SecretsRequest *req, GError **error)
{
	NMDevice *device;
	GsmDeviceInfo *devinfo;

	if (!mobile_helper_get_secrets (NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS, req, error))
		return FALSE;

	device = applet_get_device_for_connection (req->applet, req->connection);
	if (!device) {
		g_set_error (error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_INTERNAL_ERROR,
		             "%s.%d (%s): failed to find device for active connection.",
		             __FILE__, __LINE__, __func__);
		return FALSE;
	}

	devinfo = g_object_get_data (G_OBJECT (device), "devinfo");
	g_assert (devinfo);

	/* A GetSecrets PIN dialog overrides the initial unlock dialog */
	if (devinfo->dialog)
		unlock_dialog_destroy (devinfo);

	return TRUE;
}

/********************************************************************/

static void
unlock_dialog_destroy (GsmDeviceInfo *info)
{
	gtk_widget_destroy (info->dialog);
	info->dialog = NULL;
}

static void
unlock_pin_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GError *error = NULL;
	const char *dbus_error, *msg = NULL, *code1;

	if (dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID)) {
		if (applet_mobile_pin_dialog_get_auto_unlock (info->dialog)) {
			code1 = applet_mobile_pin_dialog_get_entry1 (info->dialog);
			mobile_helper_save_pin_in_keyring (info->devid, info->simid, code1);
		} else
			mobile_helper_delete_pin_in_keyring (info->devid);
		unlock_dialog_destroy (info);
		return;
	}

	dbus_error = dbus_g_error_get_name (error);
	if (dbus_error && !strcmp (dbus_error, "org.freedesktop.ModemManager.Modem.Gsm.IncorrectPassword"))
		msg = _("Wrong PIN code; please contact your provider.");
	else
		msg = error ? error->message : NULL;

	applet_mobile_pin_dialog_stop_spinner (info->dialog, msg);
	g_warning ("%s: error unlocking with PIN: %s", __func__, error ? error->message : "unknown");
	g_clear_error (&error);
}

static void
unlock_puk_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GError *error = NULL;
	const char *dbus_error, *msg = NULL;

	if (dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID)) {
		unlock_dialog_destroy (info);
		return;
	}

	dbus_error = dbus_g_error_get_name (error);
	if (dbus_error && !strcmp (dbus_error, "org.freedesktop.ModemManager.Modem.Gsm.IncorrectPassword"))
		msg = _("Wrong PUK code; please contact your provider.");
	else
		msg = error ? error->message : NULL;

	applet_mobile_pin_dialog_stop_spinner (info->dialog, msg);
	g_warning ("%s: error unlocking with PUK: %s", __func__, error ? error->message : "unknown");
	g_clear_error (&error);
}

#define UNLOCK_CODE_PIN 1
#define UNLOCK_CODE_PUK 2

static void
unlock_dialog_response (GtkDialog *dialog,
                        gint response,
                        gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	const char *code1, *code2;
	guint32 unlock_code;

	if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT) {
		unlock_dialog_destroy (info);
		return;
	}

	/* Start the spinner to show the progress of the unlock */
	applet_mobile_pin_dialog_start_spinner (info->dialog, _("Sending unlock code..."));

	unlock_code = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (info->dialog), "unlock-code"));
	if (!unlock_code) {
		g_warn_if_fail (unlock_code != 0);
		unlock_dialog_destroy (info);
		return;
	}

	code1 = applet_mobile_pin_dialog_get_entry1 (info->dialog);
	if (!code1 || !strlen (code1)) {
		g_warn_if_fail (code1 != NULL && strlen (code1));
		unlock_dialog_destroy (info);
		return;
	}

	/* Send the code to ModemManager */
	if (unlock_code == UNLOCK_CODE_PIN) {
		dbus_g_proxy_begin_call_with_timeout (info->card_proxy, "SendPin",
		                                      unlock_pin_reply, info, NULL,
		                                      15000,  /* 15 seconds */
		                                      G_TYPE_STRING, code1,
		                                      G_TYPE_INVALID);
	} else if (unlock_code == UNLOCK_CODE_PUK) {
		code2 = applet_mobile_pin_dialog_get_entry2 (info->dialog);
		if (!code2) {
			g_warn_if_fail (code2 != NULL);
			unlock_dialog_destroy (info);
			return;
		}

		dbus_g_proxy_begin_call_with_timeout (info->card_proxy, "SendPuk",
		                                      unlock_puk_reply, info, NULL,
		                                      15000,  /* 15 seconds */
		                                      G_TYPE_STRING, code1,
		                                      G_TYPE_STRING, code2,
		                                      G_TYPE_INVALID);
	}
}

static void
unlock_dialog_new (NMDevice *device, GsmDeviceInfo *info)
{
	g_return_if_fail (info->unlock_required != NULL);
	g_return_if_fail (!strcmp (info->unlock_required, "sim-pin") || !strcmp (info->unlock_required, "sim-puk"));

	if (info->dialog)
		return;

	info->dialog = applet_mobile_pin_dialog_new (info->unlock_required,
	                                             nma_utils_get_device_description (device));

	if (!strcmp (info->unlock_required, "sim-pin"))
		g_object_set_data (G_OBJECT (info->dialog), "unlock-code", GUINT_TO_POINTER (UNLOCK_CODE_PIN));
	else if (!strcmp (info->unlock_required, "sim-puk"))
		g_object_set_data (G_OBJECT (info->dialog), "unlock-code", GUINT_TO_POINTER (UNLOCK_CODE_PUK));
	else
		g_assert_not_reached ();

	g_signal_connect (info->dialog, "response", G_CALLBACK (unlock_dialog_response), info);

	/* Need to resize the dialog after hiding widgets */
	gtk_window_resize (GTK_WINDOW (info->dialog), 400, 100);

	/* Show the dialog */
	gtk_widget_realize (info->dialog);
	gtk_window_present (GTK_WINDOW (info->dialog));
}

/********************************************************************/

static void
gsm_device_info_free (gpointer data)
{
	GsmDeviceInfo *info = data;

	if (info->props_proxy)
		g_object_unref (info->props_proxy);
	if (info->card_proxy)
		g_object_unref (info->card_proxy);
	if (info->net_proxy)
		g_object_unref (info->net_proxy);
	if (info->bus)
		dbus_g_connection_unref (info->bus);

	if (info->mobile_providers_database)
		g_object_unref (info->mobile_providers_database);

	if (info->poll_id)
		g_source_remove (info->poll_id);

	if (info->dialog)
		unlock_dialog_destroy (info);

	g_object_unref (info->cancellable);

	g_free (info->devid);
	g_free (info->simid);
	g_free (info->op_code);
	g_free (info->op_name);
	memset (info, 0, sizeof (GsmDeviceInfo));
	g_free (info);
}

static void
signal_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
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

static void
notify_user_of_gsm_reg_change (GsmDeviceInfo *info)
{
	guint32 mb_state = gsm_state_to_mb_state (info);

	if (mb_state == MB_STATE_HOME) {
		applet_do_notify_with_pref (info->applet,
		                            _("GSM network."),
		                            _("You are now registered on the home network."),
		                            "nm-signal-100",
		                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
	} else if (mb_state == MB_STATE_ROAMING) {
		applet_do_notify_with_pref (info->applet,
		                            _("GSM network."),
		                            _("You are now registered on a roaming network."),
		                            "nm-signal-100",
		                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
	}
}

#define REG_INFO_TYPE (dbus_g_type_get_struct ("GValueArray", G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID))
#define DBUS_TYPE_G_MAP_OF_VARIANT (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

static void
reg_info_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GError *error = NULL;
	GValueArray *array = NULL;
	guint32 new_state = 0;
	char *new_op_code = NULL;
	char *new_op_name = NULL;
	GValue *value;

	if (dbus_g_proxy_end_call (proxy, call, &error, REG_INFO_TYPE, &array, G_TYPE_INVALID)) {
		if (array->n_values == 3) {
			value = g_value_array_get_nth (array, 0);
			if (G_VALUE_HOLDS_UINT (value))
				new_state = g_value_get_uint (value) + 1;

			value = g_value_array_get_nth (array, 1);
			if (G_VALUE_HOLDS_STRING (value)) {
				new_op_code = g_value_dup_string (value);
				if (new_op_code && !strlen (new_op_code)) {
					g_free (new_op_code);
					new_op_code = NULL;
				}
			}

			value = g_value_array_get_nth (array, 2);
			if (G_VALUE_HOLDS_STRING (value))
				new_op_name = mobile_helper_parse_3gpp_operator_name (&(info->mobile_providers_database),
				                                                      g_value_get_string (value),
				                                                      new_op_code);
		}

		g_value_array_free (array);
	}

	if (info->reg_state != new_state) {
		info->reg_state = new_state;
		notify_user_of_gsm_reg_change (info);
	}

	g_free (info->op_code);
	info->op_code = new_op_code;
	g_free (info->op_name);
	info->op_name = new_op_name;

	g_clear_error (&error);
}

static void
enabled_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
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

static char *
parse_unlock_required (GValue *value)
{
	const char *new_val;

	/* Empty string means NULL */
	new_val = g_value_get_string (value);
	if (new_val && strlen (new_val)) {
		/* PIN2/PUK2 only required for various dialing things that we don't care
		 * about; it doesn't inhibit normal operation.
		 */
		if (strcmp (new_val, "sim-puk2") && strcmp (new_val, "sim-pin2"))
			return g_strdup (new_val);
	}

	return NULL;
}

static void
keyring_unlock_pin_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GError *error = NULL;

	if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID)) {
		g_warning ("Failed to auto-unlock devid:%s simid:%s : (%s) %s",
		           info->devid ? info->devid : "(unknown)",
		           info->simid ? info->simid : "(unknown)",
		           dbus_g_error_get_name (error),
		           error->message);
		/* Ask the user */
		unlock_dialog_new (info->device, info);
		g_clear_error (&error);
	}
}

static void
keyring_pin_check_cb (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GList *iter;
	GList *list;
	SecretItem *item;
	GError *error = NULL;
	SecretValue *pin = NULL;
	GHashTable *attributes;
	const gchar *simid;

	list = secret_service_search_finish (NULL, result, &error);

	if (!list) {
		/* No saved PIN, just ask the user */
		unlock_dialog_new (info->device, info);
		g_clear_error (&error);
		return;
	}

	/* Look for a result with a matching "simid" attribute since that's
	 * better than just using a matching "devid".  The PIN is really tied
	 * to the SIM, not the modem itself.
	 */
	for (iter = list;
	     info->simid && (pin == NULL) && iter;
	     iter = g_list_next (iter)) {
		item = iter->data;

		/* Look for a matching "simid" attribute */
		attributes = secret_item_get_attributes (item);
		simid = g_hash_table_lookup (attributes, "simid");
		if (g_strcmp0 (simid, info->simid))
			pin = secret_item_get_secret (item);
		else
			pin = NULL;
		g_hash_table_unref (attributes);

		if (pin != NULL)
			break;
	}

	if (pin == NULL) {
		/* Fall back to the first result's PIN */
		pin = secret_item_get_secret (list->data);
		if (pin == NULL) {
			unlock_dialog_new (info->device, info);
			return;
		}
	}

	/* Send the PIN code to ModemManager */
	if (!dbus_g_proxy_begin_call_with_timeout (info->card_proxy, "SendPin",
	                                           keyring_unlock_pin_reply, info, NULL,
	                                           15000,  /* 15 seconds */
	                                           G_TYPE_STRING, secret_value_get (pin, NULL),
	                                           G_TYPE_INVALID)) {
		g_warning ("Failed to auto-unlock devid:%s simid:%s",
		           info->devid ? info->devid : "(unknown)",
		           info->simid ? info->simid : "(unknown)");
	}

	secret_value_unref (pin);
}

static void
simid_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GError *error = NULL;
	GValue value = { 0 };
	GHashTable *attrs;

	if (dbus_g_proxy_end_call (proxy, call, &error,
	                           G_TYPE_VALUE, &value,
	                           G_TYPE_INVALID)) {
		if (G_VALUE_HOLDS_STRING (&value)) {
			g_free (info->simid);
			info->simid = g_value_dup_string (&value);
		}
		g_value_unset (&value);
	}
	g_clear_error (&error);

	/* Procure unlock code and apply it if an unlock is now required. */
	if (info->unlock_required) {
		/* If we have a device ID ask the keyring for any saved SIM-PIN codes */
		if (info->devid && (g_strcmp0 (info->unlock_required, "sim-pin") == 0)) {
			attrs = secret_attributes_build (&mobile_secret_schema, "devid", info->devid, NULL);
			secret_service_search (NULL, &mobile_secret_schema, attrs,
			                       SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
			                       info->cancellable, keyring_pin_check_cb, info);
			g_hash_table_unref (attrs);
		} else {
			/* Couldn't get a device ID, but unlock required; present dialog */
			unlock_dialog_new (info->device, info);
		}
	}

	check_start_polling (info);
}

#define MM_OLD_DBUS_INTERFACE_MODEM_GSM_CARD "org.freedesktop.ModemManager.Modem.Gsm.Card"

static void
unlock_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GError *error = NULL;
	GHashTable *props = NULL;

	if (dbus_g_proxy_end_call (proxy, call, &error,
	                           DBUS_TYPE_G_MAP_OF_VARIANT, &props,
	                           G_TYPE_INVALID)) {
		GHashTableIter iter;
		const char *prop_name;
		GValue *value;

		g_hash_table_iter_init (&iter, props);
		while (g_hash_table_iter_next (&iter, (gpointer) &prop_name, (gpointer) &value)) {
			if ((strcmp (prop_name, "UnlockRequired") == 0) && G_VALUE_HOLDS_STRING (value)) {
				g_free (info->unlock_required);
				info->unlock_required = parse_unlock_required (value);
			}

			if ((strcmp (prop_name, "DeviceIdentifier") == 0) && G_VALUE_HOLDS_STRING (value)) {
				g_free (info->devid);
				info->devid = g_value_dup_string (value);
			}
		}
		g_hash_table_destroy (props);

		/* Get SIM card identifier */
		dbus_g_proxy_begin_call (info->props_proxy, "Get",
		                         simid_reply, info, NULL,
		                         G_TYPE_STRING, MM_OLD_DBUS_INTERFACE_MODEM_GSM_CARD,
		                         G_TYPE_STRING, "SimIdentifier",
		                         G_TYPE_INVALID);
	}

	g_clear_error (&error);
	check_start_polling (info);
}

static void
access_tech_reply (DBusGProxy *proxy, DBusGProxyCall *call, gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GError *error = NULL;
	GValue value = { 0 };

	if (dbus_g_proxy_end_call (proxy, call, &error,
	                           G_TYPE_VALUE, &value,
	                           G_TYPE_INVALID)) {
		if (G_VALUE_HOLDS_UINT (&value)) {
			info->act = g_value_get_uint (&value);
			applet_schedule_update_icon (info->applet);
		}
		g_value_unset (&value);
	}
	g_clear_error (&error);
}

static gboolean
gsm_poll_cb (gpointer user_data)
{
	GsmDeviceInfo *info = user_data;

	/* MM might have just sent an unsolicited update, in which case we just
	 * skip this poll and wait till the next one.
	 */

	if (!info->skip_reg_poll) {
		dbus_g_proxy_begin_call (info->net_proxy, "GetRegistrationInfo",
		                         reg_info_reply, info, NULL,
		                         G_TYPE_INVALID);
		info->skip_reg_poll = FALSE;
	}

	if (!info->skip_signal_poll) {
		dbus_g_proxy_begin_call (info->net_proxy, "GetSignalQuality",
		                         signal_reply, info, NULL,
		                         G_TYPE_INVALID);
		info->skip_signal_poll = FALSE;
	}

	return TRUE;  /* keep running until we're told to stop */
}

static void
check_start_polling (GsmDeviceInfo *info)
{
	NMDeviceState state;
	gboolean poll = TRUE;

	g_return_if_fail (info != NULL);

	/* Don't poll if any of the following are true:
	 *
	 * 1) NM says the device is not available
	 * 2) the modem requires an unlock code
	 * 3) the modem isn't enabled
	 */

	state = nm_device_get_state (info->device);
	if (   (state <= NM_DEVICE_STATE_UNAVAILABLE)
	    || info->unlock_required
	    || (info->modem_enabled == FALSE))
		poll = FALSE;

	if (poll) {
		if (!info->poll_id) {
			/* 33 seconds to be just a bit more than MM's poll interval, so
			 * that if we get an unsolicited update from MM between polls we'll
			 * skip the next poll.
			 */
			info->poll_id = g_timeout_add_seconds (33, gsm_poll_cb, info);
		}
		gsm_poll_cb (info);
	} else {
		if (info->poll_id)
			g_source_remove (info->poll_id);
		info->poll_id = 0;
		info->skip_reg_poll = FALSE;
		info->skip_signal_poll = FALSE;
	}
}

static void
reg_info_changed_cb (DBusGProxy *proxy,
                     guint32 reg_state,
                     const char *op_code,
                     const char *op_name,
                     gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	guint32 new_state = reg_state + 1;

	if (info->reg_state != new_state) {
		info->reg_state = new_state;
		notify_user_of_gsm_reg_change (info);
	}

	g_free (info->op_code);
	info->op_code = strlen (op_code) ? g_strdup (op_code) : NULL;
	g_free (info->op_name);
	info->op_name = mobile_helper_parse_3gpp_operator_name (&(info->mobile_providers_database),
	                                                        op_name,
	                                                        info->op_code);
	info->skip_reg_poll = TRUE;
}

static void
signal_quality_changed_cb (DBusGProxy *proxy,
                           guint32 quality,
                           gpointer user_data)
{
	GsmDeviceInfo *info = user_data;

	info->quality = quality;
	info->quality_valid = TRUE;
	info->skip_signal_poll = TRUE;

	applet_schedule_update_icon (info->applet);
}

#define MM_OLD_DBUS_INTERFACE_MODEM "org.freedesktop.ModemManager.Modem"
#define MM_OLD_DBUS_INTERFACE_MODEM_GSM_NETWORK "org.freedesktop.ModemManager.Modem.Gsm.Network"

static void
modem_properties_changed (DBusGProxy *proxy,
                          const char *interface,
                          GHashTable *props,
                          gpointer user_data)
{
	GsmDeviceInfo *info = user_data;
	GValue *value;

	if (!strcmp (interface, MM_OLD_DBUS_INTERFACE_MODEM)) {
		value = g_hash_table_lookup (props, "UnlockRequired");
		if (value && G_VALUE_HOLDS_STRING (value)) {
			g_free (info->unlock_required);
			info->unlock_required = parse_unlock_required (value);
			check_start_polling (info);
		}

		value = g_hash_table_lookup (props, "Enabled");
		if (value && G_VALUE_HOLDS_BOOLEAN (value)) {
			info->modem_enabled = g_value_get_boolean (value);
			if (!info->modem_enabled) {
				info->quality = 0;
				info->quality_valid = 0;
				info->reg_state = 0;
				info->act = 0;
				g_free (info->op_code);
				info->op_code = NULL;
				g_free (info->op_name);
				info->op_name = NULL;
			}
			check_start_polling (info);
		}
	} else if (!strcmp (interface, MM_OLD_DBUS_INTERFACE_MODEM_GSM_NETWORK)) {
		value = g_hash_table_lookup (props, "AccessTechnology");
		if (value && G_VALUE_HOLDS_UINT (value)) {
			info->act = g_value_get_uint (value);
			applet_schedule_update_icon (info->applet);
		}
	}
}

static void
gsm_device_added (NMDevice *device, NMApplet *applet)
{
	NMDeviceModem *modem = NM_DEVICE_MODEM (device);
	GsmDeviceInfo *info;
	const char *udi;
	DBusGConnection *bus;
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

	info = g_malloc0 (sizeof (GsmDeviceInfo));
	info->applet = applet;
	info->device = device;
	info->bus = bus;
	info->cancellable = g_cancellable_new ();

	info->props_proxy = dbus_g_proxy_new_for_name (info->bus,
	                                               "org.freedesktop.ModemManager",
	                                               udi,
	                                               "org.freedesktop.DBus.Properties");
	if (!info->props_proxy) {
		g_message ("%s: failed to create D-Bus properties proxy.", __func__);
		gsm_device_info_free (info);
		return;
	}

	info->card_proxy = dbus_g_proxy_new_for_name (info->bus,
	                                              "org.freedesktop.ModemManager",
	                                              udi,
	                                              "org.freedesktop.ModemManager.Modem.Gsm.Card");
	if (!info->card_proxy) {
		g_message ("%s: failed to create GSM Card proxy.", __func__);
		gsm_device_info_free (info);
		return;
	}

	info->net_proxy = dbus_g_proxy_new_for_name (info->bus,
	                                             "org.freedesktop.ModemManager",
	                                             udi,
	                                             MM_OLD_DBUS_INTERFACE_MODEM_GSM_NETWORK);
	if (!info->net_proxy) {
		g_message ("%s: failed to create GSM Network proxy.", __func__);
		gsm_device_info_free (info);
		return;
	}

	g_object_set_data_full (G_OBJECT (modem), "devinfo", info, gsm_device_info_free);

	/* Registration info signal */
	dbus_g_object_register_marshaller (_nma_marshal_VOID__UINT_STRING_STRING,
	                                   G_TYPE_NONE,
	                                   G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (info->net_proxy, "RegistrationInfo",
	                         G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (info->net_proxy, "RegistrationInfo",
	                             G_CALLBACK (reg_info_changed_cb), info, NULL);

	/* Signal quality change signal */
	dbus_g_object_register_marshaller (g_cclosure_marshal_VOID__UINT,
	                                   G_TYPE_NONE, G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (info->net_proxy, "SignalQuality", G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (info->net_proxy, "SignalQuality",
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

	/* Ask whether the device needs to be unlocked */
	dbus_g_proxy_begin_call (info->props_proxy, "GetAll",
	                         unlock_reply, info, NULL,
	                         G_TYPE_STRING, MM_OLD_DBUS_INTERFACE_MODEM,
	                         G_TYPE_INVALID);

	/* Ask whether the device is enabled */
	dbus_g_proxy_begin_call (info->props_proxy, "Get",
	                         enabled_reply, info, NULL,
	                         G_TYPE_STRING, MM_OLD_DBUS_INTERFACE_MODEM,
	                         G_TYPE_STRING, "Enabled",
	                         G_TYPE_INVALID);

	dbus_g_proxy_begin_call (info->props_proxy, "Get",
	                         access_tech_reply, info, NULL,
	                         G_TYPE_STRING, MM_OLD_DBUS_INTERFACE_MODEM_GSM_NETWORK,
	                         G_TYPE_STRING, "AccessTechnology",
	                         G_TYPE_INVALID);
}

NMADeviceClass *
applet_device_gsm_get_class (NMApplet *applet)
{
	NMADeviceClass *dclass;

	dclass = g_slice_new0 (NMADeviceClass);
	if (!dclass)
		return NULL;

	dclass->new_auto_connection = gsm_new_auto_connection;
	dclass->add_menu_item = gsm_add_menu_item;
	dclass->device_state_changed = gsm_device_state_changed;
	dclass->notify_connected = gsm_notify_connected;
	dclass->get_icon = gsm_get_icon;
	dclass->get_secrets = gsm_get_secrets;
	dclass->secrets_request_size = sizeof (MobileHelperSecretsInfo);
	dclass->device_added = gsm_device_added;

	return dclass;
}
