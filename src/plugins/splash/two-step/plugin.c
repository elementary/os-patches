/*
 *
 * Copyright (C) 2009-2019 Red Hat, Inc.
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
 * Written by: William Jon McCann, Hans de Goede <hdegoede@redhat.com>
 *
 */
#include "config.h"

#include <assert.h>
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

#include "ply-boot-splash-plugin.h"
#include "ply-buffer.h"
#include "ply-capslock-icon.h"
#include "ply-entry.h"
#include "ply-event-loop.h"
#include "ply-label.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-key-file.h"
#include "ply-keymap-icon.h"
#include "ply-trigger.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-utils.h"
#include "ply-i18n.h"

#include "ply-animation.h"
#include "ply-progress-animation.h"
#include "ply-throbber.h"
#include "ply-progress-bar.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 30
#endif

#ifndef SHOW_ANIMATION_FRACTION
#define SHOW_ANIMATION_FRACTION 0.9
#endif

#define PROGRESS_BAR_WIDTH  400
#define PROGRESS_BAR_HEIGHT 5

#define BGRT_STATUS_ORIENTATION_OFFSET_0    (0 << 1)
#define BGRT_STATUS_ORIENTATION_OFFSET_90   (1 << 1)
#define BGRT_STATUS_ORIENTATION_OFFSET_180  (2 << 1)
#define BGRT_STATUS_ORIENTATION_OFFSET_270  (3 << 1)
#define BGRT_STATUS_ORIENTATION_OFFSET_MASK (3 << 1)

typedef enum
{
        PLY_BOOT_SPLASH_DISPLAY_NORMAL,
        PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY,
        PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY
} ply_boot_splash_display_type_t;

typedef enum
{
        PROGRESS_FUNCTION_TYPE_WWOODS,
        PROGRESS_FUNCTION_TYPE_LINEAR,
} progress_function_t;

typedef struct
{
        ply_boot_splash_plugin_t *plugin;
        ply_pixel_display_t      *display;
        ply_entry_t              *entry;
        ply_keymap_icon_t        *keymap_icon;
        ply_capslock_icon_t      *capslock_icon;
        ply_animation_t          *end_animation;
        ply_progress_animation_t *progress_animation;
        ply_progress_bar_t       *progress_bar;
        ply_throbber_t           *throbber;
        ply_label_t              *label;
        ply_label_t              *message_label;
        ply_label_t              *title_label;
        ply_label_t              *subtitle_label;
        ply_rectangle_t           box_area, lock_area, watermark_area, dialog_area;
        ply_trigger_t            *end_trigger;
        ply_pixel_buffer_t       *background_buffer;
        int                       animation_bottom;
} view_t;

typedef struct
{
        bool                      suppress_messages;
        bool                      progress_bar_show_percent_complete;
        bool                      use_progress_bar;
        bool                      use_animation;
        bool                      use_end_animation;
        bool                      use_firmware_background;
        char                     *title;
        char                     *subtitle;
} mode_settings_t;

struct _ply_boot_splash_plugin
{
        ply_event_loop_t                   *loop;
        ply_boot_splash_mode_t              mode;
        mode_settings_t                     mode_settings[PLY_BOOT_SPLASH_MODE_COUNT];
        char                               *font;
        ply_image_t                        *lock_image;
        ply_image_t                        *box_image;
        ply_image_t                        *corner_image;
        ply_image_t                        *header_image;
        ply_image_t                        *background_tile_image;
        ply_image_t                        *background_bgrt_image;
        ply_image_t                        *background_bgrt_fallback_image;
        ply_image_t                        *watermark_image;
        ply_list_t                         *views;

        ply_boot_splash_display_type_t      state;

        double                              dialog_horizontal_alignment;
        double                              dialog_vertical_alignment;

        double                              title_horizontal_alignment;
        double                              title_vertical_alignment;
        char                               *title_font;

        double                              watermark_horizontal_alignment;
        double                              watermark_vertical_alignment;

        double                              animation_horizontal_alignment;
        double                              animation_vertical_alignment;
        char                               *animation_dir;

        ply_progress_animation_transition_t transition;
        double                              transition_duration;

        uint32_t                            background_start_color;
        uint32_t                            background_end_color;
        int                                 background_bgrt_raw_width;
        int                                 background_bgrt_raw_height;

        double                              progress_bar_horizontal_alignment;
        double                              progress_bar_vertical_alignment;
        long                                progress_bar_width;
        long                                progress_bar_height;
        uint32_t                            progress_bar_bg_color;
        uint32_t                            progress_bar_fg_color;

        progress_function_t                 progress_function;

        ply_trigger_t                      *idle_trigger;
        ply_trigger_t                      *stop_trigger;

        uint32_t                            root_is_mounted : 1;
        uint32_t                            is_visible : 1;
        uint32_t                            is_animating : 1;
        uint32_t                            is_idle : 1;
        uint32_t                            use_firmware_background : 1;
        uint32_t                            dialog_clears_firmware_background : 1;
        uint32_t                            message_below_animation : 1;
};

ply_boot_splash_plugin_interface_t *ply_boot_splash_plugin_get_interface (void);

static void stop_animation (ply_boot_splash_plugin_t *plugin);
static void detach_from_event_loop (ply_boot_splash_plugin_t *plugin);
static void display_message (ply_boot_splash_plugin_t *plugin,
                             const char               *message);
static void become_idle (ply_boot_splash_plugin_t *plugin,
                         ply_trigger_t            *idle_trigger);
static void view_show_message (view_t *view, const char *message);

static view_t *
view_new (ply_boot_splash_plugin_t *plugin,
          ply_pixel_display_t      *display)
{
        view_t *view;

        view = calloc (1, sizeof(view_t));
        view->plugin = plugin;
        view->display = display;

        view->entry = ply_entry_new (plugin->animation_dir);
        view->keymap_icon = ply_keymap_icon_new (display, plugin->animation_dir);
        view->capslock_icon = ply_capslock_icon_new (plugin->animation_dir);
        view->progress_animation = ply_progress_animation_new (plugin->animation_dir,
                                                               "progress-");
        ply_progress_animation_set_transition (view->progress_animation,
                                               plugin->transition,
                                               plugin->transition_duration);

        view->progress_bar = ply_progress_bar_new ();
        ply_progress_bar_set_colors (view->progress_bar,
                                     plugin->progress_bar_fg_color,
                                     plugin->progress_bar_bg_color);

        view->throbber = ply_throbber_new (plugin->animation_dir,
                                           "throbber-");

        view->label = ply_label_new ();
        ply_label_set_font (view->label, plugin->font);

        view->message_label = ply_label_new ();
        ply_label_set_font (view->message_label, plugin->font);

        view->title_label = ply_label_new ();
        ply_label_set_font (view->title_label, plugin->title_font);

        view->subtitle_label = ply_label_new ();
        ply_label_set_font (view->subtitle_label, plugin->font);

        return view;
}

static void
view_free (view_t *view)
{
        ply_entry_free (view->entry);
        ply_keymap_icon_free (view->keymap_icon);
        ply_capslock_icon_free (view->capslock_icon);
        ply_animation_free (view->end_animation);
        ply_progress_animation_free (view->progress_animation);
        ply_progress_bar_free (view->progress_bar);
        ply_throbber_free (view->throbber);
        ply_label_free (view->label);
        ply_label_free (view->message_label);
        ply_label_free (view->title_label);
        ply_label_free (view->subtitle_label);

        if (view->background_buffer != NULL)
                ply_pixel_buffer_free (view->background_buffer);

        free (view);
}

