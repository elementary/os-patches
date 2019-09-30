/*
 * Copyright (C) 2009-2011 Canonical Ltd
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
 * Authored by Jason Smith <jason.smith@canonical.com>
 *             Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <string.h>
#include "bamf-application.h"
#include "bamf-window.h"
#include "bamf-legacy-window.h"
#include "bamf-legacy-window-test.h"

#define DESKTOP_FILE TESTDIR"/data/test-bamf-app.desktop"

void ignore_fatal_errors (void);

static gboolean          signal_seen   = FALSE;
static gboolean          signal_result = FALSE;
static char *            signal_window = NULL;
static GDBusConnection * gdbus_connection = NULL;

static GFile *
write_data_to_tmp_file (const gchar *data)
{
  GFile *tmp;
  GFileIOStream *iostream;
  GOutputStream *output;

  tmp = g_file_new_tmp (NULL, &iostream, NULL);

  if (!tmp)
    {
      if (iostream)
        g_object_unref (iostream);

      return NULL;
    }

  output = g_io_stream_get_output_stream (G_IO_STREAM (iostream));
  if (!g_output_stream_write_all (output, data, strlen (data), NULL, NULL, NULL))
    {
      g_object_unref (tmp);
      tmp = NULL;
    }

  g_object_unref (output);

  return tmp;
}

static void
test_allocation (void)
{
  BamfApplication *application;

  /* Check it allocates */
  application = bamf_application_new ();
  g_assert (BAMF_IS_APPLICATION (application));

  g_object_unref (application);

  application = bamf_application_new_from_desktop_file (DESKTOP_FILE);
  g_assert (BAMF_IS_APPLICATION (application));

  g_object_unref (application);
}

static void
test_type (void)
{
  BamfApplication *application = bamf_application_new ();
  g_assert_cmpuint (bamf_application_get_application_type (application), ==, BAMF_APPLICATION_SYSTEM);

  g_object_unref (application);
}

static void
test_type_set (void)
{
  BamfApplication *application = bamf_application_new ();

  bamf_application_set_application_type (application, BAMF_APPLICATION_WEB);
  g_assert_cmpuint (bamf_application_get_application_type (application), ==, BAMF_APPLICATION_WEB);

  bamf_application_set_application_type (application, BAMF_APPLICATION_SYSTEM);
  g_assert_cmpuint (bamf_application_get_application_type (application), ==, BAMF_APPLICATION_SYSTEM);

  g_object_unref (application);
}

static void
test_type_set_invalid (void)
{
  ignore_fatal_errors();
  BamfApplication *application = bamf_application_new ();

  bamf_application_set_application_type (application, BAMF_APPLICATION_UNKNOWN);
  g_assert_cmpuint (bamf_application_get_application_type (application), ==, BAMF_APPLICATION_SYSTEM);

  bamf_application_set_application_type (application, -1);
  g_assert_cmpuint (bamf_application_get_application_type (application), ==, BAMF_APPLICATION_SYSTEM);

  g_object_unref (application);
}

static void
test_desktop_file (void)
{
  BamfApplication *application = bamf_application_new ();
  g_assert (bamf_application_get_desktop_file (application) == NULL);

  bamf_application_set_desktop_file (application, DESKTOP_FILE);
  g_assert (g_strcmp0 (bamf_application_get_desktop_file (application), DESKTOP_FILE) == 0);

  g_object_unref (application);

  application = bamf_application_new_from_desktop_file (DESKTOP_FILE);
  g_assert (g_strcmp0 (bamf_application_get_desktop_file (application), DESKTOP_FILE) == 0);

  g_object_unref (application);
}

static void
test_desktop_icon (void)
{
  BamfApplication *application;
  const char *icon_desktop = TESTDIR"/data/icon.desktop";

  application = bamf_application_new_from_desktop_file (icon_desktop);
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-icon");
  g_object_unref (application);
}

static void
test_desktop_icon_empty (void)
{
  BamfApplication *application;
  const char no_icon_desktop[] = TESTDIR"/data/no-icon.desktop";

  application = bamf_application_new_from_desktop_file (no_icon_desktop);
  g_assert_cmpstr (bamf_application_get_desktop_file (application), ==, no_icon_desktop);

  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, BAMF_APPLICATION_DEFAULT_ICON);
  g_object_unref (application);
}

static void
test_desktop_icon_invalid (void)
{
  BamfApplication *application;
  const char *invalid_icon_desktop = TESTDIR"/data/test-bamf-app.desktop";

  application = bamf_application_new_from_desktop_file (invalid_icon_desktop);

  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, BAMF_APPLICATION_DEFAULT_ICON);
  g_object_unref (application);
}

static void
test_icon_class_name (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *test;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "execution-binary");
  test = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));

  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-icon");

  g_object_unref (lwin);
  g_object_unref (test);
  g_object_unref (application);
}

