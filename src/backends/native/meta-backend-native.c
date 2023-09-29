/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/**
 * SECTION:meta-backend-native
 * @title: MetaBackendNative
 * @short_description: A native (KMS/evdev) MetaBackend
 *
 * MetaBackendNative is an implementation of #MetaBackend that uses "native"
 * technologies like DRM/KMS and libinput/evdev to perform the necessary
 * functions.
 */

#include "config.h"

#include "backends/native/meta-backend-native.h"
#include "backends/native/meta-backend-native-private.h"
#include "backends/native/meta-input-thread.h"

#include <stdlib.h>

#include "backends/meta-cursor-tracker-private.h"
#include "backends/meta-idle-manager.h"
#include "backends/meta-keymap-utils.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor-manager-private.h"
#include "backends/meta-pointer-constraint.h"
#include "backends/meta-settings-private.h"
#include "backends/meta-stage-private.h"
#include "backends/native/meta-clutter-backend-native.h"
#include "backends/native/meta-device-pool-private.h"
#include "backends/native/meta-kms.h"
#include "backends/native/meta-kms-device.h"
#include "backends/native/meta-launcher.h"
#include "backends/native/meta-monitor-manager-native.h"
#include "backends/native/meta-renderer-native.h"
#include "backends/native/meta-seat-native.h"
#include "backends/native/meta-stage-native.h"
#include "cogl/cogl.h"
#include "core/meta-border.h"
#include "meta/main.h"
#include "meta-dbus-rtkit1.h"

#ifdef HAVE_REMOTE_DESKTOP
#include "backends/meta-screen-cast.h"
#endif

#include "meta-private-enum-types.h"

enum
{
  PROP_0,

  PROP_MODE,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

struct _MetaBackendNative
{
  MetaBackend parent;

  MetaLauncher *launcher;
  MetaDevicePool *device_pool;
  MetaUdev *udev;
  MetaKms *kms;

  MetaBackendNativeMode mode;
};

static GInitableIface *initable_parent_iface;

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (MetaBackendNative, meta_backend_native, META_TYPE_BACKEND,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init))

static void
meta_backend_native_dispose (GObject *object)
{
  MetaBackendNative *native = META_BACKEND_NATIVE (object);

  if (native->kms)
    meta_kms_prepare_shutdown (native->kms);

  G_OBJECT_CLASS (meta_backend_native_parent_class)->dispose (object);

  g_clear_object (&native->kms);
  g_clear_object (&native->udev);
  g_clear_object (&native->device_pool);
  g_clear_pointer (&native->launcher, meta_launcher_free);
}

static ClutterBackend *
meta_backend_native_create_clutter_backend (MetaBackend *backend)
{
  return CLUTTER_BACKEND (meta_clutter_backend_native_new (backend));
}

static ClutterSeat *
meta_backend_native_create_default_seat (MetaBackend  *backend,
                                         GError      **error)
{
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  const char *seat_id = NULL;
  MetaSeatNativeFlag flags;

  switch (backend_native->mode)
    {
    case META_BACKEND_NATIVE_MODE_DEFAULT:
    case META_BACKEND_NATIVE_MODE_HEADLESS:
      seat_id = meta_backend_native_get_seat_id (backend_native);
      break;
    case META_BACKEND_NATIVE_MODE_TEST:
      seat_id = META_BACKEND_TEST_INPUT_SEAT;
      break;
    }

  if (meta_backend_is_headless (backend))
    flags = META_SEAT_NATIVE_FLAG_NO_LIBINPUT;
  else
    flags = META_SEAT_NATIVE_FLAG_NONE;

  return CLUTTER_SEAT (g_object_new (META_TYPE_SEAT_NATIVE,
                                     "backend", backend,
                                     "seat-id", seat_id,
                                     "flags", flags,
                                     NULL));
}

