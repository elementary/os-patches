/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
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
 * (C) Copyright 2012 Aleksander Morgado <aleksander@gnu.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <nm-device.h>
#include <nm-device-modem.h>
#include <nm-utils.h>

#include "applet.h"
#include "applet-device-broadband.h"
#include "applet-dialogs.h"
#include "mobile-helpers.h"
#include "nm-ui-utils.h"
#include "mb-menu-item.h"

typedef struct {
	NMApplet *applet;
	NMDevice *device;

	MMObject     *mm_object;
	MMModem      *mm_modem;
	MMModem3gpp  *mm_modem_3gpp;
	MMModemCdma  *mm_modem_cdma;
	MMSim        *mm_sim;

	/* Operator info */
	gchar *operator_name;
	guint operator_name_update_id;
	guint operator_code_update_id;
	guint sid_update_id;
	NMAMobileProvidersDatabase *mpd;

	/* Unlock dialog stuff */
	GtkWidget *dialog;
	GCancellable *cancellable;
} BroadbandDeviceInfo;

/********************************************************************/

static gboolean
new_auto_connection (NMDevice *device,
                     gpointer dclass_data,
                     AppletNewAutoConnectionCallback callback,
                     gpointer callback_data)
{
	return mobile_helper_wizard (nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device)),
	                             callback,
	                             callback_data);
}

/********************************************************************/

typedef struct {
	NMApplet *applet;
	NMDevice *device;
} ConnectNetworkInfo;

static void
add_and_activate_connection_done (NMClient *client,
                                  NMActiveConnection *active,
                                  const char *connection_path,
                                  GError *error,
                                  gpointer user_data)
{
	if (error)
		g_warning ("Failed to add/activate connection: (%d) %s", error->code, error->message);
}

static void
wizard_done (NMConnection *connection,
             gboolean auto_created,
             gboolean canceled,
             gpointer user_data)
{
	ConnectNetworkInfo *info = user_data;

	if (canceled == FALSE) {
		g_return_if_fail (connection != NULL);

		/* Ask NM to add the new connection and activate it; NM will fill in the
		 * missing details based on the specific object and the device.
		 */
		nm_client_add_and_activate_connection (info->applet->nm_client,
		                                       connection,
		                                       info->device,
		                                       "/",
		                                       add_and_activate_connection_done,
		                                       info->applet);
	}

	g_object_unref (info->device);
	g_free (info);
}

void
applet_broadband_connect_network (NMApplet *applet,
                                  NMDevice *device)
{
	ConnectNetworkInfo *info;

	info = g_malloc0 (sizeof (*info));
	info->applet = applet;
	info->device = g_object_ref (device);

	if (!mobile_helper_wizard (nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device)),
	                           wizard_done,
	                           info)) {
		g_warning ("Couldn't run mobile wizard for broadband device");
		g_object_unref (info->device);
		g_free (info);
	}
}

/********************************************************************/

static void unlock_dialog_new (NMDevice *device,
                               BroadbandDeviceInfo *info);

static void
unlock_dialog_destroy (BroadbandDeviceInfo *info)
{
	gtk_widget_destroy (info->dialog);
	info->dialog = NULL;
}

static void
dialog_sim_send_puk_ready (MMSim *sim,
                           GAsyncResult *res,
                           BroadbandDeviceInfo *info)
{
	GError *error = NULL;

	if (!mm_sim_send_puk_finish (sim, res, &error)) {
		const gchar *msg;

		if (g_error_matches (error,
		                     MM_MOBILE_EQUIPMENT_ERROR,
		                     MM_MOBILE_EQUIPMENT_ERROR_INCORRECT_PASSWORD))
			msg = _("Wrong PUK code; please contact your provider.");
		else {
			g_dbus_error_strip_remote_error (error);
			msg = error->message;
		}

		applet_mobile_pin_dialog_stop_spinner (info->dialog, msg);

		g_warning ("Failed to send PUK to devid: '%s' simid: '%s' : %s",
		           mm_modem_get_device_identifier (info->mm_modem),
		           mm_sim_get_identifier (info->mm_sim),
		           error->message);

		g_error_free (error);
		return;
	}

	/* Good */
	unlock_dialog_destroy (info);
}

