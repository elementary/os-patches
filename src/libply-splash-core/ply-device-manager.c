/* ply-device-manager.c - device manager
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
#include "config.h"
#include "ply-device-manager.h"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_UDEV
#include <libudev.h>
#endif

#include "ply-logger.h"
#include "ply-event-loop.h"
#include "ply-hashtable.h"
#include "ply-list.h"
#include "ply-utils.h"

#define SUBSYSTEM_DRM "drm"
#define SUBSYSTEM_FRAME_BUFFER "graphics"

#ifdef HAVE_UDEV
static void create_devices_from_udev (ply_device_manager_t *manager);
#endif

static bool create_devices_for_terminal_and_renderer_type (ply_device_manager_t *manager,
                                                           const char           *device_path,
                                                           ply_terminal_t       *terminal,
                                                           ply_renderer_type_t   renderer_type);
static void create_pixel_displays_for_renderer (ply_device_manager_t *manager,
                                                ply_renderer_t       *renderer);

struct _ply_device_manager
{
        ply_device_manager_flags_t flags;
        ply_event_loop_t          *loop;
        ply_hashtable_t           *terminals;
        ply_hashtable_t           *renderers;
        ply_terminal_t            *local_console_terminal;
        ply_list_t                *keyboards;
        ply_list_t                *text_displays;
        ply_list_t                *pixel_displays;
        struct udev               *udev_context;
        struct udev_monitor       *udev_monitor;
        ply_fd_watch_t            *fd_watch;

        ply_keyboard_added_handler_t         keyboard_added_handler;
        ply_keyboard_removed_handler_t       keyboard_removed_handler;
        ply_pixel_display_added_handler_t    pixel_display_added_handler;
        ply_pixel_display_removed_handler_t  pixel_display_removed_handler;
        ply_text_display_added_handler_t     text_display_added_handler;
        ply_text_display_removed_handler_t   text_display_removed_handler;
        void                                *event_handler_data;

        uint32_t                    local_console_managed : 1;
        uint32_t                    local_console_is_text : 1;
        uint32_t                    serial_consoles_detected : 1;
        uint32_t                    renderers_activated : 1;
        uint32_t                    keyboards_activated : 1;

        uint32_t                    paused : 1;
        uint32_t                    device_timeout_elapsed : 1;
        uint32_t                    found_drm_device : 1;
        uint32_t                    found_fb_device : 1;
};

static void
detach_from_event_loop (ply_device_manager_t *manager)
{
        assert (manager != NULL);

        manager->loop = NULL;
}

static void
attach_to_event_loop (ply_device_manager_t *manager,
                      ply_event_loop_t     *loop)
{
        assert (manager != NULL);
        assert (loop != NULL);
        assert (manager->loop == NULL);

        manager->loop = loop;

        ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                       detach_from_event_loop,
                                       manager);
}

static void
free_displays_for_renderer (ply_device_manager_t *manager,
                            ply_renderer_t       *renderer)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (manager->pixel_displays);
        while (node != NULL) {
                ply_list_node_t *next_node;
                ply_pixel_display_t *display;
                ply_renderer_t *display_renderer;

                display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (manager->pixel_displays, node);
                display_renderer = ply_pixel_display_get_renderer (display);

                if (display_renderer == renderer) {
                        if (manager->pixel_display_removed_handler != NULL)
                                manager->pixel_display_removed_handler (manager->event_handler_data, display);
                        ply_pixel_display_free (display);
                        ply_list_remove_node (manager->pixel_displays, node);

                }

                node = next_node;
        }
}

static void
free_keyboards_for_renderer (ply_device_manager_t *manager,
                            ply_renderer_t       *renderer)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (manager->keyboards);
        while (node != NULL) {
                ply_list_node_t *next_node;
                ply_keyboard_t *keyboard;
                ply_renderer_t *keyboard_renderer;

                keyboard = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (manager->keyboards, node);
                keyboard_renderer = ply_keyboard_get_renderer (keyboard);

                if (keyboard_renderer == renderer) {
                        ply_keyboard_free (keyboard);
                        ply_list_remove_node (manager->keyboards, node);
                }

                node = next_node;
        }
        if (ply_list_get_first_node (manager->keyboards) == NULL) {
                manager->local_console_managed = false;
        }
}

static void
free_devices_from_device_path (ply_device_manager_t *manager,
                               const char           *device_path,
                               bool                  close)
{
        void *key = NULL;
        void *renderer = NULL;

        ply_hashtable_lookup_full (manager->renderers,
                                   (void *) device_path,
                                   &key,
                                   &renderer);

        if (renderer == NULL)
                return;

        free_displays_for_renderer (manager, renderer);
        free_keyboards_for_renderer (manager, renderer);

        ply_hashtable_remove (manager->renderers, (void *) device_path);
        free (key);

        /*
         * Close is false when called from ply_device_manager_free (), in this
         * case we don't deactivate / close for retain-splash purposes.
         */
        if (close) {
                if (manager->renderers_activated)
                        ply_renderer_deactivate (renderer);

                ply_renderer_close (renderer);
        }

        ply_renderer_free (renderer);
}

