/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <bluetooth-client.h>
#include <bluetooth-client-private.h>
#include <bluetooth-chooser.h>
#include <bluetooth-agent.h>
#include <bluetooth-plugin-manager.h>

#include "pin.h"

#define AGENT_PATH "/org/bluez/agent/wizard"

/* We'll try to connect to the device repeatedly for that
 * amount of time before we bail out */
#define CONNECT_TIMEOUT 3.0

#define W(x) GTK_WIDGET(gtk_builder_get_object(builder, x))

enum {
	PAGE_SEARCH,
	PAGE_CONNECTING,
	PAGE_SETUP,
	PAGE_SSP_SETUP,
	PAGE_FINISHING,
	PAGE_SUMMARY
};

typedef enum {
	PAIRING_UI_NORMAL,
	PAIRING_UI_KEYBOARD,
	PAIRING_UI_ICADE
} PairingUIBehaviour;

static gboolean set_page_search_complete(void);

static BluetoothClient *client;
static BluetoothAgent *agent;

static gchar *target_address = NULL;
static gchar *target_name = NULL;
static guint target_max_digits = 0;
static PairingUIBehaviour target_ui_behaviour = PAIRING_UI_NORMAL;
static gboolean target_ssp = FALSE;
static gboolean create_started = FALSE;
static gboolean display_called = FALSE;

/* NULL means automatic, anything else is a pincode specified by the user */
static gchar *user_pincode = NULL;
/* If TRUE, then we won't display the PIN code to the user when pairing */
static gboolean automatic_pincode = FALSE;
static char *pincode = NULL;

static GtkBuilder *builder = NULL;

static GtkAssistant *window_assistant = NULL;
static GtkWidget *button_quit = NULL;
static GtkWidget *button_cancel = NULL;
static GtkWidget *page_search = NULL;
static GtkWidget *page_connecting = NULL;
static GtkWidget *page_setup = NULL;
static GtkWidget *page_ssp_setup = NULL;
static GtkWidget *page_finishing = NULL;
static GtkWidget *page_summary = NULL;

static GtkWidget *label_connecting = NULL;
static GtkWidget *spinner_connecting = NULL;

static GtkWidget *label_pin = NULL;
static GtkWidget *label_pin_help = NULL;

static GtkWidget *label_ssp_pin_help = NULL;
static GtkWidget *label_ssp_pin = NULL;
static GtkWidget *does_not_match_button = NULL;
static GtkWidget *matches_button = NULL;

static GtkWidget *label_finishing = NULL;
static GtkWidget *spinner_finishing = NULL;

static gboolean   summary_failure = FALSE;
static GtkWidget *label_summary = NULL;
static GtkWidget *extra_config_vbox = NULL;

static BluetoothChooser *selector = NULL;

static GtkWidget *pin_dialog = NULL;
static GtkWidget *radio_auto = NULL;
static GtkWidget *radio_0000 = NULL;
static GtkWidget *radio_1111 = NULL;
static GtkWidget *radio_1234 = NULL;
static GtkWidget *radio_none = NULL;
static GtkWidget *radio_custom = NULL;
static GtkWidget *entry_custom = NULL;

/* Signals */
void quit_callback(GtkWidget *assistant, gpointer data);
void prepare_callback(GtkWidget *assistant, GtkWidget *page, gpointer data);
void select_device_changed(BluetoothChooser *selector, const char *address, gpointer user_data);
gboolean entry_custom_event(GtkWidget *entry, GdkEventKey *event);
void set_user_pincode(GtkWidget *button);
void toggle_set_sensitive(GtkWidget *button, gpointer data);
void pin_option_button_clicked (GtkButton *button, gpointer data);
void entry_custom_changed(GtkWidget *entry);
void restart_button_clicked (GtkButton *button, gpointer user_data);
void does_not_match_cb (GtkButton *button, gpointer user_data);
void matches_cb (GtkButton *button, gpointer user_data);

static void
set_large_label (GtkLabel *label, const char *text)
{
	char *str;

	str = g_strdup_printf("<span font_desc=\"50\">  %s  </span>", text);
	gtk_label_set_markup(GTK_LABEL(label), str);
	g_free(str);
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

		r = g_random_int_range (1, 4);

		c = g_strdup_printf ("%d", r);
		g_string_append (pin, c);
		g_free (c);

		g_string_append (pin_display, arrows[r]);
	}
	g_string_append (pin_display, "❍");

	*pin_display_str = g_string_free (pin_display, FALSE);
	return g_string_free (pin, FALSE);
}