static void
dialog_sim_send_pin_ready (MMSim *sim,
                           GAsyncResult *res,
                           BroadbandDeviceInfo *info)
{
	GError *error = NULL;

	if (!mm_sim_send_pin_finish (sim, res, &error)) {
		if (g_error_matches (error,
		                     MM_MOBILE_EQUIPMENT_ERROR,
		                     MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK)) {
			/* Destroy previous dialog and launch a new one rebuilt to ask for PUK */
			unlock_dialog_destroy (info);
			unlock_dialog_new (info->device, info);
		} else {
			const gchar *msg = NULL;

			/* Report error and re-try PIN request */
			if (g_error_matches (error,
			                     MM_MOBILE_EQUIPMENT_ERROR,
			                     MM_MOBILE_EQUIPMENT_ERROR_INCORRECT_PASSWORD))
				msg = _("Wrong PIN code; please contact your provider.");
			else {
				g_dbus_error_strip_remote_error (error);
				msg = error->message;
			}

			applet_mobile_pin_dialog_stop_spinner (info->dialog, msg);
		}

		g_warning ("Failed to send PIN to devid: '%s' simid: '%s' : %s",
		           mm_modem_get_device_identifier (info->mm_modem),
		           mm_sim_get_identifier (info->mm_sim),
		           error->message);

		g_error_free (error);
		return;
	}

	/* Good */

	if (applet_mobile_pin_dialog_get_auto_unlock (info->dialog)) {
		const gchar *code1;

		code1 = applet_mobile_pin_dialog_get_entry1 (info->dialog);
		mobile_helper_save_pin_in_keyring (mm_modem_get_device_identifier (info->mm_modem),
		                                   mm_sim_get_identifier (info->mm_sim),
		                                   code1);
	} else
		mobile_helper_delete_pin_in_keyring (mm_modem_get_device_identifier (info->mm_modem));

	unlock_dialog_destroy (info);
}

static void
unlock_dialog_response (GtkDialog *dialog,
                        gint response,
                        gpointer user_data)
{
	BroadbandDeviceInfo *info = user_data;
	const char *code1, *code2;
	MMModemLock lock;

	if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT) {
		unlock_dialog_destroy (info);
		return;
	}

	lock = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (info->dialog), "unlock-code"));
	g_assert (lock == MM_MODEM_LOCK_SIM_PIN || lock == MM_MODEM_LOCK_SIM_PUK);

	/* Start the spinner to show the progress of the unlock */
	applet_mobile_pin_dialog_start_spinner (info->dialog, _("Sending unlock code..."));

	code1 = applet_mobile_pin_dialog_get_entry1 (info->dialog);
	if (!code1 || !strlen (code1)) {
		g_warn_if_reached ();
		unlock_dialog_destroy (info);
		return;
	}

	/* Send the code to ModemManager */
	if (lock == MM_MODEM_LOCK_SIM_PIN) {
		mm_sim_send_pin (info->mm_sim,
		                 code1,
		                 NULL, /* cancellable */
		                 (GAsyncReadyCallback)dialog_sim_send_pin_ready,
		                 info);
		return;
	}

	if (lock == MM_MODEM_LOCK_SIM_PUK) {
		code2 = applet_mobile_pin_dialog_get_entry2 (info->dialog);
		if (!code2) {
			g_warn_if_reached ();
			unlock_dialog_destroy (info);
			return;
		}

		mm_sim_send_puk (info->mm_sim,
		                 code1, /* puk */
		                 code2, /* new pin */
		                 NULL, /* cancellable */
		                 (GAsyncReadyCallback)dialog_sim_send_puk_ready,
		                 info);
		return;
	}

	g_assert_not_reached ();
}

