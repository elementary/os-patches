/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2013 Jiri Pirko <jiri@resnulli.us>
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
 * Copyright 2013 Jiri Pirko <jiri@resnulli.us>
 * Copyright 2013 - 2017  Red Hat, Inc.
 */

#include "nm-default.h"

#include <stdlib.h>
#if WITH_JANSSON
#include <jansson.h>
#endif

#include "page-team.h"
#include "page-infiniband.h"
#include "nm-connection-editor.h"
#include "connection-helpers.h"

G_DEFINE_TYPE (CEPageTeam, ce_page_team, CE_TYPE_PAGE_MASTER)

#define CE_PAGE_TEAM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_TEAM, CEPageTeamPrivate))

typedef struct {
	NMSettingTeam *setting;
	NMSettingWired *wired;

	int slave_arptype;

	GtkTextView *json_config_widget;
	GtkWidget *import_config_button;

	GtkSpinButton *mtu;
	GtkButton *advanced_button;
	GtkDialog *advanced_dialog;
	GtkNotebook *advanced_notebook;

	/* General */
	GtkSpinButton *notify_peers_count;
	GtkSpinButton *notify_peers_interval;
	GtkSpinButton *mcast_rejoin_count;
	GtkSpinButton *mcast_rejoin_interval;
	GtkEntry *hwaddr;

	/* Runner */
	GtkComboBox *runner_name;
	GtkComboBox *hwaddr_policy;
	GtkLabel *hwaddr_policy_label;
	GtkFrame *tx_hash_frame;
	GtkTreeView *tx_hash;
	GtkComboBox *tx_balancer;
	GtkLabel *tx_balancer_label;
	GtkSpinButton *tx_balancing_interval;
	GtkLabel *tx_balancing_interval_label;
	GtkToggleButton *active;
	GtkToggleButton *fast_rate;
	GtkSpinButton *system_priority;
	GtkLabel *system_priority_label;
	GtkSpinButton *minimum_ports;
	GtkLabel *minimum_ports_label;
	GtkComboBox *agg_selection_policy;
	GtkLabel *agg_selection_policy_label;

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
} CEPageTeamPrivate;

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

/* Set active items to values of "type" in a tree view with a ["type", "name"]
 * model (for setting the tx_hash). */
static gboolean
select_in_tree (GtkTreeView *tree, json_t *arr, GError **error)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	const gchar *name;
	GtkTreeSelection *selection;
	size_t i;

	model = gtk_tree_view_get_model (tree);
	selection = gtk_tree_view_get_selection (tree);

	gtk_tree_selection_unselect_all (selection);

	if (!arr)
		return TRUE;

	for (i = 0; i < json_array_size (arr); i++) {
		json_t *val = json_array_get (arr, i);
		const char *str;

		if (!json_is_string (val)) {
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC,
			                     "Expected a string.");
			return FALSE;
		}

		str = json_string_value (val);

		for (valid = gtk_tree_model_get_iter_first (model, &iter);
		     valid;
		     valid = gtk_tree_model_iter_next (model, &iter)) {
			gtk_tree_model_get (model, &iter, 0, &name, -1);
			if (strcmp (name, str) == 0) {
				gtk_tree_selection_select_iter (selection, &iter);
				break;
			}
		}

		if (!valid) {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC,
			             "Unrecognized value \"%s\".", str);
			return FALSE;
		}
	}

	return TRUE;
}

/* Set active items to values of "type" in a tree view with a ["type", "name"]
 * model (for setting the tx_hash). */
static json_t *
selected_in_tree (GtkTreeView *tree)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	const gchar *name;
	GtkTreeSelection *selection;
	json_t *arr = json_array ();

	model = gtk_tree_view_get_model (tree);
	selection = gtk_tree_view_get_selection (tree);

	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid;
	     valid = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, 0, &name, -1);
		if (!gtk_tree_selection_iter_is_selected (selection, &iter))
			continue;
		json_array_append_new (arr, json_string (name));
	}

	return arr;
}

