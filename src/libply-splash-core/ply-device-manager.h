/* ply-device-manager.h - udev monitor
 *
 * Copyright (C) 2013 Red Hat, Inc.
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
 */
#ifndef PLY_DEVICE_MANAGER_H
#define PLY_DEVICE_MANAGER_H

#include <stdbool.h>

#include "ply-keyboard.h"
#include "ply-pixel-display.h"
#include "ply-renderer.h"
#include "ply-text-display.h"

typedef enum
{
        PLY_DEVICE_MANAGER_FLAGS_NONE = 0,
        PLY_DEVICE_MANAGER_FLAGS_IGNORE_SERIAL_CONSOLES = 1 << 0,
        PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV = 1 << 1,
        PLY_DEVICE_MANAGER_FLAGS_SKIP_RENDERERS = 1 << 2
} ply_device_manager_flags_t;

typedef struct _ply_device_manager ply_device_manager_t;
typedef void (* ply_keyboard_added_handler_t) (void *, ply_keyboard_t *);
typedef void (* ply_keyboard_removed_handler_t) (void *, ply_keyboard_t *);
typedef void (* ply_pixel_display_added_handler_t) (void *, ply_pixel_display_t *);
typedef void (* ply_pixel_display_removed_handler_t) (void *, ply_pixel_display_t *);
typedef void (* ply_text_display_added_handler_t) (void *, ply_text_display_t *);
typedef void (* ply_text_display_removed_handler_t) (void *, ply_text_display_t *);

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_device_manager_t *ply_device_manager_new (const char                *default_tty,
                                              ply_device_manager_flags_t flags);
void ply_device_manager_watch_devices (ply_device_manager_t                *manager,
                                       double                               device_timeout,
                                       ply_keyboard_added_handler_t         keyboard_added_handler,
                                       ply_keyboard_removed_handler_t       keyboard_removed_handler,
                                       ply_pixel_display_added_handler_t    pixel_display_added_handler,
                                       ply_pixel_display_removed_handler_t  pixel_display_removed_handler,
                                       ply_text_display_added_handler_t     text_display_added_handler,
                                       ply_text_display_removed_handler_t   text_display_removed_handler,
                                       void                                *data);
void ply_device_manager_pause (ply_device_manager_t *manager);
void ply_device_manager_unpause (ply_device_manager_t *manager);
bool ply_device_manager_has_serial_consoles (ply_device_manager_t *manager);
bool ply_device_manager_has_displays (ply_device_manager_t *manager);
ply_list_t *ply_device_manager_get_keyboards (ply_device_manager_t *manager);
ply_list_t *ply_device_manager_get_pixel_displays (ply_device_manager_t *manager);
ply_list_t *ply_device_manager_get_text_displays (ply_device_manager_t *manager);
void ply_device_manager_free (ply_device_manager_t *manager);
void ply_device_manager_activate_keyboards (ply_device_manager_t *manager);
void ply_device_manager_deactivate_keyboards (ply_device_manager_t *manager);
void ply_device_manager_activate_renderers (ply_device_manager_t *manager);
void ply_device_manager_deactivate_renderers (ply_device_manager_t *manager);
ply_terminal_t *ply_device_manager_get_default_terminal (ply_device_manager_t *manager);

#endif

#endif
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
