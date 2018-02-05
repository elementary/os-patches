/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
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
 * Copyright 2013 Jiri Pirko <jiri@resnulli.us>
 * Copyright 2013 - 2017  Red Hat, Inc.
 */

#include "nm-default.h"

#if WITH_JANSSON
#include <jansson.h>
#endif

#include <string.h>

#include "page-team-port.h"

G_DEFINE_TYPE (CEPageTeamPort, ce_page_team_port, CE_TYPE_PAGE)

#define CE_PAGE_TEAM_PORT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_TEAM_PORT, CEPageTeamPortPrivate))

typedef struct {
	NMSettingTeamPort *setting;

	GtkTextView *json_config_widget;
	GtkWidget *import_config_button;

	GtkButton *advanced_button;
	GtkDialog *advanced_dialog;
	GtkNotebook *advanced_notebook;

	/* General */
	GtkSpinButton *queue_id;
	GtkSpinButton *port_prio;
	GtkToggleButton *port_sticky;
	GtkSpinButton *lacp_port_prio;
	GtkSpinButton *lacp_port_key;

	/* Link Watch */
	GtkComboBox *link_watcher_name;
	GtkSpinButton *delay_up;
	GtkLabel *delay_up_label;
	GtkLabel *delay_up_ms;
	GtkSpinButton *delay_down;
	GtkLabel *delay_down_label;
	GtkLabel *delay_down_ms;
	GtkSpinButton *send_interval;
	GtkLabel *send_interval_label;
	GtkLabel *send_interval_ms;
	GtkSpinButton *init_wait;
	GtkLabel *init_wait_label;
	GtkLabel *init_wait_ms;
	GtkSpinButton *missed_max;
	GtkLabel *missed_max_label;
	GtkEntry *source_host;
	GtkLabel *source_host_label;
	GtkEntry *target_host;
	GtkLabel *target_host_label;
	GtkToggleButton *validate_active;
	GtkToggleButton *validate_inactive;
	GtkToggleButton *send_all;
} CEPageTeamPortPrivate;

/* Get string "type" from a combo box with a ["type", "name"] model. */
static gchar *
get_combo_box (GtkComboBox *combo)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *name;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		g_return_val_if_reached (NULL);

	model = gtk_combo_box_get_model (combo);
	gtk_tree_model_get (model, &iter, 0, &name, -1);

	return name;
}

#if WITH_JANSSON

/* Set active item to "type" in a combo box with a ["type", "name"] model. */
static gboolean
set_combo_box (GtkComboBox *combo, const gchar *value, GError **error)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	const gchar *name;
	int i = 0;

	if (!value || !*value) {
		gtk_combo_box_set_active (combo, 0);
		return TRUE;
	}

	model = gtk_combo_box_get_model (combo);
	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid;
	     valid = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, 0, &name, -1);
		if (strcmp (name, value) == 0) {
			gtk_combo_box_set_active (combo, i);
			return TRUE;
		}
		i++;
	}

	g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
	             "Value of \"%s\" is not known", value);

	return FALSE;
}

#endif

