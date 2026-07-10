/* ply-label.c - label control
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (c) 2016 SUSE LINUX GmbH, Nuernberg, Germany.
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
 * Written by: Ray Strode <rstrode@redhat.com>
 * Written by: Fabian Vogt <fvogt@suse.com>
 */

#include <assert.h>
#include <endian.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <wchar.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "ply-logger.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-utils.h"

#include "ply-label-plugin.h"

/* This is used if fontconfig (fc-match) is not available, like in the initrd. */
#define FONT_FALLBACK "/usr/share/fonts/Plymouth.ttf"
#define MONOSPACE_FONT_FALLBACK "/usr/share/fonts/Plymouth-monospace.ttf"

/* This is a little sketchy... It relies on implementation details of the compiler
 * but it makes dealing with the fixed point math freetype uses much more pleasant,
 * imo, so I'm going to roll with it for now until it causes problems.
 */
typedef union
{
        struct
        {
#if BYTE_ORDER == LITTLE_ENDIAN
                uint32_t fractional_part : 6;
                uint32_t pixels : 26;
#else
                uint32_t pixels : 26;
                uint32_t fractional_part : 6;
#endif
        } as_pixels_unit;

        struct
        {
#if BYTE_ORDER == LITTLE_ENDIAN
                uint32_t fractional_part : 6;
                uint32_t points : 26;
#else
                uint32_t points : 26;
                uint32_t fractional_part : 6;
#endif
        } as_points_unit;

        uint32_t as_integer;
} ply_freetype_unit_t;

struct _ply_label_plugin_control
{
        ply_pixel_display_t  *display;
        ply_rectangle_t       area;

        ply_label_alignment_t alignment;
        long                  width;  /* For alignment (line wrapping?) */

        FT_Library            library;
        FT_Face               face;
        char                 *font;

        char                 *text;
        ply_rich_text_t      *rich_text;
        ply_rich_text_span_t  span;

        ply_array_t          *dimensions_of_lines;

        float                 red;
        float                 green;
        float                 blue;
        float                 alpha;

        uint32_t              scale_factor;

        uint32_t              is_hidden : 1;
        uint32_t              is_monospaced : 1;
        uint32_t              needs_size_update : 1;
};

typedef enum
{
        PLY_LOAD_GLYPH_ACTION_MEASURE,
        PLY_LOAD_GLYPH_ACTION_RENDER,
} ply_load_glyph_action_t;

ply_label_plugin_interface_t *ply_label_plugin_get_interface (void);
static void set_font_for_control (ply_label_plugin_control_t *label,
                                  const char                 *font);
static void load_glyphs (ply_label_plugin_control_t *label,
                         ply_load_glyph_action_t     action,
                         ply_pixel_buffer_t         *pixel_buffer);

static void size_control (ply_label_plugin_control_t *label,
                          bool                        force);

static const char *
find_default_font_path (void)
{
        FILE *fp;
        static char fc_match_out[PATH_MAX];

        fp = popen ("/usr/bin/fc-match -f %{file}", "r");
        if (!fp)
                return FONT_FALLBACK;

        fgets (fc_match_out, sizeof(fc_match_out), fp);

        pclose (fp);

        if (strcmp (fc_match_out, "") == 0)
                return FONT_FALLBACK;

        return fc_match_out;
}

static const char *
find_default_monospace_font_path (void)
{
        FILE *fp;
        static char fc_match_out[PATH_MAX];

        fp = popen ("/usr/bin/fc-match -f %{file} monospace", "r");
        if (!fp)
                return MONOSPACE_FONT_FALLBACK;

        fgets (fc_match_out, sizeof(fc_match_out), fp);

        pclose (fp);

        if (strcmp (fc_match_out, "") == 0)
                return FONT_FALLBACK;

        return fc_match_out;
}

static ply_label_plugin_control_t *
create_control (void)
{
        int error;
        ply_label_plugin_control_t *label;

        label = calloc (1, sizeof(ply_label_plugin_control_t));

        label->is_hidden = true;
        label->width = -1;
        label->text = NULL;
        label->scale_factor = 1;
        label->dimensions_of_lines = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_POINTER);

        error = FT_Init_FreeType (&label->library);
        if (error) {
                free (label);
                return NULL;
        }

        set_font_for_control (label, "Sans");

        return label;
}

