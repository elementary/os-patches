/*
 * Wayland Support
 *
 * Copyright (C) 2012,2013 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "wayland/meta-wayland.h"

#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <wayland-server.h>

#include "clutter/clutter.h"
#include "cogl/cogl-egl.h"
#include "compositor/meta-surface-actor-wayland.h"
#include "core/meta-context-private.h"
#include "wayland/meta-wayland-activation.h"
#include "wayland/meta-wayland-buffer.h"
#include "wayland/meta-wayland-data-device.h"
#include "wayland/meta-wayland-dma-buf.h"
#include "wayland/meta-wayland-egl-stream.h"
#include "wayland/meta-wayland-inhibit-shortcuts-dialog.h"
#include "wayland/meta-wayland-inhibit-shortcuts.h"
#include "wayland/meta-wayland-outputs.h"
#include "wayland/meta-wayland-presentation-time-private.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-region.h"
#include "wayland/meta-wayland-seat.h"
#include "wayland/meta-wayland-subsurface.h"
#include "wayland/meta-wayland-tablet-manager.h"
#include "wayland/meta-wayland-xdg-foreign.h"
#include "wayland/meta-xwayland-grab-keyboard.h"
#include "wayland/meta-xwayland-private.h"
#include "wayland/meta-xwayland.h"

#ifdef HAVE_NATIVE_BACKEND
#include "backends/native/meta-renderer-native.h"
#endif

static char *_display_name_override;

typedef struct _MetaWaylandCompositorPrivate
{
  gboolean is_wayland_egl_display_bound;
} MetaWaylandCompositorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandCompositor, meta_wayland_compositor,
                            G_TYPE_OBJECT)

MetaWaylandCompositor *
meta_wayland_compositor_get_default (void)
{
  MetaBackend *backend;
  MetaContext *context;
  MetaWaylandCompositor *wayland_compositor;

  backend = meta_get_backend ();
  context = meta_backend_get_context (backend);
  wayland_compositor = meta_context_get_wayland_compositor (context);
  g_assert (wayland_compositor);

  return wayland_compositor;
}

typedef struct
{
  GSource source;
  struct wl_display *display;
} WaylandEventSource;

static gboolean
wayland_event_source_prepare (GSource *base,
                              int     *timeout)
{
  WaylandEventSource *source = (WaylandEventSource *)base;

  *timeout = -1;

  wl_display_flush_clients (source->display);

  return FALSE;
}

static gboolean
wayland_event_source_dispatch (GSource    *base,
                               GSourceFunc callback,
                               void       *data)
{
  WaylandEventSource *source = (WaylandEventSource *)base;
  struct wl_event_loop *loop = wl_display_get_event_loop (source->display);

  wl_event_loop_dispatch (loop, 0);

  return TRUE;
}

static GSourceFuncs wayland_event_source_funcs =
{
  wayland_event_source_prepare,
  NULL,
  wayland_event_source_dispatch,
  NULL
};

static GSource *
wayland_event_source_new (struct wl_display *display)
{
  GSource *source;
  WaylandEventSource *wayland_source;
  struct wl_event_loop *loop = wl_display_get_event_loop (display);

  source = g_source_new (&wayland_event_source_funcs,
                         sizeof (WaylandEventSource));
  g_source_set_name (source, "[mutter] Wayland events");
  wayland_source = (WaylandEventSource *) source;
  wayland_source->display = display;
  g_source_add_unix_fd (&wayland_source->source,
                        wl_event_loop_get_fd (loop),
                        G_IO_IN | G_IO_ERR);

  return &wayland_source->source;
}

void
meta_wayland_compositor_set_input_focus (MetaWaylandCompositor *compositor,
                                         MetaWindow            *window)
{
  MetaWaylandSurface *surface = window ? window->surface : NULL;

  meta_wayland_seat_set_input_focus (compositor->seat, surface);
}

static void
wl_compositor_create_surface (struct wl_client   *client,
                              struct wl_resource *resource,
                              uint32_t            id)
{
  MetaWaylandCompositor *compositor = wl_resource_get_user_data (resource);

  meta_wayland_surface_create (compositor, client, resource, id);
}

static void
wl_compositor_create_region (struct wl_client   *client,
                             struct wl_resource *resource,
                             uint32_t            id)
{
  MetaWaylandCompositor *compositor = wl_resource_get_user_data (resource);

  meta_wayland_region_create (compositor, client, resource, id);
}

static const struct wl_compositor_interface meta_wayland_wl_compositor_interface = {
  wl_compositor_create_surface,
  wl_compositor_create_region
};

static void
compositor_bind (struct wl_client *client,
                 void             *data,
                 uint32_t          version,
                 uint32_t          id)
{
  MetaWaylandCompositor *compositor = data;
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_compositor_interface, version, id);
  wl_resource_set_implementation (resource,
                                  &meta_wayland_wl_compositor_interface,
                                  compositor, NULL);
}

/**
 * meta_wayland_compositor_update:
 * @compositor: the #MetaWaylandCompositor instance
 * @event: the #ClutterEvent used to update @seat's state
 *
 * This is used to update display server state like updating cursor
 * position and keeping track of buttons and keys pressed. It must be
 * called for all input events coming from the underlying devices.
 */
