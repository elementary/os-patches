/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nm-setting-connection.h>
#include <nm-setting-ppp.h>

#include "page-ppp.h"
#include "ppp-auth-methods-dialog.h"
#include "nm-connection-editor.h"
#include "nm-glib-compat.h"

G_DEFINE_TYPE (CEPagePpp, ce_page_ppp, CE_TYPE_PAGE)

#define CE_PAGE_PPP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_PPP, CEPagePppPrivate))

#define COL_NAME  0
#define COL_VALUE 1
#define COL_TAG 2

#define TAG_EAP 0
#define TAG_PAP 1
#define TAG_CHAP 2
#define TAG_MSCHAP 3
#define TAG_MSCHAPV2 4

typedef struct {
	NMSettingPPP *setting;

	GtkLabel *auth_methods_label;
	GtkButton *auth_methods_button;
	gboolean refuse_eap;
	gboolean refuse_pap;
	gboolean refuse_chap;
	gboolean refuse_mschap;
	gboolean refuse_mschapv2;

	guint orig_lcp_echo_failure;
	guint orig_lcp_echo_interval;

	GtkToggleButton *use_mppe;
	GtkToggleButton *mppe_require_128;
	GtkToggleButton *use_mppe_stateful;

	GtkToggleButton *allow_bsdcomp;
	GtkToggleButton *allow_deflate;
	GtkToggleButton *use_vj_comp;

	GtkToggleButton *send_ppp_echo;

	GtkWindowGroup *window_group;
	gboolean window_added;
	char *connection_id;
} CEPagePppPrivate;

static void
ppp_private_init (CEPagePpp *self)
{
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->auth_methods_label = GTK_LABEL (gtk_builder_get_object (builder, "auth_methods_label"));
	priv->auth_methods_button = GTK_BUTTON (gtk_builder_get_object (builder, "auth_methods_button"));

	priv->use_mppe = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ppp_use_mppe"));
	priv->mppe_require_128 = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ppp_require_mppe_128"));
	priv->use_mppe_stateful = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ppp_use_stateful_mppe"));
	priv->allow_bsdcomp = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ppp_allow_bsdcomp"));
	priv->allow_deflate = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ppp_allow_deflate"));
	priv->use_vj_comp = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ppp_usevj"));
	priv->send_ppp_echo = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ppp_send_echo_packets"));
}

static void
use_mppe_toggled_cb (GtkToggleButton *widget, CEPagePpp *self)
{
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (self);

	if (gtk_toggle_button_get_active (widget)) {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->mppe_require_128), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->use_mppe_stateful), TRUE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->mppe_require_128), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->mppe_require_128), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->use_mppe_stateful), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->use_mppe_stateful), FALSE);
	}

	ce_page_changed (CE_PAGE (self));
}

static void
add_one_auth_method (GString *string, const char *name, gboolean allowed)
{
	if (allowed) {
		if (string->len)
			g_string_append (string, ", ");
		g_string_append (string, name);
	}
}

static void
update_auth_methods_list (CEPagePpp *self)
{
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (self);
	GString *list;

	list = g_string_new ("");
	add_one_auth_method (list, _("EAP"), !priv->refuse_eap);
	add_one_auth_method (list, _("PAP"), !priv->refuse_pap);
	add_one_auth_method (list, _("CHAP"), !priv->refuse_chap);
	add_one_auth_method (list, _("MSCHAPv2"), !priv->refuse_mschapv2);
	add_one_auth_method (list, _("MSCHAP"), !priv->refuse_mschap);

	/* Translators: "none" refers to authentication methods */
	gtk_label_set_text (priv->auth_methods_label, list->len ? list->str : _("none"));
	g_string_free (list, TRUE);
}

static void
auth_methods_dialog_close_cb (GtkWidget *dialog, gpointer user_data)
{
	gtk_widget_hide (dialog);
	/* gtk_widget_destroy() will remove the window from the window group */
	gtk_widget_destroy (dialog);
}

static void
auth_methods_dialog_response_cb (GtkWidget *dialog, gint response, gpointer user_data)
{
	CEPagePpp *self = CE_PAGE_PPP (user_data);
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (self);

	if (response == GTK_RESPONSE_OK) {
		ppp_auth_methods_dialog_get_methods (dialog,
		                                     &priv->refuse_eap,
		                                     &priv->refuse_pap,
		                                     &priv->refuse_chap,
		                                     &priv->refuse_mschap,
		                                     &priv->refuse_mschapv2);
		ce_page_changed (CE_PAGE (self));
		update_auth_methods_list (self);
	}

	auth_methods_dialog_close_cb (dialog, NULL);
}

