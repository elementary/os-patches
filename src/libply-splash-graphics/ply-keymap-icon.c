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
#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ply-keymap-icon.h"
#include "ply-keymap-metadata.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-utils.h"

#define SPACING 10

struct _ply_keymap_icon
{
        ply_pixel_display_t *display;
        char                *image_dir;
        ply_pixel_buffer_t  *icon_buffer;
        ply_pixel_buffer_t  *keymap_buffer;
        int                  keymap_offset;
        int                  keymap_width;
        long                 x, y;
        unsigned long        width, height;
        bool                 is_hidden;
};

/* The keymap name we got from the renderer may contain a variant, e.g. it may
 * be "us-intl" while our pre-generated text only contains "us", the code below
 * does the same keymap name simplification as the keymap-render.py script.
 */
static char *
ply_keymap_normalize_keymap (const char *keymap_with_variant)
{
        const char *prefix[] = { "sun", "mac", NULL };
        int i, length;

        /* Special case for dvorak layouts */
        if (strstr (keymap_with_variant, "dvorak"))
                return strdup ("dvorak");

        /* Check for and skip sun / mac prefixes */
        for (i = 0; prefix[i]; i++) {
                if (strncmp (keymap_with_variant, prefix[i], strlen (prefix[i])) == 0) {
                        length = strcspn (keymap_with_variant, "_-.");
                        if (keymap_with_variant[length] != '\0')
                                keymap_with_variant += length + 1;
                        break;
                }
        }

        /* Remove the keymap-variant info after the base keymap name */
        length = strcspn (keymap_with_variant, "_-.");
        return strndup (keymap_with_variant, length);
}

static void
ply_keymap_icon_fill_keymap_info (ply_keymap_icon_t *keymap_icon)
{
        const char *keymap_with_variant;
        ply_renderer_t *renderer;
        char *keymap;
        int i;

        keymap_icon->keymap_offset = -1;

        renderer = ply_pixel_display_get_renderer (keymap_icon->display);
        keymap_with_variant = ply_renderer_get_keymap (renderer);
        if (!keymap_with_variant)
                return;

        keymap = ply_keymap_normalize_keymap (keymap_with_variant);

        for (i = 0; ply_keymap_metadata[i].name; i++) {
                if (strcmp (ply_keymap_metadata[i].name, keymap) == 0) {
                        keymap_icon->keymap_offset = ply_keymap_metadata[i].offset;
                        keymap_icon->keymap_width = ply_keymap_metadata[i].width;
                        break;
                }
        }

        if (keymap_icon->keymap_offset == -1)
                ply_trace("Error no pre-rendered text for '%s' keymap", keymap);

        free (keymap);
}

ply_keymap_icon_t *
ply_keymap_icon_new (ply_pixel_display_t *display,
                     const char          *image_dir)
{
        ply_keymap_icon_t *keymap_icon;

        keymap_icon = calloc (1, sizeof(ply_keymap_icon_t));
        keymap_icon->display = display;
        keymap_icon->image_dir = strdup (image_dir);
        keymap_icon->is_hidden = true;

        ply_keymap_icon_fill_keymap_info (keymap_icon);

        return keymap_icon;
}

void
ply_keymap_icon_free (ply_keymap_icon_t *keymap_icon)
{
        if (keymap_icon == NULL)
                return;

        ply_pixel_buffer_free (keymap_icon->icon_buffer);
        ply_pixel_buffer_free (keymap_icon->keymap_buffer);

        free (keymap_icon->image_dir);
        free (keymap_icon);
}

