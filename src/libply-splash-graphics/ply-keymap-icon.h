/* keymap-icon - Shows a keyboard-icon + the current keymap as text, e.g. "us"
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
#ifndef KEYMAP_ICON_H
#define KEYMAP_ICON_H

#include <stdbool.h>

#include "ply-event-loop.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-trigger.h"

typedef struct _ply_keymap_icon ply_keymap_icon_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_keymap_icon_t *ply_keymap_icon_new (ply_pixel_display_t *display,
                                        const char          *image_dir);
void ply_keymap_icon_free (ply_keymap_icon_t *keymap_icon);

bool ply_keymap_icon_load (ply_keymap_icon_t *keymap_icon);
bool ply_keymap_icon_show (ply_keymap_icon_t *keymap_icon,
                           long               x,
                           long               y);
void ply_keymap_icon_hide (ply_keymap_icon_t *keymap_icon);

void ply_keymap_icon_draw_area (ply_keymap_icon_t  *keymap_icon,
                                ply_pixel_buffer_t *buffer,
                                long                x,
                                long                y,
                                unsigned long       width,
                                unsigned long       height);

unsigned long ply_keymap_icon_get_width (ply_keymap_icon_t *keymap_icon);
unsigned long ply_keymap_icon_get_height (ply_keymap_icon_t *keymap_icon);
#endif

#endif /* KEYMAP_ICON_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
