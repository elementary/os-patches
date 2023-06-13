/*
 * Based on demos/minimal-gtk.c, but with automatic exit after 1 second
 * by default.
 *
 * Copyright (C) 2010-2013 Jiri Techet <techet@gmail.com>
 * Copyright (C) 2019 Simon McVittie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <gtk/gtk.h>

/* include the libchamplain header */
#include <champlain-gtk/champlain-gtk.h>

#include <clutter-gtk/clutter-gtk.h>

static gboolean
timeout_cb (gpointer user_data)
{
  gtk_window_close (user_data);
  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *widget;

  /* initialize clutter */
  if (gtk_clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  /* create the top-level window and quit the main loop when it's closed */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit),
      NULL);

  /* create the libchamplain widget and set its size */
  widget = gtk_champlain_embed_new ();
  gtk_widget_set_size_request (widget, 640, 480);

  /* insert it into the widget you wish */
  gtk_container_add (GTK_CONTAINER (window), widget);

  /* show everything */
  gtk_widget_show_all (window);

  if (g_getenv ("TEST_INTERACTIVE") == NULL)
    g_timeout_add_seconds (1, timeout_cb, window);

  /* start the main loop */
  gtk_main ();

  return 0;
}