void
meta_wayland_compositor_update (MetaWaylandCompositor *compositor,
                                const ClutterEvent    *event)
{
  if (meta_wayland_tablet_manager_consumes_event (compositor->tablet_manager, event))
    meta_wayland_tablet_manager_update (compositor->tablet_manager, event);
  else
    meta_wayland_seat_update (compositor->seat, event);
}

static void
on_after_update (ClutterStage          *stage,
                 ClutterStageView      *stage_view,
                 MetaWaylandCompositor *compositor)
{
  GList *l;
  int64_t now_us;

  now_us = g_get_monotonic_time ();

  l = compositor->frame_callback_surfaces;
  while (l)
    {
      GList *l_cur = l;
      MetaWaylandSurface *surface = l->data;
      MetaSurfaceActor *actor;
      MetaWaylandActorSurface *actor_surface;

      l = l->next;

      actor = meta_wayland_surface_get_actor (surface);
      if (!actor)
        continue;

      if (!meta_surface_actor_wayland_is_view_primary (actor,
                                                       stage_view))
        continue;

      actor_surface = META_WAYLAND_ACTOR_SURFACE (surface->role);
      meta_wayland_actor_surface_emit_frame_callbacks (actor_surface,
                                                       now_us / 1000);

      compositor->frame_callback_surfaces =
        g_list_delete_link (compositor->frame_callback_surfaces, l_cur);
    }
}

static MetaWaylandOutput *
get_output_for_stage_view (MetaWaylandCompositor *compositor,
                           ClutterStageView      *stage_view)
{
  MetaCrtc *crtc;
  MetaOutput *output;
  MetaMonitor *monitor;

  crtc = meta_renderer_view_get_crtc (META_RENDERER_VIEW (stage_view));

  /*
   * All outputs occupy the same region of the screen, as their contents are
   * the same, so pick the first one.
   */
  output = meta_crtc_get_outputs (crtc)->data;

  monitor = meta_output_get_monitor (output);
  return g_hash_table_lookup (compositor->outputs,
                              meta_monitor_get_spec (monitor));
}

static void
on_presented (ClutterStage          *stage,
              ClutterStageView      *stage_view,
              ClutterFrameInfo      *frame_info,
              MetaWaylandCompositor *compositor)
{
  MetaWaylandPresentationFeedback *feedback, *next;
  struct wl_list *feedbacks;
  MetaWaylandOutput *output;

  feedbacks =
    meta_wayland_presentation_time_ensure_feedbacks (&compositor->presentation_time,
                                                     stage_view);

  output = get_output_for_stage_view (compositor, stage_view);

  wl_list_for_each_safe (feedback, next, feedbacks, link)
    {
      meta_wayland_presentation_feedback_present (feedback,
                                                  frame_info,
                                                  output);
    }
}

/**
 * meta_wayland_compositor_handle_event:
 * @compositor: the #MetaWaylandCompositor instance
 * @event: the #ClutterEvent to be sent
 *
 * This method sends events to the focused wayland client, if any.
 *
 * Return value: whether @event was sent to a wayland client.
 */
gboolean
meta_wayland_compositor_handle_event (MetaWaylandCompositor *compositor,
                                      const ClutterEvent    *event)
{
  if (meta_wayland_tablet_manager_handle_event (compositor->tablet_manager,
                                                event))
    return TRUE;

  return meta_wayland_seat_handle_event (compositor->seat, event);
}