#ifdef HAVE_UDEV
static bool
drm_device_in_use (ply_device_manager_t *manager,
                   const char           *device_path)
{
        ply_renderer_t *renderer;

        renderer = ply_hashtable_lookup (manager->renderers, (void *) device_path);

        return renderer != NULL;
}

static bool
fb_device_has_drm_device (ply_device_manager_t *manager,
                          struct udev_device   *fb_device)
{
        struct udev_enumerate *card_matches;
        struct udev_list_entry *card_entry;
        const char *id_path;
        bool has_drm_device = false;

        /* We want to see if the framebuffer is associated with a DRM-capable
         * graphics card, if it is, we'll use the DRM device */
        card_matches = udev_enumerate_new (manager->udev_context);
        udev_enumerate_add_match_is_initialized (card_matches);
        udev_enumerate_add_match_parent (card_matches, udev_device_get_parent (fb_device));
        udev_enumerate_add_match_subsystem (card_matches, "drm");
        id_path = udev_device_get_property_value (fb_device, "ID_PATH");
        udev_enumerate_add_match_property (card_matches, "ID_PATH", id_path);

        ply_trace ("trying to find associated drm node for fb device (path: %s)", id_path);

        udev_enumerate_scan_devices (card_matches);

        /* there should only ever be at most one match so we don't iterate through
         * the list, but just look at the first entry */
        card_entry = udev_enumerate_get_list_entry (card_matches);

        if (card_entry != NULL) {
                struct udev_device *card_device = NULL;
                const char *card_node;
                const char *card_path;

                card_path = udev_list_entry_get_name (card_entry);
                card_device = udev_device_new_from_syspath (manager->udev_context, card_path);
                card_node = udev_device_get_devnode (card_device);
                if (card_node != NULL && drm_device_in_use (manager, card_node))
                        has_drm_device = true;
                else
                        ply_trace ("no card node!");

                udev_device_unref (card_device);
        } else {
                ply_trace ("no card entry!");
        }

        udev_enumerate_unref (card_matches);
        return has_drm_device;
}

static bool
create_devices_for_udev_device (ply_device_manager_t *manager,
                                struct udev_device   *device)
{
        const char *device_path;
        bool created = false;

        device_path = udev_device_get_devnode (device);

        if (device_path != NULL) {
                const char *subsystem;
                ply_renderer_type_t renderer_type = PLY_RENDERER_TYPE_NONE;

                subsystem = udev_device_get_subsystem (device);
                ply_trace ("device subsystem is %s", subsystem);

                if (subsystem != NULL && strcmp (subsystem, SUBSYSTEM_DRM) == 0) {
                        ply_trace ("found DRM device %s", device_path);
                        renderer_type = PLY_RENDERER_TYPE_DRM;
                } else if (strcmp (subsystem, SUBSYSTEM_FRAME_BUFFER) == 0) {
                        ply_trace ("found frame buffer device %s", device_path);
                        if (!fb_device_has_drm_device (manager, device))
                                renderer_type = PLY_RENDERER_TYPE_FRAME_BUFFER;
                        else
                                ply_trace ("ignoring, since there's a DRM device associated with it");
                }

                if (renderer_type != PLY_RENDERER_TYPE_NONE) {
                        ply_terminal_t *terminal = NULL;

                        if (!manager->local_console_managed) {
                                terminal = manager->local_console_terminal;
                        }

                        created = create_devices_for_terminal_and_renderer_type (manager,
                                                                                 device_path,
                                                                                 terminal,
                                                                                 renderer_type);
                        if (created) {
                                if (renderer_type == PLY_RENDERER_TYPE_DRM)
                                        manager->found_drm_device = 1;
                                if (renderer_type == PLY_RENDERER_TYPE_FRAME_BUFFER)
                                        manager->found_fb_device = 1;
                        }
                }
        }