static void
unlock_dialog_new (NMDevice *device,
                   BroadbandDeviceInfo *info)
{
	MMModemLock lock;
	const gchar *unlock_required;

	if (info->dialog)
		return;

	/* We can only unlock PIN or PUK here */
	lock = mm_modem_get_unlock_required (info->mm_modem);
	if (lock == MM_MODEM_LOCK_SIM_PIN)
		unlock_required = "sim-pin";
	else if (lock == MM_MODEM_LOCK_SIM_PUK)
		unlock_required = "sim-puk";
	else {
		g_warning ("Cannot unlock devid: '%s' simid: '%s' : unhandled lock code '%s'",
		           mm_modem_get_device_identifier (info->mm_modem),
		           mm_sim_get_identifier (info->mm_sim),
		           mm_modem_lock_get_string (lock));
		return;
	}

	info->dialog = applet_mobile_pin_dialog_new (unlock_required,
	                                             nma_utils_get_device_description (device));

	g_object_set_data (G_OBJECT (info->dialog), "unlock-code", GUINT_TO_POINTER (lock));
	g_signal_connect (info->dialog, "response", G_CALLBACK (unlock_dialog_response), info);

	/* Need to resize the dialog after hiding widgets */
	gtk_window_resize (GTK_WINDOW (info->dialog), 400, 100);

	/* Show the dialog */
	gtk_widget_realize (info->dialog);
	gtk_window_present (GTK_WINDOW (info->dialog));
}

static void
autounlock_sim_send_pin_ready (MMSim *sim,
                               GAsyncResult *res,
                               BroadbandDeviceInfo *info)
{
	GError *error = NULL;

	if (!mm_sim_send_pin_finish (sim, res, &error)) {
		g_warning ("Failed to auto-unlock devid: '%s' simid: '%s' : %s",
		           mm_modem_get_device_identifier (info->mm_modem),
		           mm_sim_get_identifier (info->mm_sim),
		           error->message);
		g_error_free (error);

		/* Remove PIN from keyring right away */
		mobile_helper_delete_pin_in_keyring (mm_modem_get_device_identifier (info->mm_modem));

		/* Ask the user */
		unlock_dialog_new (info->device, info);
	}
}

static void
keyring_pin_check_cb (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	BroadbandDeviceInfo *info = user_data;
	GList *iter;
	GList *list;
	SecretItem *item;
	SecretValue *pin = NULL;
	GHashTable *attributes;
	GError *error = NULL;
	const char *simid;

	list = secret_service_search_finish (NULL, result, &error);

	if (error != NULL) {
		/* No saved PIN, just ask the user */
		unlock_dialog_new (info->device, info);
		g_error_free (error);
		return;
	}

	/* Look for a result with a matching "simid" attribute since that's
	 * better than just using a matching "devid".  The PIN is really tied
	 * to the SIM, not the modem itself.
	 */
	simid = mm_sim_get_identifier (info->mm_sim);
	if (simid) {
		for (iter = list;
		     (pin == NULL) && iter;
		     iter = g_list_next (iter)) {
			item = iter->data;

			/* Look for a matching "simid" attribute */
			attributes = secret_item_get_attributes (item);
			if (g_strcmp0 (simid, g_hash_table_lookup (attributes, "simid")))
				pin = secret_item_get_secret (item);
			else
				pin = NULL;
			g_hash_table_unref (attributes);

			if (pin != NULL)
				break;
		}
	}

	if (pin == NULL) {
		/* Fall back to the first result's PIN if we have one */
		if (list)
			pin = secret_item_get_secret (list->data);
		if (pin == NULL) {
			unlock_dialog_new (info->device, info);
			return;
		}
	}

	/* Send the PIN code to ModemManager */
	mm_sim_send_pin (info->mm_sim,
	                 secret_value_get (pin, NULL),
	                 NULL, /* cancellable */
	                 (GAsyncReadyCallback)autounlock_sim_send_pin_ready,
	                 info);
	secret_value_unref (pin);
}

