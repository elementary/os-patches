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
#ifndef CAPSLOCK_ICON_H
#define CAPSLOCK_ICON_H

#include <stdbool.h>

#include "ply-event-loop.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-trigger.h"

typedef struct _ply_capslock_icon ply_capslock_icon_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_capslock_icon_t *ply_capslock_icon_new (const char *image_dir);
void ply_capslock_icon_free (ply_capslock_icon_t *capslock_icon);

bool ply_capslock_icon_load (ply_capslock_icon_t *capslock_icon);
bool ply_capslock_icon_show (ply_capslock_icon_t *capslock_icon,
                             ply_event_loop_t    *loop,
                             ply_pixel_display_t *display,
                             long                 x,
                             long                 y);
void ply_capslock_icon_hide (ply_capslock_icon_t *capslock_icon);

void ply_capslock_icon_draw_area (ply_capslock_icon_t *capslock_icon,
                                  ply_pixel_buffer_t  *buffer,
                                  long                 x,
                                  long                 y,
                                  unsigned long        width,
                                  unsigned long        height);

unsigned long ply_capslock_icon_get_width (ply_capslock_icon_t *capslock_icon);
unsigned long ply_capslock_icon_get_height (ply_capslock_icon_t *capslock_icon);
#endif

#endif /* CAPSLOCK_ICON_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
