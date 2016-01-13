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
 * (C) Copyright 2008 - 2011 Red Hat, Inc.
 */

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include <nm-utils.h>

#include "ppp-auth-methods-dialog.h"

static void
validate (GtkWidget *dialog)
{
	GtkBuilder *builder;
	GtkWidget *widget;
	gboolean allow_eap, allow_pap, allow_chap, allow_mschap, allow_mschapv2;

	g_return_if_fail (dialog != NULL);

	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_if_fail (builder != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_eap"));
	allow_eap = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_pap"));
	allow_pap = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_chap"));
	allow_chap = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_mschap"));
	allow_mschap = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_mschapv2"));
	allow_mschapv2 = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ok_button"));

	/* Ignore for now until we know whether any PPP servers simply don't request
	 * authentication at all.
	 */
	if (0)
		gtk_widget_set_sensitive (widget, (allow_eap || allow_pap || allow_chap || allow_mschap || allow_mschapv2));
}

GtkWidget *
ppp_auth_methods_dialog_new (gboolean refuse_eap,
                             gboolean refuse_pap,
                             gboolean refuse_chap,
                             gboolean refuse_mschap,
                             gboolean refuse_mschapv2)

{
	GtkBuilder *builder;
	GtkWidget *dialog, *widget;
	GError *error = NULL;

	builder = gtk_builder_new ();

	if (!gtk_builder_add_from_file (builder, UIDIR "/ce-ppp-auth-methods.ui", &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	dialog = GTK_WIDGET (gtk_builder_get_object (builder, "auth_methods_dialog"));
	if (!dialog) {
		g_warning ("%s: Couldn't load PPP auth methods dialog from .ui file.", __func__);
		g_object_unref (builder);
		return NULL;
	}

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	g_object_set_data_full (G_OBJECT (dialog), "builder",
	                        builder, (GDestroyNotify) g_object_unref);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_eap"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !refuse_eap);
	g_signal_connect_swapped (G_OBJECT (widget), "toggled", G_CALLBACK (validate), dialog);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_pap"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !refuse_pap);
	g_signal_connect_swapped (G_OBJECT (widget), "toggled", G_CALLBACK (validate), dialog);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_chap"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !refuse_chap);
	g_signal_connect_swapped (G_OBJECT (widget), "toggled", G_CALLBACK (validate), dialog);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_mschap"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !refuse_mschap);
	g_signal_connect_swapped (G_OBJECT (widget), "toggled", G_CALLBACK (validate), dialog);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_mschapv2"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !refuse_mschapv2);
	g_signal_connect_swapped (G_OBJECT (widget), "toggled", G_CALLBACK (validate), dialog);

	/* Update initial validity */
	validate (dialog);

	return dialog;
}

void
ppp_auth_methods_dialog_get_methods (GtkWidget *dialog,
                                     gboolean *refuse_eap,
                                     gboolean *refuse_pap,
                                     gboolean *refuse_chap,
                                     gboolean *refuse_mschap,
                                     gboolean *refuse_mschapv2)
{
	GtkBuilder *builder;
	GtkWidget *widget;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (refuse_eap != NULL);
	g_return_if_fail (refuse_pap != NULL);
	g_return_if_fail (refuse_chap != NULL);
	g_return_if_fail (refuse_mschap != NULL);
	g_return_if_fail (refuse_mschapv2 != NULL);

	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_if_fail (builder != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_eap"));
	*refuse_eap = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_pap"));
	*refuse_pap = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_chap"));
	*refuse_chap = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_mschap"));
	*refuse_mschap = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "allow_mschapv2"));
	*refuse_mschapv2 = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

