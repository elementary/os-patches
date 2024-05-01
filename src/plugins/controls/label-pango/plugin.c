/* ply-label.c - label control
 *
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
 * Written by: Ray Strode <rstrode@redhat.com>
 */

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
#include <values.h>
#include <unistd.h>
#include <wchar.h>

#include <glib.h>
#include <cairo.h>
#include <pango/pangocairo.h>

#include "ply-logger.h"
#include "ply-terminal.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-utils.h"

#include "ply-label-plugin.h"

struct _ply_label_plugin_control
{
        ply_event_loop_t    *loop;
        ply_pixel_display_t *display;
        ply_rectangle_t      area;

        char                *text;
        char                *font;

        PangoAlignment       alignment;
        PangoAttrList       *attribute_list;
        long                 width;
        float                red;
        float                green;
        float                blue;
        float                alpha;

        uint32_t             is_hidden : 1;
        uint32_t             needs_size_update : 1;
};

ply_label_plugin_interface_t *ply_label_plugin_get_interface (void);

static ply_label_plugin_control_t *
create_control (void)
{
        ply_label_plugin_control_t *label;

        label = calloc (1, sizeof(ply_label_plugin_control_t));

        label->is_hidden = true;
        label->alignment = PANGO_ALIGN_LEFT;
        label->width = -1;
        label->attribute_list = pango_attr_list_new ();

        return label;
}

static void
destroy_control (ply_label_plugin_control_t *label)
{
        GSList *attributes, *attribute;

        if (label == NULL)
                return;

        if (label->attribute_list) {
                attributes = pango_attr_list_get_attributes (label->attribute_list);
                for (attribute = attributes; attribute; attribute = attribute->next) {
                        pango_attribute_destroy (attribute->data);
                }
                pango_attr_list_unref (label->attribute_list);
                g_slist_free (attributes);
        }
        free (label);
}

static cairo_t *
get_cairo_context_for_pixel_buffer (ply_label_plugin_control_t *label,
                                    ply_pixel_buffer_t         *pixel_buffer,
                                    long                       *center_x,
                                    long                       *center_y)
{
        cairo_surface_t *cairo_surface;
        cairo_t *cairo_context;
        unsigned char *data;
        unsigned long width, height;
        uint32_t scale;
        ply_pixel_buffer_rotation_t rotation;

        data = (unsigned char *) ply_pixel_buffer_get_argb32_data (pixel_buffer);
        width = ply_pixel_buffer_get_width (pixel_buffer);
        height = ply_pixel_buffer_get_height (pixel_buffer);
        scale = ply_pixel_buffer_get_device_scale (pixel_buffer);
        rotation = ply_pixel_buffer_get_device_rotation (pixel_buffer);

        *center_x = width / 2;
        *center_y = height / 2;

        if (rotation == PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE ||
            rotation == PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE) {
                unsigned long tmp = width;
                width = height;
                height = tmp;
        }

        cairo_surface = cairo_image_surface_create_for_data (data,
                                                             CAIRO_FORMAT_ARGB32,
                                                             width * scale,
                                                             height * scale,
                                                             width * scale * 4);
        cairo_surface_set_device_scale (cairo_surface, scale, scale);
        cairo_context = cairo_create (cairo_surface);
        cairo_surface_destroy (cairo_surface);

        /* Rotate around the center */
        cairo_translate (cairo_context, width / 2, height / 2);
        switch (rotation) {
        case PLY_PIXEL_BUFFER_ROTATE_UPRIGHT:
                break;
        case PLY_PIXEL_BUFFER_ROTATE_UPSIDE_DOWN:
                cairo_rotate (cairo_context, M_PI);
                break;
        case PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE:
                cairo_rotate (cairo_context, 0.5 * M_PI);
                break;
        case PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE:
                cairo_rotate (cairo_context, -0.5 * M_PI);
                break;
        }

        return cairo_context;
}

static cairo_t *
get_cairo_context_for_sizing (ply_label_plugin_control_t *label)
{
        cairo_surface_t *cairo_surface;
        cairo_t *cairo_context;

        cairo_surface = cairo_image_surface_create_for_data (NULL, CAIRO_FORMAT_ARGB32, 0, 0, 0);
        cairo_context = cairo_create (cairo_surface);
        cairo_surface_destroy (cairo_surface);

        return cairo_context;
}