static void
test_icon_exec_string (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *test;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "class", "test-bamf-icon");
  test = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));

  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-icon");

  g_object_unref (lwin);
  g_object_unref (test);
  g_object_unref (application);
}

static void
test_icon_embedded (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *test;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "class", "python execution-script.py");
  bamf_legacy_window_test_set_icon (lwin, "test-bamf-icon");
  test = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));

  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-icon");

  g_object_unref (lwin);
  g_object_unref (test);
  g_object_unref (application);
}

static void
test_icon_priority (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *test;

  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "test-bamf-pixmap");
  bamf_legacy_window_test_set_icon (lwin, "bamf-custom-icon");
  test = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  application = bamf_application_new ();
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-icon");
  g_object_unref (application);

  application = bamf_application_new ();
  bamf_legacy_window_test_set_wmclass (lwin, NULL, NULL);
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-pixmap");
  g_object_unref (application);

  application = bamf_application_new ();
  g_free (lwin->exec);
  lwin->exec = NULL;
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "bamf-custom-icon");
  g_object_unref (application);

  g_object_unref (lwin);
  g_object_unref (test);
}

static void
test_icon_generic_class (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *test;

  lwin = bamf_legacy_window_test_new (20, "window", "python", "execution-script");
  test = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  application = bamf_application_new ();
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "python");
  g_object_unref (application);

  application = bamf_application_new ();
  bamf_legacy_window_test_set_icon (lwin, "bamf-custom-icon");
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "bamf-custom-icon");
  g_object_unref (application);

  g_object_unref (lwin);
  g_object_unref (test);
}

static void
test_icon_generic_exec (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *test;

  lwin = bamf_legacy_window_test_new (20, "window", "class", "python2.7");
  test = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  application = bamf_application_new ();
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "python2.7");
  g_object_unref (application);

  application = bamf_application_new ();
  bamf_legacy_window_test_set_icon (lwin, "bamf-custom-icon");
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "bamf-custom-icon");
  g_object_unref (application);

  g_object_unref (lwin);
  g_object_unref (test);
}

static void
test_icon_full_path (void)
{
  BamfApplication *application;
  GKeyFile *key_file;
  const char* test_app = TESTDIR"/data/test-bamf-app.desktop";
  const char* test_icon = TESTDIR"/data/icons/test-bamf-icon.png";

  g_assert (g_file_test (test_icon, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR));

  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, test_app, G_KEY_FILE_NONE, NULL);
  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, test_icon);

  gchar *key_data = g_key_file_to_data (key_file, NULL, NULL);
  GFile *tmp_file = write_data_to_tmp_file (key_data);
  gchar *path = g_file_get_path (tmp_file);

  application = bamf_application_new_from_desktop_file (path);
  g_file_delete (tmp_file, NULL, NULL);
  g_object_unref (tmp_file);
  g_key_file_free (key_file);
  g_free (key_data);
  g_free (path);

  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, test_icon);

  g_object_unref (application);
}

static void
test_icon_full_path_invalid (void)
{
  BamfApplication *application;
  GKeyFile *key_file;
  const char* test_app = TESTDIR"/data/test-bamf-app.desktop";
  const char* invalid_test_icon = TESTDIR"/data/icons/not-existent-icon-file.png";

  g_assert (!g_file_test (invalid_test_icon, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR));

  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, test_app, G_KEY_FILE_NONE, NULL);
  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, invalid_test_icon);

  gchar *key_data = g_key_file_to_data (key_file, NULL, NULL);
  GFile *tmp_file = write_data_to_tmp_file (key_data);
  gchar *path = g_file_get_path (tmp_file);

  application = bamf_application_new_from_desktop_file (path);
  g_file_delete (tmp_file, NULL, NULL);
  g_object_unref (tmp_file);
  g_key_file_free (key_file);
  g_free (key_data);
  g_free (path);

  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, BAMF_APPLICATION_DEFAULT_ICON);

  g_object_unref (application);
}

static void
test_get_mime_types (void)
{
  BamfApplication *application;
  const char* mime_types_desktop = TESTDIR"/data/mime-types.desktop";

  application = bamf_application_new_from_desktop_file (mime_types_desktop);
  g_assert_cmpstr (bamf_application_get_desktop_file (application), ==, mime_types_desktop);

  gchar** mimes = bamf_application_get_supported_mime_types (application);

  g_assert_cmpuint (g_strv_length (mimes), ==, 7);
  g_assert_cmpstr (mimes[0], ==, "text/plain");
  g_assert_cmpstr (mimes[1], ==, "text/x-chdr");
  g_assert_cmpstr (mimes[2], ==, "text/x-csrc");
  g_assert_cmpstr (mimes[3], ==, "text/html");
  g_assert_cmpstr (mimes[4], ==, "text/css");
  g_assert_cmpstr (mimes[5], ==, "text/x-diff");
  g_assert_cmpstr (mimes[6], ==, "application/xml");
  g_assert_cmpstr (mimes[7], ==, NULL);

  g_strfreev (mimes);
  g_object_unref (application);
}