static void
view_load_end_animation (view_t *view)
{
        ply_boot_splash_plugin_t *plugin = view->plugin;
        const char *animation_prefix;

        if (!plugin->mode_settings[plugin->mode].use_end_animation)
                return;

        ply_trace ("loading animation");

        switch (plugin->mode) {
        case PLY_BOOT_SPLASH_MODE_BOOT_UP:
        case PLY_BOOT_SPLASH_MODE_UPDATES:
        case PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE:
        case PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE:
                animation_prefix = "startup-animation-";
                break;
        case PLY_BOOT_SPLASH_MODE_SHUTDOWN:
        case PLY_BOOT_SPLASH_MODE_REBOOT:
                animation_prefix = "shutdown-animation-";
                break;
        case PLY_BOOT_SPLASH_MODE_INVALID:
        default:
                ply_trace ("unexpected splash mode 0x%x\n", plugin->mode);
                return;
        }

        ply_trace ("trying prefix: %s", animation_prefix);
        view->end_animation = ply_animation_new (plugin->animation_dir,
                                                 animation_prefix);

        if (ply_animation_load (view->end_animation))
                return;
        ply_animation_free (view->end_animation);

        ply_trace ("now trying more general prefix: animation-");
        view->end_animation = ply_animation_new (plugin->animation_dir,
                                                 "animation-");
        if (ply_animation_load (view->end_animation))
                return;
        ply_animation_free (view->end_animation);

        ply_trace ("now trying old compat prefix: throbber-");
        view->end_animation = ply_animation_new (plugin->animation_dir,
                                                 "throbber-");
        if (ply_animation_load (view->end_animation)) {
                /* files named throbber- are for end animation, so
                 * there's no throbber */
                ply_throbber_free (view->throbber);
                view->throbber = NULL;
                return;
        }

        ply_trace ("optional animation didn't load");
        ply_animation_free (view->end_animation);
        view->end_animation = NULL;
        plugin->mode_settings[plugin->mode].use_end_animation = false;
}

static bool
get_bgrt_sysfs_info(int *x_offset, int *y_offset,
                    ply_pixel_buffer_rotation_t *rotation)
{
        bool ret = false;
        char buf[64];
        int status;
        FILE *f;

        f = fopen("/sys/firmware/acpi/bgrt/status", "r");
        if (!f)
                return false;

        if (!fgets(buf, sizeof(buf), f))
                goto out;

        if (sscanf(buf, "%d", &status) != 1)
                goto out;

        fclose(f);

        switch (status & BGRT_STATUS_ORIENTATION_OFFSET_MASK) {
        case BGRT_STATUS_ORIENTATION_OFFSET_0:
                *rotation = PLY_PIXEL_BUFFER_ROTATE_UPRIGHT;
                break;
        case BGRT_STATUS_ORIENTATION_OFFSET_90:
                *rotation = PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE;
                break;
        case BGRT_STATUS_ORIENTATION_OFFSET_180:
                *rotation = PLY_PIXEL_BUFFER_ROTATE_UPSIDE_DOWN;
                break;
        case BGRT_STATUS_ORIENTATION_OFFSET_270:
                *rotation = PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE;
                break;
        }

        f = fopen("/sys/firmware/acpi/bgrt/xoffset", "r");
        if (!f)
                return false;

        if (!fgets(buf, sizeof(buf), f))
                goto out;

        if (sscanf(buf, "%d", x_offset) != 1)
                goto out;

        fclose(f);

        f = fopen("/sys/firmware/acpi/bgrt/yoffset", "r");
        if (!f)
                return false;

        if (!fgets(buf, sizeof(buf), f))
                goto out;

        if (sscanf(buf, "%d", y_offset) != 1)
                goto out;

        ret = true;
out:
        fclose(f);
        return ret;
}

/* The Microsoft boot logo spec says that the logo must use a black background
 * and have its center at 38.2% from the screen's top (golden ratio).
 * We reproduce this exactly here so that we get a background which is an exact
 * match of the firmware's boot splash.
 * At the time of writing this comment this is documented in a document called
 * "Boot screen components" which is available here:
 * https://docs.microsoft.com/en-us/windows-hardware/drivers/bringup/boot-screen-components
 * Note that we normally do not use the firmware reported x and y-offset as
 * that is based on the EFI fb resolution which may not be the native
 * resolution of the screen (esp. when using multiple heads).
 */
static void
view_set_bgrt_background (view_t *view)
{
        ply_pixel_buffer_rotation_t panel_rotation = PLY_PIXEL_BUFFER_ROTATE_UPRIGHT;
        ply_pixel_buffer_rotation_t bgrt_rotation = PLY_PIXEL_BUFFER_ROTATE_UPRIGHT;
        int x_offset, y_offset, sysfs_x_offset, sysfs_y_offset, width, height;
        int panel_width = 0, panel_height = 0, panel_scale = 1;
        int screen_width, screen_height, screen_scale;
        ply_pixel_buffer_t *bgrt_buffer;
        bool have_panel_props;

        if (!view->plugin->background_bgrt_image)
                return;

        if (!get_bgrt_sysfs_info(&sysfs_x_offset, &sysfs_y_offset,
                                 &bgrt_rotation)) {
                ply_trace ("get bgrt sysfs info failed");
                return;
        }

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);
        screen_scale = ply_pixel_display_get_device_scale (view->display);

        bgrt_buffer = ply_image_get_buffer (view->plugin->background_bgrt_image);

        have_panel_props = ply_renderer_get_panel_properties (ply_pixel_display_get_renderer (view->display),
                                                              &panel_width, &panel_height,
                                                              &panel_rotation, &panel_scale);

        /*
         * Some buggy Lenovo 2-in-1s with a 90 degree rotated panel, behave as
         * if the panel is mounted up-right / not rotated at all. These devices
         * have a buggy efifb size (landscape resolution instead of the actual
         * portrait resolution of the panel), this gets fixed-up by the kernel.
         * These buggy devices also do not pre-rotate the bgrt_image nor do
         * they set the ACPI-6.2 rotation status-bits. We can detect this by
         * checking that the bgrt_image is perfectly centered horizontally
         * when we use the panel's height as the width.
         */
        if (have_panel_props &&
            (panel_rotation == PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE ||
             panel_rotation == PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE) &&
            (panel_width  - view->plugin->background_bgrt_raw_width) / 2 != sysfs_x_offset &&
            (panel_height - view->plugin->background_bgrt_raw_width) / 2 == sysfs_x_offset)
                bgrt_rotation = panel_rotation;

        /*
         * Before the ACPI 6.2 specification, the BGRT table did not contain
         * any rotation information, so to make sure that the firmware-splash
         * showed the right way up the firmware would contain a pre-rotated
         * image. Starting with ACPI 6.2 the bgrt status fields has 2 bits
         * to tell the firmware the image needs to be rotated before being
         * displayed.
         * If these bits are set then the firmwares-splash is not pre-rotated,
         * in this case we must not rotate it when rendering and when doing
         * comparisons with the panel-size we must use the post rotation
         * panel-size.
         */
        if (bgrt_rotation != PLY_PIXEL_BUFFER_ROTATE_UPRIGHT) {
                if (bgrt_rotation != panel_rotation) {
                        ply_trace ("bgrt orientation mismatch, bgrt_rot %d panel_rot %d", (int)bgrt_rotation, (int)panel_rotation);
                        return;
                }

                /* Set panel properties to their post-rotations values */
                if (panel_rotation == PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE ||
                    panel_rotation == PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE) {
                        int temp = panel_width;
                        panel_width = panel_height;
                        panel_height = temp;
                }
                panel_rotation = PLY_PIXEL_BUFFER_ROTATE_UPRIGHT;
        }

        if (have_panel_props) {
                ply_pixel_buffer_set_device_rotation (bgrt_buffer, panel_rotation);
                ply_pixel_buffer_set_device_scale (bgrt_buffer, panel_scale);
        }

        width = ply_pixel_buffer_get_width (bgrt_buffer);
        height = ply_pixel_buffer_get_height (bgrt_buffer);

        x_offset = (screen_width - width) / 2;
        y_offset = screen_height * 382 / 1000 - height / 2;

        /*
         * On laptops / tablets the LCD panel is typically brought up in
         * its native resolution, so we can trust the x- and y-offset values
         * provided by the firmware to be correct for a screen with the panels
         * resolution.
         *
         * Moreover some laptop / tablet firmwares to do all kind of hacks wrt
         * the y offset. This happens especially on devices where the panel is
         * mounted 90 degrees rotated, but also on other devices.
         *
         * So on devices with an internal LCD panel, we prefer to use the
         * firmware provided offsets, to make sure we match its quirky behavior.
         *
         * We check that the x-offset matches what we expect for the panel's
         * native resolution to make sure that the values are indeed for the
         * panel's native resolution and then we correct for any difference
         * between the (external) screen's and the panel's resolution.
         */
        if (have_panel_props &&
            (panel_width - view->plugin->background_bgrt_raw_width) / 2 == sysfs_x_offset) {
                if (panel_rotation == PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE ||
                    panel_rotation == PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE) {
                        /*
                         * For left side up panels the y_offset is from the
                         * right side of the image once rotated upright (the
                         * top of the physicial LCD panel is on the right side).
                         * Our coordinates have the left side as 0, so we need
                         * to "flip" the y_offset in this case.
                         */
                        if (panel_rotation == PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE)
                                sysfs_y_offset = panel_height - view->plugin->background_bgrt_raw_height - sysfs_y_offset;

                        /* 90 degrees rotated, swap x and y */
                        x_offset = sysfs_y_offset / panel_scale;
                        y_offset = sysfs_x_offset / panel_scale;

                        x_offset += (screen_width - panel_height / panel_scale) / 2;
                        y_offset += (screen_height - panel_width / panel_scale) * 382 / 1000;
                } else {
                        /* Normal orientation */
                        x_offset = sysfs_x_offset / panel_scale;
                        y_offset = sysfs_y_offset / panel_scale;

                        x_offset += (screen_width - panel_width / panel_scale) / 2;
                        y_offset += (screen_height - panel_height / panel_scale) * 382 / 1000;
                }
        }

        /*
         * On desktops (no panel) we normally do not use the BGRT provided
         * xoffset and yoffset because the resolution they are intended for
         * may be differtent then the resolution of the current display.
         *
         * On some desktops (no panel) the image gets centered not only
         * horizontally, but also vertically. In this case our default of using
         * the golden ratio for the vertical position causes the BGRT image
         * to jump.  To avoid this we check here if the provided xoffset and
         * yoffset perfectly center the image and in that case we use them.
         */
        if (!have_panel_props && screen_scale == 1 &&
            (screen_width  - width ) / 2 == sysfs_x_offset &&
            (screen_height - height) / 2 == sysfs_y_offset) {
                x_offset = sysfs_x_offset;
                y_offset = sysfs_y_offset;
        }

        ply_trace ("using %dx%d bgrt image centered at %dx%d for %dx%d screen",
                   width, height, x_offset, y_offset, screen_width, screen_height);

        view->background_buffer = ply_pixel_buffer_new (screen_width * screen_scale, screen_height * screen_scale);
        ply_pixel_buffer_set_device_scale (view->background_buffer, screen_scale);
        ply_pixel_buffer_fill_with_hex_color (view->background_buffer, NULL, 0x000000);
        if (x_offset >= 0 && y_offset >= 0) {
                bgrt_buffer = ply_pixel_buffer_rotate_upright (bgrt_buffer);
                ply_pixel_buffer_fill_with_buffer (view->background_buffer, bgrt_buffer, x_offset, y_offset);
                ply_pixel_buffer_free (bgrt_buffer);
        }
}