static void
team_port_private_init (CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->json_config_widget = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "team_port_json_config"));
	priv->import_config_button = GTK_WIDGET (gtk_builder_get_object (builder, "import_config_button"));
	priv->advanced_button = GTK_BUTTON (gtk_builder_get_object (builder, "advanced_button"));
	priv->advanced_dialog = GTK_DIALOG (gtk_builder_get_object (builder, "advanced_dialog"));
	gtk_window_set_modal (GTK_WINDOW (priv->advanced_dialog), TRUE);
	priv->advanced_notebook = GTK_NOTEBOOK (gtk_builder_get_object (builder, "advanced_notebook"));

	/* General */
	priv->queue_id = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "queue_id"));
	priv->port_prio = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "port_prio"));
	priv->port_sticky = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "port_sticky"));
	priv->lacp_port_prio = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "lacp_port_prio"));
	priv->lacp_port_key = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "lacp_port_key"));

	/* Link Watcher */
	priv->link_watcher_name = GTK_COMBO_BOX (gtk_builder_get_object (builder, "link_watcher_name"));
	priv->send_interval = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "send_interval"));
	priv->send_interval_label = GTK_LABEL (gtk_builder_get_object (builder, "send_interval_label"));
	priv->send_interval_ms = GTK_LABEL (gtk_builder_get_object (builder, "send_interval_ms"));
	priv->init_wait = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "init_wait"));
	priv->init_wait_label = GTK_LABEL (gtk_builder_get_object (builder, "init_wait_label"));
	priv->init_wait_ms = GTK_LABEL (gtk_builder_get_object (builder, "init_wait_ms"));
	priv->missed_max = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "missed_max"));
	priv->missed_max_label = GTK_LABEL (gtk_builder_get_object (builder, "missed_max_label"));
	priv->source_host = GTK_ENTRY (gtk_builder_get_object (builder, "source_host"));
	priv->source_host_label = GTK_LABEL (gtk_builder_get_object (builder, "source_host_label"));
	priv->target_host = GTK_ENTRY (gtk_builder_get_object (builder, "target_host"));
	priv->target_host_label = GTK_LABEL (gtk_builder_get_object (builder, "target_host_label"));
	priv->validate_active = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "validate_active"));
	priv->validate_inactive = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "validate_inactive"));
	priv->send_all = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "send_all"));
	priv->delay_up = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "delay_up"));
	priv->delay_up_label = GTK_LABEL (gtk_builder_get_object (builder, "delay_up_label"));
	priv->delay_up_ms = GTK_LABEL (gtk_builder_get_object (builder, "delay_up_ms"));
	priv->delay_down = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "delay_down"));
	priv->delay_down_label = GTK_LABEL (gtk_builder_get_object (builder, "delay_down_label"));
	priv->delay_down_ms = GTK_LABEL (gtk_builder_get_object (builder, "delay_down_ms"));

	ce_spin_default_val (priv->queue_id, -1);
	ce_spin_default_val (priv->port_prio, -1);
	ce_spin_default_val (priv->lacp_port_prio, -1);
	ce_spin_default_val (priv->lacp_port_key, -1);
	ce_spin_default_val (priv->send_interval, -1);
	ce_spin_default_val (priv->init_wait, -1);
	ce_spin_default_val (priv->missed_max, -1);
	ce_spin_default_val (priv->delay_up, -1);
	ce_spin_default_val (priv->delay_down, -1);
}

static void
import_button_clicked_cb (GtkWidget *widget, CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	GtkWidget *dialog;
	GtkTextBuffer *buffer;
	char *filename;
	char *buf = NULL;
	gsize buf_len;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (CE_PAGE (self)->page);
	g_return_if_fail (toplevel);
	g_return_if_fail (gtk_widget_is_toplevel (toplevel));

	dialog = gtk_file_chooser_dialog_new (_("Select file to import"),
	                                      GTK_WINDOW (toplevel),
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                      _("_Open"), GTK_RESPONSE_ACCEPT,
	                                      NULL);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (!filename) {
			g_warning ("%s: didn't get a filename back from the chooser!", __func__);
			goto out;
		}

		/* Put the file content into JSON config text view. */
		// FIXME: do a cleverer file validity check
		g_file_get_contents (filename, &buf, &buf_len, NULL);
		if (buf_len > 100000) {
			g_free (buf);
			buf = g_strdup (_("Error: file doesn’t contain a valid JSON configuration"));
		}

		buffer = gtk_text_view_get_buffer (priv->json_config_widget);
		gtk_text_buffer_set_text (buffer, buf ? buf : "", -1);

		g_free (filename);
		g_free (buf);
	}

out:
	gtk_widget_destroy (dialog);
}

