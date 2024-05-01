/* ply-input-device.c - evdev input device handling implementation
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
#include <assert.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/input.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-input-device.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-terminal.h"
#include "ply-trigger.h"
#include "ply-utils.h"

/* The docs say this needs to be at least 7, the code enforces this, but also never uses more
 * than 5. We'll just do 7.
 */
#define PLY_XKB_KEYSYM_TO_UTF8_BUFFER_SIZE 7

struct _ply_input_device
{
        int                       fd;
        char                     *path;
        ply_event_loop_t         *loop;
        ply_trigger_t            *input_trigger;
        ply_trigger_t            *leds_changed_trigger;
        ply_trigger_t            *disconnect_trigger;
        ply_fd_watch_t           *fd_watch;

        struct xkb_context       *xkb_context;
        struct xkb_keymap        *keymap;
        struct xkb_state         *keyboard_state;
        struct xkb_compose_table *compose_table;
        struct xkb_compose_state *compose_state;

        struct libevdev          *dev;

        uint32_t                  kernel_has_vts : 1;
        uint32_t                  leds_state_invalid : 1;
};

static bool
apply_compose_sequence_to_input_buffer (ply_input_device_t *input_device,
                                        xkb_keysym_t        input_symbol,
                                        ply_buffer_t       *input_buffer)
{
        enum xkb_compose_feed_result compose_feed_result;
        enum xkb_compose_status compose_status;

        if (input_device->compose_state == NULL)
                return false;

        if (input_symbol == XKB_KEY_NoSymbol)
                return false;

        compose_feed_result = xkb_compose_state_feed (input_device->compose_state, input_symbol);

        if (compose_feed_result == XKB_COMPOSE_FEED_IGNORED)
                return false;

        assert (compose_feed_result == XKB_COMPOSE_FEED_ACCEPTED);

        compose_status = xkb_compose_state_get_status (input_device->compose_state);

        if (compose_status == XKB_COMPOSE_NOTHING)
                return false;

        if (compose_status == XKB_COMPOSE_COMPOSED) {
                xkb_keysym_t output_symbol;
                ssize_t character_size;
                char character_buf[PLY_XKB_KEYSYM_TO_UTF8_BUFFER_SIZE] = "";

                output_symbol = xkb_compose_state_get_one_sym (input_device->compose_state);
                character_size = xkb_keysym_to_utf8 (output_symbol, character_buf, sizeof(character_buf));

                if (character_size > 0)
                        ply_buffer_append_bytes (input_buffer, character_buf, character_size);
        } else {
                /* Either we're mid compose sequence (XKB_COMPOSE_COMPOSING) or the compose sequence has
                 * been aborted (XKB_COMPOSE_CANCELLED). Either way, we shouldn't append anything to the
                 * input buffer
                 */
        }

        return true;
}

static void
apply_key_to_input_buffer (ply_input_device_t *input_device,
                           xkb_keysym_t        symbol,
                           int                 keycode,
                           ply_buffer_t       *input_buffer)
{
        ssize_t character_size;
        bool was_compose_sequence;

        was_compose_sequence = apply_compose_sequence_to_input_buffer (input_device, symbol, input_buffer);

        if (was_compose_sequence)
                return;

        switch (symbol) {
        case XKB_KEY_Escape:
                ply_buffer_append_bytes (input_buffer, "\033", 1);
                break;
        case XKB_KEY_KP_Enter:
        case XKB_KEY_Return:
                ply_buffer_append_bytes (input_buffer, "\n", 1);
                break;
        case XKB_KEY_BackSpace:
                ply_buffer_append_bytes (input_buffer, "\177", 1);
                break;
        case XKB_KEY_NoSymbol:
                break;
        default:
                character_size = xkb_state_key_get_utf8 (input_device->keyboard_state, keycode, NULL, 0);

                if (character_size > 0) {
                        char character_buf[character_size + 1];

                        character_size = xkb_state_key_get_utf8 (input_device->keyboard_state, keycode, character_buf, sizeof(character_buf));

                        assert (character_size + 1 == sizeof(character_buf));

                        ply_buffer_append_bytes (input_buffer, character_buf, character_size);
                } else {
                        xkb_keysym_t keysym;

                        if (!input_device->kernel_has_vts)
                                break;

                        keysym = xkb_state_key_get_one_sym (input_device->keyboard_state, keycode);
                        if (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
                                int vt_number = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
                                ply_change_to_vt (vt_number);
                        }
                }
                break;
        }
}