/* meta_wayland_compositor_update_key_state:
 * @compositor: the #MetaWaylandCompositor
 * @key_vector: bit vector of key states
 * @key_vector_len: length of @key_vector
 * @offset: the key for the first evdev keycode is found at this offset in @key_vector
 *
 * This function is used to resynchronize the key state that Mutter
 * is tracking with the actual keyboard state. This is useful, for example,
 * to handle changes in key state when a nested compositor doesn't
 * have focus. We need to fix up the XKB modifier tracking and deliver
 * any modifier changes to clients.
 */
void
meta_wayland_compositor_update_key_state (MetaWaylandCompositor *compositor,
                                          char                  *key_vector,
                                          int                    key_vector_len,
                                          int                    offset)
{
  meta_wayland_keyboard_update_key_state (compositor->seat->keyboard,
                                          key_vector, key_vector_len, offset);
}

void
meta_wayland_compositor_add_frame_callback_surface (MetaWaylandCompositor *compositor,
                                                    MetaWaylandSurface    *surface)
{
  if (g_list_find (compositor->frame_callback_surfaces, surface))
    return;

  compositor->frame_callback_surfaces =
    g_list_prepend (compositor->frame_callback_surfaces, surface);
}

void
meta_wayland_compositor_remove_frame_callback_surface (MetaWaylandCompositor *compositor,
                                                       MetaWaylandSurface    *surface)
{
  compositor->frame_callback_surfaces =
    g_list_remove (compositor->frame_callback_surfaces, surface);
}

void
meta_wayland_compositor_add_presentation_feedback_surface (MetaWaylandCompositor *compositor,
                                                           MetaWaylandSurface    *surface)
{
  if (g_list_find (compositor->presentation_time.feedback_surfaces, surface))
    return;

  compositor->presentation_time.feedback_surfaces =
    g_list_prepend (compositor->presentation_time.feedback_surfaces, surface);
}

void
meta_wayland_compositor_remove_presentation_feedback_surface (MetaWaylandCompositor *compositor,
                                                              MetaWaylandSurface    *surface)
{
  compositor->presentation_time.feedback_surfaces =
    g_list_remove (compositor->presentation_time.feedback_surfaces, surface);
}

static void
set_gnome_env (const char *name,
	       const char *value)
{
  GDBusConnection *session_bus;
  GError *error = NULL;
  g_autoptr (GVariant) result = NULL;

  setenv (name, value, TRUE);

  session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (session_bus);

  result = g_dbus_connection_call_sync (session_bus,
			       "org.gnome.SessionManager",
			       "/org/gnome/SessionManager",
			       "org.gnome.SessionManager",
			       "Setenv",
			       g_variant_new ("(ss)", name, value),
			       NULL,
			       G_DBUS_CALL_FLAGS_NO_AUTO_START,
			       -1, NULL, &error);
  if (error)
    {
      char *remote_error;

      remote_error = g_dbus_error_get_remote_error (error);
      if (g_strcmp0 (remote_error, "org.gnome.SessionManager.NotInInitialization") != 0)
        {
          meta_warning ("Failed to set environment variable %s for gnome-session: %s",
                        name, error->message);
        }

      g_free (remote_error);
      g_error_free (error);
    }
}

void
meta_wayland_compositor_init_display (MetaWaylandCompositor *compositor,
                                      MetaDisplay           *display)
{
  meta_xwayland_init_display (&compositor->xwayland_manager, display);
}

static void meta_wayland_log_func (const char *, va_list) G_GNUC_PRINTF (1, 0);

static void
meta_wayland_log_func (const char *fmt,
                       va_list     arg)
{
  char *str = g_strdup_vprintf (fmt, arg);
  g_warning ("WL: %s", str);
  g_free (str);
}

void
meta_wayland_compositor_prepare_shutdown (MetaWaylandCompositor *compositor)
{
  MetaX11DisplayPolicy x11_display_policy;

  x11_display_policy =
    meta_context_get_x11_display_policy (compositor->context);
  if (x11_display_policy != META_X11_DISPLAY_POLICY_DISABLED)
    meta_xwayland_shutdown (&compositor->xwayland_manager);

  if (compositor->wayland_display)
    wl_display_destroy_clients (compositor->wayland_display);
}