static void
link_watcher_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (user_data);
	gchar *name;

	/* First hide everything and then show just what we need. */
	gtk_widget_hide (GTK_WIDGET (priv->delay_up));
	gtk_widget_hide (GTK_WIDGET (priv->delay_up_label));
	gtk_widget_hide (GTK_WIDGET (priv->delay_up_ms));
	gtk_widget_hide (GTK_WIDGET (priv->delay_down));
	gtk_widget_hide (GTK_WIDGET (priv->delay_down_label));
	gtk_widget_hide (GTK_WIDGET (priv->delay_down_ms));
	gtk_widget_hide (GTK_WIDGET (priv->send_interval));
	gtk_widget_hide (GTK_WIDGET (priv->send_interval_label));
	gtk_widget_hide (GTK_WIDGET (priv->send_interval_ms));
	gtk_widget_hide (GTK_WIDGET (priv->init_wait));
	gtk_widget_hide (GTK_WIDGET (priv->init_wait_label));
	gtk_widget_hide (GTK_WIDGET (priv->init_wait_ms));
	gtk_widget_hide (GTK_WIDGET (priv->missed_max));
	gtk_widget_hide (GTK_WIDGET (priv->missed_max_label));
	gtk_widget_hide (GTK_WIDGET (priv->source_host));
	gtk_widget_hide (GTK_WIDGET (priv->source_host_label));
	gtk_widget_hide (GTK_WIDGET (priv->target_host));
	gtk_widget_hide (GTK_WIDGET (priv->target_host_label));
	gtk_widget_hide (GTK_WIDGET (priv->validate_active));
	gtk_widget_hide (GTK_WIDGET (priv->validate_inactive));
	gtk_widget_hide (GTK_WIDGET (priv->send_all));

	name = get_combo_box (combo);
	if (g_strcmp0 (name, "master") == 0) {
		/* Leave everything disabled. */
	} else if (g_strcmp0 (name, "ethtool") == 0) {
		gtk_widget_show (GTK_WIDGET (priv->delay_up));
		gtk_widget_show (GTK_WIDGET (priv->delay_up_label));
		gtk_widget_show (GTK_WIDGET (priv->delay_up_ms));
		gtk_widget_show (GTK_WIDGET (priv->delay_down));
		gtk_widget_show (GTK_WIDGET (priv->delay_down_label));
		gtk_widget_show (GTK_WIDGET (priv->delay_down_ms));
	} else if (g_strcmp0 (name, "arp_ping") == 0) {
		gtk_widget_show (GTK_WIDGET (priv->send_interval));
		gtk_widget_show (GTK_WIDGET (priv->send_interval_label));
		gtk_widget_show (GTK_WIDGET (priv->send_interval_ms));
		gtk_widget_show (GTK_WIDGET (priv->init_wait));
		gtk_widget_show (GTK_WIDGET (priv->init_wait_label));
		gtk_widget_show (GTK_WIDGET (priv->init_wait_ms));
		gtk_widget_show (GTK_WIDGET (priv->missed_max));
		gtk_widget_show (GTK_WIDGET (priv->missed_max_label));
		gtk_widget_show (GTK_WIDGET (priv->source_host));
		gtk_widget_show (GTK_WIDGET (priv->source_host_label));
		gtk_widget_show (GTK_WIDGET (priv->target_host));
		gtk_widget_show (GTK_WIDGET (priv->target_host_label));
		gtk_widget_show (GTK_WIDGET (priv->validate_active));
		gtk_widget_show (GTK_WIDGET (priv->validate_inactive));
		gtk_widget_show (GTK_WIDGET (priv->send_all));
	} else if (g_strcmp0 (name, "nsna_ping") == 0) {
		gtk_widget_show (GTK_WIDGET (priv->send_interval));
		gtk_widget_show (GTK_WIDGET (priv->send_interval_label));
		gtk_widget_show (GTK_WIDGET (priv->send_interval_ms));
		gtk_widget_show (GTK_WIDGET (priv->init_wait));
		gtk_widget_show (GTK_WIDGET (priv->init_wait_label));
		gtk_widget_show (GTK_WIDGET (priv->init_wait_ms));
		gtk_widget_show (GTK_WIDGET (priv->missed_max));
		gtk_widget_show (GTK_WIDGET (priv->missed_max_label));
		gtk_widget_show (GTK_WIDGET (priv->target_host));
		gtk_widget_show (GTK_WIDGET (priv->target_host_label));
	} else {
		g_return_if_reached ();
	}
	g_free (name);
}

#if WITH_JANSSON