static void
view_set_bgrt_fallback_background (view_t *view)
{
        int width, height, x_offset, y_offset;
        int screen_width, screen_height, screen_scale;
        ply_pixel_buffer_t *image_buffer;

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);
        screen_scale = ply_pixel_display_get_device_scale (view->display);

        image_buffer = ply_image_get_buffer (view->plugin->background_bgrt_fallback_image);

        width = ply_pixel_buffer_get_width (image_buffer);
        height = ply_pixel_buffer_get_height (image_buffer);
        x_offset = (screen_width - width) / 2;
        y_offset = screen_height * 382 / 1000 - height / 2;

        view->background_buffer = ply_pixel_buffer_new (screen_width * screen_scale, screen_height * screen_scale);
        ply_pixel_buffer_set_device_scale (view->background_buffer, screen_scale);
        ply_pixel_buffer_fill_with_hex_color (view->background_buffer, NULL, 0x000000);
        ply_pixel_buffer_fill_with_buffer (view->background_buffer, image_buffer, x_offset, y_offset);
}

static bool
view_load (view_t *view)
{
        unsigned long x, y, width, title_height = 0, subtitle_height = 0;
        unsigned long screen_width, screen_height, screen_scale;
        ply_boot_splash_plugin_t *plugin;
        ply_pixel_buffer_t *buffer;

        plugin = view->plugin;

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        buffer = ply_renderer_get_buffer_for_head(
                        ply_pixel_display_get_renderer (view->display),
                        ply_pixel_display_get_renderer_head (view->display));
        screen_scale = ply_pixel_buffer_get_device_scale (buffer);

        view_set_bgrt_background (view);

        if (!view->background_buffer && plugin->background_bgrt_fallback_image != NULL)
                view_set_bgrt_fallback_background (view);

        if (!view->background_buffer && plugin->background_tile_image != NULL) {
                ply_trace ("tiling background to %lux%lu", screen_width, screen_height);

                /* Create a buffer at screen scale so that we only do the slow interpolating scale once */
                view->background_buffer = ply_pixel_buffer_new (screen_width * screen_scale, screen_height * screen_scale);
                ply_pixel_buffer_set_device_scale (view->background_buffer, screen_scale);

                if (plugin->background_start_color != plugin->background_end_color)
                        ply_pixel_buffer_fill_with_gradient (view->background_buffer, NULL,
                                                             plugin->background_start_color,
                                                             plugin->background_end_color);
                else
                        ply_pixel_buffer_fill_with_hex_color (view->background_buffer, NULL,
                                                              plugin->background_start_color);

                buffer = ply_pixel_buffer_tile (ply_image_get_buffer (plugin->background_tile_image), screen_width, screen_height);
                ply_pixel_buffer_fill_with_buffer (view->background_buffer, buffer, 0, 0);
                ply_pixel_buffer_free (buffer);
        }

        if (plugin->watermark_image != NULL) {
                view->watermark_area.width = ply_image_get_width (plugin->watermark_image);
                view->watermark_area.height = ply_image_get_height (plugin->watermark_image);
                view->watermark_area.x = screen_width * plugin->watermark_horizontal_alignment - ply_image_get_width (plugin->watermark_image) * plugin->watermark_horizontal_alignment;
                view->watermark_area.y = screen_height * plugin->watermark_vertical_alignment - ply_image_get_height (plugin->watermark_image) * plugin->watermark_vertical_alignment;
                ply_trace ("using %ldx%ld watermark centered at %ldx%ld for %ldx%ld screen",
                           view->watermark_area.width, view->watermark_area.height,
                           view->watermark_area.x, view->watermark_area.y,
                           screen_width, screen_height);
        }

        ply_trace ("loading entry");
        if (!ply_entry_load (view->entry))
                return false;

        ply_keymap_icon_load (view->keymap_icon);
        ply_capslock_icon_load (view->capslock_icon);

        view_load_end_animation (view);

        if (view->progress_animation != NULL) {
                ply_trace ("loading progress animation");
                if (!ply_progress_animation_load (view->progress_animation)) {
                        ply_trace ("optional progress animation wouldn't load");
                        ply_progress_animation_free (view->progress_animation);
                        view->progress_animation = NULL;
                }
        } else {
                ply_trace ("this theme has no progress animation");
        }

        if (view->throbber != NULL) {
                ply_trace ("loading throbber");
                if (!ply_throbber_load (view->throbber)) {
                        ply_trace ("optional throbber was not loaded");
                        ply_throbber_free (view->throbber);
                        view->throbber = NULL;
                }
        } else {
                ply_trace ("this theme has no throbber\n");
        }

        if (plugin->mode_settings[plugin->mode].title) {
                ply_label_set_text (view->title_label,
                                    _(plugin->mode_settings[plugin->mode].title));
                title_height = ply_label_get_height (view->title_label);
        } else {
                ply_label_hide (view->title_label);
        }

        if (plugin->mode_settings[plugin->mode].subtitle) {
                ply_label_set_text (view->subtitle_label,
                                    _(plugin->mode_settings[plugin->mode].subtitle));
                subtitle_height = ply_label_get_height (view->subtitle_label);
        } else {
                ply_label_hide (view->subtitle_label);
        }

        y = (screen_height - title_height - 2 * subtitle_height) * plugin->title_vertical_alignment;

        if (plugin->mode_settings[plugin->mode].title) {
                width = ply_label_get_width (view->title_label);
                x = (screen_width - width) * plugin->title_horizontal_alignment;
                ply_trace ("using %ldx%ld title centered at %ldx%ld for %ldx%ld screen",
                           width, title_height, x, y, screen_width, screen_height);
                ply_label_show (view->title_label, view->display, x, y);
                /* Use subtitle_height pixels seperation between title and subtitle */
                y += title_height + subtitle_height;
        }

        if (plugin->mode_settings[plugin->mode].subtitle) {
                width = ply_label_get_width (view->subtitle_label);
                x = (screen_width - width) * plugin->title_horizontal_alignment;
                ply_trace ("using %ldx%ld subtitle centered at %ldx%ld for %ldx%ld screen",
                           width, subtitle_height, x, y, screen_width, screen_height);
                ply_label_show (view->subtitle_label, view->display, x, y);
        }

        return true;
}

