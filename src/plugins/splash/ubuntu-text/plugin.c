/* ubuntu-text.c - boot splash plugin
 *
 * Copyright (C) 2010 Canonical Ltd.
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Scott James Remnant <scott@ubuntu.com>
 *             Adam Jackson <ajax@redhat.com>
 *             Ray Strode <rstrode@redhat.com>
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <values.h>
#include <wchar.h>

#include "ply-trigger.h"
#include "ply-boot-splash-plugin.h"
#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-key-file.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-text-display.h"
#include "ply-text-progress-bar.h"
#include "ply-utils.h"

#include <linux/kd.h>

#define CLEAR_LINE_SEQUENCE "\033[2K\r\n"
#define BACKSPACE "\b\033[0K"

typedef enum {
   PLY_BOOT_SPLASH_DISPLAY_NORMAL,
   PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY,
   PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY
} ply_boot_splash_display_type_t;

struct _ply_boot_splash_plugin
{
  ply_event_loop_t *loop;
  ply_boot_splash_mode_t mode;

  ply_list_t *views;

  ply_boot_splash_display_type_t state;

  char *message;

  uint32_t is_animating : 1;
  uint32_t black;
  uint32_t white;
  uint32_t brown;
  uint32_t blue;
  char *title;
};

typedef struct
{
  ply_boot_splash_plugin_t *plugin;
  ply_text_display_t *display;

} view_t;

static void hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                                ply_event_loop_t         *loop);

static view_t *
view_new (ply_boot_splash_plugin_t *plugin,
          ply_text_display_t       *display)
{
  view_t *view;

  view = calloc (1, sizeof (view_t));
  view->plugin = plugin;
  view->display = display;

  return view;
}

static void
view_free (view_t *view)
{
  free (view);
}

static void
view_show_message (view_t *view)
{
  ply_boot_splash_plugin_t *plugin;
  int display_width, display_height, y;
  ply_terminal_color_t color;
  char *message;

  plugin = view->plugin;

  display_width = ply_text_display_get_number_of_columns (view->display);
  display_height = ply_text_display_get_number_of_rows (view->display);

  if (!strncmp (plugin->message, "keys:", 5))
    {
      message = plugin->message + 5;
      color = PLY_TERMINAL_COLOR_WHITE;
      y = display_height - 4;
    }
  else
    {
      message = plugin->message;
      color = PLY_TERMINAL_COLOR_BLUE;
      y = display_height / 2 + 7;
    }

  ply_text_display_set_cursor_position (view->display, 0, y);
  ply_text_display_clear_line (view->display);
  ply_text_display_set_cursor_position (view->display,
                                        (display_width -
                                        strlen (message)) / 2,
                                        y);

  ply_text_display_set_foreground_color (view->display, color);
  ply_text_display_write (view->display, "%s", message);
}

static void
view_show_prompt (view_t     *view,
                  const char *prompt,
                  const char *entered_text)
{
  int display_width, display_height;

  display_width = ply_text_display_get_number_of_columns (view->display);
  display_height = ply_text_display_get_number_of_rows (view->display);

  ply_text_display_set_cursor_position (view->display, 0,
                                        display_height / 2 + 8);
  ply_text_display_clear_line (view->display);
  ply_text_display_set_cursor_position (view->display,
                                        display_width / 2 - (strlen (prompt)),
                                        display_height / 2 + 8);

  ply_text_display_write (view->display, "%s:%s", prompt, entered_text);

  ply_text_display_show_cursor (view->display);
}

static void
view_start_animation (view_t *view)
{
  ply_boot_splash_plugin_t *plugin;
  ply_terminal_t *terminal;

  assert (view != NULL);

  plugin = view->plugin;

  terminal = ply_text_display_get_terminal (view->display);

  ply_terminal_set_color_hex_value (terminal,
                                    PLY_TERMINAL_COLOR_BLACK,
                                    plugin->black);
  ply_terminal_set_color_hex_value (terminal,
                                    PLY_TERMINAL_COLOR_WHITE,
                                    plugin->white);
  ply_terminal_set_color_hex_value (terminal,
                                    PLY_TERMINAL_COLOR_BROWN,
                                    plugin->brown);
  ply_terminal_set_color_hex_value (terminal,
                                    PLY_TERMINAL_COLOR_BLUE,
                                    plugin->blue);

  ply_text_display_set_background_color (view->display,
                                         PLY_TERMINAL_COLOR_BLACK);
  ply_text_display_clear_screen (view->display);
  ply_text_display_hide_cursor (view->display);
}

static void
view_redraw (view_t *view)
{
  unsigned long screen_width, screen_height;

  screen_width = ply_text_display_get_number_of_columns (view->display);
  screen_height = ply_text_display_get_number_of_rows (view->display);

  ply_text_display_draw_area (view->display, 0, 0,
                              screen_width, screen_height);
}

static void
redraw_views (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_redraw (view);

      node = next_node;
    }
}

static void
view_hide (view_t *view)
{
  if (view->display != NULL)
    {
      ply_terminal_t *terminal;

      terminal = ply_text_display_get_terminal (view->display);

      ply_text_display_set_background_color (view->display, PLY_TERMINAL_COLOR_DEFAULT);
      ply_text_display_clear_screen (view->display);
      ply_text_display_show_cursor (view->display);

      ply_terminal_reset_colors (terminal);
    }
}

static void
hide_views (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_hide (view);

      node = next_node;
    }
}

static void
pause_views (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      ply_text_display_pause_updates (view->display);

      node = next_node;
    }
}

static void
unpause_views (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      ply_text_display_unpause_updates (view->display);

      node = next_node;
    }
}

static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
  char *option;

  ply_boot_splash_plugin_t *plugin;

  ply_trace ("creating plugin");

  plugin = calloc (1, sizeof (ply_boot_splash_plugin_t));
  plugin->message = NULL;

  plugin->views = ply_list_new ();

  /* Not a pretty API for setting defaults for your config file... */
  plugin->black = 0x2c001e;
  plugin->white = 0xffffff;
  plugin->brown = 0xff4012;
  plugin->blue  = 0x988592;

  option = ply_key_file_get_value (key_file, "ubuntu-text", "black");
  if (option)
    sscanf(option, "0x%x", &plugin->black);
  option = ply_key_file_get_value (key_file, "ubuntu-text", "white");
  if (option)
    sscanf(option, "0x%x", &plugin->white);
  option = ply_key_file_get_value (key_file, "ubuntu-text", "brown");
  if (option)
    sscanf(option, "0x%x", &plugin->brown);
  option = ply_key_file_get_value (key_file, "ubuntu-text", "blue");
  if (option)
    sscanf(option, "0x%x", &plugin->blue);

  plugin->title = ply_key_file_get_value (key_file, "ubuntu-text", "title");

  return plugin;
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
  plugin->loop = NULL;

  ply_trace ("detaching from event loop");
}