static void
test_get_mime_types_none (void)
{
  BamfApplication *application;
  const char* mime_types_desktop = TESTDIR"/data/test-bamf-app.desktop";

  application = bamf_application_new_from_desktop_file (mime_types_desktop);
  g_assert_cmpstr (bamf_application_get_desktop_file (application), ==, mime_types_desktop);

  gchar** mimes = bamf_application_get_supported_mime_types (application);
  g_assert (!mimes);

  g_object_unref (application);
}

static void
on_urgent_changed (BamfApplication *application, gboolean result, gpointer data)
{
  signal_seen = TRUE;
  signal_result = result;
}

static void
test_urgent (void)
{
  signal_seen = FALSE;

  BamfApplication *application;
  BamfWindow *window1, *window2;
  BamfLegacyWindowTest *test1, *test2;

  application = bamf_application_new ();

  g_signal_connect (G_OBJECT (application), "urgent-changed", (GCallback) on_urgent_changed, NULL);

  test1 = bamf_legacy_window_test_new (20, "Window X", "class", "exec");
  test2 = bamf_legacy_window_test_new (20, "Window Y", "class", "exec");

  window1 = bamf_window_new (BAMF_LEGACY_WINDOW (test1));
  window2 = bamf_window_new (BAMF_LEGACY_WINDOW (test2));

  // Ensure we are not visible with no windows
  g_assert (!bamf_view_is_urgent (BAMF_VIEW (application)));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Test that when added, we signaled properly
  g_assert (!bamf_view_is_urgent (BAMF_VIEW (application)));
  g_assert (!signal_seen);

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Test that we unset and signal properly
  g_assert (!bamf_view_is_urgent (BAMF_VIEW (application)));
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_attention (test1, TRUE);
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Ensure that when adding a skip-tasklist window, we dont set this to visible
  g_assert (bamf_view_is_urgent (BAMF_VIEW (application)));
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window2));

  g_assert (bamf_view_is_urgent (BAMF_VIEW (application)));
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_attention (test1, FALSE);

  g_assert (!bamf_view_is_urgent (BAMF_VIEW (application)));
  g_assert (signal_seen);
  g_assert (!signal_result);
}

static void
on_active_changed (BamfApplication *application, gboolean result, gpointer data)
{
  signal_seen = TRUE;
  signal_result = result;
}

static void
test_active (void)
{
  signal_seen = FALSE;

  BamfApplication *application;
  BamfWindow *window1, *window2;
  BamfLegacyWindowTest *test1, *test2;

  application = bamf_application_new ();

  g_signal_connect (G_OBJECT (application), "active-changed", (GCallback) on_active_changed, NULL);

  test1 = bamf_legacy_window_test_new (20, "Window X", "class", "exec");
  test2 = bamf_legacy_window_test_new (20, "Window Y", "class", "exec");

  window1 = bamf_window_new (BAMF_LEGACY_WINDOW (test1));
  window2 = bamf_window_new (BAMF_LEGACY_WINDOW (test2));

  // Ensure we are not active with no windows
  g_assert (!bamf_view_is_active (BAMF_VIEW (application)));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Test that when added, we signaled properly
  g_assert (!bamf_view_is_active (BAMF_VIEW (application)));
  g_assert (!signal_seen);

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Test that we unset and signal properly
  g_assert (!bamf_view_is_active (BAMF_VIEW (application)));
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_active (test1, TRUE);
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Ensure that when adding a skip-tasklist window, we dont set this to visible
  g_assert (bamf_view_is_active (BAMF_VIEW (application)));
  g_assert (!signal_seen);
  while (g_main_context_pending (NULL)) g_main_context_iteration(NULL, TRUE);
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window2));

  g_assert (bamf_view_is_active (BAMF_VIEW (application)));
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_active (test1, FALSE);
  g_assert (!signal_seen);
  g_assert (bamf_view_is_active (BAMF_VIEW (application)));
  while (g_main_context_pending (NULL)) g_main_context_iteration(NULL, TRUE);

  g_assert (!bamf_view_is_active (BAMF_VIEW (application)));
  g_assert (signal_seen);
  g_assert (!signal_result);
}