#ifdef HAVE_REMOTE_DESKTOP
static void
maybe_disable_screen_cast_dma_bufs (MetaBackendNative *native)
{
  MetaBackend *backend = META_BACKEND (native);
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaScreenCast *screen_cast = meta_backend_get_screen_cast (backend);
  ClutterBackend *clutter_backend =
    meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  CoglRenderer *cogl_renderer = cogl_context_get_renderer (cogl_context);
  g_autoptr (GError) error = NULL;
  g_autoptr (CoglDmaBufHandle) dmabuf_handle = NULL;

  if (!meta_renderer_is_hardware_accelerated (renderer))
    {
      g_message ("Disabling DMA buffer screen sharing "
                 "(not hardware accelerated)");
      meta_screen_cast_disable_dma_bufs (screen_cast);
    }

  dmabuf_handle = cogl_renderer_create_dma_buf (cogl_renderer,
                                                1, 1,
                                                &error);
  if (!dmabuf_handle)
    {
      g_message ("Disabling DMA buffer screen sharing "
                 "(implicit modifiers not supported)");
      meta_screen_cast_disable_dma_bufs (screen_cast);
    }
}
#endif /* HAVE_REMOTE_DESKTOP */

static void
update_viewports (MetaBackend *backend)
{
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  MetaSeatNative *seat =
    META_SEAT_NATIVE (clutter_backend_get_default_seat (clutter_backend));
  MetaViewportInfo *viewports;

  viewports = meta_monitor_manager_get_viewports (monitor_manager);
  meta_seat_native_set_viewports (seat, viewports);
  g_object_unref (viewports);
}

static void
meta_backend_native_post_init (MetaBackend *backend)
{
  MetaSettings *settings = meta_backend_get_settings (backend);

  META_BACKEND_CLASS (meta_backend_native_parent_class)->post_init (backend);

  if (meta_settings_is_experimental_feature_enabled (settings,
                                                     META_EXPERIMENTAL_FEATURE_RT_SCHEDULER))
    {
      g_autoptr (MetaDbusRealtimeKit1) rtkit_proxy = NULL;
      g_autoptr (GError) error = NULL;

      rtkit_proxy =
        meta_dbus_realtime_kit1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                        G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                        G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                                                        G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                        "org.freedesktop.RealtimeKit1",
                                                        "/org/freedesktop/RealtimeKit1",
                                                        NULL,
                                                        &error);

      if (rtkit_proxy)
        {
          uint32_t priority;

          priority = sched_get_priority_min (SCHED_RR);
          meta_dbus_realtime_kit1_call_make_thread_realtime_sync (rtkit_proxy,
                                                                  gettid (),
                                                                  priority,
                                                                  NULL,
                                                                  &error);
        }

      if (error)
        {
          g_dbus_error_strip_remote_error (error);
          g_message ("Failed to set RT scheduler: %s", error->message);
        }
    }

#ifdef HAVE_REMOTE_DESKTOP
  maybe_disable_screen_cast_dma_bufs (META_BACKEND_NATIVE (backend));
#endif

  update_viewports (backend);
}

static MetaMonitorManager *
meta_backend_native_create_monitor_manager (MetaBackend *backend,
                                            GError     **error)
{
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  MetaMonitorManager *manager;
  gboolean needs_outputs;

  needs_outputs = !(backend_native->mode & META_BACKEND_NATIVE_MODE_HEADLESS);
  manager = g_initable_new (META_TYPE_MONITOR_MANAGER_NATIVE, NULL, error,
                            "backend", backend,
                            "needs-outputs", needs_outputs,
                            NULL);
  if (!manager)
    return NULL;

  g_signal_connect_swapped (manager, "monitors-changed-internal",
                            G_CALLBACK (update_viewports), backend);

  return manager;
}

static MetaCursorRenderer *
meta_backend_native_get_cursor_renderer (MetaBackend        *backend,
                                         ClutterInputDevice *device)
{
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  MetaSeatNative *seat_native =
    META_SEAT_NATIVE (clutter_backend_get_default_seat (clutter_backend));

  return meta_seat_native_maybe_ensure_cursor_renderer (seat_native, device);
}

