/* ply-terminal-emulator.h - Minimal Terminal Emulator
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
#ifndef PLY_TERMINAL_EMULATOR_H
#define PLY_TERMINAL_EMULATOR_H

#include "ply-boot-splash.h"
#include "ply-rich-text.h"
#include <sys/syslog.h>

/* Terminal attribute values are determined from the "ECMA-48 Select Graphic Rendition" section from
 * https://man7.org/linux/man-pages/man4/console_codes.4.html
 */
#define PLY_TERMINAL_ATTRIBUTE_FOREGROUND_COLOR_OFFSET 30
#define PLY_TERMINAL_ATTRIBUTE_BACKGROUND_COLOR_OFFSET 40
#define PLY_TERMINAL_ATTRIBUTE_FOREGROUND_BRIGHT_OFFSET 90
#define PLY_TERMINAL_ATTRIBUTE_BACKGROUND_BRIGHT_OFFSET 100

typedef struct _ply_terminal_emulator ply_terminal_emulator_t;

typedef enum
{
        PLY_TERMINAL_ATTRIBUTE_RESET,
        PLY_TERMINAL_ATTRIBUTE_BOLD,
        PLY_TERMINAL_ATTRIBUTE_DIM,
        PLY_TERMINAL_ATTRIBUTE_ITALIC,
        PLY_TERMINAL_ATTRIBUTE_UNDERLINE,
        PLY_TERMINAL_ATTRIBUTE_REVERSE    = 7,
        PLY_TERMINAL_ATTRIBUTE_NO_BOLD    = 21,
        PLY_TERMINAL_ATTRIBUTE_NO_DIM,
        PLY_TERMINAL_ATTRIBUTE_NO_ITALIC,
        PLY_TERMINAL_ATTRIBUTE_NO_UNDERLINE,
        PLY_TERMINAL_ATTRIBUTE_NO_REVERSE = 27
} ply_terminal_style_attributes_t;

typedef void (*ply_terminal_emulator_output_handler_t) (void       *user_data,
                                                        const char *output);

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_terminal_emulator_t *ply_terminal_emulator_new (size_t number_of_rows,
                                                    size_t number_of_columns);
void ply_terminal_emulator_free (ply_terminal_emulator_t *terminal_emulator);
void ply_terminal_emulator_parse_lines (ply_terminal_emulator_t *terminal_emulator,
                                        const char              *text,
                                        size_t                   size);
ply_rich_text_t *ply_terminal_emulator_get_nth_line (ply_terminal_emulator_t *terminal_emulator,
                                                     int                      line_number);
int ply_terminal_emulator_get_line_count (ply_terminal_emulator_t *terminal_emulator);
void ply_terminal_emulator_convert_boot_buffer (ply_terminal_emulator_t *terminal_emulator,
                                                ply_buffer_t            *boot_buffer);
void ply_terminal_emulator_watch_for_output (ply_terminal_emulator_t               *terminal_emulator,
                                             ply_terminal_emulator_output_handler_t handler,
                                             void                                  *user_data);

#endif //PLY_HIDE_FUNCTION_DECLARATIONS
#endif //PLY_TERMINAL_EMULATOR_H