static void
clear_dimensions_of_lines (ply_label_plugin_control_t *label)
{
        ply_rectangle_t **dimensions_of_lines;
        size_t i;

        if (label->dimensions_of_lines == NULL)
                return;

        dimensions_of_lines = (ply_rectangle_t **) ply_array_steal_pointer_elements (label->dimensions_of_lines);
        for (i = 0; dimensions_of_lines[i] != NULL; i++) {
                free (dimensions_of_lines[i]);
        }
}

static void
destroy_control (ply_label_plugin_control_t *label)
{
        if (label == NULL)
                return;

        clear_dimensions_of_lines (label);
        ply_array_free (label->dimensions_of_lines);

        free (label->text);
        free (label->font);
        FT_Done_Face (label->face);
        FT_Done_FreeType (label->library);

        free (label);
}

static long
get_width_of_control (ply_label_plugin_control_t *label)
{
        size_control (label, false);
        return label->area.width;
}

static long
get_height_of_control (ply_label_plugin_control_t *label)
{
        size_control (label, false);
        return label->area.height;
}

static FT_GlyphSlot
load_glyph (ply_label_plugin_control_t *label,
            ply_load_glyph_action_t     action,
            const char                 *input_text)
{
        FT_Error error;
        size_t character_size;
        wchar_t character;
        FT_Int32 load_flags = FT_LOAD_TARGET_LIGHT;

        if (label->face == NULL)
                return NULL;

        character_size = mbrtowc (&character, input_text, PLY_UTF8_CHARACTER_SIZE_MAX, NULL);

        if (character_size <= 0) {
                character = (wchar_t) *input_text;
                character_size = 1;
        }

        if (action == PLY_LOAD_GLYPH_ACTION_RENDER)
                load_flags |= FT_LOAD_RENDER;

        error = FT_Load_Char (label->face, (FT_ULong) character, load_flags);

        if (error)
                return NULL;

        return label->face->glyph;
}

static void
size_control (ply_label_plugin_control_t *label,
              bool                        force)
{
        if (!force && !label->needs_size_update)
                return;

        if (label->rich_text == NULL && label->text == NULL) {
                label->area.width = 0;
                label->area.height = 0;
                return;
        }

        load_glyphs (label, PLY_LOAD_GLYPH_ACTION_MEASURE, NULL);
        label->needs_size_update = false;
}

static void
trigger_redraw (ply_label_plugin_control_t *label,
                bool                        adjust_size)
{
        ply_rectangle_t dirty_area = label->area;

        if (adjust_size)
                size_control (label, true);

        if (label->is_hidden || label->display == NULL)
                return;

        ply_pixel_display_draw_area (label->display,
                                     dirty_area.x, dirty_area.y,
                                     dirty_area.width, dirty_area.height);
}

static void
draw_bitmap (ply_label_plugin_control_t *label,
             uint32_t                   *target,
             ply_rectangle_t             target_size,
             FT_Bitmap                  *source,
             FT_Int                      x_start,
             FT_Int                      y_start,
             uint8_t                     rs,
             uint8_t                     gs,
             uint8_t                     bs)
{
        FT_Int x, y, xs, ys;
        FT_Int x_end = MIN (x_start + source->width, target_size.width);
        FT_Int y_end = MIN (y_start + source->rows, target_size.height);

        if ((uint32_t) x_start >= target_size.width ||
            (uint32_t) y_start >= target_size.height)
                return;

        uint8_t rd, gd, bd, ad;

        for (y = y_start, ys = 0; y < y_end; ++y, ++ys) {
                for (x = x_start, xs = 0; x < x_end; ++x, ++xs) {
                        float alpha = label->alpha *
                                      (source->buffer[xs + source->pitch * ys] / 255.0f);
                        float invalpha = 1.0f - alpha;
                        uint32_t dest = target[x + target_size.width * y];

                        /* Separate colors */
                        rd = dest >> 16;
                        gd = dest >> 8;
                        bd = dest;

                        /* Alpha blending */
                        rd = invalpha * rd + alpha * rs;
                        gd = invalpha * gd + alpha * gs;
                        bd = invalpha * bd + alpha * bs;
                        /* Semi-correct: Disregard the target alpha */
                        ad = alpha * 255;

                        target[x + target_size.width * y] =
                                (ad << 24) | (rd << 16) | (gd << 8) | bd;
                }
        }
}