static void
meta_wayland_compositor_finalize (GObject *object)
{
  MetaWaylandCompositor *compositor = META_WAYLAND_COMPOSITOR (object);

  g_clear_object (&compositor->dma_buf_manager);

  g_clear_pointer (&compositor->seat, meta_wayland_seat_free);

  g_clear_pointer (&compositor->display_name, g_free);
  g_clear_pointer (&compositor->wayland_display, wl_display_destroy);
  g_clear_pointer (&compositor->source, g_source_destroy);

  G_OBJECT_CLASS (meta_wayland_compositor_parent_class)->finalize (object);
}

static void
meta_wayland_compositor_init (MetaWaylandCompositor *compositor)
{
  compositor->scheduled_surface_associations = g_hash_table_new (NULL, NULL);

  wl_log_set_handler_server (meta_wayland_log_func);

  compositor->wayland_display = wl_display_create ();
  if (compositor->wayland_display == NULL)
    g_error ("Failed to create the global wl_display");
}

static void
meta_wayland_compositor_class_init (MetaWaylandCompositorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_wayland_compositor_finalize;
}

static bool
meta_xwayland_global_filter (const struct wl_client *client,
                             const struct wl_global *global,
                             void                   *data)
{
  MetaWaylandCompositor *compositor = (MetaWaylandCompositor *) data;
  MetaXWaylandManager *xwayland_manager = &compositor->xwayland_manager;

  /* Keyboard grabbing protocol is for Xwayland only */
  if (client != xwayland_manager->client)
    return (wl_global_get_interface (global) !=
            &zwp_xwayland_keyboard_grab_manager_v1_interface);

  /* All others are visible to all clients */
  return true;
}

void
meta_wayland_override_display_name (const char *display_name)
{
  g_clear_pointer (&_display_name_override, g_free);
  _display_name_override = g_strdup (display_name);
}

static const char *
meta_wayland_get_xwayland_auth_file (MetaWaylandCompositor *compositor)
{
  return compositor->xwayland_manager.auth_file;
}

static void
meta_wayland_init_egl (MetaWaylandCompositor *compositor)
{
  MetaWaylandCompositorPrivate *priv =
    meta_wayland_compositor_get_instance_private (compositor);
  MetaBackend *backend = meta_get_backend ();
  MetaEgl *egl = meta_backend_get_egl (backend);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  EGLDisplay egl_display = cogl_egl_context_get_egl_display (cogl_context);
  g_autoptr (GError) error = NULL;

  if (!meta_egl_has_extensions (egl, egl_display, NULL,
                                "EGL_WL_bind_wayland_display",
                                NULL))
    {
      meta_topic (META_DEBUG_WAYLAND,
                  "Not binding Wayland display, missing extension");
      return;
    }

  meta_topic (META_DEBUG_WAYLAND,
              "Binding Wayland EGL display");

  if (meta_egl_bind_wayland_display (egl,
                                     egl_display,
                                     compositor->wayland_display,
                                     &error))
    priv->is_wayland_egl_display_bound = TRUE;
  else
    g_warning ("Failed to bind Wayland display: %s", error->message);
}

static void
init_dma_buf_support (MetaWaylandCompositor *compositor)
{
  g_autoptr (GError) error = NULL;

  compositor->dma_buf_manager = meta_wayland_dma_buf_manager_new (compositor,
                                                                  &error);
  if (!compositor->dma_buf_manager)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        {
          meta_topic (META_DEBUG_WAYLAND,
                      "Wayland DMA buffer protocol support not enabled: %s",
                      error->message);
        }
      else
        {
          g_warning ("Wayland DMA buffer protocol support not enabled: %s",
                     error->message);
        }
    }
}

