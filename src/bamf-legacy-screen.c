/*
 * Copyright (C) 2010 Canonical Ltd
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
 *
 */

#include "bamf-legacy-screen.h"
#include "bamf-legacy-screen-private.h"
#include <gdk/gdkx.h>
#include <gio/gio.h>

G_DEFINE_TYPE (BamfLegacyScreen, bamf_legacy_screen, G_TYPE_OBJECT);
#define BAMF_LEGACY_SCREEN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE(obj, \
BAMF_TYPE_LEGACY_SCREEN, BamfLegacyScreenPrivate))

static BamfLegacyScreen *static_screen = NULL;

enum
{
  WINDOW_OPENED,
  WINDOW_CLOSED,
  STACKING_CHANGED,
  ACTIVE_WINDOW_CHANGED,

  LAST_SIGNAL,
};

static guint legacy_screen_signals[LAST_SIGNAL] = { 0 };
static Atom _COMPIZ_TOOLKIT_ACTION = 0;
static Atom _COMPIZ_TOOLKIT_ACTION_WINDOW_MENU = 0;

struct _BamfLegacyScreenPrivate
{
  WnckScreen * legacy_screen;
  GList *windows;
  GFile *file;
  GDataInputStream *stream;
};

static void
handle_window_closed (BamfLegacyWindow *window, BamfLegacyScreen *self)
{
  self->priv->windows = g_list_remove (self->priv->windows, window);

  g_signal_emit (self, legacy_screen_signals[WINDOW_CLOSED], 0, window);

  g_object_unref (window);
}

static gboolean
on_state_file_load_timeout (BamfLegacyScreen *self)
{
  BamfLegacyWindow *window;
  GDataInputStream *stream;
  gchar *line, *name, *class, *exec;
  GList *l;
  gchar **parts;
  gsize parts_size;
  guint32 xid;

  g_return_val_if_fail (BAMF_IS_LEGACY_SCREEN (self), FALSE);

  stream = self->priv->stream;

  line = g_data_input_stream_read_line (stream, NULL, NULL, NULL);

  if (!line)
    return FALSE;

  // Line format:
  // open       <xid>   <name>  <wmclass> <exec>
  // close      <xid>
  // attention  <xid>   <true/false>
  // skip       <xid>   <true/false>
  // geometry <xid> <x> <y> <width> <height>
  // maximized <xid> <maximized/vmaximized/hmaximized/floating>

  parts = g_strsplit (line, "\t", 0);
  g_free (line);

  parts_size = 0;
  while (parts[parts_size] != NULL)
    parts_size++;

  if (parts_size < 2)
    return FALSE;

  xid = (guint32) atol (parts[1]);
  if (g_strcmp0 (parts[0], "open") == 0 && parts_size == 5)
    {
      name  = parts[2];
      class = parts[3];
      exec  = parts[4];

      BamfLegacyWindowTest *test_win = bamf_legacy_window_test_new (xid, name, class, exec);
      _bamf_legacy_screen_open_test_window (self, test_win);
    }
  else if (g_strcmp0 (parts[0], "close") == 0 && parts_size == 2)
    {
      for (l = self->priv->windows; l; l = l->next)
        {
          window = l->data;
          if (bamf_legacy_window_get_xid (window) == xid)
            {
              _bamf_legacy_screen_close_test_window (self, BAMF_LEGACY_WINDOW_TEST (window));
              break;
            }
        }
    }
  else if (g_strcmp0 (parts[0], "attention") == 0 && parts_size == 3)
    {
      gboolean attention = FALSE;
      if (g_strcmp0 (parts[2], "true") == 0)
        attention = TRUE;
      else if (g_strcmp0 (parts[2], "false") == 0)
        attention = FALSE;
      else
        return TRUE;

      for (l = self->priv->windows; l; l = l->next)
        {
          if (bamf_legacy_window_get_xid (l->data) == xid)
            {
              bamf_legacy_window_test_set_attention (l->data, attention);
              break;
            }
        }
    }
  else if (g_strcmp0 (parts[0], "skip") == 0 && parts_size ==  3)
    {
      gboolean skip = FALSE;
      if (g_strcmp0 (parts[2], "true") == 0)
        skip = TRUE;
      else if (g_strcmp0 (parts[2], "false") == 0)
        skip = FALSE;
      else
        return TRUE;

      for (l = self->priv->windows; l; l = l->next)
        {
          if (bamf_legacy_window_get_xid (l->data) == xid)
            {
              bamf_legacy_window_test_set_skip (l->data, skip);
              break;
            }
        }
    }
  else if (g_strcmp0 (parts[0], "geometry") == 0 && parts_size == 6)
    {
      int x = atoi (parts[2]);
      int y = atoi (parts[3]);
      int width = atoi (parts[4]);
      int height = atoi (parts[5]);

      for (l = self->priv->windows; l; l = l->next)
        {
          if (bamf_legacy_window_get_xid (l->data) == xid)
            {
              bamf_legacy_window_test_set_geometry (l->data, x, y, width, height);
              break;
            }
        }
    }
  else if (g_strcmp0 (parts[0], "maximized") == 0 && parts_size == 3)
    {
      BamfWindowMaximizationType maximized;

      if (g_strcmp0 (parts[2], "maximized") == 0)
        maximized = BAMF_WINDOW_MAXIMIZED;
      else if (g_strcmp0 (parts[2], "vmaximized") == 0)
        maximized = BAMF_WINDOW_VERTICAL_MAXIMIZED;
      else if (g_strcmp0 (parts[2], "hmaximized") == 0)
        maximized = BAMF_WINDOW_HORIZONTAL_MAXIMIZED;
      else if (g_strcmp0 (parts[2], "floating") == 0)
        maximized = BAMF_WINDOW_FLOATING;
      else
        return TRUE;

      for (l = self->priv->windows; l; l = l->next)
        {
          if (bamf_legacy_window_get_xid (l->data) == xid)
            {
              bamf_legacy_window_test_set_maximized (l->data, maximized);
              break;
            }
        }
    }
  else
    {
      g_warning ("Could not parse line\n");
    }

  g_strfreev (parts);
  return TRUE;
}

