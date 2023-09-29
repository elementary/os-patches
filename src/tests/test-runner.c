/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "backends/meta-virtual-monitor.h"
#include "core/window-private.h"
#include "meta-test/meta-context-test.h"
#include "meta/util.h"
#include "meta/window.h"
#include "meta/meta-workspace-manager.h"
#include "tests/meta-test-utils.h"
#include "ui/ui.h"
#include "wayland/meta-wayland.h"
#include "x11/meta-x11-display-private.h"

typedef struct {
  MetaContext *context;
  GHashTable *clients;
  MetaAsyncWaiter *waiter;
  GString *warning_messages;
  GMainLoop *loop;
  gulong x11_display_opened_handler_id;
  MetaVirtualMonitor *virtual_monitor;
} TestCase;

static gboolean
test_case_alarm_filter (MetaX11Display        *x11_display,
                        XSyncAlarmNotifyEvent *event,
                        gpointer               data)
{
  TestCase *test = data;
  GHashTableIter iter;
  gpointer key, value;

  if (meta_async_waiter_process_x11_event (test->waiter, x11_display, event))
    return TRUE;

  g_hash_table_iter_init (&iter, test->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      MetaTestClient *client = value;

      if (meta_test_client_process_x11_event (client, x11_display, event))
        return TRUE;
    }

  return FALSE;
}

static void
on_x11_display_opened (MetaDisplay *display,
                       TestCase    *test)
{
  meta_x11_display_set_alarm_filter (display->x11_display,
                                     test_case_alarm_filter, test);
  test->waiter = meta_async_waiter_new (display->x11_display);
}

static TestCase *
test_case_new (MetaContext *context)
{
  TestCase *test = g_new0 (TestCase, 1);
  MetaDisplay *display = meta_context_get_display (context);

  if (display->x11_display)
    {
      on_x11_display_opened (display, test);
    }
  else
    {
      test->x11_display_opened_handler_id =
        g_signal_connect (meta_get_display (), "x11-display-opened",
                          G_CALLBACK (on_x11_display_opened),
                          test);
    }

  test->context = context;
  test->clients = g_hash_table_new (g_str_hash, g_str_equal);
  test->loop = g_main_loop_new (NULL, FALSE);
  test->virtual_monitor = meta_create_test_monitor (context, 800, 600, 60.0);

  return test;
}

static gboolean
test_case_loop_quit (gpointer data)
{
  TestCase *test = data;

  g_main_loop_quit (test->loop);

  return FALSE;
}

static gboolean
test_case_dispatch (TestCase *test,
                    GError  **error)
{
  MetaBackend *backend = meta_context_get_backend (test->context);
  ClutterActor *stage = meta_backend_get_stage (backend);

  /* Wait until we've done any outstanding queued up work.
   * Though we add this as BEFORE_REDRAW, the iteration that runs the
   * BEFORE_REDRAW idles will proceed on and do the redraw, so we're
   * waiting until after *all* frame processing.
   */
  meta_later_add (META_LATER_BEFORE_REDRAW,
                  test_case_loop_quit,
                  test,
                  NULL);

  clutter_stage_schedule_update (CLUTTER_STAGE (stage));
  g_main_loop_run (test->loop);

  return TRUE;
}