static void
test_get_xids (void)
{
  BamfApplication *application;
  BamfWindow *window1, *window2;
  BamfLegacyWindowTest *lwin1, *lwin2;
  GVariant *container;
  GVariantIter *xids;
  gboolean found;
  guint32 xid;

  application = bamf_application_new ();

  lwin1 = bamf_legacy_window_test_new (25, "window1", "class", "exec");
  lwin2 = bamf_legacy_window_test_new (50, "window2", "class", "exec");
  window1 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin1));
  window2 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin2));

  container = bamf_application_get_xids (application);
  g_assert (g_variant_type_equal (g_variant_get_type (container),
                                  G_VARIANT_TYPE ("(au)")));
  g_assert (g_variant_n_children (container) == 1);
  g_variant_get (container, "(au)", &xids);
  g_assert (g_variant_iter_n_children (xids) == 0);
  g_variant_iter_free (xids);
  g_variant_unref (container);

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window1));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window2));

  container = bamf_application_get_xids (application);
  g_assert (g_variant_n_children (container) == 1);
  g_variant_get (container, "(au)", &xids);
  g_assert (g_variant_iter_n_children (xids) == 2);

  found = FALSE;
  while (g_variant_iter_loop (xids, "u", &xid))
    {
      if (xid == 25)
        {
          found = TRUE;
          break;
        }
    }

  g_assert (found);

  found = FALSE;
  g_variant_get (container, "(au)", &xids);
  while (g_variant_iter_loop (xids, "u", &xid))
    {
      if (xid == 50)
        {
          found = TRUE;
          break;
        }
    }

  g_assert (found);

  g_variant_iter_free (xids);
  g_variant_unref (container);

  g_object_unref (lwin1);
  g_object_unref (lwin2);
  g_object_unref (window1);
  g_object_unref (window2);
  g_object_unref (application);
}

static void
test_manages_xid (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *test;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "class", "exec");
  test = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));

  g_assert (bamf_application_manages_xid (application, 20));

  g_object_unref (lwin);
  g_object_unref (test);
  g_object_unref (application);
}

static void
test_get_window (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *test;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "class", "exec");
  test = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (test));

  g_assert (bamf_application_get_window (application, 20) == test);

  g_object_unref (lwin);
  g_object_unref (test);
  g_object_unref (application);
}

static void
on_user_visible_changed (BamfApplication *application, gboolean result, gpointer data)
{
  signal_seen = TRUE;
  signal_result = result;
}

static void
test_user_visible (void)
{
  signal_seen = FALSE;

  BamfApplication *application;
  BamfWindow *window1, *window2;
  BamfLegacyWindowTest *test1, *test2;

  application = bamf_application_new ();

  g_signal_connect (G_OBJECT (application), "user-visible-changed", (GCallback) on_user_visible_changed, NULL);

  test1 = bamf_legacy_window_test_new (20, "Window X", "class", "exec");
  test2 = bamf_legacy_window_test_new (20, "Window Y", "class", "exec");

  window1 = bamf_window_new (BAMF_LEGACY_WINDOW (test1));
  window2 = bamf_window_new (BAMF_LEGACY_WINDOW (test2));

  // Ensure we are not visible with no windows
  g_assert (!bamf_view_is_user_visible (BAMF_VIEW (application)));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Test that when added, we signaled properly
  g_assert (bamf_view_is_user_visible (BAMF_VIEW (application)));
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Test that we unset and signal properly
  g_assert (!bamf_view_is_user_visible (BAMF_VIEW (application)));
  g_assert (signal_seen);
  g_assert (!signal_result);

  signal_seen = FALSE;

  bamf_legacy_window_test_set_skip (test1, TRUE);
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window1));

  // Ensure that when adding a skip-tasklist window, we dont set this to visible
  g_assert (!bamf_view_is_user_visible (BAMF_VIEW (application)));
  g_assert (!signal_seen);

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window2));

  g_assert (bamf_view_is_user_visible (BAMF_VIEW (application)));
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_legacy_window_test_set_skip (test2, TRUE);

  g_assert (!bamf_view_is_user_visible (BAMF_VIEW (window1)));
  g_assert (!bamf_view_is_user_visible (BAMF_VIEW (application)));
  g_assert (signal_seen);
  g_assert (!signal_result);
}

static void
on_window_added (BamfApplication *application, char *window, gpointer data)
{
  signal_seen = TRUE;
  signal_window = g_strdup (window);
}

static void
test_window_added (void)
{
  signal_seen = FALSE;

  BamfApplication *application;
  BamfWindow *window;
  BamfLegacyWindowTest *test;
  const char *path;

  application = bamf_application_new ();

  g_signal_connect (G_OBJECT (application), "window-added", (GCallback) on_window_added, NULL);

  test = bamf_legacy_window_test_new (20, "Window X", "class", "exec");
  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window));

  // Ensure we dont signal things that are not on the bus
  g_assert (!signal_seen);

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (window));

  path = bamf_view_export_on_bus (BAMF_VIEW (window), gdbus_connection);

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window));

  g_assert (signal_seen);
  g_assert_cmpstr (signal_window, ==, path);

  signal_seen = FALSE;

  g_object_unref (window);
  g_object_unref (test);
  g_object_unref (application);
}

static void
on_window_removed (BamfApplication *application, char *window, gpointer data)
{
  signal_seen = TRUE;
  signal_window = g_strdup (window);
}