static void
on_input (ply_input_device_t *input_device)
{
        struct input_event ev;
        int rc;
        unsigned int flags;
        ply_buffer_t *input_buffer = ply_buffer_new ();
        static enum { PLY_INPUT_DEVICE_DEBUG_UNKNOWN = -1,
                      PLY_INPUT_DEVICE_DEBUG_DISABLED,
                      PLY_INPUT_DEVICE_DEBUG_ENABLED } debug_key_events = PLY_INPUT_DEVICE_DEBUG_UNKNOWN;

        if (debug_key_events == PLY_INPUT_DEVICE_DEBUG_UNKNOWN) {
                if (ply_kernel_command_line_has_argument ("plymouth.debug-input-devices")) {
                        ply_trace ("WARNING: Input device debugging enabled. Passwords will be in log!");
                        debug_key_events = PLY_INPUT_DEVICE_DEBUG_ENABLED;
                } else {
                        ply_trace ("Input device debugging disabled");
                        debug_key_events = PLY_INPUT_DEVICE_DEBUG_DISABLED;
                }
        }

        flags = LIBEVDEV_READ_FLAG_NORMAL;
        for (;;) {
                ply_key_direction_t key_state;
                enum xkb_key_direction xkb_key_direction;
                xkb_keycode_t keycode;
                xkb_keysym_t symbol;
                enum xkb_state_component updated_state;

                rc = libevdev_next_event (input_device->dev, flags, &ev);

                if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                        ply_trace ("Input device %s has backlog of events", libevdev_get_name (input_device->dev));
                        flags = LIBEVDEV_READ_FLAG_SYNC;
                        continue;
                } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS && flags == LIBEVDEV_READ_FLAG_SYNC) {
                        ply_trace ("Input device %s event backlog has been processed", libevdev_get_name (input_device->dev));
                        flags = LIBEVDEV_READ_FLAG_NORMAL;
                } else if (rc != LIBEVDEV_READ_STATUS_SUCCESS) {
                        break;
                }

                if (debug_key_events == PLY_INPUT_DEVICE_DEBUG_ENABLED) {
                        ply_trace ("Received event from input device %s, type=%s code=%s value=%d.",
                                   libevdev_get_name (input_device->dev),
                                   libevdev_event_type_get_name (ev.type),
                                   libevdev_event_code_get_name (ev.type, ev.code),
                                   ev.value);
                }

                if (!libevdev_event_is_type (&ev, EV_KEY))
                        continue;

                /* According to `https://docs.kernel.org/input/event-codes.html#ev-key`:
                 * if ev.value = 2, then the key is being held down. libxkbcommon doesn't appear to define this
                 * if ev.value = 1, then key was pressed down
                 * if ev.value = 0, then key was released up
                 */
                switch (ev.value) {
                case 0:
                        key_state = PLY_KEY_UP;
                        xkb_key_direction = XKB_KEY_UP;
                        break;

                case 1:
                        key_state = PLY_KEY_DOWN;
                        xkb_key_direction = XKB_KEY_DOWN;
                        break;

                case 2:
                        key_state = PLY_KEY_HELD;
                        xkb_key_direction = XKB_KEY_UP;
                        break;
                }

                /* According to
                 * `https://xkbcommon.org/doc/current/xkbcommon_8h.html#ac29aee92124c08d1953910ab28ee1997`
                 *  A xkb keycode = linux evdev code + 8
                 */
                keycode = (xkb_keycode_t) (ev.code + 8);

                symbol = xkb_state_key_get_one_sym (input_device->keyboard_state, keycode);

                if (key_state != PLY_KEY_HELD) {
                        updated_state = xkb_state_update_key (input_device->keyboard_state, keycode, xkb_key_direction);

                        if ((updated_state & XKB_STATE_LEDS) != 0) {
                                ply_trace ("Keyboard indicator lights need update");
                                input_device->leds_state_invalid = true;
                                ply_trigger_pull (input_device->leds_changed_trigger, input_device);
                        }
                }

                /* If the key is repeating, or is being pressed down */
                if (key_state == PLY_KEY_HELD || key_state == PLY_KEY_DOWN)
                        apply_key_to_input_buffer (input_device, symbol, keycode, input_buffer);
        }
        if (rc != -EAGAIN) {
                ply_error ("There was an error reading events for device '%s': %s",
                           input_device->path, strerror (-rc));
                goto error;
        }
        if (ply_buffer_get_size (input_buffer) != 0) {
                ply_trigger_pull (input_device->input_trigger, ply_buffer_get_bytes (input_buffer));
        }