static gboolean
json_to_dialog (CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	char *json_config;
	json_t *json;
	json_error_t json_error;
	int ret;
	gboolean success = TRUE;
	GtkTextIter start, end;
	GtkTextBuffer *buffer;
	GError *error = NULL;
	/* General */
	int queue_id = -1;
	int port_prio = -1;
	int port_sticky = FALSE;
	int lacp_port_prio = -1;
	int lacp_port_key = -1;
	/* Link Watch */
	const char *link_watch_name = "";
	int link_watch_delay_up = -1;
	int link_watch_delay_down = -1;
	int link_watch_interval = -1;
	int link_watch_init_wait = -1;
	int link_watch_missed_max = -1;
	const char *link_watch_source_host = "";
	const char *link_watch_target_host = "";
	int link_watch_validate_active = FALSE;
	int link_watch_validate_inactive = FALSE;
	int link_watch_send_always = FALSE;

	buffer = gtk_text_view_get_buffer (priv->json_config_widget);
	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, -1);
	json_config = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	if (strcmp (json_config, "") == 0) {
		/* Initial empty configuration */
		json = json_object ();
	} else {
		json = json_loads (json_config, 0, &json_error);
	}

	if (!json) {
		g_message ("Failed to parse JSON: %s on line %d", json_error.text, json_error.line);
		success = FALSE;
	}

	/* For simplicity, we proceed with json==NULL. The attempt to
	 * unpack will produce an error which we'll ignore. */
	g_free (json_config);
	ret = json_unpack_ex (json, &json_error, 0,
	                      "{"
	                      " s?:i,"
	                      " s?:i,"
	                      " s?:b,"
	                      " s?:i,"
	                      " s?:i,"
	                      " s?:{s?:s, s?:i, s?:i, s?:i, s?:i, s?:i, s?:s, s?:s, s?:b, s?:b, s?:b !}"
	                      "!}",
	                      "queue_id", &queue_id,
	                      "prio", &port_prio,
	                      "sticky", &port_sticky,
	                      "lacp_prio", &lacp_port_prio,
	                      "lacp_key", &lacp_port_key,
	                      "link_watch",
	                          "name", &link_watch_name,
	                          "delay_up", &link_watch_delay_up,
	                          "delay_down", &link_watch_delay_down,
	                          "interval", &link_watch_interval,
	                          "init_wait", &link_watch_init_wait,
	                          "missed_max", &link_watch_missed_max,
	                          "source_host", &link_watch_source_host,
	                          "target_host", &link_watch_target_host,
	                          "validate_active", &link_watch_validate_active,
	                          "validate_inactive", &link_watch_validate_inactive,
	                          "send_always", &link_watch_send_always);

	if (success == TRUE && ret == -1) {
		g_message ("Failed to parse JSON: %s on line %d", json_error.text, json_error.line);
		success = FALSE;
	}

	/* We proceed setting the form fields even in case we couldn't unpack.
	 * That way we'll get at least sensible default values. Editing will
	 * be disabled anyway. */
	gtk_spin_button_set_value (priv->queue_id, queue_id);
	gtk_spin_button_set_value (priv->port_prio, port_prio);
	gtk_toggle_button_set_active (priv->port_sticky, port_sticky);
	gtk_spin_button_set_value (priv->lacp_port_prio, lacp_port_prio);
	gtk_spin_button_set_value (priv->lacp_port_key, lacp_port_key);
	if (!set_combo_box (priv->link_watcher_name, link_watch_name, &error)) {
		g_message ("Cannot read link watcher name: %s", error->message);
		g_clear_error (&error);
		success = FALSE;
	}
	gtk_spin_button_set_value (priv->delay_up, link_watch_delay_up);
	gtk_spin_button_set_value (priv->delay_down, link_watch_delay_down);
	gtk_spin_button_set_value (priv->send_interval, link_watch_interval);
	gtk_spin_button_set_value (priv->init_wait, link_watch_init_wait);
	gtk_spin_button_set_value (priv->missed_max, link_watch_missed_max);
	gtk_entry_set_text (priv->source_host, link_watch_source_host);
	gtk_entry_set_text (priv->target_host, link_watch_target_host);
	gtk_toggle_button_set_active (priv->validate_active, link_watch_validate_active);
	gtk_toggle_button_set_active (priv->validate_inactive, link_watch_validate_inactive);
	gtk_toggle_button_set_active (priv->send_all, link_watch_send_always);

	if (success) {
		/* Enable editing. */
		gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 0), 1);
		gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 1), 1);
	}

	json_decref (json);
	return success;
}

static void
maybe_set_str (json_t *json, const char *key, const char *str)
{
	if (str && *str)
		json_object_set_new(json, key, json_string (str));
}

static void
maybe_set_int (json_t *json, const char *key, int num)
{
	if (num != -1)
		json_object_set_new(json, key, json_integer (num));
}

static void
maybe_set_json (json_t *json, const char *key, json_t *val)
{
	if (   (json_is_object (val) && json_object_size (val))
	    || (json_is_array (val) && json_array_size (val))) {
		json_object_set_new (json, key, val);
	} else {
		json_decref (val);
	}
}