static MetaRenderer *
meta_backend_native_create_renderer (MetaBackend *backend,
                                     GError     **error)
{
  MetaBackendNative *native = META_BACKEND_NATIVE (backend);
  MetaRendererNative *renderer_native;

  renderer_native = meta_renderer_native_new (native, error);
  if (!renderer_native)
    return NULL;

  return META_RENDERER (renderer_native);
}

static MetaInputSettings *
meta_backend_native_get_input_settings (MetaBackend *backend)
{
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  MetaSeatNative *seat_native =
    META_SEAT_NATIVE (clutter_backend_get_default_seat (clutter_backend));

  return meta_seat_impl_get_input_settings (seat_native->impl);
}

static MetaLogicalMonitor *
meta_backend_native_get_current_logical_monitor (MetaBackend *backend)
{
  MetaCursorTracker *cursor_tracker = meta_backend_get_cursor_tracker (backend);
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  graphene_point_t point;

  meta_cursor_tracker_get_pointer (cursor_tracker, &point, NULL);
  return meta_monitor_manager_get_logical_monitor_at (monitor_manager,
                                                      point.x, point.y);
}

static void
meta_backend_native_set_keymap (MetaBackend *backend,
                                const char  *layouts,
                                const char  *variants,
                                const char  *options)
{
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  ClutterSeat *seat;

  seat = clutter_backend_get_default_seat (clutter_backend);
  meta_seat_native_set_keyboard_map (META_SEAT_NATIVE (seat),
                                     layouts, variants, options);

  meta_backend_notify_keymap_changed (backend);
}

static struct xkb_keymap *
meta_backend_native_get_keymap (MetaBackend *backend)
{
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  ClutterSeat *seat;

  seat = clutter_backend_get_default_seat (clutter_backend);
  return meta_seat_native_get_keyboard_map (META_SEAT_NATIVE (seat));
}

static xkb_layout_index_t
meta_backend_native_get_keymap_layout_group (MetaBackend *backend)
{
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  ClutterSeat *seat;

  seat = clutter_backend_get_default_seat (clutter_backend);
  return meta_seat_native_get_keyboard_layout_index (META_SEAT_NATIVE (seat));
}

static void
meta_backend_native_lock_layout_group (MetaBackend *backend,
                                       guint        idx)
{
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  xkb_layout_index_t old_idx;
  ClutterSeat *seat;

  old_idx = meta_backend_native_get_keymap_layout_group (backend);
  if (old_idx == idx)
    return;

  seat = clutter_backend_get_default_seat (clutter_backend);
  meta_seat_native_set_keyboard_layout_index (META_SEAT_NATIVE (seat), idx);
  meta_backend_notify_keymap_layout_group_changed (backend, idx);
}

const char *
meta_backend_native_get_seat_id (MetaBackendNative *backend_native)
{
  switch (backend_native->mode)
    {
    case META_BACKEND_NATIVE_MODE_DEFAULT:
    case META_BACKEND_NATIVE_MODE_TEST:
      return meta_launcher_get_seat_id (backend_native->launcher);
    case META_BACKEND_NATIVE_MODE_HEADLESS:
      return "seat0";
    }
  g_assert_not_reached ();
}

static gboolean
meta_backend_native_is_headless (MetaBackend *backend)
{
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);

  return backend_native->mode == META_BACKEND_NATIVE_MODE_HEADLESS;
}

static void
meta_backend_native_set_pointer_constraint (MetaBackend           *backend,
                                            MetaPointerConstraint *constraint)
{
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  ClutterSeat *seat = clutter_backend_get_default_seat (clutter_backend);
  MetaPointerConstraintImpl *constraint_impl = NULL;
  cairo_region_t *region;

  if (constraint)
    {
      region = meta_pointer_constraint_get_region (constraint);
      constraint_impl = meta_pointer_constraint_impl_native_new (constraint,
                                                                 region);
    }

  meta_seat_native_set_pointer_constraint (META_SEAT_NATIVE (seat),
                                           constraint_impl);
}