static gboolean
test_case_wait (TestCase *test,
                GError  **error)
{
  GHashTableIter iter;
  gpointer key, value;

  /* First have each client set a XSync counter, and wait until
   * we receive the resulting event - so we know we've received
   * everything that the client have sent us.
   */
  g_hash_table_iter_init (&iter, test->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    if (!meta_test_client_wait (value, error))
      return FALSE;

  /* Then wait until we've done any outstanding queued up work. */
  test_case_dispatch (test, error);

  /* Then set an XSync counter ourselves and and wait until
   * we receive the resulting event - this makes sure that we've
   * received back any X events we generated.
   */
  if (test->waiter)
    meta_async_waiter_set_and_wait (test->waiter);
  return TRUE;
}

static gboolean
test_case_sleep (TestCase  *test,
                 guint32    interval,
                 GError   **error)
{
  g_timeout_add_full (G_PRIORITY_LOW, interval, test_case_loop_quit, test, NULL);
  g_main_loop_run (test->loop);

  return TRUE;
}

#define BAD_COMMAND(...)                                                \
  G_STMT_START {                                                        \
      g_set_error (error,                                               \
                   META_TEST_CLIENT_ERROR,                              \
                   META_TEST_CLIENT_ERROR_BAD_COMMAND,                  \
                   __VA_ARGS__);                                        \
      return FALSE;                                                     \
  } G_STMT_END

static MetaTestClient *
test_case_lookup_client (TestCase *test,
                         char     *client_id,
                         GError  **error)
{
  MetaTestClient *client = g_hash_table_lookup (test->clients, client_id);
  if (!client)
    {
      g_set_error (error,
                   META_TEST_CLIENT_ERROR,
                   META_TEST_CLIENT_ERROR_BAD_COMMAND,
                   "No such client %s", client_id);
    }

  return client;
}

static gboolean
test_case_parse_window_id (TestCase        *test,
                           const char      *client_and_window_id,
                           MetaTestClient **client,
                           const char     **window_id,
                           GError         **error)
{
  const char *slash = strchr (client_and_window_id, '/');
  char *tmp;
  if (slash == NULL)
    BAD_COMMAND ("client/window ID %s doesn't contain a /", client_and_window_id);

  *window_id = slash + 1;

  tmp = g_strndup (client_and_window_id, slash - client_and_window_id);
  *client = test_case_lookup_client (test, tmp, error);
  g_free (tmp);

  return client != NULL;
}

static gboolean
test_case_assert_stacking (TestCase *test,
                           char    **expected_windows,
                           int       n_expected_windows,
                           GError  **error)
{
  MetaDisplay *display = meta_get_display ();
  guint64 *windows;
  int n_windows;
  GString *stack_string = g_string_new (NULL);
  GString *expected_string = g_string_new (NULL);
  int i;

  meta_stack_tracker_get_stack (display->stack_tracker, &windows, &n_windows);
  for (i = 0; i < n_windows; i++)
    {
      MetaWindow *window = meta_display_lookup_stack_id (display, windows[i]);
      if (window != NULL && window->title)
        {
          /* See comment in meta_ui_new() about why the dummy window for GTK+ theming
           * is managed as a MetaWindow.
           */
          if (META_STACK_ID_IS_X11 (windows[i]) &&
              meta_ui_window_is_dummy (display->x11_display->ui, windows[i]))
            continue;

          if (stack_string->len > 0)
            g_string_append_c (stack_string, ' ');

          if (g_str_has_prefix (window->title, "test/"))
            g_string_append (stack_string, window->title + 5);
          else
            g_string_append_printf (stack_string, "(%s)", window->title);
        }
      else if (windows[i] == display->x11_display->guard_window)
        {
          if (stack_string->len > 0)
            g_string_append_c (stack_string, ' ');

          g_string_append_c (stack_string, '|');
        }
    }

  for (i = 0; i < n_expected_windows; i++)
    {
      if (expected_string->len > 0)
        g_string_append_c (expected_string, ' ');

      g_string_append (expected_string, expected_windows[i]);
    }

  /* Don't require '| ' as a prefix if there are no hidden windows - we
   * remove the prefix from the actual string instead of adding it to the
   * expected string for clarity of the error message
   */
  if (index (expected_string->str, '|') == NULL && stack_string->str[0] == '|')
    {
      g_string_erase (stack_string,
                      0, stack_string->str[1] == ' ' ? 2 : 1);
    }

  if (strcmp (expected_string->str, stack_string->str) != 0)
    {
      g_set_error (error,
                   META_TEST_CLIENT_ERROR,
                   META_TEST_CLIENT_ERROR_ASSERTION_FAILED,
                   "stacking: expected='%s', actual='%s'",
                   expected_string->str, stack_string->str);
    }

  g_string_free (stack_string, TRUE);
  g_string_free (expected_string, TRUE);

  return *error == NULL;
}

static gboolean
test_case_assert_focused (TestCase    *test,
                          const char  *expected_window,
                          GError     **error)
{
  MetaDisplay *display = meta_get_display ();

  if (!display->focus_window)
    {
      if (g_strcmp0 (expected_window, "none") != 0)
        {
          g_set_error (error,
                       META_TEST_CLIENT_ERROR,
                       META_TEST_CLIENT_ERROR_ASSERTION_FAILED,
                       "focus: expected='%s', actual='none'", expected_window);
        }
    }
  else
    {
      const char *focused = display->focus_window->title;

      if (g_str_has_prefix (focused, "test/"))
        focused += 5;

      if (g_strcmp0 (focused, expected_window) != 0)
        g_set_error (error,
                     META_TEST_CLIENT_ERROR,
                     META_TEST_CLIENT_ERROR_ASSERTION_FAILED,
                     "focus: expected='%s', actual='%s'",
                     expected_window, focused);
    }

  return *error == NULL;
}

static gboolean
test_case_assert_size (TestCase    *test,
                       MetaWindow  *window,
                       int          expected_width,
                       int          expected_height,
                       GError     **error)
{
  MetaRectangle frame_rect;

  meta_window_get_frame_rect (window, &frame_rect);

  if (frame_rect.width != expected_width ||
      frame_rect.height != expected_height)
    {
      g_set_error (error,
                   META_TEST_CLIENT_ERROR,
                   META_TEST_CLIENT_ERROR_ASSERTION_FAILED,
                   "Expected size %dx%d didn't match actual size %dx%d",
                   expected_width, expected_height,
                   frame_rect.width, frame_rect.height);
      return FALSE;
    }

  return TRUE;
}

static gboolean
test_case_check_xserver_stacking (TestCase *test,
                                  GError  **error)
{
  MetaDisplay *display = meta_get_display ();
  GString *local_string = g_string_new (NULL);
  GString *x11_string = g_string_new (NULL);
  int i;

  if (!display->x11_display)
    return TRUE;

  guint64 *windows;
  int n_windows;
  meta_stack_tracker_get_stack (display->stack_tracker, &windows, &n_windows);

  for (i = 0; i < n_windows; i++)
    {
      if (META_STACK_ID_IS_X11 (windows[i]))
        {
          if (local_string->len > 0)
            g_string_append_c (local_string, ' ');

          g_string_append_printf (local_string, "%#lx", (Window)windows[i]);
        }
    }

  Window root;
  Window parent;
  Window *children;
  unsigned int n_children;
  XQueryTree (display->x11_display->xdisplay,
              display->x11_display->xroot,
              &root, &parent, &children, &n_children);

  for (i = 0; i < (int)n_children; i++)
    {
      if (x11_string->len > 0)
        g_string_append_c (x11_string, ' ');

      g_string_append_printf (x11_string, "%#lx", (Window)children[i]);
    }

  if (strcmp (x11_string->str, local_string->str) != 0)
    g_set_error (error,
                 META_TEST_CLIENT_ERROR,
                 META_TEST_CLIENT_ERROR_ASSERTION_FAILED,
                 "xserver stacking: x11='%s', local='%s'",
                 x11_string->str, local_string->str);

  XFree (children);

  g_string_free (local_string, TRUE);
  g_string_free (x11_string, TRUE);

  return *error == NULL;
}

static int
maybe_divide (const char *str,
              int         value)
{
  if (strstr (str, "/") == str)
    {
      int divisor;

      str += 1;
      divisor = atoi (str);

      value /= divisor;
    }

  return value;
}

static int
parse_window_size (MetaWindow *window,
                   const char *size_str)
{
  MetaLogicalMonitor *logical_monitor;
  MetaRectangle logical_monitor_layout;
  int value;

  logical_monitor = meta_window_find_monitor_from_frame_rect (window);
  g_assert_nonnull (logical_monitor);

  logical_monitor_layout = meta_logical_monitor_get_layout (logical_monitor);

  if (strstr (size_str, "MONITOR_WIDTH") == size_str)
    {
      value = logical_monitor_layout.width;
      size_str += strlen ("MONITOR_WIDTH");
      value = maybe_divide (size_str, value);
    }
  else if (strstr (size_str, "MONITOR_HEIGHT") == size_str)
    {
      value = logical_monitor_layout.height;
      size_str += strlen ("MONITOR_HEIGHT");
      value = maybe_divide (size_str, value);
    }
  else
    {
      value = atoi (size_str);
    }

  return value;
}

static gboolean
test_case_do (TestCase *test,
              int       argc,
              char    **argv,
              GError  **error)
{
  if (strcmp (argv[0], "new_client") == 0)
    {
      MetaWindowClientType type;
      MetaTestClient *client;

      if (argc != 3)
        BAD_COMMAND("usage: new_client <client-id> [wayland|x11]");

      if (strcmp (argv[2], "x11") == 0)
        type = META_WINDOW_CLIENT_TYPE_X11;
      else if (strcmp (argv[2], "wayland") == 0)
        type = META_WINDOW_CLIENT_TYPE_WAYLAND;
      else
        BAD_COMMAND("usage: new_client <client-id> [wayland|x11]");

      if (g_hash_table_lookup (test->clients, argv[1]))
        BAD_COMMAND("client %s already exists", argv[1]);

      client = meta_test_client_new (test->context, argv[1], type, error);
      if (!client)
        return FALSE;

      g_hash_table_insert (test->clients, meta_test_client_get_id (client), client);
    }
  else if (strcmp (argv[0], "quit_client") == 0)
    {
      if (argc != 2)
        BAD_COMMAND("usage: quit_client <client-id>");

      MetaTestClient *client = test_case_lookup_client (test, argv[1], error);
      if (!client)
        return FALSE;

      if (!meta_test_client_quit (client, error))
        return FALSE;

      g_hash_table_remove (test->clients, meta_test_client_get_id (client));
      meta_test_client_destroy (client);
    }
  else if (strcmp (argv[0], "create") == 0)
    {
      if (!(argc == 2 ||
            (argc == 3 && strcmp (argv[2], "override") == 0) ||
            (argc == 3 && strcmp (argv[2], "csd") == 0)))
        BAD_COMMAND("usage: %s <client-id>/<window-id > [override|csd]", argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!meta_test_client_do (client, error,
                                "create", window_id,
                                argc == 3 ? argv[2] : NULL,
                                NULL))
        return FALSE;

      if (!meta_test_client_wait (client, error))
        return FALSE;
    }
  else if (strcmp (argv[0], "set_parent") == 0 ||
           strcmp (argv[0], "set_parent_exported") == 0)
    {
      if (argc != 3)
        BAD_COMMAND("usage: %s <client-id>/<window-id> <parent-window-id>",
                    argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!meta_test_client_do (client, error,
                                argv[0], window_id,
                                argv[2],
                                NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "accept_focus") == 0)
    {
      if (argc != 3 ||
          (g_ascii_strcasecmp (argv[2], "true") != 0 &&
           g_ascii_strcasecmp (argv[2], "false") != 0))
        BAD_COMMAND("usage: %s <client-id>/<window-id> [true|false]",
                    argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!meta_test_client_do (client, error,
                                argv[0], window_id,
                                argv[2],
                                NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "can_take_focus") == 0)
    {
      if (argc != 3 ||
          (g_ascii_strcasecmp (argv[2], "true") != 0 &&
           g_ascii_strcasecmp (argv[2], "false") != 0))
        BAD_COMMAND("usage: %s <client-id>/<window-id> [true|false]",
                    argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!meta_test_client_do (client, error,
                                argv[0], window_id,
                                argv[2],
                                NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "accept_take_focus") == 0)
    {
      if (argc != 3 ||
          (g_ascii_strcasecmp (argv[2], "true") != 0 &&
           g_ascii_strcasecmp (argv[2], "false") != 0))
        BAD_COMMAND("usage: %s <client-id>/<window-id> [true|false]",
                    argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!meta_test_client_do (client, error,
                                argv[0], window_id,
                                argv[2],
                                NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "show") == 0)
    {
      MetaWindow *window;
      gboolean show_async = FALSE;

      if (argc != 2 && argc != 3)
        BAD_COMMAND("usage: %s <client-id>/<window-id> [async]", argv[0]);

      if (argc == 3 && strcmp (argv[2], "async") == 0)
        show_async = TRUE;

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!meta_test_client_do (client, error, argv[0], window_id, NULL))
        return FALSE;

      if (!test_case_wait (test, error))
        return FALSE;

      window = meta_test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      if (!show_async)
        meta_test_client_wait_for_window_shown (client, window);
    }
  else if (strcmp (argv[0], "sync_shown") == 0)
    {
      MetaWindow *window;
      MetaTestClient *client;
      const char *window_id;

      if (argc != 2)
        BAD_COMMAND("usage: %s <client-id>/<window-id>", argv[0]);

      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      window = meta_test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      meta_test_client_wait_for_window_shown (client, window);
    }
  else if (strcmp (argv[0], "resize") == 0)
    {
      if (argc != 4)
        BAD_COMMAND("usage: %s <client-id>/<window-id> width height", argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!meta_test_client_do (client, error, argv[0], window_id,
                                argv[2], argv[3], NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "move") == 0)
    {
      MetaWindow *window;

      if (argc != 4)
        BAD_COMMAND("usage: %s <client-id>/<window-id> x y", argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      window = meta_test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      meta_window_move_frame (window, TRUE, atoi (argv[2]), atoi (argv[3]));
    }
  else if (strcmp (argv[0], "tile") == 0)
    {
      MetaWindow *window;

      if (argc != 3)
        BAD_COMMAND("usage: %s <client-id>/<window-id> [right|left]", argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      window = meta_test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      MetaTileMode tile_mode;
      if (strcmp (argv[2], "right") == 0)
        {
          tile_mode = META_TILE_RIGHT;
        }
      else if (strcmp (argv[2], "left") == 0)
        {
          tile_mode = META_TILE_LEFT;
        }
      else
        {
          g_set_error (error,
                       META_TEST_CLIENT_ERROR,
                       META_TEST_CLIENT_ERROR_ASSERTION_FAILED,
                       "Invalid tile mode '%s'", argv[2]);
          return FALSE;
        }

      meta_window_tile (window, tile_mode);
    }
  else if (strcmp (argv[0], "untile") == 0)
    {
      MetaWindow *window;

      if (argc != 2)
        BAD_COMMAND("usage: %s <client-id>/<window-id>", argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      window = meta_test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      meta_window_untile (window);
    }
  else if (strcmp (argv[0], "hide") == 0 ||
           strcmp (argv[0], "activate") == 0 ||
           strcmp (argv[0], "raise") == 0 ||
           strcmp (argv[0], "lower") == 0 ||
           strcmp (argv[0], "minimize") == 0 ||
           strcmp (argv[0], "unminimize") == 0 ||
           strcmp (argv[0], "maximize") == 0 ||
           strcmp (argv[0], "unmaximize") == 0 ||
           strcmp (argv[0], "fullscreen") == 0 ||
           strcmp (argv[0], "unfullscreen") == 0 ||
           strcmp (argv[0], "freeze") == 0 ||
           strcmp (argv[0], "thaw") == 0 ||
           strcmp (argv[0], "destroy") == 0)
    {
      if (argc != 2)
        BAD_COMMAND("usage: %s <client-id>/<window-id>", argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!meta_test_client_do (client, error, argv[0], window_id, NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "local_activate") == 0)
    {
      MetaWindow *window;

      if (argc != 2)
        BAD_COMMAND("usage: %s <client-id>/<window-id>", argv[0]);

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      window = meta_test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      meta_window_activate (window, 0);
    }
  else if (strcmp (argv[0], "wait") == 0)
    {
      if (argc != 1)
        BAD_COMMAND("usage: %s", argv[0]);

      if (!test_case_wait (test, error))
        return FALSE;
    }
  else if (strcmp (argv[0], "wait_reconfigure") == 0)
    {
      if (argc != 1)
        BAD_COMMAND("usage: %s", argv[0]);

      /*
       * Wait twice, so that we
       *  1) First wait for any requests to configure being made
       *  2) Then wait until the new configuration has been applied
       */

      if (!test_case_wait (test, error))
        return FALSE;
      if (!test_case_dispatch (test, error))
        return FALSE;
      if (!test_case_wait (test, error))
        return FALSE;
    }
  else if (strcmp (argv[0], "dispatch") == 0)
    {
      if (argc != 1)
        BAD_COMMAND("usage: %s", argv[0]);

      if (!test_case_dispatch (test, error))
        return FALSE;
    }
  else if (strcmp (argv[0], "sleep") == 0)
    {
      guint64 interval;

      if (argc != 2)
        BAD_COMMAND("usage: %s <milliseconds>", argv[0]);

      if (!g_ascii_string_to_unsigned (argv[1], 10, 0, G_MAXUINT32,
                                       &interval, error))
        return FALSE;

      if (!test_case_sleep (test, (guint32) interval, error))
        return FALSE;
    }
  else if (strcmp (argv[0], "set_strut") == 0)
    {
      if (argc != 6)
        BAD_COMMAND("usage: %s <x> <y> <width> <height> <side>", argv[0]);

      int x = atoi (argv[1]);
      int y = atoi (argv[2]);
      int width = atoi (argv[3]);
      int height = atoi (argv[4]);

      MetaSide side;
      if (strcmp (argv[5], "left") == 0)
        side = META_SIDE_LEFT;
      else if (strcmp (argv[5], "right") == 0)
        side = META_SIDE_RIGHT;
      else if (strcmp (argv[5], "top") == 0)
        side = META_SIDE_TOP;
      else if (strcmp (argv[5], "bottom") == 0)
        side = META_SIDE_BOTTOM;
      else
        return FALSE;

      MetaDisplay *display = meta_get_display ();
      MetaWorkspaceManager *workspace_manager =
        meta_display_get_workspace_manager (display);
      MetaRectangle rect = { x, y, width, height };
      MetaStrut strut = { rect, side };
      GSList *struts = g_slist_append (NULL, &strut);
      GList *workspaces =
        meta_workspace_manager_get_workspaces (workspace_manager);
      GList *l;

      for (l = workspaces; l; l = l->next)
        {
          MetaWorkspace *workspace = l->data;
          meta_workspace_set_builtin_struts (workspace, struts);
        }

      g_slist_free (struts);
    }
  else if (strcmp (argv[0], "clear_struts") == 0)
    {
      if (argc != 1)
        BAD_COMMAND("usage: %s", argv[0]);

      MetaDisplay *display = meta_get_display ();
      MetaWorkspaceManager *workspace_manager =
        meta_display_get_workspace_manager (display);
      GList *workspaces =
        meta_workspace_manager_get_workspaces (workspace_manager);
      GList *l;

      for (l = workspaces; l; l = l->next)
        {
          MetaWorkspace *workspace = l->data;
          meta_workspace_set_builtin_struts (workspace, NULL);
        }
    }
  else if (strcmp (argv[0], "assert_stacking") == 0)
    {
      if (!test_case_assert_stacking (test, argv + 1, argc - 1, error))
        return FALSE;

      if (!test_case_check_xserver_stacking (test, error))
        return FALSE;
    }
  else if (strcmp (argv[0], "assert_focused") == 0)
    {
      if (!test_case_assert_focused (test, argv[1], error))
        return FALSE;
    }
  else if (strcmp (argv[0], "assert_size") == 0)
    {
      MetaWindow *window;

      if (argc != 4)
        {
          BAD_COMMAND("usage: %s <client-id>/<window-id> <width> <height>",
                      argv[0]);
        }

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      window = meta_test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      if (meta_window_get_frame (window))
        {
          g_set_error (error,
                       META_TEST_CLIENT_ERROR,
                       META_TEST_CLIENT_ERROR_ASSERTION_FAILED,
                       "Can only assert size of CSD window");
          return FALSE;
        }

      int width = parse_window_size (window, argv[2]);
      int height = parse_window_size (window, argv[3]);
      g_autofree char *width_str = g_strdup_printf ("%d", width);
      g_autofree char *height_str = g_strdup_printf ("%d", height);

      if (!meta_test_client_do (client, error, argv[0],
                                window_id,
                                width_str,
                                height_str,
                                NULL))
        return FALSE;

      if (!test_case_assert_size (test, window,
                                  width, height,
                                  error))
        return FALSE;
    }
  else if (strcmp (argv[0], "assert_position") == 0)
    {
      MetaWindow *window;

      if (argc != 4)
        {
          BAD_COMMAND("usage: %s <client-id>/<window-id> <x> <y>",
                      argv[0]);
        }

      MetaTestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      window = meta_test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      MetaRectangle frame_rect;
      meta_window_get_frame_rect (window, &frame_rect);
      int x = atoi (argv[2]);
      int y = atoi (argv[3]);
      if (frame_rect.x != x || frame_rect.y != y)
        {
          g_set_error (error,
                       META_TEST_CLIENT_ERROR,
                       META_TEST_CLIENT_ERROR_ASSERTION_FAILED,
                       "Expected window position (%d, %d) doesn't match (%d, %d)",
                       x, y, frame_rect.x, frame_rect.y);
          return FALSE;
        }
    }
  else if (strcmp (argv[0], "stop_after_next") == 0 ||
           strcmp (argv[0], "continue") == 0)
    {
      if (argc != 2)
        BAD_COMMAND("usage: %s <client-id>", argv[0]);

      MetaTestClient *client = test_case_lookup_client (test, argv[1], error);
      if (!client)
        return FALSE;

      if (!meta_test_client_do (client, error, argv[0], NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "clipboard-set") == 0)
    {
      if (argc != 4)
        BAD_COMMAND("usage: %s <client-id> <mimetype> <text>", argv[0]);

      MetaTestClient *client = test_case_lookup_client (test, argv[1], error);
      if (!client)
        return FALSE;

      if (!meta_test_client_do (client, error, argv[0], argv[2], argv[3], NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "resize_monitor") == 0)
    {
      MetaBackend *backend = meta_context_get_backend (test->context);
      MetaMonitorManager *monitor_manager =
        meta_backend_get_monitor_manager (backend);
      MetaCrtcMode *crtc_mode;
      const MetaCrtcModeInfo *crtc_mode_info;

      if (argc != 4)
        BAD_COMMAND ("usage: %s <monitor-id> <width> <height>", argv[0]);

      if (strcmp (argv[1], "0") != 0 &&
          strcmp (argv[1], "primary") != 0)
        BAD_COMMAND ("Unknown monitor %s", argv[1]);

      crtc_mode = meta_virtual_monitor_get_crtc_mode (test->virtual_monitor);
      crtc_mode_info = meta_crtc_mode_get_info (crtc_mode);
      meta_virtual_monitor_set_mode (test->virtual_monitor,
                                     atoi (argv[2]),
                                     atoi (argv[3]),
                                     crtc_mode_info->refresh_rate);
      meta_monitor_manager_reload (monitor_manager);
    }
  else
    {
      BAD_COMMAND("Unknown command %s", argv[0]);
    }

  return TRUE;
}

static gboolean
test_case_destroy (TestCase *test,
                   GError  **error)
{
  /* Failures when cleaning up the test case aren't recoverable, since we'll
   * pollute the subsequent test cases, so we just return the error, and
   * skip the rest of the cleanup.
   */
  GHashTableIter iter;
  gpointer key, value;
  MetaDisplay *display;

  g_hash_table_iter_init (&iter, test->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!meta_test_client_do (value, error, "destroy_all", NULL))
        return FALSE;

    }

  if (!test_case_wait (test, error))
    return FALSE;

  if (!test_case_assert_stacking (test, NULL, 0, error))
    return FALSE;

  g_hash_table_iter_init (&iter, test->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    meta_test_client_destroy (value);

  g_clear_pointer (&test->waiter, meta_async_waiter_destroy);

  display = meta_get_display ();
  g_clear_signal_handler (&test->x11_display_opened_handler_id, display);
  if (display->x11_display)
    meta_x11_display_set_alarm_filter (display->x11_display, NULL, NULL);

  g_hash_table_destroy (test->clients);
  g_object_unref (test->virtual_monitor);
  g_free (test);

  return TRUE;
}

/**********************************************************************/

static gboolean
run_test (MetaContext *context,
          const char  *filename,
          int          index)
{
  TestCase *test = test_case_new (context);
  GError *error = NULL;

  GFile *file = g_file_new_for_path (filename);

  GDataInputStream *in = NULL;

  GFileInputStream *in_raw = g_file_read (file, NULL, &error);
  g_object_unref (file);
  if (in_raw == NULL)
    goto out;

  in = g_data_input_stream_new (G_INPUT_STREAM (in_raw));
  g_object_unref (in_raw);

  int line_no = 0;
  while (error == NULL)
    {
      char *line = g_data_input_stream_read_line_utf8 (in, NULL, NULL, &error);
      if (line == NULL)
        break;

      line_no++;

      int argc;
      char **argv = NULL;
      if (!g_shell_parse_argv (line, &argc, &argv, &error))
        {
          if (g_error_matches (error, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING))
            {
              g_clear_error (&error);
              goto next;
            }

          goto next;
        }

      test_case_do (test, argc, argv, &error);

    next:
      if (error)
        g_prefix_error (&error, "%d: ", line_no);

      g_free (line);
      g_strfreev (argv);
    }

  {
    GError *tmp_error = NULL;
    if (!g_input_stream_close (G_INPUT_STREAM (in), NULL, &tmp_error))
      {
        if (error != NULL)
          g_clear_error (&tmp_error);
        else
          g_propagate_error (&error, tmp_error);
      }
  }

 out:
  if (in != NULL)
    g_object_unref (in);

  GError *cleanup_error = NULL;
  test_case_destroy (test, &cleanup_error);

  const char *testspos = strstr (filename, "tests/");
  char *pretty_name;
  if (testspos)
    pretty_name = g_strdup (testspos + strlen("tests/"));
  else
    pretty_name = g_strdup (filename);

  if (error || cleanup_error)
    {
      g_print ("not ok %d %s\n", index, pretty_name);

      if (error)
        g_print ("   %s\n", error->message);

      if (cleanup_error)
        {
          g_print ("   Fatal Error During Cleanup\n");
          g_print ("   %s\n", cleanup_error->message);
          exit (1);
        }
    }
  else
    {
      g_print ("ok %d %s\n", index, pretty_name);
    }

  g_free (pretty_name);

  gboolean success = error == NULL;

  g_clear_error (&error);
  g_clear_error (&cleanup_error);

  return success;
}

typedef struct
{
  int n_tests;
  char **tests;
} RunTestsInfo;

static int
run_tests (MetaContext  *context,
           RunTestsInfo *info)
{
  int i;
  gboolean success = TRUE;

  g_print ("1..%d\n", info->n_tests);

  for (i = 0; i < info->n_tests; i++)
    {
      if (!run_test (context, info->tests[i], i + 1))
        success = FALSE;
    }


  return success ? 0 : 1;
}

/**********************************************************************/

static gboolean
find_metatests_in_directory (GFile     *directory,
                             GPtrArray *results,
                             GError   **error)
{
  GFileEnumerator *enumerator = g_file_enumerate_children (directory,
                                                           "standard::name,standard::type",
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, error);
  if (!enumerator)
    return FALSE;

  while (*error == NULL)
    {
      GFileInfo *info = g_file_enumerator_next_file (enumerator, NULL, error);
      if (info == NULL)
        break;

      GFile *child = g_file_enumerator_get_child (enumerator, info);
      switch (g_file_info_get_file_type (info))
        {
        case G_FILE_TYPE_REGULAR:
          {
            const char *name = g_file_info_get_name (info);
            if (g_str_has_suffix (name, ".metatest"))
              g_ptr_array_add (results, g_file_get_path (child));
            break;
          }
        case G_FILE_TYPE_DIRECTORY:
          find_metatests_in_directory (child, results, error);
          break;
        default:
          break;
        }

      g_object_unref (child);
      g_object_unref (info);
    }

  {
    GError *tmp_error = NULL;
    if (!g_file_enumerator_close (enumerator, NULL, &tmp_error))
      {
        if (*error != NULL)
          g_clear_error (&tmp_error);
        else
          g_propagate_error (error, tmp_error);
      }
  }

  g_object_unref (enumerator);
  return *error == NULL;
}

static gboolean all_tests = FALSE;

const GOptionEntry options[] = {
  {
    "all", 0, 0, G_OPTION_ARG_NONE,
    &all_tests,
    "Run all installed tests",
    NULL
  },
  { NULL }
};

int
main (int argc, char **argv)
{
  g_autoptr (MetaContext) context = NULL;
  GPtrArray *tests;
  RunTestsInfo info;

  context = meta_create_test_context (META_CONTEXT_TEST_TYPE_HEADLESS,
                                      META_CONTEXT_TEST_FLAG_TEST_CLIENT);

  meta_context_add_option_entries (context, options, NULL);

  g_assert (meta_context_configure (context, &argc, &argv, NULL));

  tests = g_ptr_array_new ();
  if (all_tests)
    {
      GFile *test_dir = g_file_new_for_path (MUTTER_PKGDATADIR "/tests");
      g_autoptr (GError) error = NULL;

      if (!find_metatests_in_directory (test_dir, tests, &error))
        {
          g_printerr ("Error enumerating tests: %s\n", error->message);
          return EXIT_FAILURE;
        }
    }
  else
    {
      int i;
      char *curdir = g_get_current_dir ();

      for (i = 1; i < argc; i++)
        {
          if (g_path_is_absolute (argv[i]))
            g_ptr_array_add (tests, g_strdup (argv[i]));
          else
            g_ptr_array_add (tests, g_build_filename (curdir, argv[i], NULL));
        }

      g_free (curdir);
    }

  info.tests = (char **) tests->pdata;
  info.n_tests = tests->len;
  g_signal_connect (context, "run-tests", G_CALLBACK (run_tests), &info);

  return meta_context_test_run_tests (META_CONTEXT_TEST (context),
                                      META_TEST_RUN_FLAG_NONE);
}