static void
maybe_set_true (json_t *json, const char *key, int set)
{
	if (set)
		json_object_set_new(json, key, json_true ());
	else
		json_object_set_new(json, key, json_false ());
}

static void
dialog_to_json (CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	json_t *json;
	json_t *obj;
	gchar *tmp;
	char *json_config;
	GtkTextBuffer *buffer;

	/* If the JSON is being edited, don't overwrite it. */
	if (!gtk_widget_get_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 0)))
		return;

	/* Disable editing via form, until converted back from JSON. */
	gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 0), 0);
	gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 1), 0);

	json = json_object ();
	maybe_set_int (json, "queue_id", gtk_spin_button_get_value_as_int (priv->queue_id));
	maybe_set_int (json, "prio", gtk_spin_button_get_value_as_int (priv->port_prio));
	if (gtk_toggle_button_get_active (priv->port_sticky))
		json_object_set_new(json, "sticky", json_true ());
	maybe_set_int (json, "lacp_prio", gtk_spin_button_get_value_as_int (priv->lacp_port_prio));
	maybe_set_int (json, "lacp_key", gtk_spin_button_get_value_as_int (priv->lacp_port_key));

	obj = json_object ();
	tmp = get_combo_box (priv->link_watcher_name);
	if (g_strcmp0 (tmp, "master") != 0) {
		maybe_set_str (obj, "name", tmp);
		if (g_strcmp0 (tmp, "ethtool") == 0) {
			maybe_set_int (obj, "delay_up", gtk_spin_button_get_value (priv->delay_up));
			maybe_set_int (obj, "delay_down", gtk_spin_button_get_value (priv->delay_down));
		} else if (g_strcmp0 (tmp, "arp_ping") == 0) {
			maybe_set_int (obj, "interval", gtk_spin_button_get_value (priv->send_interval));
			maybe_set_int (obj, "init_wait", gtk_spin_button_get_value (priv->init_wait));
			maybe_set_int (obj, "missed_max", gtk_spin_button_get_value (priv->missed_max));
			maybe_set_str (obj, "source_host", gtk_entry_get_text (priv->source_host));
			maybe_set_str (obj, "target_host", gtk_entry_get_text (priv->target_host));
			maybe_set_true (obj, "validate_active", gtk_toggle_button_get_active (priv->validate_active));
			maybe_set_true (obj, "validate_inactive", gtk_toggle_button_get_active (priv->validate_inactive));
			maybe_set_true (obj, "send_always", gtk_toggle_button_get_active (priv->send_all));
		} else if (g_strcmp0 (tmp, "nsna_ping") == 0) {
			maybe_set_int (obj, "interval", gtk_spin_button_get_value (priv->send_interval));
			maybe_set_int (obj, "init_wait", gtk_spin_button_get_value (priv->init_wait));
			maybe_set_int (obj, "missed_max", gtk_spin_button_get_value (priv->missed_max));
			maybe_set_str (obj, "target_host", gtk_entry_get_text (priv->target_host));
		} else {
			g_return_if_reached ();
		}
	}
	maybe_set_json (json, "link_watch", obj);
	g_free (tmp);

	json_config = json_dumps (json, JSON_INDENT (4));
	json_decref (json);

	buffer = gtk_text_view_get_buffer (priv->json_config_widget);
	gtk_text_buffer_set_text (buffer, json_config, -1);
	free (json_config);
}

#else /* WITH_JANSSON */

static gboolean
json_to_dialog (CEPageTeamPort *self)
{
	return FALSE;
}

static void
dialog_to_json (CEPageTeamPort *self)
{
}

#endif /* WITH_JANSSON */

