/* progress_bar.c - boot progress_bar
 *
 * Copyright (C) 2008, 2019 Red Hat, Inc.
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
 *             Will Woods <wwoods@redhat.com>
 *             Hans de Goede <hdegoede@redhat.com>
 */
#include "config.h"

#include <assert.h>
#include <dirent.h>
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

#include "ply-progress-bar.h"
#include "ply-event-loop.h"
#include "ply-array.h"
#include "ply-logger.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-image.h"
#include "ply-utils.h"

struct _ply_progress_bar
{
        ply_pixel_display_t *display;
        ply_rectangle_t      area;

        uint32_t             fg_color;
        uint32_t             bg_color;
        double               fraction_done;

        uint32_t             is_hidden : 1;
};

ply_progress_bar_t *
ply_progress_bar_new (void)
{
        ply_progress_bar_t *progress_bar;

        progress_bar = calloc (1, sizeof(ply_progress_bar_t));

        progress_bar->is_hidden = true;
        progress_bar->fg_color = 0xffffffff; /* Solid white */
        progress_bar->bg_color = 0x01000000; /* Transparent */
        progress_bar->fraction_done = 0.0;

        return progress_bar;
}

void
ply_progress_bar_free (ply_progress_bar_t *progress_bar)
{
        if (progress_bar == NULL)
                return;
        free (progress_bar);
}

void
ply_progress_bar_draw_area (ply_progress_bar_t *progress_bar,
                            ply_pixel_buffer_t *buffer,
                            long                x,
                            long                y,
                            unsigned long       width,
                            unsigned long       height)
{
        ply_rectangle_t fill_area;

        if (progress_bar->is_hidden)
                return;

        /* Note we ignore the passed in area / rectangle to update,
         * since ply_pixel_display_draw_area() already pushes it to
         * the buffer's clip_area list.
         */

        fill_area = progress_bar->area;
        fill_area.width = progress_bar->area.width * progress_bar->fraction_done;
        ply_pixel_buffer_fill_with_hex_color (buffer, &fill_area, progress_bar->fg_color);

        fill_area.x = fill_area.x + fill_area.width;
        fill_area.width = progress_bar->area.width - fill_area.width;
        ply_pixel_buffer_fill_with_hex_color (buffer, &fill_area, progress_bar->bg_color);
}

void
ply_progress_bar_draw (ply_progress_bar_t *progress_bar)
{
        if (progress_bar->is_hidden)
                return;

        ply_pixel_display_draw_area (progress_bar->display,
                                     progress_bar->area.x,
                                     progress_bar->area.y,
                                     progress_bar->area.width,
                                     progress_bar->area.height);
}

void
ply_progress_bar_show (ply_progress_bar_t  *progress_bar,
                       ply_pixel_display_t *display,
                       long                 x,
                       long                 y,
                       unsigned long        width,
                       unsigned long        height)
{
        assert (progress_bar != NULL);

        progress_bar->display = display;
        progress_bar->area.x = x;
        progress_bar->area.y = y;
        progress_bar->area.height = height;
        progress_bar->area.width = width;

        progress_bar->is_hidden = false;
        ply_progress_bar_draw (progress_bar);
}

void
ply_progress_bar_hide (ply_progress_bar_t *progress_bar)
{
        if (progress_bar->is_hidden)
                return;

        progress_bar->is_hidden = true;
        ply_pixel_display_draw_area (progress_bar->display,
                                     progress_bar->area.x, progress_bar->area.y,
                                     progress_bar->area.width, progress_bar->area.height);

        progress_bar->display = NULL;
}

bool
ply_progress_bar_is_hidden (ply_progress_bar_t *progress_bar)
{
        return progress_bar->is_hidden;
}

long
ply_progress_bar_get_width (ply_progress_bar_t *progress_bar)
{
        return progress_bar->area.width;
}

long
ply_progress_bar_get_height (ply_progress_bar_t *progress_bar)
{
        return progress_bar->area.height;
}

void
ply_progress_bar_set_fraction_done (ply_progress_bar_t *progress_bar,
                                    double              fraction_done)
{
        progress_bar->fraction_done = fraction_done;
        ply_progress_bar_draw (progress_bar);
}

double
ply_progress_bar_get_fraction_done (ply_progress_bar_t *progress_bar)
{
        return progress_bar->fraction_done;
}

void
ply_progress_bar_set_colors (ply_progress_bar_t *progress_bar,
                             uint32_t            fg_color,
                             uint32_t            bg_color)
{
        progress_bar->fg_color = fg_color;
        progress_bar->bg_color = bg_color;
        ply_progress_bar_draw (progress_bar);
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