static void
look_up_rgb_color_from_terminal_color (ply_label_plugin_control_t *label,
                                       ply_terminal_color_t        color,
                                       uint8_t                    *red,
                                       uint8_t                    *green,
                                       uint8_t                    *blue)
{
        switch (color) {
        case PLY_TERMINAL_COLOR_BLACK:
                *red = 0x00;
                *green = 0x00;
                *blue = 0x00;
                break;
        /* Linux VT Color: 0xaa0000 */
        case PLY_TERMINAL_COLOR_RED:
                *red = 0xaa;
                *green = 0x00;
                *blue = 0x00;
                break;
        /* Linux VT Color: 0x00aa00 */
        case PLY_TERMINAL_COLOR_GREEN:
                *red = 0x00;
                *green = 0xaa;
                *blue = 0x00;
                break;
        /* Linux VT Color: 0xaa5500 */
        case PLY_TERMINAL_COLOR_BROWN:
                *red = 0xaa;
                *green = 0x55;
                *blue = 0x00;
                break;
        /* Linux VT Color: 0x0000aa */
        case PLY_TERMINAL_COLOR_BLUE:
                *red = 0x00;
                *green = 0x00;
                *blue = 0xaa;
                break;
        /* Linux VT Color: 0xaa00aa */
        case PLY_TERMINAL_COLOR_MAGENTA:
                *red = 0xaa;
                *green = 0x00;
                *blue = 0xaa;
                break;
        /* Linux VT Color: 0x00aaaa */
        case PLY_TERMINAL_COLOR_CYAN:
                *red = 0x00;
                *green = 0xaa;
                *blue = 0xaa;
                break;
        /* Linux VT Color: 0xaaaaaa */
        case PLY_TERMINAL_COLOR_WHITE:
                break;

        default:
                *red = 255 * label->red;
                *green = 255 * label->green;
                *blue = 255 * label->blue;
                break;
        }
}

static void
update_scale_factor_from_pixel_buffer (ply_label_plugin_control_t *label,
                                       ply_pixel_buffer_t         *pixel_buffer)
{
        uint32_t device_scale;

        device_scale = ply_pixel_buffer_get_device_scale (pixel_buffer);

        if (label->scale_factor == device_scale)
                return;

        label->scale_factor = device_scale;
        set_font_for_control (label, label->font?: "Sans");
        size_control (label, true);
}

static void
finish_measuring_line (ply_label_plugin_control_t *label,
                       ply_freetype_unit_t        *glyph_x,
                       ply_freetype_unit_t        *glyph_y,
                       ply_rectangle_t            *dimensions)
{

        ply_freetype_unit_t line_height;
        ply_rectangle_t *entry;

        if (label->face == NULL)
                return;

        line_height.as_integer = label->face->size->metrics.ascender + -label->face->size->metrics.descender;

        dimensions->x = label->area.x * label->scale_factor;

        dimensions->width = glyph_x->as_pixels_unit.pixels - dimensions->x;
        label->area.width = MAX (label->area.width, dimensions->width / label->scale_factor);

        dimensions->height = line_height.as_pixels_unit.pixels;
        label->area.height += dimensions->height / label->scale_factor;

        entry = calloc (1, sizeof(ply_rectangle_t));
        *entry = *dimensions;
        ply_array_add_pointer_element (label->dimensions_of_lines, entry);

        dimensions->y += dimensions->height;
}

static void
align_lines (ply_label_plugin_control_t *label)
{
        ply_rectangle_t **dimensions_of_lines;
        ply_rectangle_t *line_dimensions;
        long width;
        size_t i;

        if (label->alignment == PLY_LABEL_ALIGN_LEFT)
                return;

        if (label->dimensions_of_lines == NULL)
                return;

        width = label->width > 0? label->width : label->area.width;
        width *= label->scale_factor;

        dimensions_of_lines = (ply_rectangle_t **) ply_array_get_pointer_elements (label->dimensions_of_lines);

        for (i = 0; dimensions_of_lines[i] != NULL; i++) {
                line_dimensions = dimensions_of_lines[i];

                if (label->alignment == PLY_LABEL_ALIGN_CENTER)
                        line_dimensions->x += (width - line_dimensions->width) / 2;
                else if (label->alignment == PLY_LABEL_ALIGN_RIGHT)
                        line_dimensions->x += width - line_dimensions->width;
        }
}