static void
meta_backend_native_update_screen_size (MetaBackend *backend,
                                        int width, int height)
{
  ClutterActor *stage = meta_backend_get_stage (backend);
  ClutterStageWindow *stage_window =
    _clutter_stage_get_window (CLUTTER_STAGE (stage));
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);

  meta_stage_native_rebuild_views (stage_native);

  clutter_actor_set_size (stage, width, height);
}

static MetaGpuKms *
create_gpu_from_udev_device (MetaBackendNative  *native,
                             GUdevDevice        *device,
                             GError            **error)
{
  MetaKmsDeviceFlag flags = META_KMS_DEVICE_FLAG_NONE;
  const char *device_path;
  MetaKmsDevice *kms_device;

  if (meta_is_udev_device_platform_device (device))
    flags |= META_KMS_DEVICE_FLAG_PLATFORM_DEVICE;

  if (meta_is_udev_device_boot_vga (device))
    flags |= META_KMS_DEVICE_FLAG_BOOT_VGA;

  if (meta_is_udev_device_disable_modifiers (device))
    flags |= META_KMS_DEVICE_FLAG_DISABLE_MODIFIERS;

  if (meta_is_udev_device_disable_client_modifiers (device))
    flags |= META_KMS_DEVICE_FLAG_DISABLE_CLIENT_MODIFIERS;

  if (meta_is_udev_device_preferred_primary (device))
    flags |= META_KMS_DEVICE_FLAG_PREFERRED_PRIMARY;

  device_path = g_udev_device_get_device_file (device);

  kms_device = meta_kms_create_device (native->kms, device_path, flags,
                                       error);
  if (!kms_device)
    return NULL;

  return meta_gpu_kms_new (native, kms_device, error);
}

static gboolean
should_ignore_device (MetaBackendNative *backend_native,
                      GUdevDevice       *device)
{
  switch (backend_native->mode)
    {
    case META_BACKEND_NATIVE_MODE_DEFAULT:
    case META_BACKEND_NATIVE_MODE_HEADLESS:
      return meta_is_udev_device_ignore (device);
    case META_BACKEND_NATIVE_MODE_TEST:
      return !meta_is_udev_test_device (device);
    }
  g_assert_not_reached ();
}

static void
on_udev_device_added (MetaUdev          *udev,
                      GUdevDevice       *device,
                      MetaBackendNative *native)
{
  MetaBackend *backend = META_BACKEND (native);
  g_autoptr (GError) error = NULL;
  const char *device_path;
  MetaGpuKms *new_gpu_kms;
  GList *gpus, *l;

  if (!meta_udev_is_drm_device (udev, device))
    return;

  device_path = g_udev_device_get_device_file (device);

  gpus = meta_backend_get_gpus (backend);
  for (l = gpus; l; l = l->next)
    {
      MetaGpuKms *gpu_kms = l->data;

      if (!g_strcmp0 (device_path, meta_gpu_kms_get_file_path (gpu_kms)))
        {
          g_warning ("Failed to hotplug secondary gpu '%s': %s",
                     device_path, "device already present");
          return;
        }
    }

  if (should_ignore_device (native, device))
    {
      g_message ("Ignoring DRM device '%s'", device_path);
      return;
    }

  new_gpu_kms = create_gpu_from_udev_device (native, device, &error);
  if (!new_gpu_kms)
    {
      g_warning ("Failed to hotplug secondary gpu '%s': %s",
                 device_path, error->message);
      return;
    }

  meta_backend_add_gpu (backend, META_GPU (new_gpu_kms));
}

