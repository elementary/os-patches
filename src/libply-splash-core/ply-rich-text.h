/* ply-rich-text.h - Text with colors and styles
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
#ifndef PLY_RICH_TEXT_H
#define PLY_RICH_TEXT_H

#include "ply-array.h"
#include "ply-terminal.h"
#include <stddef.h>

typedef struct _ply_rich_text_t ply_rich_text_t;

typedef struct
{
        ply_terminal_color_t foreground_color;
        ply_terminal_color_t background_color;
        uint32_t             bold_enabled : 1;
        uint32_t             dim_enabled : 1;
        uint32_t             italic_enabled : 1;
        uint32_t             underline_enabled : 1;
        uint32_t             reverse_enabled : 1;
} ply_rich_text_character_style_t;

typedef struct
{
        char                           *bytes;
        size_t                          length;

        ply_rich_text_character_style_t style;
} ply_rich_text_character_t;

typedef struct
{
        ssize_t offset;
        ssize_t range;
} ply_rich_text_span_t;

typedef struct
{
        ply_rich_text_t     *rich_text;
        ply_rich_text_span_t span;
        ssize_t              current_offset;
} ply_rich_text_iterator_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_rich_text_t *ply_rich_text_new (void);
void ply_rich_text_take_reference (ply_rich_text_t *rich_text);
void ply_rich_text_drop_reference (ply_rich_text_t *rich_text);
char *ply_rich_text_get_string (ply_rich_text_t      *rich_text,
                                ply_rich_text_span_t *span);
size_t ply_rich_text_get_length (ply_rich_text_t *rich_text);
void ply_rich_text_set_character (ply_rich_text_t                *rich_text,
                                  ply_rich_text_character_style_t style,
                                  size_t                          index,
                                  const char                     *bytes,
                                  size_t                          length);
void ply_rich_text_move_character (ply_rich_text_t *rich_text,
                                   size_t           old_index,
                                   size_t           new_index);
void ply_rich_text_remove_character (ply_rich_text_t *rich_text,
                                     size_t           character_index);
void ply_rich_text_remove_characters (ply_rich_text_t *rich_text);
ply_rich_text_character_t **ply_rich_text_get_characters (ply_rich_text_t *rich_text);
void ply_rich_text_free (ply_rich_text_t *rich_text);

void ply_rich_text_character_style_initialize (ply_rich_text_character_style_t *default_style);
ply_rich_text_character_t *ply_rich_text_character_new (void);
void ply_rich_text_character_free (ply_rich_text_character_t *character);

void ply_rich_text_iterator_initialize (ply_rich_text_iterator_t *iterator,
                                        ply_rich_text_t          *rich_text,
                                        ply_rich_text_span_t     *span);
bool ply_rich_text_iterator_next (ply_rich_text_iterator_t   *iterator,
                                  ply_rich_text_character_t **character);

void ply_rich_text_set_mutable_span (ply_rich_text_t      *rich_text,
                                     ply_rich_text_span_t *span);
void ply_rich_text_get_mutable_span (ply_rich_text_t      *rich_text,
                                     ply_rich_text_span_t *span);

#endif //PLY_HIDE_FUNCTION_DECLARATIONS
#endif //PLY_RICH_TEXT_H
