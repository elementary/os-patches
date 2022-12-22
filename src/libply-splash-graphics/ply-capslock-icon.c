/* capslock-icon - Show an image when capslock is active
 *
 * Copyright (C) 2019 Red Hat, Inc.
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
 * Written by: Hans de Goede <hdegoede@redhat.com>
 */
#include "config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ply-capslock-icon.h"
#include "ply-event-loop.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-utils.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 30
#endif

struct _ply_capslock_icon
{
        char                *image_name;
        ply_pixel_buffer_t  *buffer;
        ply_event_loop_t    *loop;
        ply_pixel_display_t *display;
        long                 x, y;
        unsigned long        width, height;
        bool                 is_hidden;
        bool                 is_on;
};

static void ply_capslock_stop_polling (ply_capslock_icon_t *capslock_icon);

ply_capslock_icon_t *
ply_capslock_icon_new (const char *image_dir)
{
        ply_capslock_icon_t *capslock_icon;

        assert (image_dir != NULL);

        capslock_icon = calloc (1, sizeof(ply_capslock_icon_t));

        asprintf (&capslock_icon->image_name, "%s/capslock.png", image_dir);
        capslock_icon->is_hidden = true;

        return capslock_icon;
}

void
ply_capslock_icon_free (ply_capslock_icon_t *capslock_icon)
{
        if (capslock_icon == NULL)
                return;

        if (!capslock_icon->is_hidden)
                ply_capslock_stop_polling (capslock_icon);

        if (capslock_icon->buffer != NULL)
                ply_pixel_buffer_free (capslock_icon->buffer);

        free (capslock_icon->image_name);
        free (capslock_icon);
}

static void
ply_capslock_icon_update_state (ply_capslock_icon_t *capslock_icon)
{
        ply_renderer_t *renderer;

        if (!capslock_icon->display)
                return;

        renderer = ply_pixel_display_get_renderer (capslock_icon->display);
        capslock_icon->is_on = ply_renderer_get_capslock_state (renderer);
}

static void
ply_capslock_icon_draw (ply_capslock_icon_t *capslock_icon)
{
        ply_pixel_display_draw_area (capslock_icon->display,
                                     capslock_icon->x, capslock_icon->y,
                                     capslock_icon->width,
                                     capslock_icon->height);
}

static void
on_timeout (void             *user_data,
            ply_event_loop_t *loop)
{
        ply_capslock_icon_t *capslock_icon = user_data;
        bool old_is_on = capslock_icon->is_on;

        ply_capslock_icon_update_state (capslock_icon);
        
        if (capslock_icon->is_on != old_is_on)
                ply_capslock_icon_draw (capslock_icon);

        ply_event_loop_watch_for_timeout (capslock_icon->loop,
                                          1.0 / FRAMES_PER_SECOND,
                                          on_timeout, capslock_icon);
}

static void
ply_capslock_stop_polling (ply_capslock_icon_t *capslock_icon)
{
        ply_event_loop_stop_watching_for_timeout (capslock_icon->loop,
                                                  (ply_event_loop_timeout_handler_t)
                                                  on_timeout, capslock_icon);
}

bool
ply_capslock_icon_load (ply_capslock_icon_t *capslock_icon)
{
        ply_image_t *image;

        if (capslock_icon->buffer)
                return true;

        image = ply_image_new (capslock_icon->image_name);

        if (!ply_image_load (image)) {
                ply_image_free (image);
                return false;
        }

        capslock_icon->buffer = ply_image_convert_to_pixel_buffer (image);
        capslock_icon->width = ply_pixel_buffer_get_width (capslock_icon->buffer);
        capslock_icon->height = ply_pixel_buffer_get_height (capslock_icon->buffer);

        return true;
}

bool
ply_capslock_icon_show (ply_capslock_icon_t *capslock_icon,
                        ply_event_loop_t    *loop,
                        ply_pixel_display_t *display,
                        long                 x,
                        long                 y)
{
        assert (capslock_icon != NULL);
        assert (capslock_icon->loop == NULL);

        if (!capslock_icon->buffer) {
                ply_trace ("capslock_icon not loaded, can not start");
                return false;
        }

        capslock_icon->loop = loop;
        capslock_icon->display = display;
        capslock_icon->is_hidden = false;

        capslock_icon->x = x;
        capslock_icon->y = y;

        ply_capslock_icon_draw (capslock_icon);

        ply_event_loop_watch_for_timeout (capslock_icon->loop,
                                          1.0 / FRAMES_PER_SECOND,
                                          on_timeout, capslock_icon);

        return true;
}

void
ply_capslock_icon_hide (ply_capslock_icon_t *capslock_icon)
{
        if (capslock_icon->is_hidden)
                return;

        capslock_icon->is_hidden = true;

        ply_capslock_icon_draw (capslock_icon);
        ply_capslock_stop_polling (capslock_icon);

        capslock_icon->loop = NULL;
        capslock_icon->display = NULL;
}

void
ply_capslock_icon_draw_area (ply_capslock_icon_t *capslock_icon,
                             ply_pixel_buffer_t  *buffer,
                             long                 x,
                             long                 y,
                             unsigned long        width,
                             unsigned long        height)
{
        if (capslock_icon->is_hidden)
                return;

        ply_capslock_icon_update_state (capslock_icon);
        
        if (!capslock_icon->is_on)
                return;

        ply_pixel_buffer_fill_with_buffer (buffer,
                                           capslock_icon->buffer,
                                           capslock_icon->x,
                                           capslock_icon->y);
}

unsigned long
ply_capslock_icon_get_width (ply_capslock_icon_t *capslock_icon)
{
        return capslock_icon->width;
}

unsigned long
ply_capslock_icon_get_height (ply_capslock_icon_t *capslock_icon)
{
        return capslock_icon->height;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