        return created;
}

static bool
create_devices_for_subsystem (ply_device_manager_t *manager,
                              const char           *subsystem)
{
        struct udev_enumerate *matches;
        struct udev_list_entry *entry;
        bool found_device = false;

        ply_trace ("creating objects for %s devices",
                   strcmp (subsystem, SUBSYSTEM_FRAME_BUFFER) == 0 ?
                   "frame buffer" :
                   subsystem);

        matches = udev_enumerate_new (manager->udev_context);
        udev_enumerate_add_match_subsystem (matches, subsystem);
        udev_enumerate_scan_devices (matches);

        udev_list_entry_foreach (entry, udev_enumerate_get_list_entry (matches)){
                struct udev_device *device = NULL;
                const char *path;

                path = udev_list_entry_get_name (entry);

                if (path == NULL) {
                        ply_trace ("path was null!");
                        continue;
                }

                ply_trace ("found device %s", path);

                device = udev_device_new_from_syspath (manager->udev_context, path);

                /* if device isn't fully initialized, we'll get an add event later
                 */
                if (udev_device_get_is_initialized (device)) {
                        ply_trace ("device is initialized");

                        /* We only care about devices assigned to a (any) devices. Floating
                         * devices should be ignored.
                         */
                        if (udev_device_has_tag (device, "seat")) {
                                const char *node;
                                node = udev_device_get_devnode (device);
                                if (node != NULL) {
                                        ply_trace ("found node %s", node);
                                        found_device = create_devices_for_udev_device (manager, device);
                                }
                        } else {
                                ply_trace ("device doesn't have a devices tag");
                        }
                } else {
                        ply_trace ("it's not initialized");
                }

                udev_device_unref (device);
        }

        udev_enumerate_unref (matches);

        return found_device;
}

static void
on_drm_udev_add_or_change (ply_device_manager_t *manager,
                           const char           *action,
                           const char           *device_path,
                           struct udev_device   *device)
{
        ply_renderer_t *renderer;
        bool changed;

        renderer = ply_hashtable_lookup (manager->renderers, (void *) device_path);
        if (renderer == NULL) {
                /* We also try to create the renderer again on change events,
                 * renderer creation fails when no outputs are connected and
                 * this may have changed.
                 */
                create_devices_for_udev_device (manager, device);
                return;
        }

        /* Renderer exists, bail if this is not a change event */
        if (strcmp (action, "change"))
                return;

        changed = ply_renderer_handle_change_event (renderer);
        if (changed) {
                free_displays_for_renderer (manager, renderer);
                create_pixel_displays_for_renderer (manager, renderer);
        }
}

static bool
verify_add_or_change (ply_device_manager_t *manager,
                      const char           *action,
                      const char           *device_path,
                      struct udev_device   *device)
{
        const char *subsystem = udev_device_get_subsystem (device);

        if (strcmp (action, "add") && strcmp (action, "change"))
                return false;

        subsystem = udev_device_get_subsystem (device);

        if (strcmp (subsystem, SUBSYSTEM_DRM) == 0) {
                if (manager->local_console_managed && manager->local_console_is_text) {
                        ply_trace ("ignoring since we're already using text splash for local console");
                        return false;
                }
        } else {
                ply_trace ("ignoring since we only handle subsystem %s devices after timeout", subsystem);
                return false;
        }

        return true;
}

static bool
duplicate_device_path (ply_list_t *events, const char *device_path)
{
        struct udev_device *device;
        ply_list_node_t *node;

        for (node = ply_list_get_first_node (events);
             node; node = ply_list_get_next_node (events, node)) {
                device = ply_list_node_get_data (node);

                if (strcmp (udev_device_get_devnode (device), device_path) == 0)
                        return true;
        }

        return false;
}

static void
process_udev_add_or_change_events (ply_device_manager_t *manager, ply_list_t *events)
{
        const char *action, *device_path;
        struct udev_device *device;
        ply_list_node_t *node;

        while ((node = ply_list_get_first_node (events))) {
                device = ply_list_node_get_data (node);
                action = udev_device_get_action (device);
                device_path = udev_device_get_devnode (device);

                on_drm_udev_add_or_change (manager, action, device_path, device);

                ply_list_remove_node (events, node);
                udev_device_unref (device);
        }
}