error:
        ply_buffer_free (input_buffer);
}

static void
on_disconnect (ply_input_device_t *input_device)
{
        ply_trace ("Input disconnected: %s (%s)", libevdev_get_name (input_device->dev),
                   input_device->path);
        ply_trigger_pull (input_device->disconnect_trigger, input_device);

        ply_input_device_free (input_device);
}

void
ply_input_device_set_disconnect_handler (ply_input_device_t                   *input_device,
                                         ply_input_device_disconnect_handler_t callback,
                                         void                                 *user_data)
{
        ply_trigger_add_handler (input_device->disconnect_trigger, (ply_trigger_handler_t) callback, user_data);
}

ply_input_device_t *
ply_input_device_open (struct xkb_context *xkb_context,
                       struct xkb_keymap  *xkb_keymap,
                       const char         *path)
{
        int error;
        const char *locale;

        /* Look up the preferred locale, falling back to "C" as default */
        if (!(locale = getenv ("LC_ALL")))
                if (!(locale = getenv ("LC_CTYPE")))
                        if (!(locale = getenv ("LANG")))
                                locale = "C";

        ply_input_device_t *input_device = calloc (1, sizeof(ply_input_device_t));

        if (input_device == NULL) {
                ply_error ("Out of memory");
                return NULL;
        }

        input_device->disconnect_trigger = ply_trigger_new (NULL);
        input_device->path = strdup (path);
        input_device->input_trigger = ply_trigger_new (NULL);
        ply_trigger_set_instance (input_device->input_trigger, input_device);

        input_device->leds_changed_trigger = ply_trigger_new (NULL);
        input_device->loop = ply_event_loop_get_default ();

        input_device->fd = open (path, O_RDWR | O_NONBLOCK);

        if (input_device->fd < 0) {
                ply_error ("Failed to open input device \"%s\"", path);
                goto error;
        }
        input_device->dev = libevdev_new ();
        error = libevdev_set_fd (input_device->dev, input_device->fd);
        if (error != 0) {
                ply_error ("Failed to set fd for device \"%s\": %s", path, strerror (-error));
                goto error;
        }

        input_device->fd_watch = ply_event_loop_watch_fd (
                input_device->loop, input_device->fd, PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                (ply_event_handler_t) on_input, (ply_event_handler_t) on_disconnect,
                input_device);

        input_device->keymap = xkb_keymap_ref (xkb_keymap);
        input_device->keyboard_state = xkb_state_new (input_device->keymap);
        if (input_device->keyboard_state == NULL) {
                ply_error ("Failed to initialize input device \"%s\" keyboard_state", path);
                goto error;
        }
        input_device->compose_table = xkb_compose_table_new_from_locale (xkb_context, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (input_device->compose_table)
                input_device->compose_state = xkb_compose_state_new (input_device->compose_table, XKB_COMPOSE_STATE_NO_FLAGS);

        input_device->kernel_has_vts = ply_character_device_exists ("/dev/tty0");

        return input_device;

error:
        ply_input_device_free (input_device);
        return NULL;
}

void
ply_input_device_watch_for_input (ply_input_device_t                     *input_device,
                                  ply_input_device_input_handler_t        input_callback,
                                  ply_input_device_leds_changed_handler_t leds_changed_callback,
                                  void                                   *user_data)
{
        ply_trigger_add_instance_handler (input_device->input_trigger, (ply_trigger_instance_handler_t) input_callback, user_data);
        ply_trigger_add_handler (input_device->leds_changed_trigger, (ply_trigger_handler_t) leds_changed_callback, user_data);
}

void
ply_input_device_stop_watching_for_input (ply_input_device_t                     *input_device,
                                          ply_input_device_input_handler_t        input_callback,
                                          ply_input_device_leds_changed_handler_t leds_changed_callback,
                                          void                                   *user_data)
{
        ply_trigger_remove_instance_handler (input_device->input_trigger, (ply_trigger_instance_handler_t) input_callback, user_data);
        ply_trigger_remove_handler (input_device->leds_changed_trigger, (ply_trigger_handler_t) leds_changed_callback, user_data);
}

int
ply_input_device_is_keyboard (ply_input_device_t *input_device)
{
        return libevdev_has_event_type (input_device->dev, EV_KEY);
}

int
ply_input_device_is_keyboard_with_leds (ply_input_device_t *input_device)
{
        return (libevdev_has_event_type (input_device->dev, EV_KEY)) &&
               (libevdev_has_event_type (input_device->dev, EV_LED));
}

const char *
ply_input_device_get_name (ply_input_device_t *input_device)
{
        return libevdev_get_name (input_device->dev);
}

const char *
ply_input_device_get_path (ply_input_device_t *input_device)
{
        return input_device->path;
}

/*
 * from libinput's evdev_device_led_update and Weston's weston_keyboard_set_locks
 */
void
ply_input_device_set_state (ply_input_device_t       *input_device,
                            ply_xkb_keyboard_state_t *xkb_state)
{
        static struct
        {
                ply_led_t      ply_led;
                int            evdev;
                xkb_mod_mask_t status;
        } map[] = {
                { PLY_LED_NUM_LOCK,    LED_NUML,    false },
                { PLY_LED_CAPS_LOCK,   LED_CAPSL,   false },
                { PLY_LED_SCROLL_LOCK, LED_SCROLLL, false },
        };
        struct input_event ev[PLY_NUMBER_OF_ELEMENTS (map) + 1];
        xkb_mod_mask_t mods_depressed, mods_latched, mods_locked, group;
        unsigned int i;

        mods_depressed = xkb_state_serialize_mods (input_device->keyboard_state,
                                                   XKB_STATE_DEPRESSED);
        mods_latched = xkb_state_serialize_mods (input_device->keyboard_state,
                                                 XKB_STATE_LATCHED);
        mods_locked = xkb_state_serialize_mods (input_device->keyboard_state,
                                                XKB_STATE_LOCKED);
        group = xkb_state_serialize_group (input_device->keyboard_state,
                                           XKB_STATE_EFFECTIVE);

        if (mods_depressed == xkb_state->mods_depressed &&
            mods_latched == xkb_state->mods_latched &&
            mods_locked == xkb_state->mods_locked &&
            group == xkb_state->group &&
            !input_device->leds_state_invalid)
                return;

        mods_depressed = xkb_state->mods_depressed;
        mods_latched = xkb_state->mods_latched;
        mods_locked = xkb_state->mods_locked;
        group = xkb_state->group;

        xkb_state_update_mask (input_device->keyboard_state,
                               mods_depressed,
                               mods_latched,
                               mods_locked,
                               0,
                               0,
                               group);

        map[LED_NUML].status = xkb_state_led_name_is_active (input_device->keyboard_state, XKB_LED_NAME_NUM);
        map[LED_CAPSL].status = xkb_state_led_name_is_active (input_device->keyboard_state, XKB_LED_NAME_CAPS);
        map[LED_SCROLLL].status = xkb_state_led_name_is_active (input_device->keyboard_state, XKB_LED_NAME_SCROLL);

        memset (ev, 0, sizeof(ev));
        for (i = 0; i < PLY_NUMBER_OF_ELEMENTS (map); i++) {
                ev[i].type = EV_LED;
                ev[i].code = map[i].evdev;
                ev[i].value = map[i].status;
        }
        ev[i].type = EV_SYN;
        ev[i].code = SYN_REPORT;

        ply_write (input_device->fd, ev, sizeof(ev));
        input_device->leds_state_invalid = false;
}

ply_xkb_keyboard_state_t
*ply_input_device_get_state (ply_input_device_t *input_device)
{
        ply_xkb_keyboard_state_t *xkb_state = calloc (1, sizeof(ply_xkb_keyboard_state_t));

        xkb_state->mods_depressed = xkb_state_serialize_mods (input_device->keyboard_state,
                                                              XKB_STATE_DEPRESSED);
        xkb_state->mods_latched = xkb_state_serialize_mods (input_device->keyboard_state,
                                                            XKB_STATE_LATCHED);
        xkb_state->mods_locked = xkb_state_serialize_mods (input_device->keyboard_state,
                                                           XKB_STATE_LOCKED);
        xkb_state->group = xkb_state_serialize_group (input_device->keyboard_state,
                                                      XKB_STATE_EFFECTIVE);

        return xkb_state;
}

bool
ply_input_device_get_capslock_state (ply_input_device_t *input_device)
{
        return xkb_state_led_name_is_active (input_device->keyboard_state, XKB_LED_NAME_CAPS);
}

const char *
ply_input_device_get_keymap (ply_input_device_t *input_device)
{
        xkb_layout_index_t num_indices = xkb_keymap_num_layouts (input_device->keymap);
        ply_trace ("xkb layout has %d groups", num_indices);
        if (num_indices == 0) {
                return NULL;
        }
        /* According to xkbcommon docs:
         * (https://xkbcommon.org/doc/current/xkbcommon_8h.html#ad37512642806c55955e1cd5a30efcc39)
         *
         * Each layout is not required to have a name, and the names are not
         * guaranteed to be unique (though they are usually provided and
         * unique). Therefore, it is not safe to use the name as a unique
         * identifier for a layout. Layout names are case-sensitive.
         *
         * Layout names are specified in the layout's definition, for example "English
         * (US)". These are different from the (conventionally) short names
         * which are used to locate the layout, for example "us" or "us(intl)".
         * These names are not present in a compiled keymap.
         *
         * This string shouldn't be used as a unique indentifier for a keymap
         */
        return xkb_keymap_layout_get_name (input_device->keymap, num_indices - 1);
}

int
ply_input_device_get_fd (ply_input_device_t *input_device)
{
        return input_device->fd;
}

void
ply_input_device_free (ply_input_device_t *input_device)
{
        if (input_device == NULL)
                return;

        if (input_device->xkb_context)
                xkb_context_unref (input_device->xkb_context);

        if (input_device->keyboard_state)
                xkb_state_unref (input_device->keyboard_state);

        if (input_device->keymap)
                xkb_keymap_unref (input_device->keymap);

        if (input_device->compose_state)
                xkb_compose_state_unref (input_device->compose_state);

        if (input_device->compose_table)
                xkb_compose_table_unref (input_device->compose_table);

        if (input_device->dev)
                libevdev_free (input_device->dev);

        ply_trigger_free (input_device->input_trigger);
        ply_trigger_free (input_device->leds_changed_trigger);
        ply_trigger_free (input_device->disconnect_trigger);

        free (input_device->path);

        if (input_device->fd_watch != NULL)
                ply_event_loop_stop_watching_fd (input_device->loop, input_device->fd_watch);

        close (input_device->fd);

        free (input_device);
}
