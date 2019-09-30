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
 * Authored by Marco Trevisan <marco.trevisan@canonical.com>
 *
 */

#include <stdio.h>
#include <glib.h>
#include <sys/types.h>
#include <unistd.h>
#include <gio/gio.h>

void test_matcher_create_suite (void);
void test_application_create_suite (void);

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

gint
main (gint argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  const gchar *old_path = g_getenv ("PATH");
  gchar *new_path = g_strdup_printf (TESTDIR"/data/bin:%s", old_path);
  g_setenv ("BAMF_TEST_MODE", "TRUE", TRUE);
  g_setenv ("PATH", new_path, TRUE);
  g_free (new_path);

  test_matcher_create_suite ();
  test_application_create_suite ();

  return g_test_run ();
}