static void
test_window_removed (void)
{
  signal_seen = FALSE;

  BamfApplication *application;
  BamfWindow *window;
  BamfLegacyWindowTest *test;
  const char *path;

  application = bamf_application_new ();

  g_signal_connect (G_OBJECT (application), "window-removed", (GCallback) on_window_removed, NULL);

  test = bamf_legacy_window_test_new (20, "Window X", "class", "exec");
  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window));
  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (window));

  // Ensure we dont signal things that are not on the bus
  g_assert (!signal_seen);

  path = bamf_view_export_on_bus (BAMF_VIEW (window), gdbus_connection);

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (window));
  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (window));

  g_assert (signal_seen);
  g_assert (g_strcmp0 (signal_window, path) == 0);

  signal_seen = FALSE;

  g_object_unref (window);
  g_object_unref (test);
  g_object_unref (application);
}

static void
test_desktop_app_main_child (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new_from_desktop_file (DESKTOP_FILE);
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  g_assert (!bamf_application_get_main_child (application));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win));

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_desktop_app_main_child_doesnt_match_emblems (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new_from_desktop_file (DESKTOP_FILE);
  lwin = bamf_legacy_window_test_new (20, "window", "python", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), !=, "window");
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), !=, "python");

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_desktop_app_main_child_doesnt_update_emblems (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new_from_desktop_file (DESKTOP_FILE);
  lwin = bamf_legacy_window_test_new (20, "window", "python", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  bamf_legacy_window_test_set_name (lwin, "New Window Name");
  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), !=, "New Window Name");

  bamf_legacy_window_test_set_name (lwin, "even-new-name");
  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), !=, "even-new-name");

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_app_main_child (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  g_assert (!bamf_application_get_main_child (application));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win));

  g_object_unref (lwin);
  g_object_unref (win);

  g_assert (!bamf_application_get_main_child (application));
  g_object_unref (application);
}

static void
test_app_main_child_matches_emblems (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), ==, "window");
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-icon");

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_app_main_child_updates_emblems (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  bamf_legacy_window_test_set_name (lwin, "New Window Name");
  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), ==, "New Window Name");

  bamf_legacy_window_test_set_name (lwin, "even-new-name");
  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), ==, "even-new-name");

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_app_main_child_multiple_children (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win1;
  GList *wins = NULL;
  GList *lwins = NULL;
  int i;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "execution-binary");
  win1 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win1));

  for (i = 0; i < 10; ++i)
    {
      lwin = bamf_legacy_window_test_new (i, "other-window", "", "execution-binary");
      lwins = g_list_prepend (lwins, lwin);
      wins = g_list_prepend (wins, bamf_window_new (BAMF_LEGACY_WINDOW (lwin)));
      bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (wins->data));

      g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win1));
      g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), ==, "window");
      g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-icon");
    }

  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win1));

  g_object_unref (win1);
  g_object_unref (application);
  g_list_free_full (wins, g_object_unref);
  g_list_free_full (lwins, g_object_unref);
}

static void
test_app_main_child_normal_priority (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *dialog, *win;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (10, "dialog", "python", "execution-binary");
  lwin->window_type = BAMF_WINDOW_DIALOG;
  dialog = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (dialog));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (dialog));

  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), ==, "dialog");
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "python");

  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win));

  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), ==, "window");
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "test-bamf-icon");

  g_object_unref (dialog);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_app_main_child_on_window_removal (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win1, *win2, *win3, *win4, *dialog;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (10, "window1", NULL, "execution-binary");
  win1 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win1));
  g_object_unref (lwin);

  lwin = bamf_legacy_window_test_new (20, "window2", NULL, "execution-binary");
  win2 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win2));
  g_object_unref (lwin);

  lwin = bamf_legacy_window_test_new (21, "dialog", NULL, "execution-binary");
  lwin->window_type = BAMF_WINDOW_DIALOG;
  dialog = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (dialog));
  g_object_unref (lwin);

  lwin = bamf_legacy_window_test_new (30, "window3", NULL, "execution-binary");
  win3 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win3));
  g_object_unref (lwin);

  lwin = bamf_legacy_window_test_new (40, "window4", NULL, "execution-binary");
  win4 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win4));
  g_object_unref (lwin);

  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win1));

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (win4));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win1));

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (win1));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win2));

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (win2));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win3));

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (win3));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (dialog));

  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (dialog));
  g_assert (!bamf_application_get_main_child (application));

  g_object_unref (win1);
  g_object_unref (win2);
  g_object_unref (win3);
  g_object_unref (win4);
  g_object_unref (dialog);
  g_object_unref (application);
}