#endif

static void
team_private_init (CEPageTeam *self)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->json_config_widget = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "team_json_config"));
	priv->import_config_button = GTK_WIDGET (gtk_builder_get_object (builder, "import_config_button"));

	priv->mtu = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "team_mtu"));

	priv->advanced_button = GTK_BUTTON (gtk_builder_get_object (builder, "advanced_button"));
	priv->advanced_dialog = GTK_DIALOG (gtk_builder_get_object (builder, "advanced_dialog"));
	gtk_window_set_modal (GTK_WINDOW (priv->advanced_dialog), TRUE);
	priv->advanced_notebook = GTK_NOTEBOOK (gtk_builder_get_object (builder, "advanced_notebook"));

	/* General */
	priv->notify_peers_count = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "notify_peers_count"));
	priv->notify_peers_interval = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "notify_peers_interval"));
	priv->mcast_rejoin_count = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "mcast_rejoin_count"));
	priv->mcast_rejoin_interval = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "mcast_rejoin_interval"));
	priv->hwaddr = GTK_ENTRY (gtk_builder_get_object (builder, "hwaddr"));

	/* Runner */
	priv->runner_name = GTK_COMBO_BOX (gtk_builder_get_object (builder, "team_runner"));
	priv->active = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "active"));
	priv->fast_rate = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "fast_rate"));
	priv->system_priority = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "system_priority"));
	priv->system_priority_label = GTK_LABEL (gtk_builder_get_object (builder, "system_priority_label"));
	priv->minimum_ports = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "minimum_ports"));
	priv->minimum_ports_label = GTK_LABEL (gtk_builder_get_object (builder, "minimum_ports_label"));
	priv->agg_selection_policy = GTK_COMBO_BOX (gtk_builder_get_object (builder, "agg_selection_policy"));
	priv->agg_selection_policy_label = GTK_LABEL (gtk_builder_get_object (builder, "agg_selection_policy_label"));
	priv->hwaddr_policy = GTK_COMBO_BOX (gtk_builder_get_object (builder, "hwaddr_policy"));
	priv->hwaddr_policy_label = GTK_LABEL (gtk_builder_get_object (builder, "hwaddr_policy_label"));
	priv->tx_hash_frame = GTK_FRAME (gtk_builder_get_object (builder, "tx_hash_frame"));
	priv->tx_hash = GTK_TREE_VIEW (gtk_builder_get_object (builder, "tx_hash"));
	priv->tx_balancer = GTK_COMBO_BOX (gtk_builder_get_object (builder, "tx_balancer"));
	priv->tx_balancer_label = GTK_LABEL (gtk_builder_get_object (builder, "tx_balancer_label"));
	priv->tx_balancing_interval = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "tx_balancing_interval"));
	priv->tx_balancing_interval_label = GTK_LABEL (gtk_builder_get_object (builder, "tx_balancing_interval_label"));

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

	ce_spin_default_val (priv->notify_peers_count, -1);
	ce_spin_default_val (priv->notify_peers_interval, -1);
	ce_spin_default_val (priv->notify_peers_interval, -1);
	ce_spin_default_val (priv->mcast_rejoin_count, -1);
	ce_spin_default_val (priv->mcast_rejoin_interval, -1);
	ce_spin_default_val (priv->tx_balancing_interval, -1);
	ce_spin_default_val (priv->system_priority, -1);
	ce_spin_default_val (priv->minimum_ports, -1);
	ce_spin_default_val (priv->delay_up, -1);
	ce_spin_default_val (priv->delay_down, -1);
	ce_spin_default_val (priv->send_interval, -1);
	ce_spin_default_val (priv->init_wait, -1);
	ce_spin_default_val (priv->missed_max, -1);
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
import_button_clicked_cb (GtkWidget *widget, CEPageTeam *self)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
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
runner_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (user_data);
	gchar *name;

	/* First hide everything and then show just what we need. */
	gtk_widget_hide (GTK_WIDGET (priv->hwaddr_policy));
	gtk_widget_hide (GTK_WIDGET (priv->hwaddr_policy_label));
	gtk_widget_hide (GTK_WIDGET (priv->tx_hash_frame));
	gtk_widget_hide (GTK_WIDGET (priv->tx_hash));
	gtk_widget_hide (GTK_WIDGET (priv->tx_balancer));
	gtk_widget_hide (GTK_WIDGET (priv->tx_balancer_label));
	gtk_widget_hide (GTK_WIDGET (priv->tx_balancing_interval));
	gtk_widget_hide (GTK_WIDGET (priv->tx_balancing_interval_label));
	gtk_widget_hide (GTK_WIDGET (priv->active));
	gtk_widget_hide (GTK_WIDGET (priv->fast_rate));
	gtk_widget_hide (GTK_WIDGET (priv->system_priority));
	gtk_widget_hide (GTK_WIDGET (priv->system_priority_label));
	gtk_widget_hide (GTK_WIDGET (priv->minimum_ports));
	gtk_widget_hide (GTK_WIDGET (priv->minimum_ports_label));
	gtk_widget_hide (GTK_WIDGET (priv->agg_selection_policy));
	gtk_widget_hide (GTK_WIDGET (priv->agg_selection_policy_label));

	name = get_combo_box (combo);
	if (g_strcmp0 (name, "broadcast") == 0 || g_strcmp0 (name, "roundrobin") == 0) {
		/* No options for these. */
	} else if (g_strcmp0 (name, "activebackup") == 0) {
		gtk_widget_show (GTK_WIDGET (priv->hwaddr_policy));
		gtk_widget_show (GTK_WIDGET (priv->hwaddr_policy_label));
	} else if (g_strcmp0 (name, "loadbalance") == 0 || g_strcmp0 (name, "lacp") == 0) {
		gtk_widget_show (GTK_WIDGET (priv->tx_hash_frame));
		gtk_widget_show (GTK_WIDGET (priv->tx_hash));
		gtk_widget_show (GTK_WIDGET (priv->tx_balancer));
		gtk_widget_show (GTK_WIDGET (priv->tx_balancer_label));
		gtk_widget_show (GTK_WIDGET (priv->tx_balancing_interval));
		gtk_widget_show (GTK_WIDGET (priv->tx_balancing_interval_label));

		if (g_strcmp0 (name, "lacp") == 0) {
			gtk_widget_show (GTK_WIDGET (priv->active));
			gtk_widget_show (GTK_WIDGET (priv->fast_rate));
			gtk_widget_show (GTK_WIDGET (priv->system_priority));
			gtk_widget_show (GTK_WIDGET (priv->system_priority_label));
			gtk_widget_show (GTK_WIDGET (priv->minimum_ports));
			gtk_widget_show (GTK_WIDGET (priv->minimum_ports_label));
			gtk_widget_show (GTK_WIDGET (priv->agg_selection_policy));
			gtk_widget_show (GTK_WIDGET (priv->agg_selection_policy_label));
		}
	} else {
		g_return_if_reached ();
	}
	g_free (name);
}