static void
auth_methods_button_clicked_cb (GtkWidget *button, gpointer user_data)
{
	CEPagePpp *self = CE_PAGE_PPP (user_data);
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (self);
	GtkWidget *dialog, *toplevel;
	char *tmp;

	toplevel = gtk_widget_get_toplevel (CE_PAGE (self)->page);
	g_return_if_fail (gtk_widget_is_toplevel (toplevel));

	dialog = ppp_auth_methods_dialog_new (priv->refuse_eap,
	                                      priv->refuse_pap,
	                                      priv->refuse_chap,
	                                      priv->refuse_mschap,
	                                      priv->refuse_mschapv2);
	if (!dialog) {
		g_warning ("%s: failed to create the PPP authentication methods dialog!", __func__);
		return;
	}

	gtk_window_group_add_window (priv->window_group, GTK_WINDOW (dialog));
	if (!priv->window_added) {
		gtk_window_group_add_window (priv->window_group, GTK_WINDOW (toplevel));
		priv->window_added = TRUE;
	}

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	tmp = g_strdup_printf (_("Editing PPP authentication methods for %s"), priv->connection_id);
	gtk_window_set_title (GTK_WINDOW (dialog), tmp);
	g_free (tmp);

	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (auth_methods_dialog_response_cb), self);

	gtk_widget_show_all (dialog);
}

static void
populate_ui (CEPagePpp *self, NMConnection *connection)
{
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (self);
	NMSettingPPP *setting = priv->setting;
	gboolean require_mppe, require_mppe_128, mppe_stateful, nobsdcomp, nodeflate, no_vj_comp;

	g_object_get (setting,
	              NM_SETTING_PPP_REFUSE_PAP, &priv->refuse_pap,
	              NM_SETTING_PPP_REFUSE_CHAP, &priv->refuse_chap,
	              NM_SETTING_PPP_REFUSE_MSCHAPV2, &priv->refuse_mschapv2,
	              NM_SETTING_PPP_REFUSE_MSCHAP, &priv->refuse_mschap,
	              NM_SETTING_PPP_REFUSE_EAP, &priv->refuse_eap,
	              NM_SETTING_PPP_REQUIRE_MPPE, &require_mppe,
	              NM_SETTING_PPP_REQUIRE_MPPE_128, &require_mppe_128,
	              NM_SETTING_PPP_MPPE_STATEFUL, &mppe_stateful,
	              NM_SETTING_PPP_NOBSDCOMP, &nobsdcomp,
	              NM_SETTING_PPP_NODEFLATE, &nodeflate,
	              NM_SETTING_PPP_NO_VJ_COMP, &no_vj_comp,
	              NM_SETTING_PPP_LCP_ECHO_INTERVAL, &priv->orig_lcp_echo_interval,
	              NM_SETTING_PPP_LCP_ECHO_FAILURE, &priv->orig_lcp_echo_failure,
	              NULL);

	update_auth_methods_list (self);

	g_signal_connect (priv->auth_methods_button, "clicked", G_CALLBACK (auth_methods_button_clicked_cb), self);

	gtk_toggle_button_set_active (priv->use_mppe, require_mppe);
	g_signal_connect (priv->use_mppe, "toggled", G_CALLBACK (use_mppe_toggled_cb), self);
	use_mppe_toggled_cb (priv->use_mppe, self);

	gtk_toggle_button_set_active (priv->mppe_require_128, require_mppe_128);
	g_signal_connect_swapped (priv->mppe_require_128, "toggled", G_CALLBACK (ce_page_changed), self);

	gtk_toggle_button_set_active (priv->use_mppe_stateful, mppe_stateful);
	g_signal_connect_swapped (priv->use_mppe_stateful, "toggled", G_CALLBACK (ce_page_changed), self);

	gtk_toggle_button_set_active (priv->allow_bsdcomp, !nobsdcomp);
	g_signal_connect_swapped (priv->allow_bsdcomp, "toggled", G_CALLBACK (ce_page_changed), self);
	gtk_toggle_button_set_active (priv->allow_deflate, !nodeflate);
	g_signal_connect_swapped (priv->allow_deflate, "toggled", G_CALLBACK (ce_page_changed), self);
	gtk_toggle_button_set_active (priv->use_vj_comp, !no_vj_comp);
	g_signal_connect_swapped (priv->use_vj_comp, "toggled", G_CALLBACK (ce_page_changed), self);

	gtk_toggle_button_set_active (priv->send_ppp_echo, (priv->orig_lcp_echo_interval > 0) ? TRUE : FALSE);
	g_signal_connect_swapped (priv->send_ppp_echo, "toggled", G_CALLBACK (ce_page_changed), self);
}