static void
on_udev_event (ply_device_manager_t *manager)
{
        const char *action, *device_path;
        struct udev_device *device;
        ply_list_t *pending_events;

        pending_events = ply_list_new ();

        /*
         * During the initial monitor/connector enumeration on boot the kernel
         * fires a large number of change events. If we process these 1 by 1,
         * we spend a lot of time probing the drm-connectors. So instead we
         * collect them all and then coalescence them so that if there are multiple
         * change events pending for a single card, we only re-probe the card once.
         */
        while ((device = udev_monitor_receive_device (manager->udev_monitor))) {
                action = udev_device_get_action (device);
                device_path = udev_device_get_devnode (device);

                if (action == NULL || device_path == NULL)
                        goto unref;

                ply_trace ("got %s event for device %s", action, device_path);

                /*
                 * Add/change events before and after a remove may not be
                 * coalesced together. So flush the queue and then process
                 * the remove event immediately.
                 */
                if (strcmp (action, "remove") == 0) {
                        process_udev_add_or_change_events (manager, pending_events);
                        free_devices_from_device_path (manager, device_path, true);
                        goto unref;
                }

                if (!verify_add_or_change (manager, action, device_path, device))
                        goto unref;

                if (duplicate_device_path (pending_events, device_path)) {
                        ply_trace ("ignoring duplicate %s event for device %s", action, device_path);
                        goto unref;
                }

                ply_list_append_data (pending_events, udev_device_ref(device));
unref:
                udev_device_unref (device);
        }

        process_udev_add_or_change_events (manager, pending_events);

        ply_list_free (pending_events);
}

static void
watch_for_udev_events (ply_device_manager_t *manager)
{
        int fd;

        assert (manager != NULL);

        if (manager->fd_watch != NULL)
                return;

        ply_trace ("watching for udev graphics device add and remove events");

        if (manager->udev_monitor == NULL) {
                manager->udev_monitor = udev_monitor_new_from_netlink (manager->udev_context, "udev");

                udev_monitor_filter_add_match_subsystem_devtype (manager->udev_monitor, SUBSYSTEM_DRM, NULL);
                udev_monitor_filter_add_match_subsystem_devtype (manager->udev_monitor, SUBSYSTEM_FRAME_BUFFER, NULL);
                udev_monitor_filter_add_match_tag (manager->udev_monitor, "seat");
                udev_monitor_enable_receiving (manager->udev_monitor);
        }

        fd = udev_monitor_get_fd (manager->udev_monitor);
        manager->fd_watch = ply_event_loop_watch_fd (manager->loop,
                                                     fd,
                                                     PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                                     (ply_event_handler_t)
                                                     on_udev_event,
                                                     NULL,
                                                     manager);
}

static void
stop_watching_for_udev_events (ply_device_manager_t *manager)
{
        if (manager->fd_watch == NULL)
                return;

        ply_event_loop_stop_watching_fd (manager->loop, manager->fd_watch);
        manager->fd_watch = NULL;
}
#endif

static void
free_terminal (char                 *device,
               ply_terminal_t       *terminal,
               ply_device_manager_t *manager)
{
        ply_hashtable_remove (manager->terminals, device);

        ply_terminal_free (terminal);
}

static void
free_terminals (ply_device_manager_t *manager)
{
        ply_hashtable_foreach (manager->terminals,
                               (ply_hashtable_foreach_func_t *)
                               free_terminal,
                               manager);
}

static ply_terminal_t *
get_terminal (ply_device_manager_t *manager,
              const char           *device_name)
{
        char *full_name = NULL;
        ply_terminal_t *terminal;

        if (strncmp (device_name, "/dev/", strlen ("/dev/")) == 0)
                full_name = strdup (device_name);
        else
                asprintf (&full_name, "/dev/%s", device_name);

        if (strcmp (full_name, "/dev/tty0") == 0 ||
            strcmp (full_name, "/dev/tty") == 0 ||
            strcmp (full_name, ply_terminal_get_name (manager->local_console_terminal)) == 0) {
                terminal = manager->local_console_terminal;

                ply_hashtable_insert (manager->terminals,
                                      (void *) ply_terminal_get_name (terminal),
                                      terminal);
                goto done;
        }

        terminal = ply_hashtable_lookup (manager->terminals, full_name);

        if (terminal == NULL) {
                terminal = ply_terminal_new (full_name);

                ply_hashtable_insert (manager->terminals,
                                      (void *) ply_terminal_get_name (terminal),
                                      terminal);
        }

done:
        free (full_name);
        return terminal;
}