static gboolean
pincode_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy *device,
		  gpointer user_data)
{
	target_ssp = FALSE;

	/* Only show the pincode page if the pincode isn't automatic */
	if (automatic_pincode == FALSE)
		gtk_assistant_set_current_page (window_assistant, PAGE_SETUP);
	g_debug ("Using pincode \"%s\"", pincode);
	g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", pincode));

	return TRUE;
}

void
restart_button_clicked (GtkButton *button,
			gpointer user_data)
{
	/* Clean up old state */
	target_ssp = FALSE;
	display_called = FALSE;
	g_free (target_address);
	target_address = NULL;
	g_free (target_name);
	target_name = NULL;
	summary_failure = FALSE;
	target_ui_behaviour = PAIRING_UI_NORMAL;

	g_object_set (selector,
		      "device-category-filter", BLUETOOTH_CATEGORY_NOT_PAIRED_OR_TRUSTED,
		      NULL);

	gtk_assistant_set_current_page (window_assistant, PAGE_SEARCH);
}

void
does_not_match_cb (GtkButton *button,
		   gpointer user_data)
{
	GDBusMethodInvocation *invocation;
	GError *error = NULL;
	char *text;

	summary_failure = TRUE;
	gtk_assistant_set_current_page (window_assistant, PAGE_SUMMARY);

	/* translators:
	 * The '%s' is the device name, for example:
	 * Pairing with 'Sony Bluetooth Headset' cancelled
	 */
	text = g_strdup_printf(_("Pairing with '%s' cancelled"), target_name);
	gtk_label_set_text(GTK_LABEL(label_summary), text);
	g_free(text);

	invocation = g_object_get_data (G_OBJECT (button), "invocation");
	error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
			     "Agent callback cancelled");
	g_dbus_method_invocation_return_gerror (invocation, error);
	g_error_free (error);

	g_object_set_data (G_OBJECT(does_not_match_button), "invocation", NULL);
	g_object_set_data (G_OBJECT(matches_button), "invocation", NULL);
}

void
matches_cb (GtkButton *button,
	    gpointer user_data)
{
	GDBusMethodInvocation *invocation;

	invocation = g_object_get_data (G_OBJECT (button), "invocation");
	gtk_widget_set_sensitive (does_not_match_button, FALSE);
	gtk_widget_set_sensitive (matches_button, FALSE);
	g_dbus_method_invocation_return_value (invocation, NULL);

	g_object_set_data (G_OBJECT(does_not_match_button), "invocation", NULL);
	g_object_set_data (G_OBJECT(matches_button), "invocation", NULL);
}

static gboolean
confirm_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy *device,
		  guint pin,
		  gpointer user_data)
{
	char *str, *label;

	target_ssp = TRUE;
	gtk_assistant_set_current_page (window_assistant, PAGE_SSP_SETUP);

	gtk_widget_show (label_ssp_pin_help);
	label = g_strdup_printf (_("Please confirm that the PIN displayed on '%s' matches this one."),
				 target_name);
	gtk_label_set_markup(GTK_LABEL(label_ssp_pin_help), label);
	g_free (label);

	gtk_widget_show (label_ssp_pin);
	str = g_strdup_printf ("%06d", pin);
	set_large_label (GTK_LABEL (label_ssp_pin), str);
	g_free (str);

	g_object_set_data (G_OBJECT(does_not_match_button), "invocation", invocation);
	g_object_set_data (G_OBJECT(matches_button), "invocation", invocation);

	return TRUE;
}