static void
free_views (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);

  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_free (view);
      ply_list_remove_node (plugin->views, node);

      node = next_node;
    }

  ply_list_free (plugin->views);
  plugin->views = NULL;
}

static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
  ply_trace ("destroying plugin");

  if (plugin == NULL)
    return;

  /* It doesn't ever make sense to keep this plugin on screen
   * after exit
   */
  hide_splash_screen (plugin, plugin->loop);

  free_views (plugin);
  if (plugin->message != NULL)
    free (plugin->message);

  free (plugin);
}

static void
show_message (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_show_message (view);

      node = next_node;
    }
}

static void
animate_frame (ply_boot_splash_plugin_t *plugin,
               int                       frame)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;
      int display_width, display_height;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      display_width = ply_text_display_get_number_of_columns (view->display);
      display_height = ply_text_display_get_number_of_rows (view->display);

      ply_text_display_set_cursor_position (view->display,
                                            (display_width - 12) / 2,
                                            display_height / 2);

      ply_text_display_set_background_color (view->display, PLY_TERMINAL_COLOR_BLACK);
      ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_WHITE);
      ply_text_display_write (view->display, "%s", plugin->title);

      ply_text_display_set_cursor_position (view->display,
                                            (display_width - 10) / 2,
                                            (display_height / 2) + 2);

      if ((frame < 1) || (frame > 4))
        ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_WHITE);
      else
        ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_BROWN);
      ply_text_display_write (view->display, "%s", ".  ");

      if ((frame < 2) || (frame > 5))
        ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_WHITE);
      else
        ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_BROWN);
      ply_text_display_write (view->display, "%s", ".  ");

      if ((frame < 3) || (frame > 6))
        ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_WHITE);
      else
        ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_BROWN);
      ply_text_display_write (view->display, "%s", ".  ");

      if (frame < 4)
        ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_WHITE);
      else
        ply_text_display_set_foreground_color (view->display, PLY_TERMINAL_COLOR_BROWN);
      ply_text_display_write (view->display, "%s", ".");

      node = next_node;
    }
}

static void
on_timeout (ply_boot_splash_plugin_t *plugin)
{
  static int frame = 0;

  frame += 1;
  frame %= 8;

  animate_frame (plugin, frame);

  ply_event_loop_watch_for_timeout (plugin->loop, 1.0,
                                    (ply_event_loop_timeout_handler_t)
                                    on_timeout, plugin);
}