static void
free_renderer (char                 *device_path,
               ply_renderer_t       *renderer,
               ply_device_manager_t *manager)
{
        free_devices_from_device_path (manager, device_path, false);
}

static void
free_renderers (ply_device_manager_t *manager)
{
        ply_hashtable_foreach (manager->renderers,
                               (ply_hashtable_foreach_func_t *)
                               free_renderer,
                               manager);
}

ply_device_manager_t *
ply_device_manager_new (const char                *default_tty,
                        ply_device_manager_flags_t flags)
{
        ply_device_manager_t *manager;

        manager = calloc (1, sizeof(ply_device_manager_t));
        manager->loop = NULL;
        manager->terminals = ply_hashtable_new (ply_hashtable_string_hash, ply_hashtable_string_compare);
        manager->renderers = ply_hashtable_new (ply_hashtable_string_hash, ply_hashtable_string_compare);
        manager->local_console_terminal = ply_terminal_new (default_tty);
        manager->keyboards = ply_list_new ();
        manager->text_displays = ply_list_new ();
        manager->pixel_displays = ply_list_new ();
        manager->flags = flags;

#ifdef HAVE_UDEV
        if (!(flags & PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV))
                manager->udev_context = udev_new ();
#else
        manager->flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV;
#endif

        attach_to_event_loop (manager, ply_event_loop_get_default ());

        return manager;
}

void
ply_device_manager_free (ply_device_manager_t *manager)
{
        ply_trace ("freeing device manager");

        if (manager == NULL)
                return;

        ply_event_loop_stop_watching_for_exit (manager->loop,
                                               (ply_event_loop_exit_handler_t)
                                               detach_from_event_loop,
                                               manager);

        free_terminals (manager);
        ply_hashtable_free (manager->terminals);

        free_renderers (manager);
        ply_hashtable_free (manager->renderers);

#ifdef HAVE_UDEV
        ply_event_loop_stop_watching_for_timeout (manager->loop,
                                         (ply_event_loop_timeout_handler_t)
                                         create_devices_from_udev, manager);

        if (manager->udev_monitor != NULL)
                udev_monitor_unref (manager->udev_monitor);

        if (manager->udev_context != NULL)
                udev_unref (manager->udev_context);
#endif

        free (manager);
}

static bool
add_consoles_from_file (ply_device_manager_t *manager,
                        const char           *path)
{
        int fd;
        char contents[512] = "";
        ssize_t contents_length;
        bool has_serial_consoles;
        const char *remaining_file_contents;

        ply_trace ("opening %s", path);
        fd = open (path, O_RDONLY);

        if (fd < 0) {
                ply_trace ("couldn't open it: %m");
                return false;
        }

        ply_trace ("reading file");
        contents_length = read (fd, contents, sizeof(contents) - 1);

        if (contents_length <= 0) {
                ply_trace ("couldn't read it: %m");
                close (fd);
                return false;
        }
        close (fd);

        remaining_file_contents = contents;
        has_serial_consoles = false;

        while (remaining_file_contents < contents + contents_length) {
                char *console;
                size_t console_length;
                const char *console_device;
                ply_terminal_t *terminal;

                /* Advance past any leading whitespace */
                remaining_file_contents += strspn (remaining_file_contents, " \n\t\v");

                if (*remaining_file_contents == '\0')
                        /* There's nothing left after the whitespace, we're done */
                        break;

                /* Find trailing whitespace and NUL terminate.  If strcspn
                 * doesn't find whitespace, it gives us the length of the string
                 * until the next NUL byte, which we'll just overwrite with
                 * another NUL byte anyway. */
                console_length = strcspn (remaining_file_contents, " \n\t\v");
                console = strndup (remaining_file_contents, console_length);

                terminal = get_terminal (manager, console);
                console_device = ply_terminal_get_name (terminal);

                free (console);

                ply_trace ("console %s found!", console_device);

                if (terminal != manager->local_console_terminal)
                        has_serial_consoles = true;

                /* Move past the parsed console string, and the whitespace we
                 * may have found above.  If we found a NUL above and not whitespace,
                 * then we're going to jump past the end of the buffer and the loop
                 * will terminate
                 */
                remaining_file_contents += console_length + 1;
        }

        return has_serial_consoles;
}