static void
test_app_main_child_on_window_replace_on_removal (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-icon", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));

  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_assert (bamf_application_get_main_child (application) == BAMF_VIEW (win));
  bamf_view_remove_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_assert (!bamf_application_get_main_child (application));
  bamf_legacy_window_test_set_name (lwin, "don't crash here!");

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_desktop_app_create_local_desktop_file (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new ();
  bamf_application_set_desktop_file (application, DESKTOP_FILE);
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-class", "execution-binary");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));

  g_assert (!bamf_application_create_local_desktop_file (application));

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_desktopless_app_create_local_desktop_file_invalid_exec (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "test-bamf-class", NULL);
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));

  g_assert (!bamf_application_create_local_desktop_file (application));

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
verify_application_desktop_file_content (BamfApplication *application)
{
  GKeyFile *key_file;
  const gchar *desktop_file, *exec;
  gchar *str_value;
  GError *error = NULL;
  BamfView *main_child;
  BamfLegacyWindow *main_window;

  desktop_file = bamf_application_get_desktop_file (application);

  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, desktop_file, G_KEY_FILE_NONE, &error);
  g_assert (!error);

  str_value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_TYPE, &error);
  g_assert (!error);
  g_assert_cmpstr (str_value, ==, G_KEY_FILE_DESKTOP_TYPE_APPLICATION);
  g_clear_pointer (&str_value, g_free);

  str_value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     "Encoding", &error);
  g_assert (!error);
  g_assert_cmpstr (str_value, ==, "UTF-8");
  g_clear_pointer (&str_value, g_free);

  str_value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_VERSION, &error);
  g_assert (!error);
  g_assert_cmpstr (str_value, ==, "1.0");
  g_clear_pointer (&str_value, g_free);

  if (bamf_view_get_name (BAMF_VIEW (application)))
    {
      str_value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                         G_KEY_FILE_DESKTOP_KEY_NAME, &error);
      g_assert (!error);
      g_assert_cmpstr (str_value, ==, bamf_view_get_name (BAMF_VIEW (application)));
      g_clear_pointer (&str_value, g_free);
    }

  if (bamf_view_get_icon (BAMF_VIEW (application)))
    {
      str_value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                         G_KEY_FILE_DESKTOP_KEY_ICON, &error);
      g_assert (!error);
      g_assert_cmpstr (str_value, ==, bamf_view_get_icon (BAMF_VIEW (application)));
      g_clear_pointer (&str_value, g_free);
    }

  main_child = bamf_application_get_main_child (application);
  main_window = bamf_window_get_window (BAMF_WINDOW (main_child));
  exec = bamf_legacy_window_get_exec_string (main_window);

  str_value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_EXEC, &error);
  g_assert (!error);
  g_assert_cmpstr (str_value, ==, exec);
  g_clear_pointer (&str_value, g_free);

  const gchar *working_dir = bamf_legacy_window_get_working_dir (main_window);
  str_value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_PATH, &error);

  gchar *current_dir = g_get_current_dir ();

  if (g_strcmp0 (current_dir, working_dir) == 0)
    {
      g_assert (error);
      g_clear_error (&error);
      g_assert_cmpstr (str_value, ==, NULL);
    }
  else
    {
      g_assert (!error);
      g_assert_cmpstr (str_value, ==, working_dir);
      g_clear_pointer (&str_value, g_free);
    }

  g_clear_pointer (&current_dir, g_free);

  g_assert (!g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, &error));
  g_assert (!error);

  const gchar *class = bamf_legacy_window_get_class_instance_name (main_window);
  class = class ? class : bamf_legacy_window_get_class_name (main_window);

  if (class)
    {
      str_value = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                         G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS, &error);
      g_assert (!error);
      g_assert_cmpstr (str_value, ==, class);
      g_clear_pointer (&str_value, g_free);
    }

  const gchar *current_desktop = g_getenv ("XDG_CURRENT_DESKTOP");

  if (current_desktop)
    {
      gchar **list;
      gsize len;
      list = g_key_file_get_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                         G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN,
                                         &len, &error);
      g_assert (!error);
      g_assert_cmpuint (len, ==, 1);
      g_assert_cmpstr (*list, ==, current_desktop);
    }

  gchar *generator = g_strdup_printf ("X-%sGenerated", current_desktop ? current_desktop : "BAMF");
  g_assert (g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, generator, &error));
  g_assert (!error);
  g_free (generator);

  g_key_file_free (key_file);
}