static gboolean
display_callback (GDBusMethodInvocation *invocation,
		  GDBusProxy *device,
		  guint pin,
		  guint entered,
		  gpointer user_data)
{
	gchar *text, *done, *code;

	display_called = TRUE;
	target_ssp = TRUE;
	gtk_assistant_set_current_page (window_assistant, PAGE_SSP_SETUP);

	code = g_strdup_printf("%06d", pin);

	if (entered > 0) {
		GtkEntry *entry;
		gunichar invisible;
		GString *str;
		guint i;

		entry = GTK_ENTRY (gtk_entry_new ());
		invisible = gtk_entry_get_invisible_char (entry);
		g_object_unref (entry);

		str = g_string_new (NULL);
		for (i = 0; i < entered; i++)
			g_string_append_unichar (str, invisible);
		if (entered < strlen (code))
			g_string_append (str, code + entered);

		done = g_string_free (str, FALSE);
	} else {
		done = g_strdup ("");
	}

	gtk_widget_show (label_pin_help);

	gtk_label_set_markup(GTK_LABEL(label_ssp_pin_help), _("Please enter the following PIN:"));
	text = g_strdup_printf("%s%s", done, code + entered);
	set_large_label (GTK_LABEL (label_ssp_pin), text);
	g_free(text);

	g_free(done);
	g_free(code);

	g_dbus_method_invocation_return_value (invocation, NULL);

	return TRUE;
}

static gboolean
cancel_callback (GDBusMethodInvocation *invocation,
		 gpointer user_data)
{
	gchar *text;

	create_started = FALSE;

	summary_failure = TRUE;
	gtk_assistant_set_current_page (window_assistant, PAGE_SUMMARY);

	/* translators:
	 * The '%s' is the device name, for example:
	 * Pairing with 'Sony Bluetooth Headset' cancelled
	 */
	text = g_strdup_printf(_("Pairing with '%s' cancelled"), target_name);
	gtk_label_set_text(GTK_LABEL(label_summary), text);
	g_free(text);

	g_dbus_method_invocation_return_value (invocation, NULL);

	return TRUE;
}

typedef struct {
	char *path;
	GTimer *timer;
} ConnectData;

static void
connect_callback (GObject      *source_object,
		  GAsyncResult *res,
		  gpointer      user_data)
{
	ConnectData *data = (ConnectData *) user_data;
	gboolean success;

	success = bluetooth_client_connect_service_finish (client, res, NULL);

	if (success == FALSE && g_timer_elapsed (data->timer, NULL) < CONNECT_TIMEOUT) {
		bluetooth_client_connect_service (client, data->path, TRUE, NULL, connect_callback, data);
		return;
	}

	if (success == FALSE)
		g_debug ("Failed to connect to device %s", data->path);

	g_timer_destroy (data->timer);
	g_free (data->path);
	g_free (data);

	gtk_assistant_set_current_page (window_assistant, PAGE_SUMMARY);
}

static void
create_callback (BluetoothClient *_client,
		 const char *path,
		 const GError *error,
		 gpointer user_data)
{
	ConnectData *data;

	create_started = FALSE;

	/* Create failed */
	if (path == NULL) {
		char *text;

		summary_failure = TRUE;
		gtk_assistant_set_current_page (window_assistant, PAGE_SUMMARY);

		/* translators:
		 * The '%s' is the device name, for example:
		 * Setting up 'Sony Bluetooth Headset' failed
		 */
		text = g_strdup_printf(_("Setting up '%s' failed"), target_name);

		g_warning ("Setting up '%s' failed: %s", target_name, error->message);

		gtk_label_set_markup(GTK_LABEL(label_summary), text);
		g_free (text);

		return;
	}

	bluetooth_client_set_trusted(client, path, TRUE);

	data = g_new0 (ConnectData, 1);
	data->path = g_strdup (path);
	data->timer = g_timer_new ();

	bluetooth_client_connect_service (client, path, TRUE, NULL, connect_callback, data);
	gtk_assistant_set_current_page (window_assistant, PAGE_FINISHING);
}

void
quit_callback (GtkWidget *widget,
		gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET (window_assistant));
}