static void
create_pixel_displays_for_renderer (ply_device_manager_t *manager,
                                    ply_renderer_t       *renderer)
{
        ply_list_t *heads;
        ply_list_node_t *node;

        heads = ply_renderer_get_heads (renderer);

        ply_trace ("Adding displays for %d heads",
                   ply_list_get_length (heads));

        node = ply_list_get_first_node (heads);
        while (node != NULL) {
                ply_list_node_t *next_node;
                ply_renderer_head_t *head;
                ply_pixel_display_t *display;

                head = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (heads, node);

                display = ply_pixel_display_new (renderer, head);

                ply_list_append_data (manager->pixel_displays, display);

                if (manager->pixel_display_added_handler != NULL)
                        manager->pixel_display_added_handler (manager->event_handler_data, display);
                node = next_node;
        }
}

static void
create_text_displays_for_terminal (ply_device_manager_t *manager,
                                   ply_terminal_t       *terminal)
{
  ply_text_display_t *display;

  if (!ply_terminal_is_open (terminal)) {
          if (!ply_terminal_open (terminal)) {
                  ply_trace ("could not add terminal %s: %m",
                             ply_terminal_get_name (terminal));
                  return;
          }
  }

  ply_trace ("adding text display for terminal %s",
             ply_terminal_get_name (terminal));

  display = ply_text_display_new (terminal);
  ply_list_append_data (manager->text_displays, display);

  if (manager->text_display_added_handler != NULL)
          manager->text_display_added_handler (manager->event_handler_data, display);
}

static bool
create_devices_for_terminal_and_renderer_type (ply_device_manager_t *manager,
                                               const char           *device_path,
                                               ply_terminal_t       *terminal,
                                               ply_renderer_type_t   renderer_type)
{
        ply_renderer_t *renderer = NULL;
        ply_keyboard_t *keyboard = NULL;

        if (device_path != NULL)
                renderer = ply_hashtable_lookup (manager->renderers, (void *) device_path);

        if (renderer != NULL) {
                ply_trace ("ignoring device %s since it's already managed", device_path);
                return true;
        }

        ply_trace ("creating devices for %s (renderer type: %u) (terminal: %s)",
                   device_path ? : "", renderer_type, terminal ? ply_terminal_get_name (terminal) : "none");

        if (renderer_type != PLY_RENDERER_TYPE_NONE) {
                ply_renderer_t *old_renderer = NULL;
                renderer = ply_renderer_new (renderer_type, device_path, terminal);

                if (renderer != NULL && !ply_renderer_open (renderer)) {
                        ply_trace ("could not open renderer for %s", device_path);
                        ply_renderer_free (renderer);
                        renderer = NULL;

                        if (renderer_type != PLY_RENDERER_TYPE_AUTO)
                                return false;
                }

                if (renderer != NULL) {
                        old_renderer = ply_hashtable_lookup (manager->renderers,
                                                             (void *) ply_renderer_get_device_name (renderer));

                        if (old_renderer != NULL) {
                                ply_trace ("ignoring device %s since it's already managed",
                                           ply_renderer_get_device_name (renderer));
                                ply_renderer_free (renderer);

                                renderer = NULL;
                                return true;
                        }
                }
        }

        if (renderer != NULL) {
                keyboard = ply_keyboard_new_for_renderer (renderer);
                ply_list_append_data (manager->keyboards, keyboard);

                if (manager->keyboard_added_handler != NULL)
                        manager->keyboard_added_handler (manager->event_handler_data, keyboard);

                ply_hashtable_insert (manager->renderers, strdup (ply_renderer_get_device_name (renderer)), renderer);
                create_pixel_displays_for_renderer (manager, renderer);

                if (manager->renderers_activated) {
                        ply_trace ("activating renderer");
                        ply_renderer_activate (renderer);
                }

                if (terminal != NULL)
                        ply_terminal_refresh_geometry (terminal);
        } else if (terminal != NULL) {
                keyboard = ply_keyboard_new_for_terminal (terminal);
                ply_list_append_data (manager->keyboards, keyboard);

                if (manager->keyboard_added_handler != NULL)
                        manager->keyboard_added_handler (manager->event_handler_data, keyboard);
        }

        if (terminal != NULL) {
                create_text_displays_for_terminal (manager, terminal);

                if (terminal == manager->local_console_terminal) {
                        manager->local_console_is_text = renderer == NULL;
                        manager->local_console_managed = true;
                }
        }

        if (keyboard != NULL && manager->keyboards_activated) {
                ply_trace ("activating keyboards");
                ply_keyboard_watch_for_input (keyboard);
        }

        return true;
}