static gint
compare_windows_by_stack_order (gconstpointer a, gconstpointer b, gpointer data)
{
  BamfLegacyScreen *self;
  GList *l;
  guint xid_a, xid_b;

  g_return_val_if_fail (BAMF_IS_LEGACY_SCREEN (data), 1);
  self = BAMF_LEGACY_SCREEN (data);

  xid_a = bamf_legacy_window_get_xid (BAMF_LEGACY_WINDOW (a));
  xid_b = bamf_legacy_window_get_xid (BAMF_LEGACY_WINDOW (b));

  for (l = wnck_screen_get_windows_stacked (self->priv->legacy_screen); l; l = l->next)
  {
    gulong legacy_xid = wnck_window_get_xid (WNCK_WINDOW (l->data));

    if (legacy_xid == xid_a)
      return -1;

    if (legacy_xid == xid_b)
      return 1;
  }

  return 0;
}

static void
handle_window_opened (WnckScreen *screen, WnckWindow *window, BamfLegacyScreen *legacy)
{
  BamfLegacyWindow *legacy_window;
  g_return_if_fail (WNCK_IS_WINDOW (window));

  legacy_window = bamf_legacy_window_new (window);

  g_signal_connect (G_OBJECT (legacy_window), "closed",
                    (GCallback) handle_window_closed, legacy);

  legacy->priv->windows = g_list_insert_sorted_with_data (legacy->priv->windows, legacy_window,
                                                          compare_windows_by_stack_order,
                                                          legacy);

  g_signal_emit (legacy, legacy_screen_signals[WINDOW_OPENED], 0, legacy_window);
}

static void
handle_stacking_changed (WnckScreen *screen, BamfLegacyScreen *legacy)
{
  legacy->priv->windows = g_list_sort_with_data (legacy->priv->windows,
                                                 compare_windows_by_stack_order,
                                                 legacy);

  g_signal_emit (legacy, legacy_screen_signals[STACKING_CHANGED], 0);
}

/* This function allows to push into the screen a window by its xid.
 * If the window is already known, it's just ignored, otherwise it gets added
 * to the windows list. The BamfLegacyScreen should automatically update its
 * windows list when they are added/removed from the screen, but if a child
 * BamfLegacyWindow is closed, then it could be possible to re-add it.        */
void
bamf_legacy_screen_inject_window (BamfLegacyScreen *self, guint xid)
{
  g_return_if_fail (BAMF_IS_LEGACY_SCREEN (self));
  BamfLegacyWindow *window;
  GList *l;

  for (l = self->priv->windows; l; l = l->next)
    {
      window = l->data;

      if (bamf_legacy_window_get_xid (window) == xid)
        {
          return;
        }
    }

  WnckWindow *legacy_window = wnck_window_get (xid);

  if (WNCK_IS_WINDOW (legacy_window))
    {
      handle_window_opened (NULL, legacy_window, self);
    }
}