static void
modem_get_sim_ready (MMModem *modem,
                     GAsyncResult *res,
                     BroadbandDeviceInfo *info)
{
	GHashTable *attrs;

	info->mm_sim = mm_modem_get_sim_finish (modem, res, NULL);
	if (!info->mm_sim)
		/* Ok, the modem may not need it actually */
		return;

	/* Do nothing if we're not locked */
	if (mm_modem_get_state (info->mm_modem) != MM_MODEM_STATE_LOCKED)
		return;

	/* If we have a device ID ask the keyring for any saved SIM-PIN codes */
	if (mm_modem_get_device_identifier (info->mm_modem) &&
	    mm_modem_get_unlock_required (info->mm_modem) == MM_MODEM_LOCK_SIM_PIN) {
		attrs = secret_attributes_build (&mobile_secret_schema, "devid",
		                                 mm_modem_get_device_identifier (info->mm_modem),
		                                 NULL);
		secret_service_search (NULL, &mobile_secret_schema, attrs,
		                       SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
		                       info->cancellable, keyring_pin_check_cb, info);
		g_hash_table_unref (attrs);
	} else {
		/* Couldn't get a device ID, but unlock required; present dialog */
		unlock_dialog_new (info->device, info);
	}
}

/********************************************************************/

static gboolean
get_secrets (SecretsRequest *req,
             GError **error)
{
	NMDevice *device;
	BroadbandDeviceInfo *devinfo;

	device = applet_get_device_for_connection (req->applet, req->connection);
	if (!device) {
		g_set_error (error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_INTERNAL_ERROR,
		             "%s.%d (%s): failed to find device for active connection.",
		             __FILE__, __LINE__, __func__);
		return FALSE;
	}

	if (!mobile_helper_get_secrets (nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device)),
	                                req,
	                                error))
		return FALSE;

	devinfo = g_object_get_data (G_OBJECT (device), "devinfo");
	if (!devinfo) {
		g_set_error (error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_INTERNAL_ERROR,
		             "%s.%d (%s): ModemManager is not available for modem at %s",
		             __FILE__, __LINE__, __func__,
		             nm_device_get_udi (device));
		return FALSE;
	}

	/* A GetSecrets PIN dialog overrides the initial unlock dialog */
	if (devinfo->dialog)
		unlock_dialog_destroy (devinfo);

	return TRUE;
}

/********************************************************************/