static void
create_devices_for_terminal (const char           *device_path,
                             ply_terminal_t       *terminal,
                             ply_device_manager_t *manager)
{
        create_devices_for_terminal_and_renderer_type (manager,
                                                       NULL,
                                                       terminal,
                                                       PLY_RENDERER_TYPE_NONE);
}
static bool
create_devices_from_terminals (ply_device_manager_t *manager)
{
        bool has_serial_consoles;

        ply_trace ("checking for consoles");

        if (manager->flags & PLY_DEVICE_MANAGER_FLAGS_IGNORE_SERIAL_CONSOLES) {
                has_serial_consoles = false;
                ply_trace ("ignoring all consoles but default console because explicitly told to.");
        } else {
                has_serial_consoles = add_consoles_from_file (manager, "/sys/class/tty/console/active");
        }

        if (has_serial_consoles) {
                ply_trace ("serial consoles detected, managing them with details forced");
                manager->serial_consoles_detected = true;

                ply_hashtable_foreach (manager->terminals,
                                       (ply_hashtable_foreach_func_t *)
                                       create_devices_for_terminal,
                                       manager);
                return true;
        }

        return false;
}

static void
create_non_graphical_devices (ply_device_manager_t *manager)
{
        create_devices_for_terminal_and_renderer_type (manager,
                                                       NULL,
                                                       manager->local_console_terminal,
                                                       PLY_RENDERER_TYPE_NONE);
}

#ifdef HAVE_UDEV
static void
create_devices_from_udev (ply_device_manager_t *manager)
{
        manager->device_timeout_elapsed = true;

        if (manager->paused) {
                ply_trace ("create_devices_from_udev timeout elapsed while paused, deferring execution");
                return;
        }

        ply_trace ("Timeout elapsed, looking for devices from udev");

        create_devices_for_subsystem (manager, SUBSYSTEM_DRM);
        create_devices_for_subsystem (manager, SUBSYSTEM_FRAME_BUFFER);

        if (manager->found_drm_device || manager->found_fb_device)
                return;

        ply_trace ("Creating non-graphical devices, since there's no suitable graphics hardware");
        create_non_graphical_devices (manager);
}
#endif

static void
create_fallback_devices (ply_device_manager_t *manager)
{
        create_devices_for_terminal_and_renderer_type (manager,
                                                       NULL,
                                                       manager->local_console_terminal,
                                                       PLY_RENDERER_TYPE_AUTO);
}

void
ply_device_manager_watch_devices (ply_device_manager_t                *manager,
                                  double                               device_timeout,
                                  ply_keyboard_added_handler_t         keyboard_added_handler,
                                  ply_keyboard_removed_handler_t       keyboard_removed_handler,
                                  ply_pixel_display_added_handler_t    pixel_display_added_handler,
                                  ply_pixel_display_removed_handler_t  pixel_display_removed_handler,
                                  ply_text_display_added_handler_t     text_display_added_handler,
                                  ply_text_display_removed_handler_t   text_display_removed_handler,
                                  void                                *data)
{
        bool done_with_initial_devices_setup;

        manager->keyboard_added_handler = keyboard_added_handler;
        manager->keyboard_removed_handler = keyboard_removed_handler;
        manager->pixel_display_added_handler = pixel_display_added_handler;
        manager->pixel_display_removed_handler = pixel_display_removed_handler;
        manager->text_display_added_handler = text_display_added_handler;
        manager->text_display_removed_handler = text_display_removed_handler;
        manager->event_handler_data = data;

        /* Try to create devices for each serial device right away, if possible
         */
        done_with_initial_devices_setup = create_devices_from_terminals (manager);

        if (done_with_initial_devices_setup)
                return;

        if ((manager->flags & PLY_DEVICE_MANAGER_FLAGS_SKIP_RENDERERS)) {
                ply_trace ("Creating non-graphical devices, since renderers are being explicitly skipped");
                create_non_graphical_devices (manager);
                return;
        }

        if ((manager->flags & PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV)) {
                ply_trace ("udev support disabled, creating fallback devices");
                create_fallback_devices (manager);
                return;
        }

#ifdef HAVE_UDEV
        watch_for_udev_events (manager);
        create_devices_for_subsystem (manager, SUBSYSTEM_DRM);
        ply_event_loop_watch_for_timeout (manager->loop,
                                         device_timeout,
                                         (ply_event_loop_timeout_handler_t)
                                         create_devices_from_udev, manager);
#endif
}