static void
finish_setup (CEPagePpp *self, gpointer unused, GError *error, gpointer user_data)
{
	if (!error)
		populate_ui (self, CE_PAGE (self)->connection);
}

CEPage *
ce_page_ppp_new (NMConnectionEditor *editor,
                 NMConnection *connection,
                 GtkWindow *parent_window,
                 NMClient *client,
                 NMRemoteSettings *settings,
                 const char **out_secrets_setting_name,
                 GError **error)
{
	CEPagePpp *self;
	CEPagePppPrivate *priv;
	NMSettingConnection *s_con;

	self = CE_PAGE_PPP (ce_page_new (CE_TYPE_PAGE_PPP,
	                                 editor,
	                                 connection,
	                                 parent_window,
	                                 client,
	                                 settings,
	                                 UIDIR "/ce-page-ppp.ui",
	                                 "PppPage",
	                                 _("PPP Settings")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load PPP user interface."));
		return NULL;
	}

	ppp_private_init (self);
	priv = CE_PAGE_PPP_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_ppp (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_PPP (nm_setting_ppp_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	priv->window_group = gtk_window_group_new ();

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);
	priv->connection_id = g_strdup (nm_setting_connection_get_id (s_con));

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	*out_secrets_setting_name = NM_SETTING_PPP_SETTING_NAME;

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPagePpp *self)
{
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (self);
	gboolean require_mppe;
	gboolean require_mppe_128;
	gboolean mppe_stateful;
	gboolean nobsdcomp;
	gboolean nodeflate;
	gboolean no_vj_comp;
	guint lcp_echo_failure = 0, lcp_echo_interval = 0;

	require_mppe = gtk_toggle_button_get_active (priv->use_mppe);
	require_mppe_128 = gtk_toggle_button_get_active (priv->mppe_require_128);
	mppe_stateful = gtk_toggle_button_get_active (priv->use_mppe_stateful);

	nobsdcomp = !gtk_toggle_button_get_active (priv->allow_bsdcomp);
	nodeflate = !gtk_toggle_button_get_active (priv->allow_deflate);
	no_vj_comp = !gtk_toggle_button_get_active (priv->use_vj_comp);

	if (gtk_toggle_button_get_active (priv->send_ppp_echo)) {
		if (priv->orig_lcp_echo_failure && priv->orig_lcp_echo_interval) {
			lcp_echo_failure = priv->orig_lcp_echo_failure;
			lcp_echo_interval = priv->orig_lcp_echo_interval;
		} else {
			/* Set defaults */
			lcp_echo_failure = 5;
			lcp_echo_interval = 30;
		}
	}

	g_object_set (priv->setting,
	              NM_SETTING_PPP_REFUSE_EAP, priv->refuse_eap,
	              NM_SETTING_PPP_REFUSE_PAP, priv->refuse_pap,
	              NM_SETTING_PPP_REFUSE_CHAP, priv->refuse_chap,
	              NM_SETTING_PPP_REFUSE_MSCHAP, priv->refuse_mschap,
	              NM_SETTING_PPP_REFUSE_MSCHAPV2, priv->refuse_mschapv2,
	              NM_SETTING_PPP_NOBSDCOMP, nobsdcomp,
	              NM_SETTING_PPP_NODEFLATE, nodeflate,
	              NM_SETTING_PPP_NO_VJ_COMP, no_vj_comp,
	              NM_SETTING_PPP_REQUIRE_MPPE, require_mppe,
	              NM_SETTING_PPP_REQUIRE_MPPE_128, require_mppe_128,
	              NM_SETTING_PPP_MPPE_STATEFUL, mppe_stateful,
	              NM_SETTING_PPP_LCP_ECHO_FAILURE, lcp_echo_failure,
	              NM_SETTING_PPP_LCP_ECHO_INTERVAL, lcp_echo_interval,
	              NULL);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPagePpp *self = CE_PAGE_PPP (page);
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (self);

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_ppp_init (CEPagePpp *self)
{
}

static void
dispose (GObject *object)
{
	CEPagePppPrivate *priv = CE_PAGE_PPP_GET_PRIVATE (object);

	g_clear_object (&priv->window_group);
	g_clear_pointer (&priv->connection_id, g_free);

	G_OBJECT_CLASS (ce_page_ppp_parent_class)->dispose (object);
}

static void
ce_page_ppp_class_init (CEPagePppClass *ppp_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (ppp_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (ppp_class);

	g_type_class_add_private (object_class, sizeof (CEPagePppPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
	object_class->dispose = dispose;
}