static void
link_watcher_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (user_data);
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
	if (g_strcmp0 (name, "ethtool") == 0) {
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
json_to_dialog (CEPageTeam *self)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
	char *json_config;
	json_t *json;
	json_error_t json_error;
	int ret;
	gboolean success = TRUE;
	GtkTextIter start, end;
	GtkTextBuffer *buffer;
	GError *error = NULL;
	/* General */
	int notify_peers_count = -1;
	int notify_peers_interval = -1;
	int mcast_rejoin_count = -1;
	int mcast_rejoin_interval = -1;
	const char *hwaddr = "";
	/* Runner */
	const char *runner_name = "";
	const char *runner_hwaddr_policy = "";
	json_t *runner_tx_hash = NULL;
	const char *runner_tx_balancer_name = "";
	int runner_tx_balancer_balancing_interval = -1;
	int runner_active = TRUE;
	int runner_fast_rate = FALSE;
	int runner_sys_prio = -1;
	int runner_min_ports = -1;
	const char *runner_agg_select_policy = "";
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
	                      " s?:s,"
	                      " s?:{s?:i, s?:i !},"
	                      " s?:{s?:i, s?:i !},"
	                      " s?:{s?:s, s?:s, s?:o, s?:{s?:s, s?:i !}, s?:b, s?:b, s?:i, s?:i, s?:s !},"
	                      " s?:{s?:s, s?:i, s?:i, s?:i, s?:i, s?:i, s?:s, s?:s, s?:b, s?:b, s?:b !}"
	                      "!}",
	                      "hwaddr", &hwaddr,
	                      "notify_peers",
	                          "interval", &notify_peers_interval,
	                          "count", &notify_peers_count,
	                      "mcast_rejoin",
	                          "count", &mcast_rejoin_count,
	                          "interval", &mcast_rejoin_interval,
	                      "runner",
	                          "name", &runner_name,
	                          "hwaddr_policy", &runner_hwaddr_policy,
	                          "tx_hash", &runner_tx_hash,
	                          "tx_balancer",
	                              "name", &runner_tx_balancer_name,
	                              "balancing_interval", &runner_tx_balancer_balancing_interval,
	                          "active", &runner_active,
	                          "fast_rate", &runner_fast_rate,
	                          "sys_prio", &runner_sys_prio,
	                          "min_ports", &runner_min_ports,
	                          "agg_select_policy", &runner_agg_select_policy,
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
	gtk_entry_set_text (priv->hwaddr, hwaddr);
	gtk_spin_button_set_value (priv->notify_peers_count, notify_peers_count);
	gtk_spin_button_set_value (priv->notify_peers_interval,	notify_peers_interval);
	gtk_spin_button_set_value (priv->mcast_rejoin_count, mcast_rejoin_count);
	gtk_spin_button_set_value (priv->mcast_rejoin_interval, mcast_rejoin_interval);
	if (!set_combo_box (priv->link_watcher_name, link_watch_name, &error)) {
		g_message ("Cannot read link watcher name: %s", error->message);
		g_clear_error (&error);
		success = FALSE;
	}
	if (!set_combo_box (priv->runner_name, runner_name, &error)) {
		g_message ("Cannot read runner name: %s", error->message);
		g_clear_error (&error);
		success = FALSE;
	}
	if (!set_combo_box (priv->hwaddr_policy, runner_hwaddr_policy, &error)) {
		g_message ("Cannot read hardware address policy: %s", error->message);
		g_clear_error (&error);
		success = FALSE;
	}
	if (!select_in_tree (priv->tx_hash, runner_tx_hash, &error)) {
		g_message ("Cannot read transmission hash: %s", error->message);
		g_clear_error (&error);
		success = FALSE;
	}
	if (!set_combo_box (priv->tx_balancer, runner_tx_balancer_name, &error)) {
		g_message ("Cannot read balancer name: %s", error->message);
		g_clear_error (&error);
		success = FALSE;
	}
	gtk_spin_button_set_value (priv->tx_balancing_interval, runner_tx_balancer_balancing_interval);
	gtk_toggle_button_set_active (priv->active, runner_active);
	gtk_toggle_button_set_active (priv->fast_rate, runner_fast_rate);
	gtk_spin_button_set_value (priv->system_priority, runner_sys_prio);
	gtk_spin_button_set_value (priv->minimum_ports, runner_min_ports);
	if (!set_combo_box (priv->agg_selection_policy, runner_agg_select_policy, &error)) {
		g_message ("Cannot read aggergator select policy: %s", error->message);
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
		gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 2), 1);
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
dialog_to_json (CEPageTeam *self)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
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
	gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 2), 0);

	json = json_object ();
	maybe_set_str (json, "hwaddr", gtk_entry_get_text (priv->hwaddr));

	obj = json_object ();
	maybe_set_int (obj, "count", gtk_spin_button_get_value_as_int (priv->notify_peers_count));
	maybe_set_int (obj, "interval", gtk_spin_button_get_value_as_int (priv->notify_peers_interval));
	maybe_set_json (json, "notify_peers", obj);

	obj = json_object ();
	maybe_set_int (obj, "count", gtk_spin_button_get_value_as_int (priv->mcast_rejoin_count));
	maybe_set_int (obj, "interval", gtk_spin_button_get_value_as_int (priv->mcast_rejoin_interval));
	maybe_set_json (json, "mcast_rejoin", obj);

	obj = json_object ();
	tmp = get_combo_box (priv->runner_name);
	maybe_set_str (obj, "name", tmp);
	if (g_strcmp0 (tmp, "roundrobin") == 0) {
	} else if (g_strcmp0 (tmp, "activebackup") == 0) {
		maybe_set_str (obj, "hwaddr_policy", get_combo_box (priv->hwaddr_policy));
	} else if (g_strcmp0 (tmp, "loadbalance") == 0 || g_strcmp0 (tmp, "lacp") == 0) {
		json_t *obj2;
		gchar *str;

		obj2 = json_object ();

		/* Glade won't let us have a "" in the model :( */
		str = get_combo_box (priv->tx_balancer);
		if (strcmp (str, "none"))
			maybe_set_str (obj2, "name", str);
		g_free (str);

		maybe_set_int (obj2, "balancing_interval", gtk_spin_button_get_value_as_int (priv->tx_balancing_interval));
		maybe_set_json (obj, "tx_balancer", obj2);
		maybe_set_json (obj, "tx_hash", selected_in_tree (priv->tx_hash));

		if (g_strcmp0 (tmp, "lacp") == 0) {
			maybe_set_true (obj, "active", gtk_toggle_button_get_active (priv->active));
			maybe_set_true (obj, "fast_rate", gtk_toggle_button_get_active (priv->fast_rate));
			maybe_set_int (obj, "sys_prio", gtk_spin_button_get_value (priv->system_priority));
			maybe_set_int (obj, "min_ports", gtk_spin_button_get_value (priv->minimum_ports));
			str = get_combo_box (priv->agg_selection_policy);
			maybe_set_str (obj, "agg_select_policy", str);
			g_free (str);
		}
	}
	maybe_set_json (json, "runner", obj);
	g_free (tmp);

	obj = json_object ();
	tmp = get_combo_box (priv->link_watcher_name);
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
json_to_dialog (CEPageTeam *self)
{
	return FALSE;
}