bool
ply_device_manager_has_displays (ply_device_manager_t *manager)
{
        return ply_list_get_length (manager->pixel_displays) > 0 ||
                ply_list_get_length (manager->text_displays) > 0;
}

ply_list_t *
ply_device_manager_get_keyboards (ply_device_manager_t *manager)
{
        return manager->keyboards;
}

ply_list_t *
ply_device_manager_get_pixel_displays (ply_device_manager_t *manager)
{
        return manager->pixel_displays;
}

ply_list_t *
ply_device_manager_get_text_displays (ply_device_manager_t *manager)
{
        return manager->text_displays;
}

ply_terminal_t *
ply_device_manager_get_default_terminal (ply_device_manager_t *manager)
{
        return manager->local_console_terminal;
}

bool
ply_device_manager_has_serial_consoles (ply_device_manager_t *manager)
{
        return manager->serial_consoles_detected;
}

static void
activate_renderer (char                 *device_path,
                   ply_renderer_t       *renderer,
                   ply_device_manager_t *manager)
{
        ply_renderer_activate (renderer);
}

void
ply_device_manager_activate_renderers (ply_device_manager_t *manager)
{
        ply_trace ("activating renderers");
        ply_hashtable_foreach (manager->renderers,
                               (ply_hashtable_foreach_func_t *)
                               activate_renderer,
                               manager);

        manager->renderers_activated = true;
}

static void
deactivate_renderer (char                 *device_path,
                     ply_renderer_t       *renderer,
                     ply_device_manager_t *manager)
{
        ply_renderer_deactivate (renderer);
}

void
ply_device_manager_deactivate_renderers (ply_device_manager_t *manager)
{
        ply_hashtable_foreach (manager->renderers,
                               (ply_hashtable_foreach_func_t *)
                               deactivate_renderer,
                               manager);

        manager->renderers_activated = false;
}

void
ply_device_manager_activate_keyboards (ply_device_manager_t *manager)
{
        ply_list_node_t *node;

        ply_trace ("activating keyboards");
        node = ply_list_get_first_node (manager->keyboards);
        while (node != NULL) {
                ply_keyboard_t *keyboard;
                ply_list_node_t *next_node;

                keyboard = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (manager->keyboards, node);

                ply_keyboard_watch_for_input (keyboard);

                node = next_node;
        }

        manager->keyboards_activated = true;
}

void
ply_device_manager_deactivate_keyboards (ply_device_manager_t *manager)
{
        ply_list_node_t *node;

        ply_trace ("deactivating keyboards");
        node = ply_list_get_first_node (manager->keyboards);
        while (node != NULL) {
                ply_keyboard_t *keyboard;
                ply_list_node_t *next_node;

                keyboard = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (manager->keyboards, node);

                ply_keyboard_stop_watching_for_input (keyboard);

                node = next_node;
        }

        manager->keyboards_activated = false;
}

void
ply_device_manager_pause (ply_device_manager_t *manager)
{
        ply_trace ("ply_device_manager_pause() called, stopping watching for udev events");
        manager->paused = true;
#ifdef HAVE_UDEV
        stop_watching_for_udev_events (manager);
#endif
}

void
ply_device_manager_unpause (ply_device_manager_t *manager)
{
        ply_trace ("ply_device_manager_unpause() called, resuming watching for udev events");
        manager->paused = false;
#ifdef HAVE_UDEV
        if (manager->device_timeout_elapsed) {
                ply_trace ("ply_device_manager_unpause(): timeout elapsed while paused, looking for udev devices");
                create_devices_from_udev (manager);
        }
        watch_for_udev_events (manager);
#endif
}
