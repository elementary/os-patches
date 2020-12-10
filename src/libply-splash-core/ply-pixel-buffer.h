/* ply-pixel-buffer.h - pixel buffer abstraction
 *
 * Copyright (C) 2007, 2009 Red Hat, Inc.
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
 * Written By: Ray Strode <rstrode@redhat.com>
 */
#ifndef PLY_PIXEL_BUFFER_H
#define PLY_PIXEL_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "ply-rectangle.h"
#include "ply-region.h"
#include "ply-utils.h"

typedef struct _ply_pixel_buffer ply_pixel_buffer_t;

#define PLY_PIXEL_BUFFER_COLOR_TO_PIXEL_VALUE(r, g, b, a)                        \
        (((uint8_t) (CLAMP (a * 255.0, 0.0, 255.0)) << 24)                        \
         | ((uint8_t) (CLAMP (r * 255.0, 0.0, 255.0)) << 16)                     \
         | ((uint8_t) (CLAMP (g * 255.0, 0.0, 255.0)) << 8)                      \
         | ((uint8_t) (CLAMP (b * 255.0, 0.0, 255.0))))

typedef enum
{
        PLY_PIXEL_BUFFER_ROTATE_UPRIGHT = 0,
        PLY_PIXEL_BUFFER_ROTATE_UPSIDE_DOWN,
        PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE,
        PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE
} ply_pixel_buffer_rotation_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_pixel_buffer_t *ply_pixel_buffer_new (unsigned long width,
                                          unsigned long height);
ply_pixel_buffer_t *
ply_pixel_buffer_new_with_device_rotation (unsigned long width,
                                           unsigned long height,
                                           ply_pixel_buffer_rotation_t device_rotation);
void ply_pixel_buffer_free (ply_pixel_buffer_t *buffer);
void ply_pixel_buffer_get_size (ply_pixel_buffer_t *buffer,
                                ply_rectangle_t    *size);
int  ply_pixel_buffer_get_device_scale (ply_pixel_buffer_t *buffer);
void ply_pixel_buffer_set_device_scale (ply_pixel_buffer_t *buffer,
                                        int                 scale);

ply_pixel_buffer_rotation_t
ply_pixel_buffer_get_device_rotation (ply_pixel_buffer_t *buffer);
/* Note calling this removes all pushed clip-areas */
void ply_pixel_buffer_set_device_rotation (ply_pixel_buffer_t *buffer,
                                           ply_pixel_buffer_rotation_t rotation);

unsigned long ply_pixel_buffer_get_width (ply_pixel_buffer_t *buffer);
unsigned long ply_pixel_buffer_get_height (ply_pixel_buffer_t *buffer);

bool ply_pixel_buffer_is_opaque (ply_pixel_buffer_t *buffer);
void ply_pixel_buffer_set_opaque (ply_pixel_buffer_t *buffer,
                                  bool                is_opaque);

ply_region_t *ply_pixel_buffer_get_updated_areas (ply_pixel_buffer_t *buffer);

void ply_pixel_buffer_fill_with_color (ply_pixel_buffer_t *buffer,
                                       ply_rectangle_t    *fill_area,
                                       double              red,
                                       double              green,
                                       double              blue,
                                       double              alpha);
void ply_pixel_buffer_fill_with_hex_color (ply_pixel_buffer_t *buffer,
                                           ply_rectangle_t    *fill_area,
                                           uint32_t            hex_color);

void ply_pixel_buffer_fill_with_hex_color_at_opacity (ply_pixel_buffer_t *buffer,
                                                      ply_rectangle_t    *fill_area,
                                                      uint32_t            hex_color,
                                                      double              opacity);

void ply_pixel_buffer_fill_with_gradient (ply_pixel_buffer_t *buffer,
                                          ply_rectangle_t    *fill_area,
                                          uint32_t            start,
                                          uint32_t            end);

void ply_pixel_buffer_fill_with_argb32_data (ply_pixel_buffer_t *buffer,
                                             ply_rectangle_t    *fill_area,
                                             uint32_t           *data);
void ply_pixel_buffer_fill_with_argb32_data_at_opacity (ply_pixel_buffer_t *buffer,
                                                        ply_rectangle_t    *fill_area,
                                                        uint32_t           *data,
                                                        double              opacity);

void ply_pixel_buffer_fill_with_argb32_data_with_clip (ply_pixel_buffer_t *buffer,
                                                       ply_rectangle_t    *fill_area,
                                                       ply_rectangle_t    *clip_area,
                                                       uint32_t           *data);
void ply_pixel_buffer_fill_with_argb32_data_at_opacity_with_clip (ply_pixel_buffer_t *buffer,
                                                                  ply_rectangle_t    *fill_area,
                                                                  ply_rectangle_t    *clip_area,
                                                                  uint32_t           *data,
                                                                  double              opacity);
void ply_pixel_buffer_fill_with_argb32_data_at_opacity_with_clip_and_scale (ply_pixel_buffer_t *buffer,
                                                                            ply_rectangle_t    *fill_area,
                                                                            ply_rectangle_t    *clip_area,
                                                                            uint32_t           *data,
                                                                            double              opacity,
                                                                            int                 scale);

void ply_pixel_buffer_fill_with_buffer_at_opacity_with_clip (ply_pixel_buffer_t *canvas,
                                                             ply_pixel_buffer_t *source,
                                                             int                 x_offset,
                                                             int                 y_offset,
                                                             ply_rectangle_t    *clip_area,
                                                             float               opacity);
void ply_pixel_buffer_fill_with_buffer_at_opacity (ply_pixel_buffer_t *canvas,
                                                   ply_pixel_buffer_t *source,
                                                   int                 x_offset,
                                                   int                 y_offset,
                                                   float               opacity);
void ply_pixel_buffer_fill_with_buffer_with_clip (ply_pixel_buffer_t *canvas,
                                                  ply_pixel_buffer_t *source,
                                                  int                 x_offset,
                                                  int                 y_offset,
                                                  ply_rectangle_t    *clip_area);
void ply_pixel_buffer_fill_with_buffer (ply_pixel_buffer_t *canvas,
                                        ply_pixel_buffer_t *source,
                                        int                 x_offset,
                                        int                 y_offset);


void ply_pixel_buffer_push_clip_area (ply_pixel_buffer_t *buffer,
                                      ply_rectangle_t    *clip_area);
void ply_pixel_buffer_pop_clip_area (ply_pixel_buffer_t *buffer);

uint32_t *ply_pixel_buffer_get_argb32_data (ply_pixel_buffer_t *buffer);

ply_pixel_buffer_t *ply_pixel_buffer_resize (ply_pixel_buffer_t *old_buffer,
                                             long                width,
                                             long                height);

ply_pixel_buffer_t *ply_pixel_buffer_rotate (ply_pixel_buffer_t *old_buffer,
                                             long                center_x,
                                             long                center_y,
                                             double              theta_offset);

ply_pixel_buffer_t *ply_pixel_buffer_tile (ply_pixel_buffer_t *old_buffer,
                                           long                width,
                                           long                height);

/* Return the upright version of a buffer which is non upright.
 * This is the *only* ply_pixel_buffer function which works correctly with a
 * non upright buffer as source.
 */
ply_pixel_buffer_t *ply_pixel_buffer_rotate_upright (ply_pixel_buffer_t *old_buffer);

#endif

#endif /* PLY_PIXEL_BUFFER_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
