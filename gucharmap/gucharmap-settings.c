/*
 * Copyright © 2005 Jason Allen
 * Copyright © 2007 Christian Persch
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

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gucharmap/gucharmap.h>
#include "gucharmap-settings.h"

#define WINDOW_STATE_TIMEOUT 1 /* s */

typedef struct {
  GSettings *settings;
  guint timeout_id;
  int width;
  int height;
  guint is_maximised : 1;
  guint is_fullscreen : 1;
} WindowState;

static gboolean
window_state_timeout_cb (WindowState *state)
{
  g_settings_set (state->settings, "size", "(ii)", state->width, state->height);

  state->timeout_id = 0;
  return FALSE;
}

static void
free_window_state (WindowState *state)
{
  if (state->timeout_id != 0) {
    g_source_remove (state->timeout_id);

    /* And store now */
    window_state_timeout_cb (state);
  }

  g_object_unref (state->settings);

  g_slice_free (WindowState, state);
}

static gboolean
window_configure_event_cb (GtkWidget *widget,
                           GdkEventConfigure *event,
                           WindowState *state)
{
  if (!state->is_maximised && !state->is_fullscreen &&
      (state->width != event->width || state->height != event->height)) {
    state->width = event->width;
    state->height = event->height;

    if (state->timeout_id == 0) {
      state->timeout_id = g_timeout_add_seconds (WINDOW_STATE_TIMEOUT,
                                                 (GSourceFunc) window_state_timeout_cb,
                                                 state);
    }
  }

  return FALSE;
}

static gboolean
window_state_event_cb (GtkWidget *widget,
                       GdkEventWindowState *event,
                       WindowState *state)
{
  if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
    state->is_maximised = (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    g_settings_set_boolean (state->settings, "maximized", state->is_maximised);
  }
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
    state->is_fullscreen = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
    g_settings_set_boolean (state->settings, "fullscreen", state->is_fullscreen);
  }

  return FALSE;
}

/**
 * gucharma_settings_add_window:
 * @window: a #GtkWindow
 *
 * Restore the window configuration, and persist changes to the window configuration:
 * window width and height, and maximised and fullscreen state.
 * @window must not be realised yet.
 */
void
gucharmap_settings_add_window (GtkWindow *window)
{
  WindowState *state;
  int width, height;
  gboolean maximised, fullscreen;

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!gtk_widget_get_realized (GTK_WIDGET (window)));

  state = g_slice_new0 (WindowState);
  state->settings = g_settings_new ("org.gnome.Charmap.WindowState");
  g_object_set_data_full (G_OBJECT (window), "GamesConf::WindowState",
                          state, (GDestroyNotify) free_window_state);

  g_signal_connect (window, "configure-event",
                    G_CALLBACK (window_configure_event_cb), state);
  g_signal_connect (window, "window-state-event",
                    G_CALLBACK (window_state_event_cb), state);

  maximised = g_settings_get_boolean (state->settings, "maximized");
  fullscreen = g_settings_get_boolean (state->settings, "fullscreen");
  g_settings_get (state->settings, "size", "(ii)", &width, &height);

  if (width > 0 && height > 0) {
    gtk_window_set_default_size (GTK_WINDOW (window), width, height);
  }
  if (maximised) {
    gtk_window_maximize (GTK_WINDOW (window));
  }
  if (fullscreen) {
    gtk_window_fullscreen (GTK_WINDOW (window));
  }
}