void prepare_callback (GtkWidget *assistant,
		       GtkWidget *page,
		       gpointer data)
{
	gboolean complete = TRUE;

	gtk_widget_hide (button_quit);
	gtk_widget_hide (button_cancel);

	if (page == page_search) {
		complete = set_page_search_complete ();
		bluetooth_chooser_start_discovery(selector);
	} else {
		bluetooth_chooser_stop_discovery(selector);
	}

	if (page == page_connecting) {
		char *text;

		complete = FALSE;

		gtk_spinner_start (GTK_SPINNER (spinner_connecting));

		/* translators:
		 * The '%s' is the device name, for example:
		 * Connecting to 'Sony Bluetooth Headset'...
		 */
		text = g_strdup_printf (_("Connecting to '%s'..."), target_name);
		gtk_label_set_text (GTK_LABEL (label_connecting), text);
		g_free (text);

		gtk_widget_show (button_cancel);
	} else {
		gtk_spinner_stop (GTK_SPINNER (spinner_connecting));
	}

	if ((page == page_setup || page == page_connecting) && (create_started == FALSE)) {
		const char *path = AGENT_PATH;

		/* Set the filter on the selector, so we can use it to get more
		 * info later, in page_summary */
		g_object_set (selector,
			      "device-category-filter", BLUETOOTH_CATEGORY_ALL,
			      NULL);

		/* Do we pair, or don't we? */
		if (automatic_pincode && pincode == NULL) {
			g_debug ("Not pairing as %s", automatic_pincode ? "pincode is NULL" : "automatic_pincode is FALSE");
			path = NULL;
		}

		g_object_ref(agent);
		bluetooth_client_create_device (client, target_address,
						path, create_callback, assistant);
		create_started = TRUE;
	}

	if (page == page_setup) {
		complete = FALSE;

		if (automatic_pincode == FALSE && target_ssp == FALSE) {
			char *help, *pincode_display;

			g_free (pincode);
			pincode = NULL;
			pincode_display = NULL;

			switch (target_ui_behaviour) {
			case PAIRING_UI_NORMAL:
				help = g_strdup_printf (_("Please enter the following PIN on '%s':"), target_name);
				break;
			case PAIRING_UI_KEYBOARD:
				help = g_strdup_printf (_("Please enter the following PIN on '%s' and press “Enter” on the keyboard:"), target_name);
				pincode = get_random_pincode (target_max_digits);
				pincode_display = g_strdup_printf ("%s⏎", pincode);
				break;
			case PAIRING_UI_ICADE:
				help = g_strdup (_("Please move the joystick of your iCade in the following directions:"));
				pincode = get_icade_pincode (&pincode_display);
				break;
			default:
				g_assert_not_reached ();
			}

			if (pincode == NULL)
				pincode = get_random_pincode (target_max_digits);

			gtk_label_set_markup(GTK_LABEL(label_pin_help), help);
			g_free (help);
			set_large_label (GTK_LABEL (label_pin), pincode_display ? pincode_display : pincode);
			g_free (pincode_display);
		} else {
			g_assert_not_reached ();
		}

		gtk_widget_show (button_cancel);
	}

	if (page == page_finishing) {
		char *text;

		complete = FALSE;

		gtk_spinner_start (GTK_SPINNER (spinner_finishing));

		/* translators:
		 * The '%s' is the device name, for example:
		 * Please wait while finishing setup on 'Sony Bluetooth Headset'...
		 */
		text = g_strdup_printf (_("Please wait while finishing setup on device '%s'..."), target_name);
		gtk_label_set_text (GTK_LABEL (label_finishing), text);
		g_free (text);

		gtk_widget_show (button_quit);
	} else {
		gtk_spinner_stop (GTK_SPINNER (spinner_finishing));
	}

	if (page == page_summary && summary_failure == FALSE) {
		GList *widgets = NULL;
		GValue value = { 0, };
		char **uuids, *text;

		/* FIXME remove this code when bluetoothd has pair/unpair code */
		g_object_set (G_OBJECT (selector), "device-selected", target_address, NULL);

		bluetooth_chooser_get_selected_device_info (selector, "name", &value);
		text = g_strdup_printf (_("Successfully set up new device '%s'"), g_value_get_string (&value));
		g_value_unset (&value);
		gtk_label_set_text (GTK_LABEL (label_summary), text);
		g_free (text);

		if (bluetooth_chooser_get_selected_device_info (selector, "uuids", &value) != FALSE) {
			uuids = g_value_get_boxed (&value);
			widgets = bluetooth_plugin_manager_get_widgets (target_address,
									(const char **) uuids);
			g_value_unset (&value);
		}
		if (widgets != NULL) {
			GList *l;

			for (l = widgets; l != NULL; l = l->next) {
				GtkWidget *widget = l->data;
				gtk_box_pack_start (GTK_BOX (extra_config_vbox),
						    widget,
						    FALSE,
						    TRUE,
						    0);
			}
			g_list_free (widgets);
			gtk_widget_show_all (extra_config_vbox);
		}
		gtk_widget_show (button_quit);
	}

	/* Setup the buttons some */
	if (page == page_summary && summary_failure) {
		complete = FALSE;
		gtk_assistant_add_action_widget (GTK_ASSISTANT (assistant), W("restart_button"));
		gtk_widget_show (button_quit);
	} else {
		if (gtk_widget_get_parent (W("restart_button")) != NULL)
			gtk_assistant_remove_action_widget (GTK_ASSISTANT (assistant), W("restart_button"));
	}

	if (page == page_ssp_setup) {
		if (display_called == FALSE) {
			complete = FALSE;
			gtk_assistant_add_action_widget (GTK_ASSISTANT (assistant), W("matches_button"));
			gtk_assistant_add_action_widget (GTK_ASSISTANT (assistant), W("does_not_match_button"));
		} else {
			gtk_widget_show (button_cancel);
		}
	} else {
		if (gtk_widget_get_parent (W("does_not_match_button")) != NULL)
			gtk_assistant_remove_action_widget (GTK_ASSISTANT (assistant), W("does_not_match_button"));
		if (gtk_widget_get_parent (W("matches_button")) != NULL)
			gtk_assistant_remove_action_widget (GTK_ASSISTANT (assistant), W("matches_button"));
	}

	gtk_assistant_set_page_complete (GTK_ASSISTANT(assistant),
					 page, complete);
}