static void
dialog_to_json (CEPageTeam *self)
{
}

#endif /* WITH_JANSSON */

static void
advanced_button_clicked_cb (GtkWidget *button, gpointer user_data)
{
	CEPageTeam *self = CE_PAGE_TEAM (user_data);
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
	NMSettingTeam *s_team = priv->setting;
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
	gtk_text_buffer_set_text (buffer, nm_setting_team_get_config (s_team) ?: "", -1);

	/* Fill in the form fields. */
	if (json_to_dialog (self)) {
		gtk_notebook_set_current_page (priv->advanced_notebook, 0);
	} else {
		/* First disable the pages, so that potentially
		 * inconsistent changes are not propageated to JSON. */
		gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 0), 0);
		gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 1), 0);
		gtk_widget_set_sensitive (gtk_notebook_get_nth_page (priv->advanced_notebook, 2), 0);
		gtk_notebook_set_current_page (priv->advanced_notebook, 3);
	}

	runner_changed (priv->runner_name, self);
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
	CEPageTeam *self = CE_PAGE_TEAM (user_data);

	/* Keep the JSON and the form in sync if possible. */
	if (gtk_notebook_get_current_page (notebook) == 3)
		json_to_dialog (self);
	else if (page_num == 3)
		dialog_to_json (self);

	return TRUE;
}

