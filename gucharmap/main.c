/*
 * Copyright © 2004 Noah Levitt
 * Copyright © 2007, 2008 Christian Persch
 * Copyright © 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02110-1301  USA
 */

#include <config.h>

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <gucharmap/gucharmap.h>
#include "gucharmap-window.h"

#define UI_RESOURCE "/org/gnome/charmap/ui/menus.ui"
 
static gboolean
option_version_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  g_print ("%s %s\n", _("GNOME Character Map"), VERSION);

  exit (EXIT_SUCCESS);
  return FALSE;
}

static GAction *
get_corresponding_window_action (GtkApplication *app,
                                 GAction        *action)
{
  GList *windows = gtk_application_get_windows (app);
  const char *name;

  name = g_action_get_name (G_ACTION (action));
  return g_action_map_lookup_action (G_ACTION_MAP (windows->data), name);
}

static void
activate_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       data)
{
  GAction *win_action = get_corresponding_window_action (GTK_APPLICATION (data),
                                                         G_ACTION (action));
  g_action_activate (win_action, parameter);

  if (parameter)
    g_action_change_state (G_ACTION (action), parameter);
}

static void
activate_toggle_action (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data)
{
  GVariant *state = g_action_get_state (G_ACTION (action));
  gboolean value = g_variant_get_boolean (state);
  GAction *win_action;

  win_action = get_corresponding_window_action (GTK_APPLICATION (data),
                                                G_ACTION (action));
  g_action_change_state (win_action, g_variant_new_boolean (!value));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!value));
  g_variant_unref (state);
}

static void
change_toggle_state (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       data)
{
  g_simple_action_set_state (action, state);
}

static void
activate_close (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GList *windows, *l;

  /* FIXME: use gtk_application_get_active_window() once it exists */
  windows = gtk_application_get_windows (GTK_APPLICATION (user_data));
  for (l = windows; l != NULL; l = l->next) {
    GtkWidget *window = l->data;

    if (!GTK_IS_APPLICATION_WINDOW (window))
      continue;

    gtk_widget_destroy (window);
    break;
  }
}

static void
startup_cb (GApplication *application,
            gpointer      data)
{
  GtkBuilder *builder = gtk_builder_new ();
  GMenuModel *model;
  gboolean show_app_menu;

  const GActionEntry app_entries[] =
  {
    { "group-by", activate_action, "s", "\"script\"", NULL },

    { "show-only-glyphs-in-font", activate_toggle_action, NULL, "false",
      change_toggle_state },

    { "zoom-in", activate_action, NULL, NULL, NULL },
    { "zoom-out", activate_action, NULL, NULL, NULL },
    { "normal-size", activate_action, NULL, NULL, NULL },

    { "find", activate_action, NULL, NULL, NULL },

    { "help", activate_action, NULL, NULL, NULL },
    { "about", activate_action, NULL, NULL, NULL },
    { "close", activate_close, NULL, NULL, NULL },
  };

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   application);


  gtk_builder_add_from_resource (builder, UI_RESOURCE, NULL);

  /* app menu */
  g_object_get (gtk_settings_get_default (),
                "gtk-shell-shows-app-menu", &show_app_menu,
                NULL);
  if (show_app_menu) {
    model = G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu"));
    gtk_application_set_app_menu (GTK_APPLICATION (application), model);
  }

  /* window menu */

#ifdef ENABLE_PRINTING
  model = G_MENU_MODEL (gtk_builder_get_object (builder, "printing"));

  g_menu_append (G_MENU (model), _("Page _Setup"), "win.page-setup");
/* g_menu_append (G_MENU (model), _("Print Preview"), "win.print-preview"); */
  g_menu_append (G_MENU (model), _("_Print"), "win.print");
#endif

  model = G_MENU_MODEL (gtk_builder_get_object (builder, "go-chapter"));
  g_object_set_data (G_OBJECT (application), "go-chapter-menu", model);

  model = G_MENU_MODEL (gtk_builder_get_object (builder, "menubar"));
  gtk_application_set_menubar (GTK_APPLICATION (application), model);

  gtk_application_add_accelerator (GTK_APPLICATION (application),
                                   "<Primary>Page_Down", "win.next-chapter",
                                   NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (application),
                                   "<Primary>Page_Up", "win.previous-chapter",
                                   NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (application),
                                   "F1", "app.help", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (application),
                                   "<Primary>q", "app.close", NULL);
  gtk_application_add_accelerator (GTK_APPLICATION (application),
                                   "<Primary>w", "app.close", NULL);


  g_object_unref (builder);
}

static void
gucharmap_activate (GApplication *application,
                    gpointer      unused)
{
  GList *windows = gtk_application_get_windows (GTK_APPLICATION (application));
  gtk_window_present (GTK_WINDOW (windows->data));
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GdkScreen *screen;
  int monitor;
  GdkRectangle rect;
  GError *error = NULL;
  char *font = NULL;
  GtkApplication *application;
  guint status;
  GOptionEntry goptions[] =
  {
    { "font", 0, 0, G_OPTION_ARG_STRING, &font,
      N_("Font to start with; ex: 'Serif 27'"), N_("FONT") },
    { "version", 0, G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG, 
      G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
    { NULL }
  };

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

#ifdef HAVE_GCONF
  /* GConf uses ORBit2 which need GThread. See bug #565516 */
  g_thread_init (NULL);
#endif

  /* Set programme name explicitly (see bug #653115) */
  g_set_prgname("gucharmap");

  if (!gtk_init_with_args (&argc, &argv, NULL, goptions, GETTEXT_PACKAGE, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);

      exit (1);
    }

  g_set_application_name (_("Character Map"));
  gtk_window_set_default_icon_name (GUCHARMAP_ICON_NAME);

  application = gtk_application_new ("org.gnome.Charmap",
                                     G_APPLICATION_NON_UNIQUE);
  g_signal_connect (application, "startup", G_CALLBACK (startup_cb), NULL);
  g_signal_connect (application, "activate",
                    G_CALLBACK (gucharmap_activate), NULL);

  g_application_register (G_APPLICATION (application), NULL, NULL);

  window = gucharmap_window_new (application);

  screen = gtk_window_get_screen (GTK_WINDOW (window));
  monitor = gdk_screen_get_monitor_at_point (screen, 0, 0);
#if GTK_CHECK_VERSION (3, 3, 5)
  gdk_screen_get_monitor_workarea (screen, monitor, &rect);
#else
  gdk_screen_get_monitor_geometry (screen, monitor, &rect);
#endif
  gtk_window_set_default_size (GTK_WINDOW (window), rect.width * 9/16, rect.height * 9/16);

  if (font)
    {
      gucharmap_window_set_font (GUCHARMAP_WINDOW (window), font);
      g_free (font);
    }

  gtk_window_present (GTK_WINDOW (window));

  status = g_application_run (G_APPLICATION (application), argc, argv);
  g_object_unref (application);

  return status;
}