static void
load_glyphs (ply_label_plugin_control_t *label,
             ply_load_glyph_action_t     action,
             ply_pixel_buffer_t         *pixel_buffer)
{
        FT_GlyphSlot glyph = NULL;
        ply_rich_text_iterator_t rich_text_iterator;
        ply_utf8_string_iterator_t utf8_string_iterator;
        uint32_t *target = NULL;
        ply_rectangle_t target_size;
        ply_freetype_unit_t glyph_x = { .as_pixels_unit = { .pixels = label->area.x * label->scale_factor } };
        ply_freetype_unit_t glyph_y = { .as_pixels_unit = { .pixels = label->area.y * label->scale_factor } };
        FT_Error error;
        FT_UInt previous_glyph_index = 0;
        bool is_first_character = true;
        ply_rectangle_t *line_dimensions = NULL;
        ply_rectangle_t **dimensions_of_lines = NULL;
        size_t line_number;

        if (label->rich_text == NULL &&
            label->text == NULL)
                return;

        if (label->rich_text != NULL) {
                ply_rich_text_iterator_initialize (&rich_text_iterator,
                                                   label->rich_text,
                                                   &label->span);
        } else {
                ply_utf8_string_iterator_initialize (&utf8_string_iterator,
                                                     label->text,
                                                     0,
                                                     ply_utf8_string_get_length (label->text, strlen (label->text)));
        }

        if (action == PLY_LOAD_GLYPH_ACTION_MEASURE) {
                clear_dimensions_of_lines (label);

                line_dimensions = alloca (sizeof(ply_rectangle_t));
                line_dimensions->x = label->area.x * label->scale_factor;
                line_dimensions->y = label->area.y * label->scale_factor;
                line_dimensions->width = 0;
                line_dimensions->height = 0;
                label->area.width = 0;
                label->area.height = 0;
        } else if (ply_array_get_size (label->dimensions_of_lines) == 0) {
                return;
        } else {
                dimensions_of_lines = (ply_rectangle_t **) ply_array_get_pointer_elements (label->dimensions_of_lines);
                line_number = 0;

                line_dimensions = dimensions_of_lines[line_number++];

                assert (line_dimensions != NULL);

                glyph_x.as_pixels_unit.pixels = line_dimensions->x;
        }

        if (action == PLY_LOAD_GLYPH_ACTION_RENDER) {
                target = ply_pixel_buffer_get_argb32_data (pixel_buffer);
                ply_pixel_buffer_get_size (pixel_buffer, &target_size);

                if (target_size.height == 0)
                        return;

                target_size.width *= label->scale_factor;
                target_size.height *= label->scale_factor;
        }

        /* Go through each line */
        do {
                bool should_stop;
                const char *current_character;
                uint8_t red, green, blue;

                FT_Int extra_advance = 0, positive_bearing_x = 0;
                ply_rich_text_character_t *rich_text_character;

                if (action == PLY_LOAD_GLYPH_ACTION_RENDER) {
                        red = 255 * label->red;
                        green = 255 * label->green;
                        blue = 255 * label->blue;
                }

                if (label->rich_text != NULL) {
                        should_stop = !ply_rich_text_iterator_next (&rich_text_iterator,
                                                                    &rich_text_character);
                        if (should_stop)
                                break;

                        current_character = rich_text_character->bytes;

                        if (action == PLY_LOAD_GLYPH_ACTION_RENDER) {
                                look_up_rgb_color_from_terminal_color (label,
                                                                       rich_text_character->style.foreground_color,
                                                                       &red,
                                                                       &green,
                                                                       &blue);
                        }
                } else {
                        size_t character_size;

                        should_stop = !ply_utf8_string_iterator_next (&utf8_string_iterator,
                                                                      &current_character,
                                                                      &character_size);
                        if (should_stop)
                                break;
                }

                glyph = load_glyph (label, action, current_character);

                if (glyph == NULL)
                        continue;

                if (is_first_character) {
                        /* Move pen to the first character's base line */
                        glyph_y.as_integer += label->face->size->metrics.ascender;
                }

                if (*current_character == '\n') {
                        if (action == PLY_LOAD_GLYPH_ACTION_MEASURE)
                                finish_measuring_line (label, &glyph_x, &glyph_y, line_dimensions);
                        else
                                line_dimensions = dimensions_of_lines[line_number++];

                        glyph_x.as_pixels_unit.pixels = line_dimensions->x;
                        glyph_y.as_pixels_unit.pixels = line_dimensions->y;

                        glyph_y.as_integer += label->face->size->metrics.ascender;
                        continue;
                }

                /* We consider negative left bearing an increment in size,
                 * as we draw full character boxes and don't "go back" in
                 * this plugin. Positive left bearing is treated as usual.
                 * For definitions see
                 * https://freetype.org/freetype2/docs/glyphs/glyphs-3.html
                 */
                if (glyph->bitmap_left < 0)
                        extra_advance = -glyph->bitmap_left;
                else
                        positive_bearing_x = glyph->bitmap_left;

                if (action == PLY_LOAD_GLYPH_ACTION_RENDER) {
                        draw_bitmap (label, target, target_size, &glyph->bitmap,
                                     glyph_x.as_pixels_unit.pixels + positive_bearing_x,
                                     glyph_y.as_pixels_unit.pixels - glyph->bitmap_top,
                                     red,
                                     green,
                                     blue);
                }

                glyph_x.as_integer += glyph->advance.x + extra_advance;

                if (!is_first_character) {
                        FT_Vector kerning_space;

                        error = FT_Get_Kerning (label->face, previous_glyph_index, glyph->glyph_index, FT_KERNING_DEFAULT, &kerning_space);

                        if (error == 0)
                                glyph_x.as_integer += kerning_space.x;

                        previous_glyph_index = glyph->glyph_index;
                } else {
                        is_first_character = false;
                }
        } while (true);

        if (action == PLY_LOAD_GLYPH_ACTION_MEASURE) {
                if (!is_first_character) {
                        char *text = NULL;

                        finish_measuring_line (label, &glyph_x, &glyph_y, line_dimensions);

                        if (ply_is_tracing ()) {
                                if (label->rich_text != NULL)
                                        text = ply_rich_text_get_string (label->rich_text, &label->span);

                                ply_trace ("Text '%s' has dimensions %ldx%ld", text?: label->text,
                                           line_dimensions->width,
                                           line_dimensions->height);

                                free (text);
                        }
                }

                align_lines (label);
        }
}

