/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by Marco Trevisan (Treviño) <marco.trevisan@canonical.com>
 *
 */

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <libbamf-private/bamf-private.h>
#include "bamf-view-private.h"

#define DATA_DIR TESTDIR "/data"

void ignore_fatal_errors (void);

static void
test_allocation (void)
{
  BamfApplication *application;

  /* Check it allocates */
  application = g_object_new (BAMF_TYPE_APPLICATION, NULL);
  g_assert (BAMF_IS_APPLICATION (application));

  g_object_unref (application);
}

static void
test_favorite_invalid_desktop (void)
{
  BamfApplication *application;

  application = bamf_application_new_favorite (DATA_DIR"/invalid-type.desktop");
  g_assert (!BAMF_IS_APPLICATION (application));

  application = bamf_application_new_favorite (DATA_DIR"/not-existing-file.desktop");
  g_assert (!BAMF_IS_APPLICATION (application));

  ignore_fatal_errors ();
  application = bamf_application_new_favorite (NULL);
  g_assert (!BAMF_IS_APPLICATION (application));
}

static void
test_favorite_valid_desktop_file_system (void)
{
  BamfApplication *application;
  const gchar *desktop_file = DATA_DIR"/gnome-control-center.desktop";
  application = bamf_application_new_favorite (desktop_file);

  g_assert_cmpstr (bamf_application_get_desktop_file (application), ==, desktop_file);
  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), ==, "System Settings");
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, "preferences-system");
  g_assert_cmpstr (bamf_application_get_application_type (application), ==, "system");

  g_object_unref (application);
}

static void
test_favorite_full_name (void)
{
  BamfApplication *application;

  application = bamf_application_new_favorite (DATA_DIR"/full-name.desktop");
  g_assert_cmpstr (bamf_view_get_name (BAMF_VIEW (application)), ==, "Full Application Name");

  g_object_unref (application);
}

static void
test_favorite_no_icon (void)
{
  BamfApplication *application;

  application = bamf_application_new_favorite (DATA_DIR"/no-icon.desktop");
  g_assert_cmpstr (bamf_view_get_icon (BAMF_VIEW (application)), ==, BAMF_APPLICATION_DEFAULT_ICON);

  g_object_unref (application);
}

static void
test_favorite_mime_type_filled (void)
{
  BamfApplication *application;

  application = bamf_application_new_favorite (TESTDIR"/data/mime-types.desktop");

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
test_favorite_mime_type_empty (void)
{
  BamfApplication *application;

  application = bamf_application_new_favorite (TESTDIR"/data/test-bamf-app.desktop");
  gchar** mimes = bamf_application_get_supported_mime_types (application);
  g_assert (!mimes);

  g_object_unref (application);
}

void
test_application_create_suite (void)
{
#define DOMAIN "/Application"

  g_test_add_func (DOMAIN"/Allocation", test_allocation);
  g_test_add_func (DOMAIN"/Favorite/DesktopFile/Invalid", test_favorite_invalid_desktop);
  g_test_add_func (DOMAIN"/Favorite/DesktopFile/Valid/System", test_favorite_valid_desktop_file_system);
  g_test_add_func (DOMAIN"/Favorite/FullName", test_favorite_full_name);
  g_test_add_func (DOMAIN"/Favorite/NoIcon", test_favorite_no_icon);
  g_test_add_func (DOMAIN"/Favorite/MimeType/Filled", test_favorite_mime_type_filled);
  g_test_add_func (DOMAIN"/Favorite/MimeType/Empty", test_favorite_mime_type_empty);
}