static bool
load_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        bool view_loaded;
        view_t *view;

        view_loaded = false;
        node = ply_list_get_first_node (plugin->views);

        while (node != NULL) {
                view = ply_list_node_get_data (node);

                if (view_load (view))
                        view_loaded = true;

                node = ply_list_get_next_node (plugin->views, node);
        }

        return view_loaded;
}

static void
view_redraw (view_t *view)
{
        unsigned long screen_width, screen_height;

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        ply_pixel_display_draw_area (view->display, 0, 0,
                                     screen_width, screen_height);
}

static void
redraw_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                view_redraw (view);
                node = ply_list_get_next_node (plugin->views, node);
        }
}

static void
pause_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        ply_trace ("pausing views");

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                ply_pixel_display_pause_updates (view->display);
                node = ply_list_get_next_node (plugin->views, node);
        }
}

static void
unpause_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        ply_trace ("unpausing views");

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                ply_pixel_display_unpause_updates (view->display);
                node = ply_list_get_next_node (plugin->views, node);
        }
}

static void
view_start_end_animation (view_t        *view,
                          ply_trigger_t *trigger)
{
        ply_boot_splash_plugin_t *plugin = view->plugin;
        unsigned long screen_width, screen_height;
        long x, y, width, height;

        ply_progress_bar_hide (view->progress_bar);
        if (view->progress_animation != NULL)
                ply_progress_animation_hide (view->progress_animation);

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);
        width = ply_animation_get_width (view->end_animation);
        height = ply_animation_get_height (view->end_animation);
        x = plugin->animation_horizontal_alignment * screen_width - width / 2.0;
        y = plugin->animation_vertical_alignment * screen_height - height / 2.0;

        ply_trace ("starting end sequence animation for %ldx%ld view", width, height);
        ply_animation_start (view->end_animation,
                             view->display,
                             trigger, x, y);
        view->animation_bottom = y + height;
}

static void
on_view_throbber_stopped (view_t *view)
{
        view_start_end_animation (view, view->end_trigger);
        view->end_trigger = NULL;
}

static void
view_start_progress_animation (view_t *view)
{
        ply_boot_splash_plugin_t *plugin;

        long x, y;
        long width, height;
        unsigned long screen_width, screen_height;

        assert (view != NULL);

        plugin = view->plugin;

        plugin->is_idle = false;

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        ply_pixel_display_draw_area (view->display, 0, 0,
                                     screen_width, screen_height);

        if (plugin->mode_settings[plugin->mode].use_progress_bar) {
                if (plugin->progress_bar_width != -1)
                        width = plugin->progress_bar_width;
                else
                        width = screen_width;
                height = plugin->progress_bar_height;
                x = plugin->progress_bar_horizontal_alignment * (screen_width - width);
                y = plugin->progress_bar_vertical_alignment * (screen_height - height);
                ply_progress_bar_show (view->progress_bar, view->display,
                                       x, y, width, height);
                ply_pixel_display_draw_area (view->display, x, y, width, height);
                view->animation_bottom = y + height;
        }

        if (plugin->mode_settings[plugin->mode].use_animation &&
            view->throbber != NULL) {
                width = ply_throbber_get_width (view->throbber);
                height = ply_throbber_get_height (view->throbber);
                x = plugin->animation_horizontal_alignment * screen_width - width / 2.0;
                y = plugin->animation_vertical_alignment * screen_height - height / 2.0;
                ply_throbber_start (view->throbber,
                                    plugin->loop,
                                    view->display, x, y);
                ply_pixel_display_draw_area (view->display, x, y, width, height);
                view->animation_bottom = y + height;
        }

        /* We don't really know how long shutdown will so
         * don't show the progress animation
         */
        if (plugin->mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
            plugin->mode == PLY_BOOT_SPLASH_MODE_REBOOT)
                return;

        if (plugin->mode_settings[plugin->mode].use_animation &&
            view->progress_animation != NULL) {
                width = ply_progress_animation_get_width (view->progress_animation);
                height = ply_progress_animation_get_height (view->progress_animation);
                x = plugin->animation_horizontal_alignment * screen_width - width / 2.0;
                y = plugin->animation_vertical_alignment * screen_height - height / 2.0;
                ply_progress_animation_show (view->progress_animation,
                                             view->display, x, y);

                ply_pixel_display_draw_area (view->display, x, y, width, height);
                view->animation_bottom = y + height;
        }
}

