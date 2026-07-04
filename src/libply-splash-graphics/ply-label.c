/* ply-label.c - APIs for showing text
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
#include "ply-label.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>

#include "ply-label-plugin.h"
#include "ply-event-loop.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include "ply-array.h"

struct _ply_label
{
        ply_event_loop_t                   *loop;
        ply_module_handle_t                *module_handle;
        const ply_label_plugin_interface_t *plugin_interface;
        ply_label_plugin_control_t         *control;

        char                               *text;

        ply_rich_text_t                    *rich_text;
        ply_rich_text_span_t                span;

        ply_label_alignment_t               alignment;
        long                                width;
        char                               *font;
        float                               red;
        float                               green;
        float                               blue;
        float                               alpha;
};

typedef const ply_label_plugin_interface_t *
(*get_plugin_interface_function_t) (void);

static void ply_label_unload_plugin (ply_label_t *label);

ply_label_t *
ply_label_new (void)
{
        ply_label_t *label;

        label = calloc (1, sizeof(struct _ply_label));
        label->red = 1;
        label->green = 1;
        label->blue = 1;
        label->alpha = 1;
        label->alignment = PLY_LABEL_ALIGN_LEFT;
        label->width = -1;
        return label;
}

void
ply_label_free (ply_label_t *label)
{
        if (label == NULL)
                return;

        if (label->plugin_interface != NULL) {
                ply_trace ("Unloading label control plugin");
                ply_label_unload_plugin (label);
        }

        free (label->text);
        free (label->font);

        if (label->rich_text) {
                ply_rich_text_drop_reference (label->rich_text);
                label->rich_text = NULL;
        }

        free (label);
}

static bool
ply_label_load_plugin (ply_label_t *label)
{
        assert (label != NULL);

        get_plugin_interface_function_t get_label_plugin_interface;

        label->module_handle = ply_open_module (PLYMOUTH_PLUGIN_PATH "label-pango.so");

        /* and the FreeType based one after that, it is not a complete substitute (yet). */
        if (label->module_handle == NULL)
                label->module_handle = ply_open_module (PLYMOUTH_PLUGIN_PATH "label-freetype.so");

        if (label->module_handle == NULL)
                return false;

        get_label_plugin_interface = (get_plugin_interface_function_t)
                                     ply_module_look_up_function (label->module_handle,
                                                                  "ply_label_plugin_get_interface");

        if (get_label_plugin_interface == NULL) {
                ply_save_errno ();
                ply_close_module (label->module_handle);
                label->module_handle = NULL;
                ply_restore_errno ();
                return false;
        }

        label->plugin_interface = get_label_plugin_interface ();

        if (label->plugin_interface == NULL) {
                ply_save_errno ();
                ply_close_module (label->module_handle);
                label->module_handle = NULL;
                ply_restore_errno ();
                return false;
        }

        label->control = label->plugin_interface->create_control ();

        if (label->control == NULL) {
                ply_save_errno ();
                label->plugin_interface = NULL;
                ply_close_module (label->module_handle);
                label->module_handle = NULL;
                ply_restore_errno ();
                return false;
        }

        if (label->font != NULL)
                label->plugin_interface->set_font_for_control (label->control,
                                                               label->font);

        if (label->text != NULL) {
                if (label->rich_text == NULL) {
                        label->plugin_interface->set_text_for_control (label->control,
                                                                       label->text?: "");
                } else {
                        label->plugin_interface->set_rich_text_for_control (label->control,
                                                                            label->rich_text,
                                                                            &label->span);
                }
        }
        label->plugin_interface->set_alignment_for_control (label->control,
                                                            label->alignment);
        label->plugin_interface->set_width_for_control (label->control,
                                                        label->width);
        label->plugin_interface->set_color_for_control (label->control,
                                                        label->red,
                                                        label->green,
                                                        label->blue,
                                                        label->alpha);
        return true;
}

static void
ply_label_unload_plugin (ply_label_t *label)
{
        assert (label != NULL);
        assert (label->plugin_interface != NULL);
        assert (label->module_handle != NULL);

        ply_close_module (label->module_handle);
        label->plugin_interface = NULL;
        label->module_handle = NULL;
}

