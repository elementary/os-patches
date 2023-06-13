/*
 * Based on demos/minimal.c, but with automatic exit after 1 second
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

#include <champlain/champlain.h>

static gboolean
timeout_cb (gpointer user_data)
{
  clutter_main_quit ();
  return FALSE;
}

int
main (int argc, char *argv[])
{
  ClutterActor *actor, *stage;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 800, 600);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Create the map view */
  actor = champlain_view_new ();
  clutter_actor_set_size (actor, 800, 600);
  clutter_actor_add_child (stage, actor);

  if (g_getenv ("TEST_INTERACTIVE") == NULL)
    g_timeout_add_seconds (1, timeout_cb, NULL);

  clutter_actor_show (stage);
  clutter_main ();

  return 0;
}