static void
test_desktopless_app_create_local_desktop_file_using_instance_class_basename (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;
  const gchar *desktop_path;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", NULL, "awesome --exec");
  bamf_legacy_window_test_set_wmclass (lwin, NULL, "instance-class");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));

  g_assert (bamf_application_create_local_desktop_file (application));

  desktop_path = bamf_application_get_desktop_file (application);
  g_assert (desktop_path);
  g_assert (g_str_has_suffix (desktop_path, "instance-class.desktop"));
  g_assert (g_file_test (desktop_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR));
  verify_application_desktop_file_content (application);

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_desktopless_app_create_local_desktop_file_using_name_class_basename (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;
  const gchar *desktop_path;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", "Application!/?Class", "awesome --exec");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));

  g_assert (bamf_application_create_local_desktop_file (application));

  desktop_path = bamf_application_get_desktop_file (application);
  g_assert (desktop_path);
  g_assert (g_str_has_suffix (desktop_path, "application___class.desktop"));
  g_assert (g_file_test (desktop_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR));
  verify_application_desktop_file_content (application);

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_desktopless_app_create_local_desktop_file_using_exec_basename (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;
  const gchar *desktop_path;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", NULL, "awesome --exec");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));

  g_assert (bamf_application_create_local_desktop_file (application));

  desktop_path = bamf_application_get_desktop_file (application);
  g_assert (desktop_path);
  g_assert (g_str_has_suffix (desktop_path, "awesome.desktop"));
  g_assert (g_file_test (desktop_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR));
  verify_application_desktop_file_content (application);

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_desktopless_app_create_local_desktop_file_using_trimmed_exec_basename (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;
  const gchar *desktop_path;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", NULL, "python awesome-script.py");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));

  g_assert (bamf_application_create_local_desktop_file (application));

  desktop_path = bamf_application_get_desktop_file (application);
  g_assert (desktop_path);
  g_assert (g_str_has_suffix (desktop_path, "awesome-script.desktop"));
  g_assert (g_file_test (desktop_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR));
  verify_application_desktop_file_content (application);

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_desktopless_app_create_local_desktop_file_with_working_dir (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", NULL, "python ./awesome-script.py");
  g_clear_pointer (&lwin->working_dir, g_free);
  lwin->working_dir = g_strdup ("/home/user/my/fantastic/path");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));

  g_assert (bamf_application_create_local_desktop_file (application));
  verify_application_desktop_file_content (application);

  g_object_unref (lwin);
  g_object_unref (win);
  g_object_unref (application);
}

static void
test_contain_similar_to_window (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win, *win1, *win2, *win3;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", NULL, "binary");
  bamf_legacy_window_test_set_wmclass (lwin, "ClassName", "ClassInstance");
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_object_unref (lwin);

  lwin = bamf_legacy_window_test_new (30, "window1", NULL, "binary1");
  bamf_legacy_window_test_set_wmclass (lwin, "ClassName", "ClassInstance");
  win1 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);
  g_assert (bamf_application_contains_similar_to_window (application, win1));

  lwin = bamf_legacy_window_test_new (40, "window2", NULL, "binary2");
  bamf_legacy_window_test_set_wmclass (lwin, "ClassName", "ClassInstance2");
  win2 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);
  g_assert (!bamf_application_contains_similar_to_window (application, win2));

  lwin = bamf_legacy_window_test_new (50, "window3", NULL, "binary3");
  bamf_legacy_window_test_set_wmclass (lwin, "ClassName3", "ClassInstance");
  win3 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);
  g_assert (!bamf_application_contains_similar_to_window (application, win3));

  g_object_unref (win);
  g_object_unref (win1);
  g_object_unref (win2);
  g_object_unref (win3);
  g_object_unref (application);
}

static void
test_contain_similar_to_window_null (void)
{
  BamfApplication *application;
  BamfLegacyWindowTest *lwin;
  BamfWindow *win, *win1, *win2, *win3;

  application = bamf_application_new ();
  lwin = bamf_legacy_window_test_new (20, "window", NULL, "binary");
  bamf_legacy_window_test_set_wmclass (lwin, NULL, NULL);
  win = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  bamf_view_add_child (BAMF_VIEW (application), BAMF_VIEW (win));
  g_object_unref (lwin);

  lwin = bamf_legacy_window_test_new (30, "window1", NULL, "binary1");
  bamf_legacy_window_test_set_wmclass (lwin, NULL, NULL);
  win1 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);
  g_assert (bamf_application_contains_similar_to_window (application, win1));

  lwin = bamf_legacy_window_test_new (40, "window2", NULL, "binary2");
  bamf_legacy_window_test_set_wmclass (lwin, "ClassName", NULL);
  win2 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);
  g_assert (!bamf_application_contains_similar_to_window (application, win2));

  lwin = bamf_legacy_window_test_new (50, "window3", NULL, "binary3");
  bamf_legacy_window_test_set_wmclass (lwin, NULL, "ClassInstance");
  win3 = bamf_window_new (BAMF_LEGACY_WINDOW (lwin));
  g_object_unref (lwin);
  g_assert (!bamf_application_contains_similar_to_window (application, win3));

  g_object_unref (win);
  g_object_unref (win1);
  g_object_unref (win2);
  g_object_unref (win3);
  g_object_unref (application);
}

/* Initialize test suite */