bool
ply_label_show (ply_label_t         *label,
                ply_pixel_display_t *display,
                long                 x,
                long                 y)
{
        if (label->plugin_interface == NULL)
                if (!ply_label_load_plugin (label))
                        return false;

        return label->plugin_interface->show_control (label->control,
                                                      display, x, y);
}

void
ply_label_draw (ply_label_t *label)
{
        if (label->plugin_interface == NULL)
                return;
}

void
ply_label_draw_area (ply_label_t        *label,
                     ply_pixel_buffer_t *buffer,
                     long                x,
                     long                y,
                     unsigned long       width,
                     unsigned long       height)
{
        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->draw_control (label->control,
                                               buffer,
                                               x, y, width, height);
}

void
ply_label_hide (ply_label_t *label)
{
        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->hide_control (label->control);
}

bool
ply_label_is_hidden (ply_label_t *label)
{
        if (label->plugin_interface == NULL)
                return true;

        return label->plugin_interface->is_control_hidden (label->control);
}

void
ply_label_set_text (ply_label_t *label,
                    const char  *text)
{
        free (label->text);
        label->text = NULL;

        if (text != NULL)
                label->text = strdup (text);

        if (label->rich_text) {
                ply_rich_text_drop_reference (label->rich_text);
                label->rich_text = NULL;
        }

        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->set_text_for_control (label->control,
                                                       label->text?: "");
}

void
ply_label_set_rich_text (ply_label_t          *label,
                         ply_rich_text_t      *rich_text,
                         ply_rich_text_span_t *span)
{
        free (label->text);
        label->text = ply_rich_text_get_string (rich_text, span);

        if (label->rich_text)
                ply_rich_text_drop_reference (label->rich_text);
        label->rich_text = rich_text;
        ply_rich_text_take_reference (rich_text);

        label->span = *span;

        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->set_rich_text_for_control (label->control,
                                                            label->rich_text,
                                                            &label->span);
}

void
ply_label_set_alignment (ply_label_t          *label,
                         ply_label_alignment_t alignment)
{
        label->alignment = alignment;

        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->set_alignment_for_control (label->control,
                                                            alignment);
}

void
ply_label_set_width (ply_label_t *label,
                     long         width)
{
        label->width = width;

        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->set_width_for_control (label->control,
                                                        width);
}

/*
 * Please see pango documentation, for font format:
 * http://library.gnome.org/devel/pango/stable/pango-Fonts.html#pango-font-description-from-string
 * If you pass NULL, it will use default font.
 */
void
ply_label_set_font (ply_label_t *label,
                    const char  *font)
{
        free (label->font);
        if (font)
                label->font = strdup (font);
        else
                label->font = NULL;

        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->set_font_for_control (label->control,
                                                       font);
}

void
ply_label_set_hex_color (ply_label_t *label,
                         uint32_t     hex_color)
{
        double red;
        double green;
        double blue;
        double alpha;

        red = ((double) (hex_color & 0xff000000) / 0xff000000);
        green = ((double) (hex_color & 0x00ff0000) / 0x00ff0000);
        blue = ((double) (hex_color & 0x0000ff00) / 0x0000ff00);
        alpha = ((double) (hex_color & 0x000000ff) / 0x000000ff);

        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->set_color_for_control (label->control,
                                                        red,
                                                        green,
                                                        blue,
                                                        alpha);
}

void
ply_label_set_color (ply_label_t *label,
                     float        red,
                     float        green,
                     float        blue,
                     float        alpha)
{
        label->red = red;
        label->green = green;
        label->blue = blue;
        label->alpha = alpha;

        if (label->plugin_interface == NULL)
                return;

        label->plugin_interface->set_color_for_control (label->control,
                                                        red,
                                                        green,
                                                        blue,
                                                        alpha);
}

long
ply_label_get_width (ply_label_t *label)
{
        if (label->plugin_interface == NULL)
                if (!ply_label_load_plugin (label))
                        return 0;

        return label->plugin_interface->get_width_of_control (label->control);
}

long
ply_label_get_height (ply_label_t *label)
{
        if (label->plugin_interface == NULL)
                if (!ply_label_load_plugin (label))
                        return 0;

        return label->plugin_interface->get_height_of_control (label->control);
}