static guint32
broadband_state_to_mb_state (BroadbandDeviceInfo *info)
{
	MMModemState state;

	state = mm_modem_get_state (info->mm_modem);

	switch (state) {
	case MM_MODEM_STATE_FAILED:
	case MM_MODEM_STATE_UNKNOWN:
	case MM_MODEM_STATE_INITIALIZING:
	case MM_MODEM_STATE_LOCKED:
	case MM_MODEM_STATE_DISABLED:
	case MM_MODEM_STATE_DISABLING:
	case MM_MODEM_STATE_ENABLING:
		return MB_STATE_UNKNOWN;

	case MM_MODEM_STATE_ENABLED:
		return MB_STATE_IDLE;

	case MM_MODEM_STATE_SEARCHING:
		return MB_STATE_SEARCHING;

	case MM_MODEM_STATE_REGISTERED:
	case MM_MODEM_STATE_DISCONNECTING:
	case MM_MODEM_STATE_CONNECTING:
	case MM_MODEM_STATE_CONNECTED:
		break;
	default:
		g_warn_if_reached ();
		return MB_STATE_UNKNOWN;
	}

	/* home or roaming? */

	if (info->mm_modem_3gpp) {
		switch (mm_modem_3gpp_get_registration_state (info->mm_modem_3gpp)) {
		case MM_MODEM_3GPP_REGISTRATION_STATE_HOME:
			return MB_STATE_HOME;
		case MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING:
			return MB_STATE_ROAMING;
		default:
			/* Skip, we may be registered in EVDO/CDMA1x instead... */
			break;
		}
	}

	if (info->mm_modem_cdma) {
		/* EVDO state overrides 1X state for now */
		switch (mm_modem_cdma_get_evdo_registration_state (info->mm_modem_cdma)) {
		case MM_MODEM_CDMA_REGISTRATION_STATE_HOME:
			return MB_STATE_HOME;
		case MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING:
			return MB_STATE_ROAMING;
		case MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED:
			/* Assume home... */
			return MB_STATE_HOME;
		default:
			/* Skip, we may be registered in CDMA1x instead... */
			break;
		}

		switch (mm_modem_cdma_get_cdma1x_registration_state (info->mm_modem_cdma)) {
		case MM_MODEM_CDMA_REGISTRATION_STATE_HOME:
			return MB_STATE_HOME;
		case MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING:
			return MB_STATE_ROAMING;
		case MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED:
			/* Assume home... */
			return MB_STATE_HOME;
		default:
			break;
		}
	}

	return MB_STATE_UNKNOWN;
}

static guint32
broadband_act_to_mb_act (BroadbandDeviceInfo *info)
{
	MMModemAccessTechnology act;

	act = mm_modem_get_access_technologies (info->mm_modem);

	g_return_val_if_fail (act != MM_MODEM_ACCESS_TECHNOLOGY_ANY, MB_TECH_UNKNOWN);

	/* We get a MASK of values, but we need to report only ONE.
	 * So just return the 'best' one found */

	/* Prefer 4G technologies over 3G and 2G */
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_LTE)
		return MB_TECH_LTE;

	/* Prefer 3GPP 3G technologies over 3GPP 2G or 3GPP2 */
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS)
		return MB_TECH_HSPA_PLUS;
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_HSPA)
		return MB_TECH_HSPA;
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_HSUPA)
		return MB_TECH_HSUPA;
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_HSDPA)
		return MB_TECH_HSDPA;
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_UMTS)
		return MB_TECH_UMTS;

	/* Prefer 3GPP2 3G technologies over 2G */
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_EVDO0 ||
	    act & MM_MODEM_ACCESS_TECHNOLOGY_EVDOA ||
	    act & MM_MODEM_ACCESS_TECHNOLOGY_EVDOB)
		return MB_TECH_EVDO;

	/* Prefer 3GPP 2G technologies over 3GPP2 2G */
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_EDGE)
		return MB_TECH_EDGE;
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_GPRS)
		return MB_TECH_GPRS;
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_GSM ||
		act & MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT)
		return MB_TECH_GSM;

	/* Last, 3GPP2 2G */
	if (act & MM_MODEM_ACCESS_TECHNOLOGY_1XRTT)
		return MB_TECH_1XRTT;

	return MB_TECH_UNKNOWN;
}

static void
get_icon (NMDevice *device,
          NMDeviceState state,
          NMConnection *connection,
          GdkPixbuf **out_pixbuf,
          const char **out_icon_name,
          char **tip,
          NMApplet *applet)
{
	BroadbandDeviceInfo *info;

	if (!applet->mm1) {
		g_warning ("ModemManager is not available for modem at %s", nm_device_get_udi (device));
		return;
	}

	info = g_object_get_data (G_OBJECT (device), "devinfo");
	if (!info) {
		g_warning ("ModemManager is not available for modem at %s",
		           nm_device_get_udi (device));
		return;
	}

	mobile_helper_get_icon (device,
	                        state,
	                        connection,
	                        out_pixbuf,
	                        out_icon_name,
	                        tip,
	                        applet,
	                        broadband_state_to_mb_state (info),
	                        broadband_act_to_mb_act (info),
	                        mm_modem_get_signal_quality (info->mm_modem, NULL),
	                        (mm_modem_get_state (info->mm_modem) >= MM_MODEM_STATE_ENABLED));
}