static void
view_show_prompt (view_t     *view,
                  const char *prompt,
                  const char *entry_text,
                  int         number_of_bullets)
{
        ply_boot_splash_plugin_t *plugin;
        unsigned long screen_width, screen_height, entry_width, entry_height;
        unsigned long keyboard_indicator_width, keyboard_indicator_height;
        bool show_keyboard_indicators = false;
        long dialog_bottom;
        int x, y;

        assert (view != NULL);

        plugin = view->plugin;

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        if (ply_entry_is_hidden (view->entry)) {
                view->lock_area.width = ply_image_get_width (plugin->lock_image);
                view->lock_area.height = ply_image_get_height (plugin->lock_image);

                entry_width = ply_entry_get_width (view->entry);
                entry_height = ply_entry_get_height (view->entry);

                if (plugin->box_image) {
                        view->box_area.width = ply_image_get_width (plugin->box_image);
                        view->box_area.height = ply_image_get_height (plugin->box_image);
                        view->box_area.x = (screen_width - view->box_area.width) * plugin->dialog_horizontal_alignment;
                        view->box_area.y = (screen_height - view->box_area.height) * plugin->dialog_vertical_alignment;
                        view->dialog_area = view->box_area;
                } else {
                        view->dialog_area.width = view->lock_area.width + entry_width;
                        view->dialog_area.height = MAX(view->lock_area.height, entry_height);
                        view->dialog_area.x = (screen_width - view->dialog_area.width) * plugin->dialog_horizontal_alignment;
                        view->dialog_area.y = (screen_height - view->dialog_area.height) * plugin->dialog_vertical_alignment;
                }

                view->lock_area.x =
                    view->dialog_area.x +
                    (view->dialog_area.width -
                     (view->lock_area.width + entry_width)) / 2.0;
                view->lock_area.y =
                    view->dialog_area.y +
                    (view->dialog_area.height - view->lock_area.height) / 2.0;

                x = view->lock_area.x + view->lock_area.width;
                y = view->dialog_area.y +
                    (view->dialog_area.height - entry_height) / 2.0;

                ply_entry_show (view->entry, plugin->loop, view->display, x, y);

                show_keyboard_indicators = true;
        }

        if (entry_text != NULL)
                ply_entry_set_text (view->entry, entry_text);

        if (number_of_bullets != -1)
                ply_entry_set_bullet_count (view->entry, number_of_bullets);

        dialog_bottom = view->dialog_area.y + view->dialog_area.height;

        if (prompt != NULL) {
                ply_label_set_text (view->label, prompt);

                /* We center the prompt in the middle and use 80% of the horizontal space */
                int label_width = screen_width * 100 / 80;
                ply_label_set_alignment (view->label, PLY_LABEL_ALIGN_CENTER);
                ply_label_set_width (view->label, label_width);

                x = (screen_width - label_width) / 2;
                y = dialog_bottom;

                ply_label_show (view->label, view->display, x, y);

                dialog_bottom += ply_label_get_height (view->label);
        }

        if (show_keyboard_indicators) {
                keyboard_indicator_width =
                        ply_keymap_icon_get_width (view->keymap_icon);
                keyboard_indicator_height = MAX(
                        ply_capslock_icon_get_height (view->capslock_icon),
                        ply_keymap_icon_get_height (view->keymap_icon));

                x = (screen_width - keyboard_indicator_width) * plugin->dialog_horizontal_alignment;
                y = dialog_bottom + keyboard_indicator_height / 2 +
                    (keyboard_indicator_height - ply_keymap_icon_get_height (view->keymap_icon)) / 2.0;
                ply_keymap_icon_show (view->keymap_icon, x, y);

                x += ply_keymap_icon_get_width (view->keymap_icon);
                y = dialog_bottom + keyboard_indicator_height / 2 +
                    (keyboard_indicator_height - ply_capslock_icon_get_height (view->capslock_icon)) / 2.0;
                ply_capslock_icon_show (view->capslock_icon, plugin->loop, view->display, x, y);
        }
}

static void
view_hide_prompt (view_t *view)
{
        assert (view != NULL);

        ply_entry_hide (view->entry);
        ply_capslock_icon_hide (view->capslock_icon);
        ply_keymap_icon_hide (view->keymap_icon);
        ply_label_hide (view->label);
}

static void
load_mode_settings (ply_boot_splash_plugin_t *plugin,
                    ply_key_file_t           *key_file,
                    const char               *group_name,
                    ply_boot_splash_mode_t    mode)
{
        mode_settings_t *settings = &plugin->mode_settings[mode];

        settings->suppress_messages =
                ply_key_file_get_bool (key_file, group_name, "SuppressMessages");
        settings->progress_bar_show_percent_complete =
                ply_key_file_get_bool (key_file, group_name, "ProgressBarShowPercentComplete");
        settings->use_progress_bar =
                ply_key_file_get_bool (key_file, group_name, "UseProgressBar");
        settings->use_firmware_background =
                ply_key_file_get_bool (key_file, group_name, "UseFirmwareBackground");

        /* This defaults to !use_progress_bar for compat. with older themes */
        if (ply_key_file_has_key (key_file, group_name, "UseAnimation"))
                settings->use_animation =
                        ply_key_file_get_bool (key_file, group_name, "UseAnimation");
        else
                settings->use_animation = !settings->use_progress_bar;

        /* This defaults to true for compat. with older themes */
        if (ply_key_file_has_key (key_file, group_name, "UseEndAnimation"))
                settings->use_end_animation =
                        ply_key_file_get_bool (key_file, group_name, "UseEndAnimation");
        else
                settings->use_end_animation = true;

        /* If any mode uses the firmware background, then we need to load it */
        if (settings->use_firmware_background)
                plugin->use_firmware_background = true;

        settings->title = ply_key_file_get_value (key_file, group_name, "Title");
        settings->subtitle = ply_key_file_get_value (key_file, group_name, "SubTitle");
}

static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
        ply_boot_splash_plugin_t *plugin;
        char *image_dir, *image_path;
        char *transition;
        char *progress_function;

        srand ((int) ply_get_timestamp ());
        plugin = calloc (1, sizeof(ply_boot_splash_plugin_t));

        image_dir = ply_key_file_get_value (key_file, "two-step", "ImageDir");

        ply_trace ("Using '%s' as working directory", image_dir);

        asprintf (&image_path, "%s/lock.png", image_dir);
        plugin->lock_image = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/box.png", image_dir);
        plugin->box_image = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/corner-image.png", image_dir);
        plugin->corner_image = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/header-image.png", image_dir);
        plugin->header_image = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/background-tile.png", image_dir);
        plugin->background_tile_image = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/watermark.png", image_dir);
        plugin->watermark_image = ply_image_new (image_path);
        free (image_path);

        plugin->animation_dir = image_dir;

        plugin->font = ply_key_file_get_value (key_file, "two-step", "Font");
        plugin->title_font = ply_key_file_get_value (key_file, "two-step", "TitleFont");

        /* Throbber, progress- and end-animation alignment */
        plugin->animation_horizontal_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "HorizontalAlignment", 0.5);
        plugin->animation_vertical_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "VerticalAlignment", 0.5);

        /* Progressbar alignment, this defaults to the animation alignment
         * for compatibility with older themes.
         */
        plugin->progress_bar_horizontal_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "ProgressBarHorizontalAlignment",
                                         plugin->animation_horizontal_alignment);
        plugin->progress_bar_vertical_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "ProgressBarVerticalAlignment",
                                         plugin->animation_vertical_alignment);

        /* Watermark alignment */
        plugin->watermark_horizontal_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "WatermarkHorizontalAlignment", 1.0);
        plugin->watermark_vertical_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "WatermarkVerticalAlignment", 0.5);

        /* Password (or other) dialog alignment */
        plugin->dialog_horizontal_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "DialogHorizontalAlignment", 0.5);
        plugin->dialog_vertical_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "DialogVerticalAlignment", 0.5);

        /* Title alignment */
        plugin->title_horizontal_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "TitleHorizontalAlignment", 0.5);
        plugin->title_vertical_alignment =
                ply_key_file_get_double (key_file, "two-step",
                                         "TitleVerticalAlignment", 0.5);

        plugin->transition = PLY_PROGRESS_ANIMATION_TRANSITION_NONE;
        transition = ply_key_file_get_value (key_file, "two-step", "Transition");
        if (transition != NULL) {
                if (strcmp (transition, "fade-over") == 0)
                        plugin->transition = PLY_PROGRESS_ANIMATION_TRANSITION_FADE_OVER;
                else if (strcmp (transition, "cross-fade") == 0)
                        plugin->transition = PLY_PROGRESS_ANIMATION_TRANSITION_CROSS_FADE;
                else if (strcmp (transition, "merge-fade") == 0)
                        plugin->transition = PLY_PROGRESS_ANIMATION_TRANSITION_MERGE_FADE;
        }
        free (transition);

        plugin->transition_duration =
                ply_key_file_get_double (key_file, "two-step",
                                         "TransitionDuration", 0.0);

        plugin->background_start_color =
                ply_key_file_get_long (key_file, "two-step",
                                       "BackgroundStartColor",
                                       PLYMOUTH_BACKGROUND_START_COLOR);
        plugin->background_end_color =
                ply_key_file_get_long (key_file, "two-step",
                                       "BackgroundEndColor",
                                       PLYMOUTH_BACKGROUND_END_COLOR);

        plugin->progress_bar_bg_color =
                ply_key_file_get_long (key_file, "two-step",
                                       "ProgressBarBackgroundColor",
                                       0xffffff /* white */);
        plugin->progress_bar_fg_color =
                ply_key_file_get_long (key_file, "two-step",
                                       "ProgressBarForegroundColor",
                                       0x000000 /* black */);
        plugin->progress_bar_width =
                ply_key_file_get_long (key_file, "two-step",
                                       "ProgressBarWidth",
                                       PROGRESS_BAR_WIDTH);
        plugin->progress_bar_height =
                ply_key_file_get_long (key_file, "two-step",
                                       "ProgressBarHeight",
                                       PROGRESS_BAR_HEIGHT);

        load_mode_settings (plugin, key_file, "boot-up", PLY_BOOT_SPLASH_MODE_BOOT_UP);
        load_mode_settings (plugin, key_file, "shutdown", PLY_BOOT_SPLASH_MODE_SHUTDOWN);
        load_mode_settings (plugin, key_file, "reboot", PLY_BOOT_SPLASH_MODE_REBOOT);
        load_mode_settings (plugin, key_file, "updates", PLY_BOOT_SPLASH_MODE_UPDATES);
        load_mode_settings (plugin, key_file, "system-upgrade", PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE);
        load_mode_settings (plugin, key_file, "firmware-upgrade", PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE);

        if (plugin->use_firmware_background) {
                plugin->background_bgrt_image = ply_image_new ("/sys/firmware/acpi/bgrt/image");

                asprintf (&image_path, "%s/bgrt-fallback.png", image_dir);
                plugin->background_bgrt_fallback_image = ply_image_new (image_path);
                free (image_path);
        }

        plugin->dialog_clears_firmware_background =
                ply_key_file_get_bool (key_file, "two-step", "DialogClearsFirmwareBackground");

        plugin->message_below_animation =
                ply_key_file_get_bool (key_file, "two-step", "MessageBelowAnimation");

        progress_function = ply_key_file_get_value (key_file, "two-step", "ProgressFunction");

        if (progress_function != NULL) {
                if (strcmp (progress_function, "wwoods") == 0) {
                        ply_trace ("Using wwoods progress function");
                        plugin->progress_function = PROGRESS_FUNCTION_TYPE_WWOODS;
                } else if (strcmp (progress_function, "linear") == 0) {
                        ply_trace ("Using linear progress function");
                        plugin->progress_function = PROGRESS_FUNCTION_TYPE_LINEAR;
                } else {
                        ply_trace ("unknown progress function %s, defaulting to linear", progress_function);
                        plugin->progress_function = PROGRESS_FUNCTION_TYPE_LINEAR;
                }

                free (progress_function);
        }

        plugin->views = ply_list_new ();

        return plugin;
}