static gboolean
init_gpus (MetaBackendNative  *native,
           GError            **error)
{
  MetaBackend *backend = META_BACKEND (native);
  MetaUdev *udev = meta_backend_native_get_udev (native);
  GList *devices;
  GList *l;

  devices = meta_udev_list_drm_devices (udev, error);
  if (*error)
    return FALSE;

  for (l = devices; l; l = l->next)
    {
      GUdevDevice *device = l->data;
      MetaGpuKms *gpu_kms;
      GError *local_error = NULL;

      if (should_ignore_device (native, device))
        {
          g_message ("Ignoring DRM device '%s'",
                     g_udev_device_get_device_file (device));
          continue;
        }

      gpu_kms = create_gpu_from_udev_device (native, device, &local_error);

      if (!gpu_kms)
        {
          g_warning ("Failed to open gpu '%s': %s",
                     g_udev_device_get_device_file (device),
                     local_error->message);
          g_clear_error (&local_error);
          continue;
        }

      meta_backend_add_gpu (backend, META_GPU (gpu_kms));
    }

  g_list_free_full (devices, g_object_unref);

  if (!meta_backend_is_headless (backend) &&
      g_list_length (meta_backend_get_gpus (backend)) == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "No GPUs found");
      return FALSE;
    }

  g_signal_connect_object (native->udev, "device-added",
                           G_CALLBACK (on_udev_device_added), native,
                           0);

  return TRUE;
}

static gboolean
meta_backend_native_initable_init (GInitable     *initable,
                                   GCancellable  *cancellable,
                                   GError       **error)
{
  MetaBackendNative *native = META_BACKEND_NATIVE (initable);
  MetaBackend *backend = META_BACKEND (native);
  MetaKmsFlags kms_flags;

  if (!meta_is_stage_views_enabled ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "The native backend requires stage views");
      return FALSE;
    }

  if (!meta_backend_is_headless (backend))
    {
      const char *session_id = NULL;
      const char *seat_id = NULL;

      switch (native->mode)
        {
        case META_BACKEND_NATIVE_MODE_DEFAULT:
          break;
        case META_BACKEND_NATIVE_MODE_HEADLESS:
          g_assert_not_reached ();
          break;
        case META_BACKEND_NATIVE_MODE_TEST:
          session_id = "dummy";
          seat_id = "seat0";
          break;
        }

      native->launcher = meta_launcher_new (session_id, seat_id, error);
      if (!native->launcher)
        return FALSE;
    }

  native->device_pool = meta_device_pool_new (native->launcher);
  native->udev = meta_udev_new (native);

  kms_flags = META_KMS_FLAG_NONE;
  if (meta_backend_is_headless (backend))
    kms_flags |= META_KMS_FLAG_NO_MODE_SETTING;

  native->kms = meta_kms_new (META_BACKEND (native), kms_flags, error);
  if (!native->kms)
    return FALSE;

  if (!init_gpus (native, error))
    return FALSE;

  return initable_parent_iface->init (initable, cancellable, error);
}

static void
meta_backend_native_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (object);

  switch (prop_id)
    {
    case PROP_MODE:
      backend_native->mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_parent_iface = g_type_interface_peek_parent (initable_iface);

  initable_iface->init = meta_backend_native_initable_init;
}

static void
meta_backend_native_class_init (MetaBackendNativeClass *klass)
{
  MetaBackendClass *backend_class = META_BACKEND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = meta_backend_native_set_property;
  object_class->dispose = meta_backend_native_dispose;

  backend_class->create_clutter_backend = meta_backend_native_create_clutter_backend;
  backend_class->create_default_seat = meta_backend_native_create_default_seat;

  backend_class->post_init = meta_backend_native_post_init;

  backend_class->create_monitor_manager = meta_backend_native_create_monitor_manager;
  backend_class->get_cursor_renderer = meta_backend_native_get_cursor_renderer;
  backend_class->create_renderer = meta_backend_native_create_renderer;
  backend_class->get_input_settings = meta_backend_native_get_input_settings;

  backend_class->get_current_logical_monitor = meta_backend_native_get_current_logical_monitor;

  backend_class->set_keymap = meta_backend_native_set_keymap;
  backend_class->get_keymap = meta_backend_native_get_keymap;
  backend_class->get_keymap_layout_group = meta_backend_native_get_keymap_layout_group;
  backend_class->lock_layout_group = meta_backend_native_lock_layout_group;
  backend_class->update_screen_size = meta_backend_native_update_screen_size;

  backend_class->set_pointer_constraint = meta_backend_native_set_pointer_constraint;

  backend_class->is_headless = meta_backend_native_is_headless;

  obj_props[PROP_MODE] =
    g_param_spec_enum ("mode",
                       "mode",
                       "mode",
                       META_TYPE_BACKEND_NATIVE_MODE,
                       META_BACKEND_NATIVE_MODE_DEFAULT,
                       G_PARAM_WRITABLE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, obj_props);
}

