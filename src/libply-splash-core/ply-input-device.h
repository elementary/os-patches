/* ply-input-device.h - evdev input device handling
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
 * Written By: Diego Augusto <diego.augusto@protonmail.com>
 */
#ifndef PLY_INPUT_DEVICE_H
#define PLY_INPUT_DEVICE_H

#include "ply-buffer.h"
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

typedef enum
{
        PLY_LED_NUM_LOCK    = (1 << 0),
        PLY_LED_CAPS_LOCK   = (1 << 1),
        PLY_LED_SCROLL_LOCK = (1 << 2)
} ply_led_t;

typedef enum
{
        PLY_KEY_UP,
        PLY_KEY_DOWN,
        PLY_KEY_HELD,
} ply_key_direction_t;

typedef enum
{
        PLY_INPUT_RESULT_PROPAGATED = false,
        PLY_INPUT_RESULT_CONSUMED   = true,
} ply_input_device_input_result_t;

typedef struct _ply_input_device ply_input_device_t;
typedef ply_input_device_input_result_t (*ply_input_device_input_handler_t) (void               *user_data,
                                                                             ply_input_device_t *input_device,
                                                                             const char         *buf);
typedef void (*ply_input_device_leds_changed_handler_t) (void               *user_data,
                                                         ply_input_device_t *input_device);
typedef void (*ply_input_device_disconnect_handler_t) (void               *user_data,
                                                       ply_input_device_t *input_device);
typedef struct
{
        xkb_mod_mask_t mods_depressed;
        xkb_mod_mask_t mods_latched;
        xkb_mod_mask_t mods_locked;
        xkb_mod_mask_t group;
} ply_xkb_keyboard_state_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS

ply_input_device_t *ply_input_device_open (struct xkb_context *xkb_context,
                                           struct xkb_keymap  *xkb_keymap,
                                           const char         *path);
void ply_input_device_free (ply_input_device_t *input_device);
void ply_input_device_watch_for_input (ply_input_device_t                     *input_device,
                                       ply_input_device_input_handler_t        input_callback,
                                       ply_input_device_leds_changed_handler_t led_callback,
                                       void                                   *user_data);

void ply_input_device_stop_watching_for_input (ply_input_device_t                     *input_device,
                                               ply_input_device_input_handler_t        input_callback,
                                               ply_input_device_leds_changed_handler_t led_callback,
                                               void                                   *user_data);


ply_xkb_keyboard_state_t *ply_input_device_get_state (ply_input_device_t *input_device);
void ply_input_device_set_state (ply_input_device_t       *input_device,
                                 ply_xkb_keyboard_state_t *xkb_state);

void ply_input_device_set_disconnect_handler (ply_input_device_t                   *input_device,
                                              ply_input_device_disconnect_handler_t callback,
                                              void                                 *user_data);
int ply_input_device_get_fd (ply_input_device_t *input_device);
int  ply_input_device_is_keyboard (ply_input_device_t *input_device);
int  ply_input_device_is_keyboard_with_leds (ply_input_device_t *input_device);
const char *ply_input_device_get_name (ply_input_device_t *input_device);
bool ply_input_device_get_capslock_state (ply_input_device_t *input_device);
/* The value shouldn't be used as a unique indentifier for a keymap */
const char *ply_input_device_get_keymap (ply_input_device_t *input_device);
const char *ply_input_device_get_path (ply_input_device_t *input_device);

#endif //PLY_HIDE_FUNCTION_DECLARATIONS

#endif //PLY_INPUT_DEVICE_H