bool
ply_keymap_icon_load (ply_keymap_icon_t *keymap_icon)
{
        ply_image_t *keymap_image = NULL;
        ply_image_t *icon_image;
        char *filename;
        bool success;

        /* Bail if we did not find the keymap metadata */
        if (keymap_icon->keymap_offset == -1)
                return false;

        if (keymap_icon->icon_buffer)
                return true;

        asprintf (&filename, "%s/keyboard.png", keymap_icon->image_dir);
        icon_image = ply_image_new (filename);
        success = ply_image_load (icon_image);
        ply_trace("loading '%s': %s", filename, success ? "success" : "failed");
        free (filename);

        if (success) {
                asprintf (&filename, "%s/keymap-render.png", keymap_icon->image_dir);
                keymap_image = ply_image_new (filename);
                success = ply_image_load (keymap_image);
                ply_trace("loading '%s': %s", filename, success ? "success" : "failed");
                free (filename);
        }

        if (!success) {
                ply_image_free (keymap_image);
                ply_image_free (icon_image);
                return false;
        }

        keymap_icon->icon_buffer = ply_image_convert_to_pixel_buffer (icon_image);
        keymap_icon->keymap_buffer = ply_image_convert_to_pixel_buffer (keymap_image);

        keymap_icon->width =
                ply_pixel_buffer_get_width (keymap_icon->icon_buffer) +
                SPACING + keymap_icon->keymap_width;
        keymap_icon->height = MAX(
                ply_pixel_buffer_get_height (keymap_icon->icon_buffer),
                ply_pixel_buffer_get_height (keymap_icon->keymap_buffer));

        return true;
}

bool
ply_keymap_icon_show (ply_keymap_icon_t *keymap_icon,
                      long               x,
                      long               y)
{
        if (!keymap_icon->icon_buffer) {
                ply_trace ("keymap_icon not loaded, can not start");
                return false;
        }

        keymap_icon->x = x;
        keymap_icon->y = y;
        keymap_icon->is_hidden = false;

        ply_pixel_display_draw_area (keymap_icon->display,
                                     keymap_icon->x, keymap_icon->y,
                                     keymap_icon->width,
                                     keymap_icon->height);
        return true;
}

void
ply_keymap_icon_hide (ply_keymap_icon_t *keymap_icon)
{
        if (keymap_icon->is_hidden)
                return;

        keymap_icon->is_hidden = true;

        ply_pixel_display_draw_area (keymap_icon->display,
                                     keymap_icon->x, keymap_icon->y,
                                     keymap_icon->width,
                                     keymap_icon->height);
}

void
ply_keymap_icon_draw_area (ply_keymap_icon_t  *keymap_icon,
                           ply_pixel_buffer_t *buffer,
                           long                x,
                           long                y,
                           unsigned long       width,
                           unsigned long       height)
{
        ply_rectangle_t icon_area, keymap_area;

        if (keymap_icon->is_hidden)
                return;

        /* Draw keyboard icon */
        ply_pixel_buffer_get_size (keymap_icon->icon_buffer, &icon_area);
        icon_area.x = keymap_icon->x;
        icon_area.y = keymap_icon->y +
                      (keymap_icon->height - icon_area.height) / 2;

        ply_pixel_buffer_fill_with_buffer (buffer, keymap_icon->icon_buffer,
                                           icon_area.x, icon_area.y);

        /* Draw pre-rendered keyboard layout text */
        keymap_area.width = keymap_icon->keymap_width;
        keymap_area.height =
                ply_pixel_buffer_get_height (keymap_icon->keymap_buffer);
        keymap_area.x = keymap_icon->x + icon_area.width + SPACING;
        keymap_area.y = keymap_icon->y +
                        (keymap_icon->height - keymap_area.height) / 2;

        /* Draw keyboard layout text, shift the pre-rendered image to the left
         * so that the text we want lines out at the place we want it and set
         * the area we want to draw to as clip-area to only draw what we want.
         */
        ply_pixel_buffer_fill_with_buffer_with_clip (
                buffer,
                keymap_icon->keymap_buffer,
                keymap_area.x - keymap_icon->keymap_offset,
                keymap_area.y,
                &keymap_area);
}

unsigned long
ply_keymap_icon_get_width (ply_keymap_icon_t *keymap_icon)
{
        return keymap_icon->width;
}

unsigned long
ply_keymap_icon_get_height (ply_keymap_icon_t *keymap_icon)
{
        return keymap_icon->height;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