MetaWaylandCompositor *
meta_wayland_compositor_new (MetaContext *context)
{
  MetaBackend *backend = meta_context_get_backend (context);
  ClutterActor *stage = meta_backend_get_stage (backend);
  MetaWaylandCompositor *compositor;
  GSource *wayland_event_source;
  MetaX11DisplayPolicy x11_display_policy;

  compositor = g_object_new (META_TYPE_WAYLAND_COMPOSITOR, NULL);
  compositor->context = context;

  wayland_event_source = wayland_event_source_new (compositor->wayland_display);

  /* XXX: Here we are setting the wayland event source to have a
   * slightly lower priority than the X event source, because we are
   * much more likely to get confused being told about surface changes
   * relating to X clients when we don't know what's happened to them
   * according to the X protocol.
   */
  g_source_set_priority (wayland_event_source, GDK_PRIORITY_EVENTS + 1);
  g_source_attach (wayland_event_source, NULL);
  compositor->source = wayland_event_source;
  g_source_unref (wayland_event_source);

  g_signal_connect (stage, "after-update",
                    G_CALLBACK (on_after_update), compositor);
  g_signal_connect (stage, "presented",
                    G_CALLBACK (on_presented), compositor);

  if (!wl_global_create (compositor->wayland_display,
                         &wl_compositor_interface,
                         META_WL_COMPOSITOR_VERSION,
                         compositor, compositor_bind))
    g_error ("Failed to register the global wl_compositor");

  meta_wayland_init_egl (compositor);
  meta_wayland_init_shm (compositor);

  meta_wayland_outputs_init (compositor);
  meta_wayland_data_device_manager_init (compositor);
  meta_wayland_data_device_primary_manager_init (compositor);
  meta_wayland_data_device_primary_legacy_manager_init (compositor);
  meta_wayland_subsurfaces_init (compositor);
  meta_wayland_shell_init (compositor);
  meta_wayland_pointer_gestures_init (compositor);
  meta_wayland_tablet_manager_init (compositor);
  meta_wayland_seat_init (compositor);
  meta_wayland_relative_pointer_init (compositor);
  meta_wayland_pointer_constraints_init (compositor);
  meta_wayland_xdg_foreign_init (compositor);
  init_dma_buf_support (compositor);
  meta_wayland_keyboard_shortcuts_inhibit_init (compositor);
  meta_wayland_surface_inhibit_shortcuts_dialog_init ();
  meta_wayland_text_input_init (compositor);
  meta_wayland_init_presentation_time (compositor);
  meta_wayland_activation_init (compositor);

  /* Xwayland specific protocol, needs to be filtered out for all other clients */
  if (meta_xwayland_grab_keyboard_init (compositor))
    wl_display_set_global_filter (compositor->wayland_display,
                                  meta_xwayland_global_filter,
                                  compositor);

#ifdef HAVE_WAYLAND_EGLSTREAM
  {
    gboolean should_enable_eglstream_controller = TRUE;
#if defined(HAVE_EGL_DEVICE) && defined(HAVE_NATIVE_BACKEND)
    MetaRenderer *renderer = meta_backend_get_renderer (backend);

    if (META_IS_RENDERER_NATIVE (renderer))
      {
        MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);

        if (meta_renderer_native_get_mode (renderer_native) ==
            META_RENDERER_NATIVE_MODE_GBM)
          should_enable_eglstream_controller = FALSE;
      }
#endif /* defined(HAVE_EGL_DEVICE) && defined(HAVE_NATIVE_BACKEND) */

    if (should_enable_eglstream_controller)
      meta_wayland_eglstream_controller_init (compositor);
  }
#endif /* HAVE_WAYLAND_EGLSTREAM */

  x11_display_policy =
    meta_context_get_x11_display_policy (compositor->context);
  if (x11_display_policy != META_X11_DISPLAY_POLICY_DISABLED)
    {
      g_autoptr (GError) error = NULL;

      if (!meta_xwayland_init (&compositor->xwayland_manager,
                               compositor,
                               compositor->wayland_display,
                               &error))
        g_error ("Failed to start X Wayland: %s", error->message);
    }

  if (_display_name_override)
    {
      compositor->display_name = g_steal_pointer (&_display_name_override);

      if (wl_display_add_socket (compositor->wayland_display,
                                 compositor->display_name) != 0)
        g_error ("Failed to create_socket");
    }
  else
    {
      const char *display_name;

      display_name = wl_display_add_socket_auto (compositor->wayland_display);
      if (!display_name)
        g_error ("Failed to create socket");

      compositor->display_name = g_strdup (display_name);
    }

  g_message ("Using Wayland display name '%s'", compositor->display_name);

  if (x11_display_policy != META_X11_DISPLAY_POLICY_DISABLED)
    {
      set_gnome_env ("GNOME_SETUP_DISPLAY", compositor->xwayland_manager.private_connection.name);
      set_gnome_env ("DISPLAY", compositor->xwayland_manager.public_connection.name);
      set_gnome_env ("XAUTHORITY", meta_wayland_get_xwayland_auth_file (compositor));
    }

  set_gnome_env ("WAYLAND_DISPLAY", meta_wayland_get_wayland_display_name (compositor));

  return compositor;
}