void
bamf_legacy_screen_set_state_file (BamfLegacyScreen *self,
                                   const char *file)
{
  GFile *gfile;
  GDataInputStream *stream;

  g_return_if_fail (BAMF_IS_LEGACY_SCREEN (self));

  // Disconnect our handlers so we can work purely on the file
  g_signal_handlers_disconnect_by_func (self->priv->legacy_screen, handle_window_opened, self);
  g_signal_handlers_disconnect_by_func (self->priv->legacy_screen, handle_window_closed, self);
  g_signal_handlers_disconnect_by_func (self->priv->legacy_screen, handle_stacking_changed, self);

  gfile = g_file_new_for_path (file);

  if (!file)
    {
      g_error ("Could not open file %s", file);
    }

  stream = g_data_input_stream_new (G_INPUT_STREAM (g_file_read (gfile, NULL, NULL)));

  if (!stream)
    {
      g_error ("Could not open file stream for %s", file);
    }

  self->priv->file = gfile;
  self->priv->stream = stream;

  g_timeout_add (500, (GSourceFunc) on_state_file_load_timeout, self);
}

GList *
bamf_legacy_screen_get_windows (BamfLegacyScreen *screen)
{
  g_return_val_if_fail (BAMF_IS_LEGACY_SCREEN (screen), NULL);

  return screen->priv->windows;
}

BamfLegacyWindow *
bamf_legacy_screen_get_active_window (BamfLegacyScreen *screen)
{
  BamfLegacyWindow *window;
  GList *l;

  g_return_val_if_fail (BAMF_IS_LEGACY_SCREEN (screen), NULL);

  for (l = screen->priv->windows; l; l = l->next)
    {
      window = l->data;

      if (bamf_legacy_window_is_active (window))
        return window;
    }

  return NULL;
}

static BamfLegacyWindow *
bamf_legacy_screen_get_window_by_xid (BamfLegacyScreen *screen, Window xid)
{
  BamfLegacyWindow *window;
  GList *l;

  g_return_val_if_fail (BAMF_IS_LEGACY_SCREEN (screen), NULL);

  for (l = screen->priv->windows; l; l = l->next)
    {
      window = l->data;

      if (bamf_legacy_window_get_xid (window) == xid)
        return window;
    }

  return NULL;
}

static void
handle_active_window_changed (WnckScreen *screen, WnckWindow *previous, BamfLegacyScreen *self)
{
  g_return_if_fail (BAMF_IS_LEGACY_SCREEN (self));

  g_signal_emit (self, legacy_screen_signals[ACTIVE_WINDOW_CHANGED], 0);
}

static void
bamf_legacy_screen_finalize (GObject *object)
{
  BamfLegacyScreen *self = BAMF_LEGACY_SCREEN (object);

  if (self->priv->windows)
    g_list_free_full (self->priv->windows, g_object_unref);

  if (self->priv->file)
    g_object_unref (self->priv->file);

  if (self->priv->stream)
    g_object_unref (self->priv->stream);

  wnck_shutdown ();
  static_screen = NULL;

  G_OBJECT_CLASS (bamf_legacy_screen_parent_class)->finalize (object);
}

static void
bamf_legacy_screen_init (BamfLegacyScreen * self)
{
  self->priv = BAMF_LEGACY_SCREEN_GET_PRIVATE (self);
}