static void
draw_control (ply_label_plugin_control_t *label,
              ply_pixel_buffer_t         *pixel_buffer,
              long                        x,
              long                        y,
              unsigned long               width,
              unsigned long               height)
{
        if (label->is_hidden)
                return;

        if (label->rich_text == NULL &&
            label->text == NULL)
                return;

        update_scale_factor_from_pixel_buffer (label, pixel_buffer);

        /* Check for overlap.
         * TODO: Don't redraw everything if only a part should be drawn! */
        if (label->area.x > x + (long) width || label->area.y > y + (long) height
            || label->area.x + (long) label->area.width < x
            || label->area.y + (long) label->area.height < y)
                return;

        load_glyphs (label, PLY_LOAD_GLYPH_ACTION_RENDER, pixel_buffer);
}

static void
set_alignment_for_control (ply_label_plugin_control_t *label,
                           ply_label_alignment_t       alignment)
{
        if (label->alignment != alignment) {
                label->alignment = alignment;
                label->needs_size_update = true;
                trigger_redraw (label, true);
        }
}

static void
set_width_for_control (ply_label_plugin_control_t *label,
                       long                        width)
{
        if (label->width != width) {
                label->width = width;
                label->needs_size_update = true;
                trigger_redraw (label, true);
        }
}

static void
clear_text (ply_label_plugin_control_t *label)
{
        free (label->text);
        label->text = NULL;

        if (label->rich_text != NULL) {
                ply_rich_text_drop_reference (label->rich_text);
                label->rich_text = NULL;
                label->span.offset = 0;
                label->span.range = 0;
        }

        clear_dimensions_of_lines (label);
}

static void
set_text_for_control (ply_label_plugin_control_t *label,
                      const char                 *text)
{
        if (label->text != text) {
                clear_text (label);
                label->text = strdup (text);
                label->needs_size_update = true;
                trigger_redraw (label, true);
        }
}

static void
set_rich_text_for_control (ply_label_plugin_control_t *label,
                           ply_rich_text_t            *rich_text,
                           ply_rich_text_span_t       *span)
{
        clear_text (label);

        label->rich_text = rich_text;
        ply_rich_text_take_reference (rich_text);
        label->span = *span;

        label->needs_size_update = true;
        trigger_redraw (label, true);
}

