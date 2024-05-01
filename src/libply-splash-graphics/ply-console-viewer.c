/* ply-console-view.c - console message viewer
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
 */

#include <stdlib.h>
#include <assert.h>

#include "ply-label.h"
#include "ply-logger.h"
#include "ply-array.h"
#include "ply-pixel-display.h"
#include "ply-image.h"
#include "ply-kmsg-reader.h"
#include "ply-console-viewer.h"
#include "ply-rich-text.h"

#define TERMINAL_OUTPUT_UPDATE_INTERVAL (1.0 / 60)

struct _ply_console_viewer
{
        ply_event_loop_t        *loop;

        ply_terminal_emulator_t *terminal_emulator;

        ply_pixel_display_t     *display;
        ply_rectangle_t          area;

        ply_list_t              *message_labels;

        uint32_t                 is_hidden : 1;
        uint32_t                 output_queued : 1;
        uint32_t                 needs_redraw : 1;

        char                    *font;
        long                     font_height;
        long                     font_width;
        int                      line_max_chars;

        uint32_t                 text_color;
};

static void update_console_messages (ply_console_viewer_t *console_viewer);
static void on_terminal_emulator_output (ply_console_viewer_t *console_viewer);

bool
ply_console_viewer_preferred (void)
{
        static enum { PLY_CONSOLE_VIEWER_PREFERENCE_UNKNOWN = -1,
                      PLY_CONSOLE_VIEWER_PREFERENCE_NO_VIEWER,
                      PLY_CONSOLE_VIEWER_PREFERENCE_VIEWER } preference = PLY_CONSOLE_VIEWER_PREFERENCE_UNKNOWN;
        ply_label_t *label = NULL;

        if (preference != PLY_CONSOLE_VIEWER_PREFERENCE_UNKNOWN)
                goto out;

        if (ply_kernel_command_line_has_argument ("plymouth.prefer-fbcon")) {
                ply_trace ("Not using console viewer because plymouth.prefer-fbcon is on kernel command line");
                preference = PLY_CONSOLE_VIEWER_PREFERENCE_NO_VIEWER;
                goto out;
        }

        label = ply_label_new ();
        ply_label_set_text (label, " ");

        if (ply_label_get_width (label) <= 1 || ply_label_get_height (label) <= 1) {
                ply_trace ("Not using console viewer because text renderering isn't working");
                preference = PLY_CONSOLE_VIEWER_PREFERENCE_NO_VIEWER;
                goto out;
        } else {
                ply_trace ("Using console viewer instead of kernel framebuffer console");
                preference = PLY_CONSOLE_VIEWER_PREFERENCE_VIEWER;
                goto out;
        }

out:
        ply_label_free (label);
        return (bool) preference;
}

ply_console_viewer_t *
ply_console_viewer_new (ply_pixel_display_t *display,
                        const char          *font)
{
        ply_console_viewer_t *console_viewer;
        ply_label_t *console_message_label, *measure_label;
        size_t line_count;

        console_viewer = calloc (1, sizeof(struct _ply_console_viewer));

        console_viewer->message_labels = ply_list_new ();
        console_viewer->is_hidden = true;

        console_viewer->font = strdup (font);

        measure_label = ply_label_new ();
        ply_label_set_text (measure_label, " ");
        ply_label_set_font (measure_label, console_viewer->font);

        console_viewer->text_color = PLY_CONSOLE_VIEWER_LOG_TEXT_COLOR;

        console_viewer->font_height = ply_label_get_height (measure_label);
        console_viewer->font_width = ply_label_get_width (measure_label);
        /* Allow the label to be the size of how many characters can fit in the width of the screeen, minus one for larger fonts that have some size overhead */
        console_viewer->line_max_chars = ply_pixel_display_get_width (display) / console_viewer->font_width - 1;
        line_count = ply_pixel_display_get_height (display) / console_viewer->font_height;

        /* Display at least one line */
        if (line_count == 0)
                line_count = 1;

        ply_label_free (measure_label);

        for (size_t label_index = 0; label_index < line_count; label_index++) {
                console_message_label = ply_label_new ();
                ply_label_set_font (console_message_label, console_viewer->font);
                ply_list_append_data (console_viewer->message_labels, console_message_label);
        }

        console_viewer->terminal_emulator = ply_terminal_emulator_new (line_count, console_viewer->line_max_chars);

        ply_terminal_emulator_watch_for_output (console_viewer->terminal_emulator,
                                                (ply_terminal_emulator_output_handler_t)
                                                on_terminal_emulator_output,
                                                console_viewer);

        return console_viewer;
}