void
remove_hexboxes_from_pango_layout (PangoLayout *pango_layout)
{
        PangoLayoutIter *iter;
        bool hexbox_removed = false;

        iter = pango_layout_get_iter (pango_layout);
        do {
                PangoLayoutRun *run;

                run = pango_layout_iter_get_run (iter);
                if (!run)
                        continue;

                for (size_t i = 0; i < run->glyphs->num_glyphs; i++) {
                        if (run->glyphs->glyphs[i].glyph & PANGO_GLYPH_UNKNOWN_FLAG) {
                                run->glyphs->glyphs[i].glyph = PANGO_GLYPH_INVALID_INPUT;
                                hexbox_removed = true;
                        }
                }
        } while (pango_layout_iter_next_run (iter));
        pango_layout_iter_free (iter);

        if (hexbox_removed)
                pango_layout_context_changed (pango_layout);
}

void
look_up_rgb_color_from_terminal_color (ply_terminal_color_t color,
                                       uint16_t            *red,
                                       uint16_t            *green,
                                       uint16_t            *blue)
{
        switch (color) {
        case PLY_TERMINAL_COLOR_BLACK:
                *red = 0x0000;
                *green = 0x0000;
                *blue = 0x0000;
                break;
        /* Linux VT Color: 0xaa0000 */
        case PLY_TERMINAL_COLOR_RED:
                *red = 0xaa00;
                *green = 0x0000;
                *blue = 0x0000;
                break;
        /* Linux VT Color: 0x00aa00 */
        case PLY_TERMINAL_COLOR_GREEN:
                *red = 0x0000;
                *green = 0xaa00;
                *blue = 0x0000;
                break;
        /* Linux VT Color: 0xaa5500 */
        case PLY_TERMINAL_COLOR_BROWN:
                *red = 0xaa00;
                *green = 0x5500;
                *blue = 0x0000;
                break;
        /* Linux VT Color: 0x0000aa */
        case PLY_TERMINAL_COLOR_BLUE:
                *red = 0x0000;
                *green = 0x0000;
                *blue = 0xaa00;
                break;
        /* Linux VT Color: 0xaa00aa */
        case PLY_TERMINAL_COLOR_MAGENTA:
                *red = 0xaa00;
                *green = 0x0000;
                *blue = 0xaa00;
                break;
        /* Linux VT Color: 0x00aaaa */
        case PLY_TERMINAL_COLOR_CYAN:
                *red = 0x0000;
                *green = 0xaa00;
                *blue = 0xaa00;
                break;
        /* Linux VT Color: 0xaaaaaa */
        case PLY_TERMINAL_COLOR_WHITE:
        default:
                *red = 0xaa00;
                *green = 0xaa00;
                *blue = 0xaa00;
                break;
        }
}

static PangoLayout *
init_pango_text_layout (cairo_t       *cairo_context,
                        char          *text,
                        char          *font_description,
                        PangoAlignment alignment,
                        PangoAttrList *attribute_list,
                        long           width)
{
        PangoLayout *pango_layout;
        PangoFontDescription *description;

        pango_layout = pango_cairo_create_layout (cairo_context);

        if (!font_description)
                description = pango_font_description_from_string ("Sans 12");
        else
                description = pango_font_description_from_string (font_description);

        pango_layout_set_font_description (pango_layout, description);
        pango_font_description_free (description);

        pango_layout_set_alignment (pango_layout, alignment);
        if (width >= 0)
                pango_layout_set_width (pango_layout, width * PANGO_SCALE);

        pango_layout_set_text (pango_layout, text ?: "", -1);
        pango_layout_set_attributes (pango_layout, attribute_list);
        pango_cairo_update_layout (cairo_context, pango_layout);

        return pango_layout;
}

static void
size_control (ply_label_plugin_control_t *label,
              bool                        force)
{
        cairo_t *cairo_context;
        PangoLayout *pango_layout;
        int text_width = 0;
        int text_height = 0;

        if (!force && !label->needs_size_update)
                return; /* Size already is up to date */

        if (!force && label->is_hidden) {
                label->needs_size_update = true;
                return;
        }

        cairo_context = get_cairo_context_for_sizing (label);

        pango_layout = init_pango_text_layout (cairo_context, label->text, label->font, label->alignment, label->attribute_list, label->width);
        pango_layout_get_pixel_size (pango_layout, &text_width, &text_height);

        if (label->width < 0) {
                g_object_unref (pango_layout);
                pango_layout = init_pango_text_layout (cairo_context, label->text, label->font, label->alignment, label->attribute_list, text_width);
                pango_layout_get_pixel_size (pango_layout, &text_width, &text_height);
        }

        ply_trace ("Text '%s' has dimensions %dx%d", label->text, text_width, text_height);

        label->area.width = text_width;
        label->area.height = text_height;

        g_object_unref (pango_layout);
        cairo_destroy (cairo_context);
        label->needs_size_update = false;
}

