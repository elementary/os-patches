/* ply-rich-text.c - Text with colors and styles
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

#include "ply-array.h"
#include "ply-rich-text.h"
#include "ply-logger.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <stdio.h>

struct _ply_rich_text_t
{
        ply_array_t         *characters;
        ply_rich_text_span_t span;
        size_t               reference_count;
};

ply_rich_text_t *
ply_rich_text_new (void)
{
        ply_rich_text_t *rich_text;

        rich_text = calloc (1, sizeof(ply_rich_text_t));
        rich_text->characters = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_POINTER);
        rich_text->reference_count = 1;

        return rich_text;
}

void
ply_rich_text_take_reference (ply_rich_text_t *rich_text)
{
        rich_text->reference_count++;
}

void
ply_rich_text_drop_reference (ply_rich_text_t *rich_text)
{
        rich_text->reference_count--;

        if (rich_text->reference_count == 0)
                ply_rich_text_free (rich_text);
}

void
ply_rich_text_free (ply_rich_text_t *rich_text)
{
        ply_rich_text_character_t **characters;

        if (rich_text == NULL)
                return;

        characters = ply_rich_text_get_characters (rich_text);
        characters = (ply_rich_text_character_t **) ply_array_get_pointer_elements (rich_text->characters);
        for (size_t i = 0; characters[i] != NULL; i++) {
                ply_rich_text_character_free (characters[i]);
        }

        ply_array_free (rich_text->characters);
        free (rich_text);
}

char *
ply_rich_text_get_string (ply_rich_text_t      *rich_text,
                          ply_rich_text_span_t *span)
{
        char *string = NULL;
        ply_buffer_t *buffer;
        ply_rich_text_character_t **characters;

        characters = ply_rich_text_get_characters (rich_text);

        buffer = ply_buffer_new ();
        for (size_t i = span->offset; characters[i] != NULL; i++) {
                if (span->range >= 0) {
                        if (i >= span->offset + span->range)
                                break;
                }

                ply_buffer_append_bytes (buffer,
                                         characters[i]->bytes,
                                         characters[i]->length);
        }

        string = ply_buffer_steal_bytes (buffer);

        ply_buffer_free (buffer);

        return string;
}

void
ply_rich_text_remove_characters (ply_rich_text_t *rich_text)
{
        ply_rich_text_character_t **characters;

        if (rich_text == NULL)
                return;

        characters = ply_rich_text_get_characters (rich_text);
        characters = (ply_rich_text_character_t **) ply_array_get_pointer_elements (rich_text->characters);
        for (size_t i = 0; characters[i] != NULL; i++) {
                ply_rich_text_character_free (characters[i]);
        }

        ply_array_free (rich_text->characters);
        rich_text->characters = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_POINTER);
}

size_t
ply_rich_text_get_length (ply_rich_text_t *rich_text)
{
        ply_rich_text_character_t **characters;
        size_t length = 0;

        characters = ply_rich_text_get_characters (rich_text);

        for (length = 0; characters[length] != NULL; length++) {
        }

        return length;
}

void
ply_rich_text_character_style_initialize (ply_rich_text_character_style_t *default_style)
{
        default_style->foreground_color = PLY_TERMINAL_COLOR_DEFAULT;
        default_style->background_color = PLY_TERMINAL_COLOR_DEFAULT;
        default_style->bold_enabled = false;
        default_style->dim_enabled = false;
        default_style->italic_enabled = false;
        default_style->underline_enabled = false;
        default_style->reverse_enabled = false;
}

ply_rich_text_character_t *
ply_rich_text_character_new (void)
{
        ply_rich_text_character_t *character = calloc (1, sizeof(ply_rich_text_character_t));
        character->bytes = NULL;
        character->length = 0;

        return character;
}

void
ply_rich_text_character_free (ply_rich_text_character_t *character)
{
        if (character == NULL)
                return;

        free (character->bytes);
        free (character);
}

ply_rich_text_character_t **
ply_rich_text_get_characters (ply_rich_text_t *rich_text)
{
        ply_rich_text_character_t **characters;

        characters = (ply_rich_text_character_t **) ply_array_get_pointer_elements (rich_text->characters);
        return characters;
}

void
ply_rich_text_remove_character (ply_rich_text_t *rich_text,
                                size_t           character_index)
{
        ply_rich_text_character_t **characters;

        characters = ply_rich_text_get_characters (rich_text);

        if (character_index < rich_text->span.offset)
                return;

        if (character_index >= rich_text->span.offset + rich_text->span.range)
                return;


        if (characters[character_index] == NULL)
                return;

        ply_rich_text_character_free (characters[character_index]);
        characters[character_index] = NULL;
}

void
ply_rich_text_move_character (ply_rich_text_t *rich_text,
                              size_t           old_index,
                              size_t           new_index)
{
        ply_rich_text_character_t **characters = ply_rich_text_get_characters (rich_text);

        if (old_index < rich_text->span.offset)
                return;

        if (new_index < rich_text->span.offset)
                return;

        if (old_index >= rich_text->span.offset + rich_text->span.range)
                return;

        if (new_index >= rich_text->span.offset + rich_text->span.range)
                return;


        characters[new_index] = characters[old_index];
        characters[old_index] = NULL;
}


void
ply_rich_text_set_character (ply_rich_text_t                *rich_text,
                             ply_rich_text_character_style_t style,
                             size_t                          character_index,
                             const char                     *character_string,
                             size_t                          length)
{
        ply_rich_text_character_t **characters;
        ply_rich_text_character_t *character;

        while (ply_array_get_size (rich_text->characters) <= character_index) {
                ply_array_add_pointer_element (rich_text->characters, NULL);
        }


        if (character_index < rich_text->span.offset)
                return;

        if (character_index >= rich_text->span.offset + rich_text->span.range)
                return;


        characters = ply_rich_text_get_characters (rich_text);

        if (characters[character_index] == NULL) {
                character = ply_rich_text_character_new ();
        } else {
                character = characters[character_index];
                if (character->bytes) {
                        free (character->bytes);
                        character->bytes = NULL;
                }
        }

        characters[character_index] = character;
        character->bytes = strdup (character_string);
        character->length = length;
        character->style = style;
}

void
ply_rich_text_iterator_initialize (ply_rich_text_iterator_t *iterator,
                                   ply_rich_text_t          *rich_text,
                                   ply_rich_text_span_t     *span)
{
        iterator->rich_text = rich_text;
        iterator->span = *span;
        iterator->current_offset = span->offset;
}

bool
ply_rich_text_iterator_next (ply_rich_text_iterator_t   *iterator,
                             ply_rich_text_character_t **character)
{
        ply_rich_text_t *rich_text = iterator->rich_text;
        ply_rich_text_span_t *span = &iterator->span;
        ply_rich_text_character_t **characters = ply_rich_text_get_characters (rich_text);

        if (iterator->current_offset >= span->offset + span->range) {
                return false;
        }

        if (characters[iterator->current_offset] == NULL) {
                return false;
        }

        *character = characters[iterator->current_offset];

        iterator->current_offset++;

        return true;
}

void
ply_rich_text_set_mutable_span (ply_rich_text_t      *rich_text,
                                ply_rich_text_span_t *span)
{
        rich_text->span = *span;
}

void
ply_rich_text_get_mutable_span (ply_rich_text_t      *rich_text,
                                ply_rich_text_span_t *span)
{
        *span = rich_text->span;
}
