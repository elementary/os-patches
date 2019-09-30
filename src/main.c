/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <locale.h>
#include <stdlib.h> /* exit() */

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "service.h"

/***
****
***/

static void
on_name_lost (gpointer instance G_GNUC_UNUSED, gpointer loop)
{
  g_warning ("exiting: service couldn't acquire, or lost ownership of, busname");

  g_main_loop_quit (loop);
}

int
main (int argc G_GNUC_UNUSED, char ** argv G_GNUC_UNUSED)
{
  GMainLoop * loop;
  IndicatorSessionService * service;

  /* Work around a deadlock in glib's type initialization. It can be
   * removed when https://bugzilla.gnome.org/show_bug.cgi?id=674885 is
   * fixed.
   */
  g_type_ensure (G_TYPE_DBUS_CONNECTION);

  /* boilerplate i18n */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  textdomain (GETTEXT_PACKAGE);

  /* run */
  service = indicator_session_service_new ();
  loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (service, INDICATOR_SESSION_SERVICE_SIGNAL_NAME_LOST,
                    G_CALLBACK(on_name_lost), loop);
  g_main_loop_run (loop);

  /* cleanup */
  g_clear_object (&service);
  g_main_loop_unref (loop);
  return 0;
}