void
ply_console_viewer_free (ply_console_viewer_t *console_viewer)
{
        ply_list_node_t *node;
        ply_label_t *console_message_label;

        if (console_viewer == NULL)
                return;

        ply_list_foreach (console_viewer->message_labels, node) {
                console_message_label = ply_list_node_get_data (node);
                ply_label_free (console_message_label);
        }
        ply_list_free (console_viewer->message_labels);
        ply_terminal_emulator_free (console_viewer->terminal_emulator);

        free (console_viewer->font);
        free (console_viewer);
}

static void
update_console_messages (ply_console_viewer_t *console_viewer)
{
        ply_list_node_t *node;
        ply_label_t *console_message_label;
        size_t message_number, number_of_messages, visible_line_count;
        ssize_t characters_left;
        ply_rich_text_span_t span;

        console_viewer->output_queued = false;

        if (console_viewer->terminal_emulator == NULL)
                return;

        if (console_viewer->display == NULL)
                return;

        visible_line_count = ply_list_get_length (console_viewer->message_labels);

        number_of_messages = ply_terminal_emulator_get_line_count (console_viewer->terminal_emulator);

        message_number = ply_terminal_emulator_get_line_count (console_viewer->terminal_emulator);
        if (message_number < visible_line_count) {
                message_number = 0;
        } else {
                message_number = number_of_messages - visible_line_count;
        }

        if (number_of_messages < 0)
                return;

        ply_pixel_display_pause_updates (console_viewer->display);
        node = ply_list_get_first_node (console_viewer->message_labels);
        while (node != NULL) {
                ply_rich_text_t *line = NULL;

                characters_left = 0;
                if (message_number >= 0) {
                        line = ply_terminal_emulator_get_nth_line (console_viewer->terminal_emulator, message_number);

                        if (line != NULL) {
                                ply_rich_text_take_reference (line);
                                characters_left = ply_rich_text_get_length (line);
                        }
                }

                span.offset = characters_left;
                while (characters_left >= 0) {
                        console_message_label = ply_list_node_get_data (node);

                        span.range = span.offset % console_viewer->line_max_chars;
                        if (span.range == 0)
                                span.range = console_viewer->line_max_chars;

                        characters_left = span.offset - span.range - 1;

                        if (span.offset - span.range >= 0)
                                span.offset -= span.range;
                        else
                                span.offset = 0;

                        if (line != NULL) {
                                ply_label_set_rich_text (console_message_label, line, &span);
                        } else {
                                ply_label_set_text (console_message_label, "");
                        }

                        node = ply_list_get_next_node (console_viewer->message_labels, node);
                        if (node == NULL)
                                break;
                }

                if (line != NULL)
                        ply_rich_text_drop_reference (line);

                message_number++;

                if (message_number >= number_of_messages)
                        break;
        }
        console_viewer->needs_redraw = true;
        ply_pixel_display_draw_area (console_viewer->display, 0, 0,
                                     ply_pixel_display_get_width (console_viewer->display),
                                     ply_pixel_display_get_height (console_viewer->display));
        ply_pixel_display_unpause_updates (console_viewer->display);
}