static void
free_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        ply_trace ("freeing views");

        node = ply_list_get_first_node (plugin->views);

        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_free (view);
                ply_list_remove_node (plugin->views, node);

                node = next_node;
        }

        ply_list_free (plugin->views);
        plugin->views = NULL;
}

static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
        int i;

        if (plugin == NULL)
                return;

        ply_trace ("destroying plugin");

        if (plugin->loop != NULL) {
                stop_animation (plugin);

                ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);
                detach_from_event_loop (plugin);
        }

        ply_image_free (plugin->lock_image);

        if (plugin->box_image != NULL)
                ply_image_free (plugin->box_image);

        if (plugin->corner_image != NULL)
                ply_image_free (plugin->corner_image);

        if (plugin->header_image != NULL)
                ply_image_free (plugin->header_image);

        if (plugin->background_tile_image != NULL)
                ply_image_free (plugin->background_tile_image);

        if (plugin->background_bgrt_image != NULL)
                ply_image_free (plugin->background_bgrt_image);

        if (plugin->background_bgrt_fallback_image != NULL)
                ply_image_free (plugin->background_bgrt_fallback_image);

        if (plugin->watermark_image != NULL)
                ply_image_free (plugin->watermark_image);

        for (i = 0; i < PLY_BOOT_SPLASH_MODE_COUNT; i++) {
                free (plugin->mode_settings[i].title);
                free (plugin->mode_settings[i].subtitle);
        }

        free (plugin->font);
        free (plugin->title_font);
        free (plugin->animation_dir);
        free_views (plugin);
        free (plugin);
}

static void
start_end_animation (ply_boot_splash_plugin_t *plugin,
                     ply_trigger_t            *trigger)
{
        ply_list_node_t *node;
        view_t *view;

        if (!plugin->mode_settings[plugin->mode].use_animation) {
                ply_trigger_pull (trigger, NULL);
                return;
        }

        if (!plugin->mode_settings[plugin->mode].use_end_animation) {
                node = ply_list_get_first_node (plugin->views);
                while (node != NULL) {
                        view = ply_list_node_get_data (node);

                        ply_progress_bar_hide (view->progress_bar);

                        if (view->throbber != NULL)
                                ply_throbber_stop (view->throbber, NULL);

                        if (view->progress_animation != NULL)
                                ply_progress_animation_hide (view->progress_animation);

                        node = ply_list_get_next_node (plugin->views, node);
                }
                ply_trigger_pull (trigger, NULL);
                return;
        }

        ply_trace ("starting end animation");

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);

                ply_trigger_ignore_next_pull (trigger);

                if (view->throbber != NULL) {
                        ply_trigger_t *throbber_trigger;
                        ply_trace ("stopping throbber");
                        view->end_trigger = trigger;
                        throbber_trigger = ply_trigger_new (NULL);
                        ply_trigger_add_handler (throbber_trigger,
                                                 (ply_trigger_handler_t)
                                                 on_view_throbber_stopped,
                                                 view);
                        ply_throbber_stop (view->throbber, throbber_trigger);
                } else {
                        view_start_end_animation (view, trigger);
                }

                node = ply_list_get_next_node (plugin->views, node);
        }
        ply_trigger_pull (trigger, NULL);
}

static void
start_progress_animation (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        if (plugin->is_animating)
                return;

        ply_trace ("starting animation");

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                view_start_progress_animation (view);
                node = ply_list_get_next_node (plugin->views, node);
        }

        plugin->is_animating = true;

        /* We don't really know how long shutdown will, take
         * but it's normally really fast, so just jump to
         * the end animation
         */
        if (plugin->mode_settings[plugin->mode].use_end_animation &&
            (plugin->mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
             plugin->mode == PLY_BOOT_SPLASH_MODE_REBOOT))
                become_idle (plugin, NULL);
}

static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        assert (plugin != NULL);
        assert (plugin->loop != NULL);

        if (!plugin->is_animating)
                return;

        ply_trace ("stopping animation");
        plugin->is_animating = false;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);

                ply_progress_bar_hide (view->progress_bar);
                if (view->progress_animation != NULL)
                        ply_progress_animation_hide (view->progress_animation);
                if (view->throbber != NULL)
                        ply_throbber_stop (view->throbber, NULL);
                if (view->end_animation != NULL)
                        ply_animation_stop (view->end_animation);

                node = ply_list_get_next_node (plugin->views, node);
        }
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
        plugin->loop = NULL;
}

static void
draw_background (view_t             *view,
                 ply_pixel_buffer_t *pixel_buffer,
                 int                 x,
                 int                 y,
                 int                 width,
                 int                 height)
{
        ply_boot_splash_plugin_t *plugin;
        ply_rectangle_t area;
        bool use_black_background = false;
        bool using_fw_background;

        plugin = view->plugin;

        using_fw_background = (plugin->background_bgrt_image || plugin->background_bgrt_fallback_image);

        area.x = x;
        area.y = y;
        area.width = width;
        area.height = height;

        /* When using the firmware logo as background and we should not use
         * it for this mode, use solid black as background.
         */
        if (using_fw_background &&
            !plugin->mode_settings[plugin->mode].use_firmware_background)
                use_black_background = true;

        /* When using the firmware logo as background, use solid black as
         * background for dialogs.
         */
        if ((plugin->state == PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY ||
             plugin->state == PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY) &&
            using_fw_background && plugin->dialog_clears_firmware_background)
                use_black_background = true;

        if (use_black_background)
                ply_pixel_buffer_fill_with_hex_color (pixel_buffer, &area, 0);
        else if (view->background_buffer != NULL)
                ply_pixel_buffer_fill_with_buffer (pixel_buffer, view->background_buffer, 0, 0);
        else if (plugin->background_start_color != plugin->background_end_color)
                ply_pixel_buffer_fill_with_gradient (pixel_buffer, &area,
                                                     plugin->background_start_color,
                                                     plugin->background_end_color);
        else
                ply_pixel_buffer_fill_with_hex_color (pixel_buffer, &area,
                                                      plugin->background_start_color);

        if (plugin->watermark_image != NULL) {
                uint32_t *data;

                data = ply_image_get_data (plugin->watermark_image);
                ply_pixel_buffer_fill_with_argb32_data (pixel_buffer, &view->watermark_area, data);
        }
}