static void
draw_control (ply_label_plugin_control_t *label,
              ply_pixel_buffer_t         *pixel_buffer,
              long                        x,
              long                        y,
              unsigned long               width,
              unsigned long               height)
{
        cairo_t *cairo_context;
        PangoLayout *pango_layout;
        long center_x;
        long center_y;
        int text_width;
        int text_height;

        if (label->is_hidden)
                return;

        cairo_context = get_cairo_context_for_pixel_buffer (label, pixel_buffer, &center_x, &center_y);

        pango_layout = init_pango_text_layout (cairo_context, label->text, label->font, label->alignment, label->attribute_list, label->width);
        remove_hexboxes_from_pango_layout (pango_layout);

        pango_layout_get_size (pango_layout, &text_width, &text_height);
        label->area.width = (long) ((double) text_width / PANGO_SCALE);
        label->area.height = (long) ((double) text_height / PANGO_SCALE);

        cairo_rectangle (cairo_context, x - center_x, y - center_y, width, height);
        cairo_clip (cairo_context);
        cairo_move_to (cairo_context,
                       label->area.x - center_x,
                       label->area.y - center_y);
        cairo_set_source_rgba (cairo_context,
                               label->red,
                               label->green,
                               label->blue,
                               label->alpha);
        pango_cairo_show_layout (cairo_context,
                                 pango_layout);

        g_object_unref (pango_layout);
        cairo_destroy (cairo_context);
}

static void
set_alignment_for_control (ply_label_plugin_control_t *label,
                           ply_label_alignment_t       alignment)
{
        ply_rectangle_t dirty_area;
        PangoAlignment pango_alignment;

        switch (alignment) {
        case PLY_LABEL_ALIGN_CENTER:
                pango_alignment = PANGO_ALIGN_CENTER;
                break;
        case PLY_LABEL_ALIGN_RIGHT:
                pango_alignment = PANGO_ALIGN_RIGHT;
                break;
        case PLY_LABEL_ALIGN_LEFT:
        default:
                pango_alignment = PANGO_ALIGN_LEFT;
                break;
        }

        if (label->alignment != pango_alignment) {
                dirty_area = label->area;
                label->alignment = pango_alignment;
                size_control (label, false);
                if (!label->is_hidden && label->display != NULL)
                        ply_pixel_display_draw_area (label->display,
                                                     dirty_area.x, dirty_area.y,
                                                     dirty_area.width, dirty_area.height);
        }
}

static void
set_width_for_control (ply_label_plugin_control_t *label,
                       long                        width)
{
        ply_rectangle_t dirty_area;

        if (label->width != width) {
                dirty_area = label->area;
                label->width = width;
                size_control (label, false);
                if (!label->is_hidden && label->display != NULL)
                        ply_pixel_display_draw_area (label->display,
                                                     dirty_area.x, dirty_area.y,
                                                     dirty_area.width, dirty_area.height);
        }
}

static void
clear_text (ply_label_plugin_control_t *label)
{
        GSList *attributes, *attribute;

        if (label->attribute_list) {
                attributes = pango_attr_list_get_attributes (label->attribute_list);
                for (attribute = attributes; attribute; attribute = attribute->next) {
                        pango_attribute_destroy (attribute->data);
                }
                pango_attr_list_unref (label->attribute_list);
                g_slist_free (attributes);
                label->attribute_list = pango_attr_list_new ();
        }
}

static void
set_text (ply_label_plugin_control_t *label,
          const char                 *text)
{
        ply_rectangle_t dirty_area;

        if (strcmp (label->text ?: "", text ?: "") != 0) {
                dirty_area = label->area;
                free (label->text);
                label->text = strdup (text);
                size_control (label, false);
                if (!label->is_hidden && label->display != NULL)
                        ply_pixel_display_draw_area (label->display,
                                                     dirty_area.x, dirty_area.y,
                                                     dirty_area.width, dirty_area.height);
        }
}

static void
set_text_for_control (ply_label_plugin_control_t *label,
                      const char                 *text)
{
        clear_text (label);
        set_text (label, text);
}