/********************************************************************/

typedef struct {
	NMApplet *applet;
	NMDevice *device;
	NMConnection *connection;
} BroadbandMenuItemInfo;

static void
menu_item_info_destroy (BroadbandMenuItemInfo *info)
{
	g_object_unref (G_OBJECT (info->device));
	if (info->connection)
		g_object_unref (info->connection);
	g_slice_free (BroadbandMenuItemInfo, info);
}

static void
menu_item_activate (GtkMenuItem *item,
                    BroadbandMenuItemInfo *info)
{
	applet_menu_item_activate_helper (info->device,
	                                  info->connection,
	                                  "/",
	                                  info->applet,
	                                  info);
}

static void
add_connection_item (NMDevice *device,
                     NMConnection *connection,
                     GtkWidget *item,
                     GtkWidget *menu,
                     NMApplet *applet)
{
	BroadbandMenuItemInfo *info;

	info = g_slice_new0 (BroadbandMenuItemInfo);
	info->applet = applet;
	info->device = g_object_ref (G_OBJECT (device));
	info->connection = connection ? g_object_ref (connection) : NULL;

	g_signal_connect_data (item, "activate",
	                       G_CALLBACK (menu_item_activate),
	                       info,
	                       (GClosureNotify) menu_item_info_destroy, 0);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
add_menu_item (NMDevice *device,
               gboolean multiple_devices,
               GSList *connections,
               NMConnection *active,
               GtkWidget *menu,
               NMApplet *applet)
{
	BroadbandDeviceInfo *info;
	char *text;
	GtkWidget *item;
	GSList *iter;

	info = g_object_get_data (G_OBJECT (device), "devinfo");
	if (!info) {
		g_warning ("ModemManager is not available for modem at %s",
		           nm_device_get_udi (device));
		return;
	}

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
		                            mm_modem_get_signal_quality (info->mm_modem, NULL),
		                            info->operator_name,
		                            TRUE,
		                            broadband_act_to_mb_act (info),
		                            broadband_state_to_mb_state (info),
		                            mm_modem_get_state (info->mm_modem) >= MM_MODEM_STATE_ENABLED,
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
		                            mm_modem_get_signal_quality (info->mm_modem, NULL),
		                            info->operator_name,
		                            FALSE,
		                            broadband_act_to_mb_act (info),
		                            broadband_state_to_mb_state (info),
		                            mm_modem_get_state (info->mm_modem) >= MM_MODEM_STATE_ENABLED,
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
			item = gtk_check_menu_item_new_with_label (_("New Mobile Broadband connection..."));
			add_connection_item (device, NULL, item, menu, applet);
		}
	}
}

/********************************************************************/