static void
bamf_legacy_screen_class_init (BamfLegacyScreenClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bamf_legacy_screen_finalize;

  g_type_class_add_private (klass, sizeof (BamfLegacyScreenPrivate));

  legacy_screen_signals [WINDOW_OPENED] =
    g_signal_new (BAMF_LEGACY_SCREEN_SIGNAL_WINDOW_OPENED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfLegacyScreenClass, window_opened),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_LEGACY_WINDOW);

  legacy_screen_signals [WINDOW_CLOSED] =
    g_signal_new (BAMF_LEGACY_SCREEN_SIGNAL_WINDOW_CLOSED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfLegacyScreenClass, window_closed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_LEGACY_WINDOW);

  legacy_screen_signals [STACKING_CHANGED] =
    g_signal_new (BAMF_LEGACY_SCREEN_SIGNAL_STACKING_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfLegacyScreenClass, stacking_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  legacy_screen_signals [ACTIVE_WINDOW_CHANGED] =
    g_signal_new (BAMF_LEGACY_SCREEN_SIGNAL_ACTIVE_WINDOW_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfLegacyScreenClass, active_window_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

#include <gdk/gdkx.h>

GdkFilterReturn filter_compiz_messages(GdkXEvent *gdkxevent, GdkEvent *event, gpointer data)
{
  BamfLegacyScreen *self = data;
  BamfLegacyWindow *window;
  XEvent *xevent = gdkxevent;

  if (xevent->type == ClientMessage)
    {
      if (xevent->xclient.message_type == _COMPIZ_TOOLKIT_ACTION)
        {
          Atom msg = xevent->xclient.data.l[0];

          if (msg == _COMPIZ_TOOLKIT_ACTION_WINDOW_MENU)
            {
              window = bamf_legacy_screen_get_window_by_xid (self, xevent->xany.window);

              if (BAMF_IS_LEGACY_WINDOW (window))
                {
                  Time time = xevent->xclient.data.l[1];
                  int button = xevent->xclient.data.l[2];
                  int x = xevent->xclient.data.l[3];
                  int y = xevent->xclient.data.l[4];

                  bamf_legacy_window_show_action_menu (window, time, button, x, y);

                  return GDK_FILTER_REMOVE;
                }
            }
        }
    }

  return GDK_FILTER_CONTINUE;
}

BamfLegacyScreen *
bamf_legacy_screen_get_default ()
{
  BamfLegacyScreen *self;

  if (static_screen)
    return static_screen;

  self = (BamfLegacyScreen *) g_object_new (BAMF_TYPE_LEGACY_SCREEN, NULL);
  static_screen = self;

  if (g_strcmp0 (g_getenv ("BAMF_TEST_MODE"), "TRUE") == 0)
    return static_screen;

  wnck_set_default_icon_size (BAMF_DEFAULT_ICON_SIZE);
  wnck_set_default_mini_icon_size (BAMF_DEFAULT_MINI_ICON_SIZE);

  self->priv->legacy_screen = wnck_screen_get_default ();

  g_signal_connect (G_OBJECT (self->priv->legacy_screen), "window-opened",
                    (GCallback) handle_window_opened, self);

  g_signal_connect (G_OBJECT (self->priv->legacy_screen), "window-stacking-changed",
                    (GCallback) handle_stacking_changed, self);

  g_signal_connect (G_OBJECT (self->priv->legacy_screen), "active-window-changed",
                    (GCallback) handle_active_window_changed, self);

  if (g_strcmp0 (g_getenv ("XDG_CURRENT_DESKTOP"), "Unity") == 0)
    {
      Display *dpy = gdk_x11_get_default_xdisplay ();
      _COMPIZ_TOOLKIT_ACTION = XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION", False);
      _COMPIZ_TOOLKIT_ACTION_WINDOW_MENU = XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION_WINDOW_MENU", False);
      gdk_window_add_filter (NULL, filter_compiz_messages, self);
    }

  return static_screen;
}


// Private functions for testing purposes

void _bamf_legacy_screen_open_test_window (BamfLegacyScreen *self, BamfLegacyWindowTest *test_window)
{
  GList *l;
  BamfLegacyWindow *window;
  guint xid;

  g_return_if_fail (BAMF_IS_LEGACY_SCREEN (self));
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (test_window));

  window = BAMF_LEGACY_WINDOW (test_window);
  xid = bamf_legacy_window_get_xid (window);

  for (l = self->priv->windows; l; l = l->next)
    {
      if (bamf_legacy_window_get_xid (BAMF_LEGACY_WINDOW (l->data)) == xid)
        {
          return;
        }
    }

  self->priv->windows = g_list_append (self->priv->windows, window);
  g_signal_emit (self, legacy_screen_signals[STACKING_CHANGED], 0);

  g_signal_connect (G_OBJECT (window), "closed",
                    (GCallback) handle_window_closed, self);

  g_signal_emit (self, legacy_screen_signals[WINDOW_OPENED], 0, window);
}

void _bamf_legacy_screen_close_test_window (BamfLegacyScreen *self, BamfLegacyWindowTest *test_window)
{
  g_return_if_fail (BAMF_IS_LEGACY_SCREEN (self));
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (test_window));

  // This will cause handle_window_closed to be called
  bamf_legacy_window_test_close (BAMF_LEGACY_WINDOW_TEST (test_window));
}