static void
stage_pango_attribute_for_list (PangoAttrList   *attribute_list,
                                PangoAttribute **staged_attributes,
                                PangoAttribute  *new_attribute)
{
        PangoAttrType attribute_type = new_attribute->klass->type;

        if (staged_attributes[attribute_type] != NULL) {
                if (!pango_attribute_equal (staged_attributes[attribute_type], new_attribute)) {
                        pango_attr_list_insert (attribute_list, staged_attributes[attribute_type]);
                        staged_attributes[attribute_type] = new_attribute;
                } else {
                        staged_attributes[attribute_type]->end_index = new_attribute->end_index;
                        pango_attribute_destroy (new_attribute);
                }
        } else {
                staged_attributes[attribute_type] = new_attribute;
        }
}

static void
flush_pango_attributes_to_list (PangoAttrList   *attribute_list,
                                PangoAttribute **staged_attributes)
{
        for (size_t i = 0; i <= PANGO_ATTR_FONT_SCALE; i++) {
                if (staged_attributes[i] == NULL)
                        continue;

                pango_attr_list_insert (attribute_list, staged_attributes[i]);
                staged_attributes[i] = NULL;
        }
}

static void
set_rich_text_for_control (ply_label_plugin_control_t *label,
                           ply_rich_text_t            *rich_text,
                           ply_rich_text_span_t       *span)
{
        int i;
        size_t start_index = 0;
        size_t length;
        char *string;
        PangoAttribute *staged_attributes[PANGO_ATTR_FONT_SCALE + 1] = { NULL };
        ply_rich_text_character_t **characters;

        clear_text (label);
        if (label->attribute_list) {
                pango_attr_list_unref (label->attribute_list);
                label->attribute_list = pango_attr_list_new ();
        }

        characters = ply_rich_text_get_characters (rich_text);
        for (i = span->offset; characters[i] != NULL; i++) {
                PangoAttribute *pango_attribute = NULL;
                uint16_t foreground_red, background_red;
                uint16_t foreground_green, background_green;
                uint16_t foreground_blue, background_blue;

                length = characters[i]->length;

                PangoWeight bold_style = PANGO_WEIGHT_NORMAL;
                PangoStyle italic_style = PANGO_STYLE_NORMAL;
                PangoUnderline underline_style = PANGO_UNDERLINE_NONE;

                ply_terminal_color_t foreground_color = PLY_TERMINAL_COLOR_DEFAULT;
                ply_terminal_color_t background_color = PLY_TERMINAL_COLOR_DEFAULT;

                if (!characters[i]->style.reverse_enabled) {
                        foreground_color = characters[i]->style.foreground_color;
                        background_color = characters[i]->style.background_color;
                } else {
                        foreground_color = characters[i]->style.background_color;
                        background_color = characters[i]->style.foreground_color;

                        /* if no background color is specified, the label is transparent.
                         * When reversed, and the background color is default
                         */
                        if (background_color == PLY_TERMINAL_COLOR_DEFAULT) {
                                background_color = PLY_TERMINAL_COLOR_WHITE;

                                if (foreground_color == PLY_TERMINAL_COLOR_DEFAULT)
                                        foreground_color = PLY_TERMINAL_COLOR_BLACK;
                        }
                }

                /* Default to a black background when none is set so bright text is readable on bright backgrounds */
                if (background_color == PLY_TERMINAL_COLOR_DEFAULT)
                        background_color = PLY_TERMINAL_COLOR_BLACK;

                look_up_rgb_color_from_terminal_color (foreground_color,
                                                       &foreground_red,
                                                       &foreground_green,
                                                       &foreground_blue);

                look_up_rgb_color_from_terminal_color (background_color,
                                                       &background_red,
                                                       &background_green,
                                                       &background_blue);

                if (characters[i]->style.bold_enabled && characters[i]->style.dim_enabled) {
                        /* xterm subtracts 0x44 when bold and dim*/
                        if (foreground_red > 0x4400) {
                                foreground_red -= 0x4400;
                        } else {
                                foreground_red = 0;
                        }

                        if (foreground_green > 0x4400) {
                                foreground_green -= 0x4400;
                        } else {
                                foreground_green = 0;
                        }

                        if (foreground_blue > 0x4400) {
                                foreground_blue -= 0x4400;
                        } else {
                                foreground_blue = 0;
                        }
                        bold_style = PANGO_WEIGHT_SEMIBOLD;
                } else {
                        if (characters[i]->style.bold_enabled) {
                                /* Linux VT adds 0x55 when bold */
                                if (foreground_red + 0x55ff < 0xffff) {
                                        foreground_red += 0x55ff;
                                } else {
                                        foreground_red = 0xffff;
                                }

                                if (foreground_green + 0x55ff < 0xffff) {
                                        foreground_green += 0x55ff;
                                } else {
                                        foreground_green = 0xffff;
                                }

                                if (foreground_blue + 0x55ff < 0xffff) {
                                        foreground_blue += 0x55ff;
                                } else {
                                        foreground_blue = 0xffff;
                                }
                                bold_style = PANGO_WEIGHT_BOLD;
                        }

                        if (characters[i]->style.dim_enabled) {
                                /* xterm subtracts 0x23 when dim */
                                if (foreground_red > 0x2300) {
                                        foreground_red -= 0x2300;
                                } else {
                                        foreground_red = 0;
                                }

                                if (foreground_green > 0x2300) {
                                        foreground_green -= 0x2300;
                                } else {
                                        foreground_green = 0;
                                }

                                if (foreground_blue > 0x2300) {
                                        foreground_blue -= 0x2300;
                                } else {
                                        foreground_blue = 0;
                                }
                                bold_style = PANGO_WEIGHT_LIGHT;
                        }
                }

                if (foreground_color != PLY_TERMINAL_COLOR_DEFAULT) {
                        pango_attribute = pango_attr_foreground_new (foreground_red, foreground_green, foreground_blue);
                        pango_attribute->start_index = start_index;
                        pango_attribute->end_index = start_index + length;

                        stage_pango_attribute_for_list (label->attribute_list, staged_attributes, pango_attribute);
                }

                if (background_color != PLY_TERMINAL_COLOR_DEFAULT) {
                        pango_attribute = pango_attr_background_new (background_red, background_green, background_blue);
                        pango_attribute->start_index = start_index;
                        pango_attribute->end_index = start_index + length;
                        stage_pango_attribute_for_list (label->attribute_list, staged_attributes, pango_attribute);
                }

                pango_attribute = pango_attr_weight_new (bold_style);
                pango_attribute->start_index = start_index;
                pango_attribute->end_index = start_index + length;
                stage_pango_attribute_for_list (label->attribute_list, staged_attributes, pango_attribute);


                if (characters[i]->style.italic_enabled == true)
                        italic_style = PANGO_STYLE_ITALIC;

                pango_attribute = pango_attr_style_new (italic_style);
                pango_attribute->start_index = start_index;
                pango_attribute->end_index = start_index + length;
                stage_pango_attribute_for_list (label->attribute_list, staged_attributes, pango_attribute);

                if (characters[i]->style.underline_enabled == true)
                        underline_style = PANGO_UNDERLINE_SINGLE;

                pango_attribute = pango_attr_underline_new (underline_style);
                pango_attribute->start_index = start_index;
                pango_attribute->end_index = start_index + length;
                stage_pango_attribute_for_list (label->attribute_list, staged_attributes, pango_attribute);

                start_index += length;

                if (i >= span->offset + span->range)
                        break;
        }
        flush_pango_attributes_to_list (label->attribute_list, staged_attributes);

        string = ply_rich_text_get_string (rich_text, span);
        set_text (label, string);
        free (string);
}