static void
on_draw (view_t             *view,
         ply_pixel_buffer_t *pixel_buffer,
         int                 x,
         int                 y,
         int                 width,
         int                 height)
{
        ply_boot_splash_plugin_t *plugin;
        ply_rectangle_t screen_area;
        ply_rectangle_t image_area;

        plugin = view->plugin;

        draw_background (view, pixel_buffer, x, y, width, height);

        ply_pixel_buffer_get_size (pixel_buffer, &screen_area);

        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY ||
            plugin->state == PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY) {
                uint32_t *box_data, *lock_data;

                if (plugin->box_image) {
                        box_data = ply_image_get_data (plugin->box_image);
                        ply_pixel_buffer_fill_with_argb32_data (pixel_buffer,
                                                                &view->box_area,
                                                                box_data);
                }

                ply_entry_draw_area (view->entry,
                                     pixel_buffer,
                                     x, y, width, height);
                ply_keymap_icon_draw_area (view->keymap_icon,
                                           pixel_buffer,
                                           x, y, width, height);
                ply_capslock_icon_draw_area (view->capslock_icon,
                                             pixel_buffer,
                                             x, y, width, height);
                ply_label_draw_area (view->label,
                                     pixel_buffer,
                                     x, y, width, height);

                lock_data = ply_image_get_data (plugin->lock_image);
                ply_pixel_buffer_fill_with_argb32_data (pixel_buffer,
                                                        &view->lock_area,
                                                        lock_data);
        } else {
                if (plugin->mode_settings[plugin->mode].use_progress_bar)
                        ply_progress_bar_draw_area (view->progress_bar, pixel_buffer,
                                                    x, y, width, height);

                if (plugin->mode_settings[plugin->mode].use_animation &&
                    view->throbber != NULL)
                        ply_throbber_draw_area (view->throbber, pixel_buffer,
                                                x, y, width, height);

                if (plugin->mode_settings[plugin->mode].use_animation &&
                    view->progress_animation != NULL)
                        ply_progress_animation_draw_area (view->progress_animation,
                                                          pixel_buffer,
                                                          x, y, width, height);

                if (plugin->mode_settings[plugin->mode].use_animation &&
                    view->end_animation != NULL)
                        ply_animation_draw_area (view->end_animation,
                                                 pixel_buffer,
                                                 x, y, width, height);

                if (plugin->corner_image != NULL) {
                        image_area.width = ply_image_get_width (plugin->corner_image);
                        image_area.height = ply_image_get_height (plugin->corner_image);
                        image_area.x = screen_area.width - image_area.width - 20;
                        image_area.y = screen_area.height - image_area.height - 20;

                        ply_pixel_buffer_fill_with_argb32_data (pixel_buffer, &image_area, ply_image_get_data (plugin->corner_image));
                }

                if (plugin->header_image != NULL) {
                        long sprite_height;


                        if (view->progress_animation != NULL)
                                sprite_height = ply_progress_animation_get_height (view->progress_animation);
                        else
                                sprite_height = 0;

                        if (view->throbber != NULL)
                                sprite_height = MAX (ply_throbber_get_height (view->throbber),
                                                     sprite_height);

                        image_area.width = ply_image_get_width (plugin->header_image);
                        image_area.height = ply_image_get_height (plugin->header_image);
                        image_area.x = screen_area.width / 2.0 - image_area.width / 2.0;
                        image_area.y = plugin->animation_vertical_alignment * screen_area.height - sprite_height / 2.0 - image_area.height;

                        ply_pixel_buffer_fill_with_argb32_data (pixel_buffer, &image_area, ply_image_get_data (plugin->header_image));
                }
                ply_label_draw_area (view->title_label,
                                     pixel_buffer,
                                     x, y, width, height);
                ply_label_draw_area (view->subtitle_label,
                                     pixel_buffer,
                                     x, y, width, height);
        }
        ply_label_draw_area (view->message_label,
                             pixel_buffer,
                             x, y, width, height);
}

static void
add_pixel_display (ply_boot_splash_plugin_t *plugin,
                   ply_pixel_display_t      *display)
{
        view_t *view;

        ply_trace ("adding pixel display to plugin");
        view = view_new (plugin, display);

        ply_pixel_display_set_draw_handler (view->display,
                                            (ply_pixel_display_draw_handler_t)
                                            on_draw, view);
        if (plugin->is_visible) {
                if (view_load (view)) {
                        ply_list_append_data (plugin->views, view);
                        if (plugin->is_animating)
                                view_start_progress_animation (view);
                } else {
                        view_free (view);
                }
        } else {
                ply_list_append_data (plugin->views, view);
        }
}

static void
remove_pixel_display (ply_boot_splash_plugin_t *plugin,
                      ply_pixel_display_t      *display)
{
        ply_list_node_t *node;

        ply_trace ("removing pixel display from plugin");
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view_t *view;
                ply_list_node_t *next_node;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                if (view->display == display) {
                        ply_pixel_display_set_draw_handler (view->display, NULL, NULL);
                        view_free (view);
                        ply_list_remove_node (plugin->views, node);
                        return;
                }

                node = next_node;
        }
}

static bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer,
                    ply_boot_splash_mode_t    mode)
{
        assert (plugin != NULL);

        plugin->loop = loop;
        plugin->mode = mode;

        ply_trace ("loading lock image");
        if (!ply_image_load (plugin->lock_image))
                return false;

        if (plugin->box_image != NULL) {
                ply_trace ("loading box image");

                if (!ply_image_load (plugin->box_image)) {
                        ply_image_free (plugin->box_image);
                        plugin->box_image = NULL;
                }
        }

        if (plugin->corner_image != NULL) {
                ply_trace ("loading corner image");

                if (!ply_image_load (plugin->corner_image)) {
                        ply_image_free (plugin->corner_image);
                        plugin->corner_image = NULL;
                }
        }

        if (plugin->header_image != NULL) {
                ply_trace ("loading header image");

                if (!ply_image_load (plugin->header_image)) {
                        ply_image_free (plugin->header_image);
                        plugin->header_image = NULL;
                }
        }

        if (plugin->background_tile_image != NULL) {
                ply_trace ("loading background tile image");
                if (!ply_image_load (plugin->background_tile_image)) {
                        ply_image_free (plugin->background_tile_image);
                        plugin->background_tile_image = NULL;
                }
        }

        if (plugin->background_bgrt_image != NULL) {
                ply_trace ("loading background bgrt image");
                if (ply_image_load (plugin->background_bgrt_image)) {
                        plugin->background_bgrt_raw_width = ply_image_get_width (plugin->background_bgrt_image);
                        plugin->background_bgrt_raw_height = ply_image_get_height (plugin->background_bgrt_image);
                } else {
                        ply_image_free (plugin->background_bgrt_image);
                        plugin->background_bgrt_image = NULL;
                }
        }

        if (plugin->background_bgrt_fallback_image != NULL) {
                ply_trace ("loading background bgrt fallback image");
                if (!ply_image_load (plugin->background_bgrt_fallback_image)) {
                        ply_image_free (plugin->background_bgrt_fallback_image);
                        plugin->background_bgrt_fallback_image = NULL;
                }
        }

        if (plugin->watermark_image != NULL) {
                ply_trace ("loading watermark image");
                if (!ply_image_load (plugin->watermark_image)) {
                        ply_image_free (plugin->watermark_image);
                        plugin->watermark_image = NULL;
                }
        }

        if (!load_views (plugin)) {
                ply_trace ("couldn't load views");
                return false;
        }

        ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                       detach_from_event_loop,
                                       plugin);

        ply_trace ("starting boot animations");
        start_progress_animation (plugin);

        plugin->is_visible = true;

        return true;
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
        assert (plugin != NULL);
}