void
test_application_create_suite (GDBusConnection *connection)
{
#define DOMAIN "/Application"

  gdbus_connection = connection;

  g_test_add_func (DOMAIN"/Allocation", test_allocation);
  g_test_add_func (DOMAIN"/ContainsSimilarToWindow", test_contain_similar_to_window);
  g_test_add_func (DOMAIN"/ContainsSimilarToWindow/Null", test_contain_similar_to_window_null);
  g_test_add_func (DOMAIN"/Type", test_type);
  g_test_add_func (DOMAIN"/Type/Set", test_type_set);
  g_test_add_func (DOMAIN"/Type/Set/Invalid", test_type_set_invalid);
  g_test_add_func (DOMAIN"/DesktopFile", test_desktop_file);
  g_test_add_func (DOMAIN"/DesktopFile/Icon", test_desktop_icon);
  g_test_add_func (DOMAIN"/DesktopFile/Icon/Empty", test_desktop_icon_empty);
  g_test_add_func (DOMAIN"/DesktopFile/Icon/Invalid", test_desktop_icon_invalid);
  g_test_add_func (DOMAIN"/DesktopFile/Icon/FullPath", test_icon_full_path);
  g_test_add_func (DOMAIN"/DesktopFile/Icon/FullPath/Invalid", test_icon_full_path_invalid);
  g_test_add_func (DOMAIN"/DesktopFile/MimeTypes/Valid", test_get_mime_types);
  g_test_add_func (DOMAIN"/DesktopFile/MimeTypes/None", test_get_mime_types_none);
  g_test_add_func (DOMAIN"/DesktopFile/MainChild", test_desktop_app_main_child);
  g_test_add_func (DOMAIN"/DesktopFile/MainChild/NotMatchEmblems", test_desktop_app_main_child_doesnt_match_emblems);
  g_test_add_func (DOMAIN"/DesktopFile/MainChild/NotUpdatesEmblems", test_desktop_app_main_child_doesnt_update_emblems);
  g_test_add_func (DOMAIN"/DesktopFile/CreateLocalDesktopFile", test_desktop_app_create_local_desktop_file);
  g_test_add_func (DOMAIN"/DesktopLess/Icon/ClassName", test_icon_class_name);
  g_test_add_func (DOMAIN"/DesktopLess/Icon/Exec", test_icon_exec_string);
  g_test_add_func (DOMAIN"/DesktopLess/Icon/Embedded", test_icon_embedded);
  g_test_add_func (DOMAIN"/DesktopLess/Icon/Priority", test_icon_priority);
  g_test_add_func (DOMAIN"/DesktopLess/Icon/Generic/Class", test_icon_generic_class);
  g_test_add_func (DOMAIN"/DesktopLess/Icon/Generic/Exec", test_icon_generic_exec);
  g_test_add_func (DOMAIN"/DesktopLess/MainChild", test_app_main_child);
  g_test_add_func (DOMAIN"/DesktopLess/MainChild/MatchesEmblems", test_app_main_child_matches_emblems);
  g_test_add_func (DOMAIN"/DesktopLess/MainChild/UpdatesEmblems", test_app_main_child_updates_emblems);
  g_test_add_func (DOMAIN"/DesktopLess/MainChild/MultipleChildren", test_app_main_child_multiple_children);
  g_test_add_func (DOMAIN"/DesktopLess/MainChild/NormalPriority", test_app_main_child_normal_priority);
  g_test_add_func (DOMAIN"/DesktopLess/MainChild/Removal", test_app_main_child_on_window_removal);
  g_test_add_func (DOMAIN"/DesktopLess/MainChild/ReplaceOnRemoval", test_app_main_child_on_window_replace_on_removal);
  g_test_add_func (DOMAIN"/DesktopLess/CreateLocalDesktopFile/InvalidExec", test_desktopless_app_create_local_desktop_file_invalid_exec);
  g_test_add_func (DOMAIN"/DesktopLess/CreateLocalDesktopFile/UsingClassInstance", test_desktopless_app_create_local_desktop_file_using_instance_class_basename);
  g_test_add_func (DOMAIN"/DesktopLess/CreateLocalDesktopFile/UsingClassName", test_desktopless_app_create_local_desktop_file_using_name_class_basename);
  g_test_add_func (DOMAIN"/DesktopLess/CreateLocalDesktopFile/UsingExec", test_desktopless_app_create_local_desktop_file_using_exec_basename);
  g_test_add_func (DOMAIN"/DesktopLess/CreateLocalDesktopFile/UsingTrimmedExec", test_desktopless_app_create_local_desktop_file_using_trimmed_exec_basename);
  g_test_add_func (DOMAIN"/DesktopLess/CreateLocalDesktopFile/WithWorkingDir", test_desktopless_app_create_local_desktop_file_with_working_dir);
  g_test_add_func (DOMAIN"/ManagesXid", test_manages_xid);
  g_test_add_func (DOMAIN"/GetWindow", test_get_window);
  g_test_add_func (DOMAIN"/Xids", test_get_xids);
  g_test_add_func (DOMAIN"/Events/Active", test_active);
  g_test_add_func (DOMAIN"/Events/Urgent", test_urgent);
  g_test_add_func (DOMAIN"/Events/UserVisible", test_user_visible);
  g_test_add_func (DOMAIN"/Events/WindowAdded", test_window_added);
  g_test_add_func (DOMAIN"/Events/WindowRemoved", test_window_removed);
}