static void
advanced_button_clicked_cb (GtkWidget *button, gpointer user_data)
{
	CEPageTeamPort *self = CE_PAGE_TEAM_PORT (user_data);
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	NMSettingTeamPort *s_port = priv->setting;
	GtkWidget *toplevel;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *json_config = NULL;

	toplevel = gtk_widget_get_toplevel (CE_PAGE (self)->page);
	g_return_if_fail (toplevel);
	gtk_window_set_transient_for (GTK_WINDOW (priv->advanced_dialog), GTK_WINDOW (toplevel));
	g_return_if_fail (gtk_widget_is_toplevel (toplevel));

	/* Load in the JSON from settings to dialog. */
	buffer = gtk_text_view_get_buffer (priv->json_config_widget);
	gtk_text_buffer_set_text (buffer, nm_setting_team_port_get_config (s_port) ?: "", -1);

	/* Fill in the form fields. */
	if (json_to_dialog (self)) {
		gtk_notebook_set_current_page (priv->advanced_notebook, 0);
	} else {
		/* First disable the pages, so that potentially
		 * inconsistent changes are not propageated to JSON. */
		gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 0), 0);
		gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 1), 0);
		gtk_notebook_set_current_page (priv->advanced_notebook, 2);
	}

	link_watcher_changed (priv->link_watcher_name, self);

	if (gtk_dialog_run (priv->advanced_dialog) == GTK_RESPONSE_OK) {
		dialog_to_json (self);

		/* Set the JSON from the dialog to setting. */
		gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
		gtk_text_buffer_get_iter_at_offset (buffer, &end, -1);
		json_config = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

		g_object_set (priv->setting,
		              NM_SETTING_TEAM_CONFIG,
		              g_strcmp0 (json_config, "") == 0 ? NULL : json_config,
		              NULL);
		g_free (json_config);
		ce_page_changed (CE_PAGE (self));
	}
	gtk_widget_hide (GTK_WIDGET (priv->advanced_dialog));
}

static gboolean
switch_page (GtkNotebook *notebook,
             GtkWidget   *page,
             guint        page_num,
             gpointer     user_data)
{
	CEPageTeamPort *self = CE_PAGE_TEAM_PORT (user_data);

	/* Keep the JSON and the form in sync if possible. */
	if (gtk_notebook_get_current_page (notebook) == 2)
		json_to_dialog (self);
	else if (page_num == 2)
		dialog_to_json (self);

	return TRUE;
}

static void
populate_ui (CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	NMSettingTeamPort *s_port = priv->setting;
	GtkTextBuffer *buffer;
	const char *json_config;

	buffer = gtk_text_view_get_buffer (priv->json_config_widget);
	json_config = nm_setting_team_port_get_config (s_port);
	gtk_text_buffer_set_text (buffer, json_config ? json_config : "", -1);

	g_signal_connect (priv->import_config_button, "clicked", G_CALLBACK (import_button_clicked_cb), self);
	g_signal_connect (priv->link_watcher_name, "changed", G_CALLBACK (link_watcher_changed), self);
	g_signal_connect (priv->advanced_button, "clicked", G_CALLBACK (advanced_button_clicked_cb), self);
	g_signal_connect (priv->advanced_notebook, "switch-page", G_CALLBACK (switch_page), self);
}

static void
finish_setup (CEPageTeamPort *self, gpointer unused, GError *error, gpointer user_data)
{
	if (error)
		return;

	populate_ui (self);
}

CEPage *
ce_page_team_port_new (NMConnectionEditor *editor,
                       NMConnection *connection,
                       GtkWindow *parent_window,
                       NMClient *client,
                       const char **out_secrets_setting_name,
                       GError **error)
{
	CEPageTeamPort *self;
	CEPageTeamPortPrivate *priv;

	self = CE_PAGE_TEAM_PORT (ce_page_new (CE_TYPE_PAGE_TEAM_PORT,
	                                       editor,
	                                       connection,
	                                       parent_window,
	                                       client,
	                                       "/org/freedesktop/network-manager-applet/ce-page-team-port.ui",
	                                       "TeamPortPage",
	                                       /* Translators: a "Team Port" is a network
	                                        * device that is part of a team.
	                                        */
	                                       _("Team Port")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load team port user interface."));
		return NULL;
	}

	team_port_private_init (self);
	priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_team_port (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_TEAM_PORT (nm_setting_team_port_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *json_config;

	buffer = gtk_text_view_get_buffer (priv->json_config_widget);
	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, -1);
	json_config = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	if (g_strcmp0 (json_config, "") == 0)
		json_config = NULL;
	g_object_set (priv->setting,
	              NM_SETTING_TEAM_PORT_CONFIG, json_config,
	              NULL);
	g_free (json_config);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageTeamPort *self = CE_PAGE_TEAM_PORT (page);
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_team_port_init (CEPageTeamPort *self)
{
}

static void
ce_page_team_port_class_init (CEPageTeamPortClass *team_port_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (team_port_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (team_port_class);

	g_type_class_add_private (object_class, sizeof (CEPageTeamPortPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}
