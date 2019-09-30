/*
 * Copyright (C) 2011 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <gio/gio.h>
#include <sys/types.h>
#include <unistd.h>
#include <glibtop.h>
#include <libbamf-private/bamf-private.h>

void test_application_create_suite (GDBusConnection *connection);
void test_matcher_create_suite (GDBusConnection *connection);
void test_view_create_suite (GDBusConnection *connection);
void test_window_create_suite (void);

static int result = 1;

static gboolean
not_fatal_log_handler (const gchar *log_domain, GLogLevelFlags log_level,
                       const gchar *message, gpointer user_data)
{
  // Don't crash if used
  return FALSE;
}

void
ignore_fatal_errors (void)
{
  g_test_log_set_fatal_handler (not_fatal_log_handler, NULL);
}

static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer data)
{
  GMainLoop *loop = data;
  GtkIconTheme *icon_theme;

  g_setenv ("BAMF_TEST_MODE", "TRUE", TRUE);
  g_setenv ("PATH", TESTDIR"/data/bin", TRUE);

  icon_theme = gtk_icon_theme_get_default ();
  gtk_icon_theme_prepend_search_path (icon_theme, TESTDIR"/data/icons");

  test_matcher_create_suite (connection);
  test_view_create_suite (connection);
  test_window_create_suite ();
  test_application_create_suite (connection);

  result = g_test_run ();

  g_main_loop_quit (loop);
}

static void
on_name_lost (GDBusConnection *connection, const gchar *name, gpointer data)
{
  GMainLoop *loop = data;
  g_main_loop_quit (loop);
}

gint
main (gint argc, gchar *argv[])
{
  GMainLoop *loop;

  GFile *tmp_dir;
  gchar *tmp_path;

  tmp_path = g_dir_make_tmp (".bamfhomedataXXXXXX", NULL);
  tmp_dir = g_file_new_for_path (tmp_path);
  g_file_make_directory (tmp_dir, NULL, NULL);
  g_setenv ("XDG_DATA_HOME", tmp_path, TRUE);
  g_free (tmp_path);

  gtk_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);
  glibtop_init ();

  loop = g_main_loop_new (NULL, FALSE);

  g_bus_own_name (G_BUS_TYPE_SESSION,
                  BAMF_DBUS_SERVICE_NAME,
                  G_BUS_NAME_OWNER_FLAGS_NONE,
                  on_bus_acquired,
                  NULL,
                  on_name_lost,
                  loop,
                  NULL);

  g_main_loop_run (loop);

  g_file_delete (tmp_dir, NULL, NULL);
  g_object_unref (tmp_dir);

  return result;
}