static void
set_font_for_control (ply_label_plugin_control_t *label,
                      const char                 *font)
{
        /* Only able to set size and monospaced/nonmonospaced */

        int error = 0;
        char *size_str_after;
        const char *size_str, *font_path;
        char *new_font;
        ply_freetype_unit_t size = { .as_points_unit = { .points = 12 } };
        int dpi = 96;
        bool size_in_pixels = false;

        label->needs_size_update = true;

        new_font = strdup (font);
        free (label->font);
        label->font = new_font;

        if (strstr (font, "Mono") || strstr (font, "mono")) {
                if (!label->is_monospaced) {
                        FT_Done_Face (label->face);

                        font_path = find_default_monospace_font_path ();

                        if (font_path != NULL)
                                error = FT_New_Face (label->library, font_path, 0, &label->face);

                        label->is_monospaced = true;
                }
        } else {
                if (label->is_monospaced || label->face == NULL) {
                        FT_Done_Face (label->face);

                        font_path = find_default_font_path ();

                        if (font_path != NULL)
                                error = FT_New_Face (label->library, font_path, 0, &label->face);

                        label->is_monospaced = false;
                }
        }
        if (error != 0) {
                FT_Done_Face (label->face);
                label->face = NULL;

                ply_trace ("Could not load font, error %d", error);
                return;
        }

        /* Format is "Family 1[,Family 2[,..]] [25[px]]" .
         * [] means optional. */
        size_str = strrchr (font, ' ');

        if (size_str) {
                unsigned long parsed_size;
                parsed_size = strtoul (size_str, &size_str_after, 10);

                if (size_str_after != size_str) {
                        if (strcmp (size_str_after, "px") == 0) {
                                size_in_pixels = true;
                                size.as_pixels_unit.pixels = parsed_size;
                        } else {
                                size.as_points_unit.points = parsed_size;
                        }
                }
        }

        if (size_in_pixels)
                FT_Set_Pixel_Sizes (label->face, 0, size.as_pixels_unit.pixels * label->scale_factor);
        else
                FT_Set_Char_Size (label->face, size.as_integer, 0, dpi * label->scale_factor, 0);

        /* Ignore errors, to keep the current size. */
        trigger_redraw (label, true);
}

static void
set_color_for_control (ply_label_plugin_control_t *label,
                       float                       red,
                       float                       green,
                       float                       blue,
                       float                       alpha)
{
        label->red = red;
        label->green = green;
        label->blue = blue;
        label->alpha = alpha;

        trigger_redraw (label, false);
}

static bool
show_control (ply_label_plugin_control_t *label,
              ply_pixel_display_t        *display,
              long                        x,
              long                        y)
{
        ply_rectangle_t dirty_area;
        bool force_resize = false;

        dirty_area = label->area;
        label->display = display;
        if (label->area.x != x || label->area.y != y) {
                label->area.x = x;
                label->area.y = y;
                force_resize = true;
        }

        label->is_hidden = false;

        size_control (label, force_resize);

        if (!label->is_hidden && label->display != NULL)
                ply_pixel_display_draw_area (label->display,
                                             dirty_area.x, dirty_area.y,
                                             dirty_area.width, dirty_area.height);

        label->is_hidden = false;

        return true;
}

static void
hide_control (ply_label_plugin_control_t *label)
{
        label->is_hidden = true;
        if (label->display != NULL)
                ply_pixel_display_draw_area (label->display,
                                             label->area.x, label->area.y,
                                             label->area.width, label->area.height);

        label->display = NULL;
}

static bool
is_control_hidden (ply_label_plugin_control_t *label)
{
        return label->is_hidden;
}

ply_label_plugin_interface_t *
ply_label_plugin_get_interface (void)
{
        static ply_label_plugin_interface_t plugin_interface =
        {
                .create_control            = create_control,
                .destroy_control           = destroy_control,
                .show_control              = show_control,
                .hide_control              = hide_control,
                .draw_control              = draw_control,
                .is_control_hidden         = is_control_hidden,
                .set_text_for_control      = set_text_for_control,
                .set_rich_text_for_control = set_rich_text_for_control,
                .set_alignment_for_control = set_alignment_for_control,
                .set_width_for_control     = set_width_for_control,
                .set_font_for_control      = set_font_for_control,
                .set_color_for_control     = set_color_for_control,
                .get_width_of_control      = get_width_of_control,
                .get_height_of_control     = get_height_of_control
        };

        return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s, (0: */