static gboolean
set_page_search_complete (void)
{
	char *name, *address;
	gboolean complete = FALSE;

	address = bluetooth_chooser_get_selected_device (selector);
	name = bluetooth_chooser_get_selected_device_name (selector);

	if (address == NULL)
		complete = FALSE;
	else if (name == NULL)
		complete = (user_pincode != NULL && strlen(user_pincode) >= 4);
	else
		complete = (user_pincode == NULL || strlen(user_pincode) >= 4);

	g_free (address);
	g_free (name);

	gtk_assistant_set_page_complete (GTK_ASSISTANT (window_assistant),
					 page_search, complete);

	return complete;
}

gboolean
entry_custom_event (GtkWidget *entry, GdkEventKey *event)
{
	gunichar c;

	if (event->length == 0)
		return FALSE;

	/* Not a printable character? */
	c = gdk_keyval_to_unicode (event->keyval);
	if (c == 0 ||
	    g_unichar_iscntrl (c) ||
	    g_unichar_isdigit (c))
		return FALSE;

	return TRUE;
}

void
entry_custom_changed (GtkWidget *entry)
{
	g_free (user_pincode);
	user_pincode = g_strdup (gtk_entry_get_text(GTK_ENTRY(entry)));
	gtk_dialog_set_response_sensitive (GTK_DIALOG (pin_dialog),
					   GTK_RESPONSE_ACCEPT,
					   gtk_entry_get_text_length (GTK_ENTRY (entry)) >= 1);
}

void
toggle_set_sensitive (GtkWidget *button,
		      gpointer data)
{
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	gtk_widget_set_sensitive(entry_custom, active);
	/* When selecting another PIN, make sure the "Close" button is sensitive */
	if (!active)
		gtk_dialog_set_response_sensitive (GTK_DIALOG (pin_dialog),
						   GTK_RESPONSE_ACCEPT, TRUE);
	else
		entry_custom_changed (entry_custom);
}

void
set_user_pincode (GtkWidget *button)
{
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		return;

	g_free (user_pincode);
	user_pincode = g_strdup (g_object_get_data (G_OBJECT (button), "pin"));
}