static void
populate_ui (CEPageTeam *self)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
	guint32 mtu_def, mtu_val;

	/* MTU */
	if (priv->wired) {
		mtu_def = ce_get_property_default (NM_SETTING (priv->wired), NM_SETTING_WIRED_MTU);
		mtu_val = nm_setting_wired_get_mtu (priv->wired);
	} else {
		mtu_def = mtu_val = 0;
	}
	ce_spin_automatic_val (priv->mtu, mtu_def);
	gtk_spin_button_set_value (priv->mtu, (gdouble) mtu_val);

	g_signal_connect (priv->import_config_button, "clicked", G_CALLBACK (import_button_clicked_cb), self);
	g_signal_connect (priv->runner_name, "changed", G_CALLBACK (runner_changed), self);
	g_signal_connect (priv->link_watcher_name, "changed", G_CALLBACK (link_watcher_changed), self);
	g_signal_connect (priv->advanced_button, "clicked", G_CALLBACK (advanced_button_clicked_cb), self);
	g_signal_connect (priv->advanced_notebook, "switch-page", G_CALLBACK (switch_page), self);
}

static void
connection_removed (CEPageMaster *master, NMConnection *connection)
{
	CEPageTeam *self = CE_PAGE_TEAM (master);
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);

	if (!ce_page_master_has_slaves (master))
		priv->slave_arptype = ARPHRD_VOID;
}