static void
notify_connected (NMDevice *device,
                  const char *msg,
                  NMApplet *applet)
{
	applet_do_notify_with_pref (applet,
	                            _("Connection Established"),
	                            msg ? msg : _("You are now connected to the Mobile Broadband network."),
	                            "nm-device-wwan",
	                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
}

/********************************************************************/

static void
signal_quality_updated (GObject *object,
                        GParamSpec *pspec,
                        BroadbandDeviceInfo *info)
{
	applet_schedule_update_icon (info->applet);
}

static void
access_technologies_updated (GObject *object,
                             GParamSpec *pspec,
                             BroadbandDeviceInfo *info)
{
	applet_schedule_update_icon (info->applet);
}

static void
operator_info_updated (GObject *object,
                       GParamSpec *pspec,
                       BroadbandDeviceInfo *info)
{
	g_free (info->operator_name);
	info->operator_name = NULL;

	/* Prefer 3GPP info if given */

	if (info->mm_modem_3gpp) {
		info->operator_name = (mobile_helper_parse_3gpp_operator_name (
			                       &(info->mpd),
			                       mm_modem_3gpp_get_operator_name (info->mm_modem_3gpp),
			                       mm_modem_3gpp_get_operator_code (info->mm_modem_3gpp)));
		if (info->operator_name)
			return;
	}

	if (info->mm_modem_cdma)
		info->operator_name = (mobile_helper_parse_3gpp2_operator_name (
			                       &(info->mpd),
			                       mm_modem_cdma_get_sid (info->mm_modem_cdma)));
}

static void
setup_signals (BroadbandDeviceInfo *info,
               gboolean enable)
{
	if (enable) {
		g_assert (info->mm_modem_3gpp == NULL);
		g_assert (info->mm_modem_cdma == NULL);
		g_assert (info->operator_name_update_id == 0);
		g_assert (info->operator_code_update_id == 0);
		g_assert (info->sid_update_id == 0);

		info->mm_modem_3gpp = mm_object_get_modem_3gpp (info->mm_object);
		info->mm_modem_cdma = mm_object_get_modem_cdma (info->mm_object);

		if (info->mm_modem_3gpp) {
			info->operator_name_update_id = g_signal_connect (info->mm_modem_3gpp,
			                                                  "notify::operator-name",
			                                                  G_CALLBACK (operator_info_updated),
			                                                  info);
			info->operator_code_update_id = g_signal_connect (info->mm_modem_3gpp,
			                                                  "notify::operator-code",
			                                                  G_CALLBACK (operator_info_updated),
			                                                  info);
		}

		if (info->mm_modem_cdma) {
			info->sid_update_id = g_signal_connect (info->mm_modem_cdma,
			                                        "notify::sid",
			                                        G_CALLBACK (operator_info_updated),
			                                        info);
		}

		/* Load initial values */
		operator_info_updated (NULL, NULL, info);
	} else {
		if (info->mm_modem_3gpp) {
			if (info->operator_name_update_id) {
				if (g_signal_handler_is_connected (info->mm_modem_3gpp, info->operator_name_update_id))
					g_signal_handler_disconnect (info->mm_modem_3gpp, info->operator_name_update_id);
				info->operator_name_update_id = 0;
			}
			if (info->operator_code_update_id) {
				if (g_signal_handler_is_connected (info->mm_modem_3gpp, info->operator_code_update_id))
					g_signal_handler_disconnect (info->mm_modem_3gpp, info->operator_code_update_id);
				info->operator_code_update_id = 0;
			}
			g_clear_object (&info->mm_modem_3gpp);
		}

		if (info->mm_modem_cdma) {
			if (info->sid_update_id) {
				if (g_signal_handler_is_connected (info->mm_modem_cdma, info->sid_update_id))
					g_signal_handler_disconnect (info->mm_modem_cdma, info->sid_update_id);
				info->sid_update_id = 0;
			}
			g_clear_object (&info->mm_modem_cdma);
		}
	}
}

static void
modem_state_changed (MMModem *object,
                     gint old,
                     gint new,
                     guint reason,
                     BroadbandDeviceInfo *info)
{
	/* Modem just got enabled */
	if (old < MM_MODEM_STATE_ENABLED &&
	    new >= MM_MODEM_STATE_ENABLED) {
		setup_signals (info, TRUE);
	}
	/* Modem just got disabled */
	else if (old >= MM_MODEM_STATE_ENABLED &&
	    new < MM_MODEM_STATE_ENABLED) {
		setup_signals (info, FALSE);
	}

	/* Modem just got registered */
	if ((old < MM_MODEM_STATE_REGISTERED &&
	     new >= MM_MODEM_STATE_REGISTERED)) {
		guint32 mb_state;

		/* Notify about new registration info */
		mb_state = broadband_state_to_mb_state (info);
		if (mb_state == MB_STATE_HOME) {
			applet_do_notify_with_pref (info->applet,
			                            _("Mobile Broadband network."),
			                            _("You are now registered on the home network."),
			                            "nm-signal-100",
			                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
		} else if (mb_state == MB_STATE_ROAMING) {
			applet_do_notify_with_pref (info->applet,
			                            _("Mobile Broadband network."),
			                            _("You are now registered on a roaming network."),
			                            "nm-signal-100",
			                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
		}
	}
}

/********************************************************************/

static void
broadband_device_info_free (BroadbandDeviceInfo *info)
{
	setup_signals (info, FALSE);

	g_free (info->operator_name);
	if (info->mpd)
		g_object_unref (info->mpd);

	if (info->mm_sim)
		g_object_unref (info->mm_sim);
	if (info->mm_modem) {
		g_signal_handlers_disconnect_matched (info->mm_modem, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, info);
		g_object_unref (info->mm_modem);
	}
	if (info->mm_object)
		g_object_unref (info->mm_object);

	if (info->dialog)
		unlock_dialog_destroy (info);
	g_object_unref (info->cancellable);

	g_slice_free (BroadbandDeviceInfo, info);
}

static void
device_added (NMDevice *device,
              NMApplet *applet)
{
	NMDeviceModem *modem = NM_DEVICE_MODEM (device);
	BroadbandDeviceInfo *info;
	const char *udi;
	GDBusObject *modem_object;

	udi = nm_device_get_udi (device);
	if (!udi)
		return;

	if (!applet->mm1) {
		g_warning ("Cannot grab information for modem at %s: No ModemManager support",
		           nm_device_get_udi (device));
		return;
	}

	modem_object = g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (applet->mm1),
	                                                 nm_device_get_udi (device));
	if (!modem_object) {
		g_warning ("Cannot grab information for modem at %s: Not found",
		           nm_device_get_udi (device));
		return;
	}

	info = g_slice_new0 (BroadbandDeviceInfo);
	info->applet = applet;
	info->device = device;
	info->mm_object = MM_OBJECT (modem_object);
	info->mm_modem = mm_object_get_modem (info->mm_object);
	info->cancellable = g_cancellable_new ();

	/* Setup signals */

	g_signal_connect (info->mm_modem,
	                  "state-changed",
	                  G_CALLBACK (modem_state_changed),
	                  info);
	g_signal_connect (info->mm_modem,
	                  "notify::signal-quality",
	                  G_CALLBACK (signal_quality_updated),
	                  info);
	g_signal_connect (info->mm_modem,
	                  "notify::access-technologies",
	                  G_CALLBACK (access_technologies_updated),
	                  info);

	/* Load initial values */
	signal_quality_updated (NULL, NULL, info);
	access_technologies_updated (NULL, NULL, info);
	if (mm_modem_get_state (info->mm_modem) >= MM_MODEM_STATE_ENABLED)
		setup_signals (info, TRUE);

	/* Asynchronously get SIM */
	mm_modem_get_sim (info->mm_modem,
	                  NULL, /* cancellable */
	                  (GAsyncReadyCallback)modem_get_sim_ready,
	                  info);

	/* Store device info */
	g_object_set_data_full (G_OBJECT (modem),
	                        "devinfo",
	                        info,
	                        (GDestroyNotify)broadband_device_info_free);
}

/********************************************************************/

NMADeviceClass *
applet_device_broadband_get_class (NMApplet *applet)
{
	NMADeviceClass *dclass;

	dclass = g_slice_new0 (NMADeviceClass);
	if (!dclass)
		return NULL;

	dclass->new_auto_connection = new_auto_connection;
	dclass->add_menu_item = add_menu_item;
	dclass->device_added = device_added;
	dclass->notify_connected = notify_connected;
	dclass->get_icon = get_icon;
	dclass->get_secrets = get_secrets;
	dclass->secrets_request_size = sizeof (MobileHelperSecretsInfo);

	return dclass;
}