void
ply_console_viewer_show (ply_console_viewer_t *console_viewer,
                         ply_pixel_display_t  *display)
{
        uint32_t label_color;
        size_t label_index;
        ply_list_node_t *node;

        assert (console_viewer != NULL);

        console_viewer->display = display;
        console_viewer->is_hidden = false;

        label_color = console_viewer->text_color;

        label_index = 0;
        ply_list_foreach (console_viewer->message_labels, node) {
                ply_label_t *console_message_label;
                console_message_label = ply_list_node_get_data (node);
                ply_label_show (console_message_label, console_viewer->display,
                                console_viewer->font_width / 2,
                                console_viewer->font_height * label_index);
                ply_label_set_hex_color (console_message_label, label_color);
                label_index++;
        }

        update_console_messages (console_viewer);
}

void
ply_console_viewer_draw_area (ply_console_viewer_t *console_viewer,
                              ply_pixel_buffer_t   *buffer,
                              long                  x,
                              long                  y,
                              unsigned long         width,
                              unsigned long         height)
{
        ply_list_node_t *node;
        size_t label_index;
        ply_label_t *console_message_label;

        if (!console_viewer->needs_redraw)
                return;

        if (console_viewer->is_hidden)
                return;

        label_index = 0;
        ply_list_foreach (console_viewer->message_labels, node) {
                console_message_label = ply_list_node_get_data (node);
                ply_label_draw_area (console_message_label, buffer,
                                     MAX (x, console_viewer->font_width / 2),
                                     MAX (y, console_viewer->font_height * label_index),
                                     MIN (ply_label_get_width (console_message_label), width),
                                     MIN (height, console_viewer->font_height));
                label_index++;
        }

        console_viewer->needs_redraw = false;
}

void
ply_console_viewer_hide (ply_console_viewer_t *console_viewer)
{
        ply_list_node_t *node;
        ply_label_t *console_message_label;

        if (console_viewer->is_hidden)
                return;

        console_viewer->is_hidden = true;

        ply_list_foreach (console_viewer->message_labels, node) {
                console_message_label = ply_list_node_get_data (node);
                ply_label_hide (console_message_label);
        }

        console_viewer->display = NULL;
}

static void
on_terminal_emulator_output (ply_console_viewer_t *console_viewer)
{
        if (console_viewer->output_queued)
                return;

        if (console_viewer->is_hidden)
                return;

        ply_event_loop_watch_for_timeout (ply_event_loop_get_default (),
                                          TERMINAL_OUTPUT_UPDATE_INTERVAL,
                                          (ply_event_loop_timeout_handler_t)
                                          update_console_messages, console_viewer);
        console_viewer->output_queued = true;
}

void
ply_console_viewer_set_text_color (ply_console_viewer_t *console_viewer,
                                   uint32_t              hex_color)
{
        console_viewer->text_color = hex_color;
}

void
ply_console_viewer_convert_boot_buffer (ply_console_viewer_t *console_viewer,
                                        ply_buffer_t         *boot_buffer)
{
        ply_terminal_emulator_convert_boot_buffer (console_viewer->terminal_emulator, boot_buffer);
}

void
ply_console_viewer_write (ply_console_viewer_t *console_viewer,
                          const char           *text,
                          size_t                size)
{
        ply_terminal_emulator_parse_lines (console_viewer->terminal_emulator, text, size);
}

void
ply_console_viewer_print (ply_console_viewer_t *console_viewer,
                          const char           *format,
                          ...)
{
        va_list arguments;
        char *buffer = NULL;
        int length;

        if (format == NULL)
                return;

        va_start (arguments, format);
        length = vsnprintf (NULL, 0, format, arguments);
        if (length > 0)
                buffer = calloc (1, length + 1);
        va_end (arguments);

        if (buffer == NULL)
                return;

        va_start (arguments, format);
        vsnprintf (buffer, length + 1, format, arguments);
        ply_console_viewer_write (console_viewer, buffer, length);
        va_end (arguments);

        free (buffer);
}

void
ply_console_viewer_clear_line (ply_console_viewer_t *console_viewer)
{
        ply_console_viewer_print (console_viewer, "\033[2K\033[0G");
}