void
select_device_changed (BluetoothChooser *selector,
		       const char *address,
		       gpointer user_data)
{
	GValue value = { 0, };
	guint target_type = BLUETOOTH_TYPE_ANY;
	gboolean is_custom_pin = FALSE;
	int legacypairing;

	if (gtk_assistant_get_current_page (GTK_ASSISTANT (window_assistant)) != PAGE_SEARCH)
		return;

	set_page_search_complete ();

	/* Device was deselected */
	if (address == NULL)
		return;

	if (bluetooth_chooser_get_selected_device_info (selector, "legacypairing", &value) != FALSE) {
		legacypairing = g_value_get_int (&value);
		if (legacypairing == -1)
			legacypairing = TRUE;
	} else {
		legacypairing = TRUE;
	}

	g_free(target_address);
	target_address = g_strdup (address);

	g_free(target_name);
	target_name = bluetooth_chooser_get_selected_device_name (selector);

	target_type = bluetooth_chooser_get_selected_device_type (selector);
	target_ssp = !legacypairing;
	automatic_pincode = FALSE;
	target_ui_behaviour = PAIRING_UI_NORMAL;

	g_free (pincode);
	pincode = NULL;

	g_free (user_pincode);
	user_pincode = get_pincode_for_device (target_type, target_address, target_name, &target_max_digits);
	if (user_pincode != NULL &&
	    g_str_equal (user_pincode, "NULL") == FALSE) {
		if (g_str_equal (user_pincode, "KEYBOARD")) {
			target_ui_behaviour = PAIRING_UI_KEYBOARD;
			is_custom_pin = TRUE;
		} else if (g_str_equal (user_pincode, "ICADE")) {
			target_ui_behaviour = PAIRING_UI_ICADE;
			is_custom_pin = TRUE;
		} else {
			pincode = g_strdup (user_pincode);
		}
	}

	if (is_custom_pin)
		automatic_pincode = FALSE;
	else
		automatic_pincode = user_pincode != NULL;

	g_free (user_pincode);
	user_pincode = NULL;

	gtk_entry_set_max_length (GTK_ENTRY (entry_custom), target_max_digits);
}