static void
connection_added (CEPageMaster *master, NMConnection *connection)
{
	CEPageTeam *self = CE_PAGE_TEAM (master);
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);

	if (nm_connection_is_type (connection, NM_SETTING_INFINIBAND_SETTING_NAME))
		priv->slave_arptype = ARPHRD_INFINIBAND;
	else
		priv->slave_arptype = ARPHRD_ETHER;
}

static void
create_connection (CEPageMaster *master, NMConnection *connection)
{
	NMSetting *s_port;

	s_port = nm_connection_get_setting (connection, NM_TYPE_SETTING_TEAM_PORT);
	if (!s_port) {
		s_port = nm_setting_team_port_new ();
		nm_connection_add_setting (connection, s_port);
	}
}

static gboolean
connection_type_filter (FUNC_TAG_NEW_CONNECTION_TYPE_FILTER_IMPL,
                        GType type,
                        gpointer self)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);

	if (!nm_utils_check_virtual_device_compatibility (NM_TYPE_SETTING_TEAM, type))
		return FALSE;

	/* Can only have connections of a single arptype. Note that we don't
	 * need to check the reverse case here since we don't need to call
	 * new_connection_dialog() in the InfiniBand case.
	 */
	if (   priv->slave_arptype == ARPHRD_ETHER
	    && type == NM_TYPE_SETTING_INFINIBAND)
		return FALSE;

	return TRUE;
}

static void
add_slave (CEPageMaster *master, NewConnectionResultFunc result_func)
{
	CEPageTeam *self = CE_PAGE_TEAM (master);
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (CE_PAGE (self)->page);
	g_return_if_fail (toplevel);
	g_return_if_fail (gtk_widget_is_toplevel (toplevel));

	if (priv->slave_arptype == ARPHRD_INFINIBAND) {
		new_connection_of_type (GTK_WINDOW (toplevel),
		                        NULL,
		                        NULL,
		                        NULL,
		                        CE_PAGE (self)->client,
		                        infiniband_connection_new,
		                        result_func,
		                        master);
	} else {
		new_connection_dialog (GTK_WINDOW (toplevel),
		                       CE_PAGE (self)->client,
		                       connection_type_filter,
		                       result_func,
		                       master);
	}
}

static void
finish_setup (CEPageTeam *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self);

	g_signal_connect (priv->mtu, "value-changed", G_CALLBACK (stuff_changed), self);
}