const char *
meta_wayland_get_wayland_display_name (MetaWaylandCompositor *compositor)
{
  return compositor->display_name;
}

const char *
meta_wayland_get_public_xwayland_display_name (MetaWaylandCompositor *compositor)
{
  return compositor->xwayland_manager.public_connection.name;
}

const char *
meta_wayland_get_private_xwayland_display_name (MetaWaylandCompositor *compositor)
{
  return compositor->xwayland_manager.private_connection.name;
}

void
meta_wayland_compositor_restore_shortcuts (MetaWaylandCompositor *compositor,
                                           ClutterInputDevice    *source)
{
  MetaWaylandKeyboard *keyboard;

  /* Clutter is not multi-seat aware yet, use the default seat instead */
  keyboard = compositor->seat->keyboard;
  if (!keyboard || !keyboard->focus_surface)
    return;

  if (!meta_wayland_surface_is_shortcuts_inhibited (keyboard->focus_surface,
                                                    compositor->seat))
    return;

  meta_wayland_surface_restore_shortcuts (keyboard->focus_surface,
                                          compositor->seat);
}

gboolean
meta_wayland_compositor_is_shortcuts_inhibited (MetaWaylandCompositor *compositor,
                                                ClutterInputDevice    *source)
{
  MetaWaylandKeyboard *keyboard;

  /* Clutter is not multi-seat aware yet, use the default seat instead */
  keyboard = compositor->seat->keyboard;
  if (keyboard && keyboard->focus_surface != NULL)
    return meta_wayland_surface_is_shortcuts_inhibited (keyboard->focus_surface,
                                                        compositor->seat);

  return FALSE;
}

void
meta_wayland_compositor_flush_clients (MetaWaylandCompositor *compositor)
{
  wl_display_flush_clients (compositor->wayland_display);
}

static void on_scheduled_association_unmanaged (MetaWindow *window,
                                                gpointer    user_data);

static void
meta_wayland_compositor_remove_surface_association (MetaWaylandCompositor *compositor,
                                                    int                    id)
{
  MetaWindow *window;

  window = g_hash_table_lookup (compositor->scheduled_surface_associations,
                                GINT_TO_POINTER (id));
  if (window)
    {
      g_signal_handlers_disconnect_by_func (window,
                                            on_scheduled_association_unmanaged,
                                            GINT_TO_POINTER (id));
      g_hash_table_remove (compositor->scheduled_surface_associations,
                           GINT_TO_POINTER (id));
    }
}

static void
on_scheduled_association_unmanaged (MetaWindow *window,
                                    gpointer    user_data)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();

  meta_wayland_compositor_remove_surface_association (compositor,
                                                      GPOINTER_TO_INT (user_data));
}

void
meta_wayland_compositor_schedule_surface_association (MetaWaylandCompositor *compositor,
                                                      int                    id,
                                                      MetaWindow            *window)
{
  g_signal_connect (window, "unmanaged",
                    G_CALLBACK (on_scheduled_association_unmanaged),
                    GINT_TO_POINTER (id));
  g_hash_table_insert (compositor->scheduled_surface_associations,
                       GINT_TO_POINTER (id), window);
}

void
meta_wayland_compositor_notify_surface_id (MetaWaylandCompositor *compositor,
                                           int                    id,
                                           MetaWaylandSurface    *surface)
{
  MetaWindow *window;

  window = g_hash_table_lookup (compositor->scheduled_surface_associations,
                                GINT_TO_POINTER (id));
  if (window)
    {
      meta_xwayland_associate_window_with_surface (window, surface);
      meta_wayland_compositor_remove_surface_association (compositor, id);
    }
}

gboolean
meta_wayland_compositor_is_egl_display_bound (MetaWaylandCompositor *compositor)
{
  MetaWaylandCompositorPrivate *priv =
    meta_wayland_compositor_get_instance_private (compositor);

  return priv->is_wayland_egl_display_bound;
}

MetaXWaylandManager *
meta_wayland_compositor_get_xwayland_manager (MetaWaylandCompositor *compositor)
{
  return &compositor->xwayland_manager;
}

MetaContext *
meta_wayland_compositor_get_context (MetaWaylandCompositor *compositor)
{
  return compositor->context;
}