void
pin_option_button_clicked (GtkButton *button,
			       gpointer data)
{
	GtkWidget *radio;

	gtk_window_set_transient_for (GTK_WINDOW (pin_dialog),
				      GTK_WINDOW (window_assistant));
	gtk_window_present (GTK_WINDOW (pin_dialog));

	/* When reopening, try to guess where the pincode was set */
	if (user_pincode == NULL)
		radio = radio_auto;
	else if (g_str_equal (user_pincode, "0000"))
		radio = radio_0000;
	else if (g_str_equal (user_pincode, "1111"))
		radio = radio_1111;
	else if (g_str_equal (user_pincode, "1234"))
		radio = radio_1234;
	else if (g_str_equal (user_pincode, "NULL"))
		radio = radio_none;
	else {
		radio = radio_custom;
		gtk_entry_set_text (GTK_ENTRY (entry_custom), user_pincode);
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

	gtk_dialog_run (GTK_DIALOG (pin_dialog));
	gtk_widget_hide (pin_dialog);
	g_free (pincode);
	pincode = g_strdup (user_pincode);
	automatic_pincode = user_pincode != NULL;
}

static int
page_func (gint current_page,
	   gpointer data)
{
	if (current_page == PAGE_SEARCH) {
		if (target_ssp != FALSE || automatic_pincode != FALSE)
			return PAGE_CONNECTING;
		else
			return PAGE_SETUP;
	}
	if (current_page == PAGE_SETUP)
		return PAGE_SUMMARY;
	return current_page + 1;
}

static gboolean
create_wizard (void)
{
	GtkAssistant *assistant;
	GError *err = NULL;

	builder = gtk_builder_new ();
	if (gtk_builder_add_from_file (builder, "wizard.ui", NULL) == 0) {
		if (gtk_builder_add_from_file (builder, PKGDATADIR "/wizard.ui", &err) == 0) {
			g_warning ("Could not load UI from %s: %s", PKGDATADIR "/wizard.ui", err->message);
			g_error_free(err);
			return FALSE;
		}
	}

	window_assistant = GTK_ASSISTANT(gtk_builder_get_object(builder, "assistant"));
	assistant = window_assistant;

	gtk_assistant_set_forward_page_func (assistant, page_func, NULL, NULL);

	/* The 2 custom buttons */
	button_quit = W("quit_button");
	button_cancel = W("cancel_button");
	gtk_assistant_add_action_widget (assistant, button_quit);
	gtk_assistant_add_action_widget (assistant, button_cancel);
	gtk_widget_hide (button_quit);
	gtk_widget_hide (button_cancel);

	/* Intro page, nothing to do */

	/* Search page */
	page_search = W("page_search");
	selector = BLUETOOTH_CHOOSER (gtk_builder_get_object (builder, "selector"));

	/* Connecting page */
	page_connecting = W("page_connecting");
	label_connecting = W("label_connecting");
	spinner_connecting = W("spinner_connecting");

	/* Setup page */
	page_setup = W("page_setup");
	label_pin_help = W("label_pin_help");
	label_pin = W("label_pin");

	/* SSP Setup page */
	page_ssp_setup = W("page_ssp_setup");
	gtk_assistant_set_page_complete(assistant, page_ssp_setup, FALSE);
	label_ssp_pin_help = W("label_ssp_pin_help");
	label_ssp_pin = W("label_ssp_pin");
	does_not_match_button = W("does_not_match_button");
	matches_button = W("matches_button");

	/* Finishing page */
	page_finishing = W("page_finishing");
	label_finishing = W("label_finishing");
	spinner_finishing = W("spinner_finishing");

	/* Summary page */
	page_summary = W("page_summary");
	label_summary = W("label_summary");
	extra_config_vbox = W("extra_config_vbox");

	/* PIN dialog */
	pin_dialog = W("pin_dialog");
	radio_auto = W("radio_auto");
	radio_0000 = W("radio_0000");
	radio_1111 = W("radio_1111");
	radio_1234 = W("radio_1234");
	radio_none = W("radio_none");
	radio_custom = W("radio_custom");
	entry_custom = W("entry_custom");

	g_object_set_data (G_OBJECT (radio_auto), "pin", NULL);
	g_object_set_data (G_OBJECT (radio_0000), "pin", "0000");
	g_object_set_data (G_OBJECT (radio_1111), "pin", "1111");
	g_object_set_data (G_OBJECT (radio_1234), "pin", "1234");
	g_object_set_data (G_OBJECT (radio_none), "pin", "NULL");
	g_object_set_data (G_OBJECT (radio_custom), "pin", "");
	g_object_set_data (G_OBJECT (radio_custom), "entry", entry_custom);

	gtk_builder_connect_signals(builder, NULL);

	gtk_widget_show (GTK_WIDGET(assistant));

	gtk_assistant_update_buttons_state(GTK_ASSISTANT(assistant));

	return TRUE;
}

static void
activate_cb (GApplication      *app,
	     gpointer           user_data)
{
	gtk_window_present_with_time (GTK_WINDOW (user_data), GDK_CURRENT_TIME);
}

static GOptionEntry options[] = {
	{ NULL },
};

int main (int argc, char **argv)
{
	GtkApplication *app;
	GError *error = NULL;

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	if (gtk_init_with_args(&argc, &argv, NULL,
				options, GETTEXT_PACKAGE, &error) == FALSE) {
		if (error) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		} else
			g_printerr("An unknown error occurred\n");

		return 1;
	}

	app = gtk_application_new ("org.gnome.Bluetooth.wizard", G_APPLICATION_FLAGS_NONE);
	if (g_application_register (G_APPLICATION (app), NULL, &error) == FALSE) {
		g_warning ("Could not register application: %s", error->message);
		g_error_free (error);
		return 1;
	}

	if (g_application_get_is_remote (G_APPLICATION (app))) {
		g_application_activate (G_APPLICATION (app));
		gdk_notify_startup_complete ();
		return 0;
	}

	gtk_window_set_default_icon_name("bluetooth");

	client = bluetooth_client_new();

	agent = bluetooth_agent_new();
	g_object_add_weak_pointer (G_OBJECT (agent), (gpointer *) (&agent));

	bluetooth_agent_set_pincode_func(agent, pincode_callback, NULL);
	bluetooth_agent_set_display_func(agent, display_callback, NULL);
	bluetooth_agent_set_cancel_func(agent, cancel_callback, NULL);
	bluetooth_agent_set_confirm_func(agent, confirm_callback, NULL);

	bluetooth_agent_setup(agent, AGENT_PATH);

	bluetooth_plugin_manager_init ();

	if (create_wizard() == FALSE)
		return 1;
	gtk_application_add_window (app,
				    GTK_WINDOW (window_assistant));

	g_signal_connect (app, "activate",
			  G_CALLBACK (activate_cb), window_assistant);

	g_application_run (G_APPLICATION (app), argc, argv);

	bluetooth_plugin_manager_cleanup ();

	if (agent != NULL)
		g_object_unref (agent);

	g_object_unref(client);

	g_object_unref(app);

	return 0;
}