static void
on_animation_stopped (ply_boot_splash_plugin_t *plugin)
{
        if (plugin->idle_trigger != NULL) {
                ply_trigger_pull (plugin->idle_trigger, NULL);
                plugin->idle_trigger = NULL;
        }
        plugin->is_idle = true;
}

static void
update_progress_animation (ply_boot_splash_plugin_t *plugin,
                           double                    fraction_done)
{
        ply_list_node_t *node;
        view_t *view;
        char buf[64];

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);

                if (view->progress_animation != NULL)
                        ply_progress_animation_set_fraction_done (view->progress_animation,
                                                                  fraction_done);

                ply_progress_bar_set_fraction_done (view->progress_bar, fraction_done);
                if (!ply_progress_bar_is_hidden (view->progress_bar) &&
                    plugin->mode_settings[plugin->mode].progress_bar_show_percent_complete) {
                        snprintf (buf, sizeof(buf), _("%d%% complete"), (int)(fraction_done * 100));
                        view_show_message (view, buf);
                }

                node = ply_list_get_next_node (plugin->views, node);
        }
}

static void
on_boot_progress (ply_boot_splash_plugin_t *plugin,
                  double                    duration,
                  double                    fraction_done)
{
        if (plugin->mode == PLY_BOOT_SPLASH_MODE_UPDATES ||
            plugin->mode == PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE ||
            plugin->mode == PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE)
                return;

        if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                return;

        if (plugin->is_idle)
                return;

        /*
         * If we do not have an end animation, we keep showing progress until
         * become_idle gets called.
         */
        if (plugin->mode_settings[plugin->mode].use_end_animation &&
            fraction_done >= SHOW_ANIMATION_FRACTION) {
                if (plugin->stop_trigger == NULL) {
                        ply_trace ("boot progressed to end");

                        plugin->stop_trigger = ply_trigger_new (&plugin->stop_trigger);
                        ply_trigger_add_handler (plugin->stop_trigger,
                                                 (ply_trigger_handler_t)
                                                 on_animation_stopped,
                                                 plugin);
                        start_end_animation (plugin, plugin->stop_trigger);
                }
        } else {
                double total_duration;

                fraction_done *= (1 / SHOW_ANIMATION_FRACTION);

                switch (plugin->progress_function) {
                /* Fun made-up smoothing function to make the growth asymptotic:
                 * fraction(time,estimate)=1-2^(-(time^1.45)/estimate) */
                case PROGRESS_FUNCTION_TYPE_WWOODS:
                        total_duration = duration / fraction_done;
                        fraction_done = 1.0 - pow (2.0, -pow (duration, 1.45) / total_duration) * (1.0 - fraction_done);
                        break;

                case PROGRESS_FUNCTION_TYPE_LINEAR:
                        break;
                }

                update_progress_animation (plugin, fraction_done);
        }
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
        assert (plugin != NULL);

        ply_trace ("hiding splash");
        if (plugin->loop != NULL) {
                stop_animation (plugin);

                ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);
                detach_from_event_loop (plugin);
        }

        plugin->is_visible = false;
}

static void
show_prompt (ply_boot_splash_plugin_t *plugin,
             const char               *prompt,
             const char               *entry_text,
             int                       number_of_bullets)
{
        ply_list_node_t *node;
        view_t *view;

        ply_trace ("showing prompt");
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                view_show_prompt (view, prompt, entry_text, number_of_bullets);
                node = ply_list_get_next_node (plugin->views, node);
        }
}

static void
on_root_mounted (ply_boot_splash_plugin_t *plugin)
{
        ply_trace ("root filesystem mounted");
        plugin->root_is_mounted = true;
}

static void
become_idle (ply_boot_splash_plugin_t *plugin,
             ply_trigger_t            *idle_trigger)
{
        ply_trace ("deactivation requested");
        if (plugin->is_idle) {
                ply_trace ("plugin is already idle");
                ply_trigger_pull (idle_trigger, NULL);
                return;
        }

        plugin->idle_trigger = idle_trigger;

        if (plugin->stop_trigger == NULL) {
                ply_trace ("waiting for plugin to stop");
                plugin->stop_trigger = ply_trigger_new (&plugin->stop_trigger);
                ply_trigger_add_handler (plugin->stop_trigger,
                                         (ply_trigger_handler_t)
                                         on_animation_stopped,
                                         plugin);
                start_end_animation (plugin, plugin->stop_trigger);
        } else {
                ply_trace ("already waiting for plugin to stop");
        }
}

static void
hide_prompt (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        ply_trace ("hiding prompt");
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                view_hide_prompt (view);
                node = ply_list_get_next_node (plugin->views, node);
        }
}

static void
view_show_message (view_t     *view,
                   const char  *message)
{
        ply_boot_splash_plugin_t *plugin = view->plugin;
        int x, y, width, height;

        if (plugin->message_below_animation)
                ply_label_set_alignment (view->message_label, PLY_LABEL_ALIGN_CENTER);

        ply_label_set_text (view->message_label, message);
        width = ply_label_get_width (view->message_label);
        height = ply_label_get_height (view->message_label);

        if (plugin->message_below_animation) {
                x = (ply_pixel_display_get_width (view->display) - width) * 0.5;
                y = view->animation_bottom + 10;
        } else {
                x = 10;
                y = 10;
        }

        ply_label_show (view->message_label, view->display, x, y);
        ply_pixel_display_draw_area (view->display, x, y, width, height);
}

static void
show_message (ply_boot_splash_plugin_t *plugin,
              const char               *message)
{
        ply_list_node_t *node;
        view_t *view;

        if (plugin->mode_settings[plugin->mode].suppress_messages) {
                ply_trace ("Suppressing message '%s'", message);
                return;
        }
        ply_trace ("Showing message '%s'", message);
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                view_show_message (view, message);
                node = ply_list_get_next_node (plugin->views, node);
        }
}

static void
system_update (ply_boot_splash_plugin_t *plugin,
               int                       progress)
{
        if (plugin->mode != PLY_BOOT_SPLASH_MODE_UPDATES &&
            plugin->mode != PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE &&
            plugin->mode != PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE)
                return;

        update_progress_animation (plugin, progress / 100.0);
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
        pause_views (plugin);
        if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                hide_prompt (plugin);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;
        start_progress_animation (plugin);
        redraw_views (plugin);
        unpause_views (plugin);
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
        pause_views (plugin);
        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                stop_animation (plugin);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY;
        show_prompt (plugin, prompt, NULL, bullets);
        redraw_views (plugin);
        unpause_views (plugin);
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
        pause_views (plugin);
        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                stop_animation (plugin);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY;
        show_prompt (plugin, prompt, entry_text, -1);
        redraw_views (plugin);
        unpause_views (plugin);
}

static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
        show_message (plugin, message);
}

ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
        static ply_boot_splash_plugin_interface_t plugin_interface =
        {
                .create_plugin        = create_plugin,
                .destroy_plugin       = destroy_plugin,
                .add_pixel_display    = add_pixel_display,
                .remove_pixel_display = remove_pixel_display,
                .show_splash_screen   = show_splash_screen,
                .update_status        = update_status,
                .on_boot_progress     = on_boot_progress,
                .hide_splash_screen   = hide_splash_screen,
                .on_root_mounted      = on_root_mounted,
                .become_idle          = become_idle,
                .display_normal       = display_normal,
                .display_password     = display_password,
                .display_question     = display_question,
                .display_message      = display_message,
                .system_update        = system_update,
        };

        return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
