/*
 * Copyright (C) 2010-2011 Canonical Ltd
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
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#include "config.h"
#include "bamf-daemon.h"
#include "bamf-legacy-screen.h"

#include "main.h"

int
main (int argc, char **argv)
{
  BamfDaemon *daemon;
  GOptionContext *options;
  GError *error = NULL;
  char *state_file = NULL;

  gtk_init (&argc, &argv);
  glibtop_init ();

  options = g_option_context_new ("");
  g_option_context_set_help_enabled (options, TRUE);
  g_option_context_set_summary (options, "It's one, and so are we...");

  GOptionEntry entries[] =
  {
    {"load-file", 'l', 0, G_OPTION_ARG_STRING, &state_file, "Load bamf state from file instead of the system", NULL },
    {NULL}
  };

  g_option_context_add_main_entries (options, entries, NULL);
  g_option_context_add_group (options, gtk_get_option_group (FALSE));
  g_option_context_parse (options, &argc, &argv, &error);

  if (error)
    {
      g_print ("%s, error: %s\n", g_option_context_get_help (options, TRUE, NULL), error->message);
      g_clear_error (&error);
      exit (1);
    }

  if (state_file)
    {
      bamf_legacy_screen_set_state_file (bamf_legacy_screen_get_default (), state_file);
    }

  daemon = bamf_daemon_get_default ();
  bamf_daemon_start (daemon);

  g_object_unref (daemon);

  return 0;
}