static void
meta_backend_native_init (MetaBackendNative *native)
{
}

MetaLauncher *
meta_backend_native_get_launcher (MetaBackendNative *native)
{
  return native->launcher;
}

MetaDevicePool *
meta_backend_native_get_device_pool (MetaBackendNative *native)
{
  return native->device_pool;
}

MetaUdev *
meta_backend_native_get_udev (MetaBackendNative *native)
{
  return native->udev;
}

MetaKms *
meta_backend_native_get_kms (MetaBackendNative *native)
{
  return native->kms;
}

gboolean
meta_activate_vt (int vt, GError **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaBackendNative *native = META_BACKEND_NATIVE (backend);
  MetaLauncher *launcher = meta_backend_native_get_launcher (native);

  switch (native->mode)
    {
    case META_BACKEND_NATIVE_MODE_DEFAULT:
      return meta_launcher_activate_vt (launcher, vt, error);
    case META_BACKEND_NATIVE_MODE_HEADLESS:
    case META_BACKEND_NATIVE_MODE_TEST:
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Can't switch VT while headless");
      return FALSE;
    }

  g_assert_not_reached ();
}

void
meta_backend_native_pause (MetaBackendNative *native)
{
  MetaBackend *backend = META_BACKEND (native);
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerNative *monitor_manager_native =
    META_MONITOR_MANAGER_NATIVE (monitor_manager);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  MetaSeatNative *seat =
    META_SEAT_NATIVE (clutter_backend_get_default_seat (clutter_backend));
  MetaRenderer *renderer = meta_backend_get_renderer (backend);

  COGL_TRACE_BEGIN_SCOPED (MetaBackendNativePause,
                           "Backend (pause)");

  meta_seat_native_release_devices (seat);
  meta_renderer_pause (renderer);
  meta_udev_pause (native->udev);

  meta_monitor_manager_native_pause (monitor_manager_native);
}

void meta_backend_native_resume (MetaBackendNative *native)
{
  MetaBackend *backend = META_BACKEND (native);
  ClutterStage *stage = CLUTTER_STAGE (meta_backend_get_stage (backend));
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerNative *monitor_manager_native =
    META_MONITOR_MANAGER_NATIVE (monitor_manager);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  MetaSeatNative *seat =
    META_SEAT_NATIVE (clutter_backend_get_default_seat (clutter_backend));
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaIdleManager *idle_manager;
  MetaInputSettings *input_settings;

  COGL_TRACE_BEGIN_SCOPED (MetaBackendNativeResume,
                           "Backend (resume)");

  meta_monitor_manager_native_resume (monitor_manager_native);
  meta_udev_resume (native->udev);
  meta_kms_resume (native->kms);

  meta_seat_native_reclaim_devices (seat);
  meta_renderer_resume (renderer);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  idle_manager = meta_backend_get_idle_manager (backend);
  meta_idle_manager_reset_idle_time (idle_manager);

  input_settings = meta_backend_get_input_settings (backend);
  meta_input_settings_maybe_restore_numlock_state (input_settings);

  clutter_seat_ensure_a11y_state (CLUTTER_SEAT (seat));
}