CEPage *
ce_page_team_new (NMConnectionEditor *editor,
                  NMConnection *connection,
                  GtkWindow *parent_window,
                  NMClient *client,
                  const char **out_secrets_setting_name,
                  GError **error)
{
	CEPageTeam *self;
	CEPageTeamPrivate *priv;

	self = CE_PAGE_TEAM (ce_page_new (CE_TYPE_PAGE_TEAM,
	                                  editor,
	                                  connection,
	                                  parent_window,
	                                  client,
	                                  "/org/freedesktop/network-manager-applet/ce-page-team.ui",
	                                  "TeamPage",
	                                  _("Team")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC,
		                     _("Could not load team user interface."));
		return NULL;
	}

	team_private_init (self);
	priv = CE_PAGE_TEAM_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_team (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_TEAM (nm_setting_team_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}
	priv->wired = nm_connection_get_setting_wired (connection);

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageTeam *self)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
	NMConnection *connection = CE_PAGE (self)->connection;
	guint32 mtu;

	mtu = gtk_spin_button_get_value_as_int (priv->mtu);
	if (mtu && !priv->wired) {
		priv->wired = NM_SETTING_WIRED (nm_setting_wired_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->wired));
	}
	if (priv->wired)
		g_object_set (priv->wired, NM_SETTING_WIRED_MTU, mtu, NULL);

}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageTeam *self = CE_PAGE_TEAM (page);
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);

	if (!CE_PAGE_CLASS (ce_page_team_parent_class)->ce_page_validate_v (page, connection, error))
		return FALSE;

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), connection, error);
}

static void
ce_page_team_init (CEPageTeam *self)
{
	CEPageTeamPrivate *priv = CE_PAGE_TEAM_GET_PRIVATE (self);
	CEPageMaster *master = CE_PAGE_MASTER (self);

	priv->slave_arptype = ARPHRD_VOID;
	master->aggregating = TRUE;
}

static void
ce_page_team_class_init (CEPageTeamClass *team_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (team_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (team_class);
	CEPageMasterClass *master_class = CE_PAGE_MASTER_CLASS (team_class);

	g_type_class_add_private (object_class, sizeof (CEPageTeamPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
	master_class->create_connection = create_connection;
	master_class->connection_added = connection_added;
	master_class->connection_removed = connection_removed;
	master_class->add_slave = add_slave;
}


void
team_connection_new (FUNC_TAG_PAGE_NEW_CONNECTION_IMPL,
                     GtkWindow *parent,
                     const char *detail,
                     gpointer detail_data,
                     NMConnection *connection,
                     NMClient *client,
                     PageNewConnectionResultFunc result_func,
                     gpointer user_data)
{
	NMSettingConnection *s_con;
	int team_num, num, i;
	const GPtrArray *connections;
	NMConnection *conn2;
	const char *iface;
	char *my_iface;
	gs_unref_object NMConnection *connection_tmp = NULL;

	connection = _ensure_connection_other (connection, &connection_tmp);
	ce_page_complete_connection (connection,
	                             _("Team connection %d"),
	                             NM_SETTING_TEAM_SETTING_NAME,
	                             TRUE,
	                             client);
	nm_connection_add_setting (connection, nm_setting_team_new ());

	/* Find an available interface name */
	team_num = 0;
	connections = nm_client_get_connections (client);
	for (i = 0; i < connections->len; i++) {
		conn2 = connections->pdata[i];

		if (!nm_connection_is_type (conn2, NM_SETTING_TEAM_SETTING_NAME))
			continue;
		iface = nm_connection_get_interface_name (conn2);
		if (!iface || strncmp (iface, "team", 4) != 0 || !g_ascii_isdigit (iface[4]))
			continue;

		num = atoi (iface + 4);
		if (team_num <= num)
			team_num = num + 1;
	}

	s_con = nm_connection_get_setting_connection (connection);
	my_iface = g_strdup_printf ("team%d", team_num);
	g_object_set (G_OBJECT (s_con),
	              NM_SETTING_CONNECTION_INTERFACE_NAME, my_iface,
	              NULL);
	g_free (my_iface);

	(*result_func) (FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_CALL, connection, FALSE, NULL, user_data);
}