static void
set_font_for_control (ply_label_plugin_control_t *label,
                      const char                 *font)
{
        ply_rectangle_t dirty_area;

        if (label->font != font) {
                dirty_area = label->area;
                free (label->font);
                if (font)
                        label->font = strdup (font);
                else
                        label->font = NULL;
                size_control (label, false);
                if (!label->is_hidden && label->display != NULL)
                        ply_pixel_display_draw_area (label->display,
                                                     dirty_area.x, dirty_area.y,
                                                     dirty_area.width, dirty_area.height);
        }
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

        if (!label->is_hidden && label->display != NULL)
                ply_pixel_display_draw_area (label->display,
                                             label->area.x, label->area.y,
                                             label->area.width, label->area.height);
}

static bool
show_control (ply_label_plugin_control_t *label,
              ply_pixel_display_t        *display,
              long                        x,
              long                        y)
{
        ply_rectangle_t dirty_area;

        dirty_area = label->area;
        label->display = display;
        label->area.x = x;
        label->area.y = y;

        label->is_hidden = false;

        size_control (label, true);

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
        label->loop = NULL;
}

static bool
is_control_hidden (ply_label_plugin_control_t *label)
{
        return label->is_hidden;
}

static long
get_width_of_control (ply_label_plugin_control_t *label)
{
        size_control (label, true);
        return label->area.width;
}

static long
get_height_of_control (ply_label_plugin_control_t *label)
{
        size_control (label, true);
        return label->area.height;
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