static void
start_animation (ply_boot_splash_plugin_t *plugin)
{
  ply_list_node_t *node;

  assert (plugin != NULL);
  assert (plugin->loop != NULL);

  redraw_views (plugin);

  if (plugin->message != NULL)
    show_message (plugin);

  if (plugin->is_animating)
     return;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_start_animation (view);

      node = next_node;
    }

  plugin->is_animating = true;

  animate_frame (plugin, 0);
  ply_event_loop_watch_for_timeout (plugin->loop, 1.0,
                                    (ply_event_loop_timeout_handler_t)
                                    on_timeout, plugin);
}

static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
  assert (plugin != NULL);
  assert (plugin->loop != NULL);

  if (!plugin->is_animating)
     return;

  plugin->is_animating = false;

  ply_event_loop_stop_watching_for_timeout (plugin->loop,
                                            (ply_event_loop_timeout_handler_t)
                                            on_timeout, plugin);

  redraw_views (plugin);
}

static void
on_draw (view_t                   *view,
         ply_terminal_t           *terminal,
         int                       x,
         int                       y,
         int                       width,
         int                       height)
{
}

static void
add_text_display (ply_boot_splash_plugin_t *plugin,
                  ply_text_display_t       *display)
{
  view_t *view;
  ply_terminal_t *terminal;

  view = view_new (plugin, display);

  terminal = ply_text_display_get_terminal (view->display);
  if (ply_terminal_open (terminal))
    {
      ply_terminal_set_mode (terminal, PLY_TERMINAL_MODE_TEXT);
      ply_terminal_activate_vt (terminal);
    }

  ply_text_display_set_draw_handler (view->display,
                                     (ply_text_display_draw_handler_t)
                                     on_draw, view);

  ply_list_append_data (plugin->views, view);
}

static void
remove_text_display (ply_boot_splash_plugin_t *plugin,
                     ply_text_display_t       *display)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      view_t *view;
      ply_list_node_t *next_node;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      if (view->display == display)
        {
          ply_text_display_set_draw_handler (view->display,
                                             NULL, NULL);
          view_free (view);
          ply_list_remove_node (plugin->views, node);
          return;
        }

      node = next_node;
    }
}

static bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer,
                    ply_boot_splash_mode_t    mode)
{
  assert (plugin != NULL);

  plugin->loop = loop;
  plugin->mode = mode;
  ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                 detach_from_event_loop,
                                 plugin);

  ply_show_new_kernel_messages (false);
  start_animation (plugin);

  return true;
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
  assert (plugin != NULL);

  ply_trace ("status update");
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
  assert (plugin != NULL);

  ply_trace ("hiding splash screen");

  if (plugin->loop != NULL)
    {
      stop_animation (plugin);

      ply_event_loop_stop_watching_for_exit (plugin->loop,
                                             (ply_event_loop_exit_handler_t)
                                             detach_from_event_loop,
                                             plugin);
      detach_from_event_loop (plugin);
    }

  hide_views (plugin);
  ply_show_new_kernel_messages (true);
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
  pause_views (plugin);
  if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_NORMAL)
    {
      plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;
      start_animation (plugin);
      redraw_views (plugin);
    }
  unpause_views (plugin);
}

static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
  if (plugin->message != NULL)
    free (plugin->message);

  plugin->message = strdup (message);
  start_animation (plugin);
}

static void
show_password_prompt (ply_boot_splash_plugin_t *plugin,
                      const char               *prompt,
                      int                       bullets)
{
  ply_list_node_t *node;
  int i;
  char *entered_text;

  entered_text = calloc (bullets + 1, sizeof (char));

  for (i = 0; i < bullets; i++)
    entered_text[i] = '*';

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_show_prompt (view, prompt, entered_text);

      node = next_node;
    }
  free (entered_text);
}

static void
show_prompt (ply_boot_splash_plugin_t *plugin,
             const char               *prompt,
             const char               *text)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (plugin->views);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      view_t *view;

      view = ply_list_node_get_data (node);
      next_node = ply_list_get_next_node (plugin->views, node);

      view_show_prompt (view, prompt, text);

      node = next_node;
    }
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
  pause_views (plugin);
  if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
    stop_animation (plugin);

  plugin->state = PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY;

  if (!prompt)
    prompt = "Password";

  show_password_prompt (plugin, prompt, bullets);

  unpause_views (plugin);
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
  pause_views (plugin);
  if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
    stop_animation (plugin);

  plugin->state = PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY;

  if (!prompt)
    prompt = "Password";

  show_prompt (plugin, prompt, entry_text);

  unpause_views (plugin);
}


ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
  static ply_boot_splash_plugin_interface_t plugin_interface =
    {
      .create_plugin = create_plugin,
      .destroy_plugin = destroy_plugin,
      .add_text_display = add_text_display,
      .remove_text_display = remove_text_display,
      .show_splash_screen = show_splash_screen,
      .update_status = update_status,
      .hide_splash_screen = hide_splash_screen,
      .display_normal = display_normal,
      .display_message = display_message,
      .display_password = display_password,
      .display_question = display_question,
    };

  return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
