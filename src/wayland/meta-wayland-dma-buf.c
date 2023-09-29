/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat Inc.
 * Copyright (C) 2017 Intel Corporation
 * Copyright (C) 2018,2019 DisplayLink (UK) Ltd.
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
 *     Jonas Ådahl <jadahl@gmail.com>
 *     Daniel Stone <daniels@collabora.com>
 */

/**
 * SECTION:meta-wayland-dma-buf
 * @title: MetaWaylandDmaBuf
 * @short_description: Handles passing DMA-BUFs in Wayland
 *
 * The MetaWaylandDmaBuf namespace contains several objects and functions to
 * handle DMA-BUF buffers that are passed through from clients in Wayland (e.g.
 * using the linux_dmabuf_unstable_v1 protocol).
 */

#include "config.h"

#include "wayland/meta-wayland-dma-buf.h"

#include <drm_fourcc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-egl-ext.h"
#include "backends/meta-egl.h"
#include "cogl/cogl-egl.h"
#include "cogl/cogl.h"
#include "meta/meta-backend.h"
#include "wayland/meta-wayland-buffer.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-versions.h"

#ifdef HAVE_NATIVE_BACKEND
#include "backends/native/meta-drm-buffer-gbm.h"
#include "backends/native/meta-kms-device.h"
#include "backends/native/meta-kms-utils.h"
#include "backends/native/meta-onscreen-native.h"
#include "backends/native/meta-renderer-native.h"
#endif

#include "linux-dmabuf-unstable-v1-server-protocol.h"

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

#define META_WAYLAND_DMA_BUF_MAX_FDS 4

/* Compatible with zwp_linux_dmabuf_feedback_v1.tranche_flags */
typedef enum _MetaWaylandDmaBufTrancheFlags
{
  META_WAYLAND_DMA_BUF_TRANCHE_FLAG_NONE = 0,
  META_WAYLAND_DMA_BUF_TRANCHE_FLAG_SCANOUT = 1,
} MetaWaylandDmaBufTrancheFlags;

typedef enum _MetaWaylandDmaBufTranchePriority
{
  META_WAYLAND_DMA_BUF_TRANCHE_PRIORITY_HIGH = 0,
  META_WAYLAND_DMA_BUF_TRANCHE_PRIORITY_DEFAULT = 10,
} MetaWaylandDmaBufTranchePriority;

typedef struct _MetaWaylandDmaBufFormat
{
  uint32_t drm_format;
  uint64_t drm_modifier;
  uint16_t table_index;
} MetaWaylandDmaBufFormat;

typedef struct _MetaWaylandDmaBufTranche
{
  MetaWaylandDmaBufTranchePriority priority;
  dev_t target_device_id;
  GArray *formats;
  MetaWaylandDmaBufTrancheFlags flags;
  uint64_t scanout_crtc_id;
} MetaWaylandDmaBufTranche;

typedef struct _MetaWaylandDmaBufFeedback
{
  dev_t main_device_id;
  GList *tranches;
} MetaWaylandDmaBufFeedback;

typedef struct _MetaWaylandDmaBufSurfaceFeedback
{
  MetaWaylandDmaBufManager *dma_buf_manager;
  MetaWaylandSurface *surface;
  MetaWaylandDmaBufFeedback *feedback;
  GList *resources;
  gulong scanout_candidate_changed_id;
} MetaWaylandDmaBufSurfaceFeedback;

struct _MetaWaylandDmaBufManager
{
  GObject parent;

  MetaWaylandCompositor *compositor;
  dev_t main_device_id;

  GArray *formats;
  MetaAnonymousFile *format_table_file;
  MetaWaylandDmaBufFeedback *default_feedback;
};

struct _MetaWaylandDmaBufBuffer
{
  GObject parent;

  MetaWaylandDmaBufManager *manager;

  int width;
  int height;
  uint32_t drm_format;
  uint64_t drm_modifier;
  bool is_y_inverted;
  int fds[META_WAYLAND_DMA_BUF_MAX_FDS];
  uint32_t offsets[META_WAYLAND_DMA_BUF_MAX_FDS];
  uint32_t strides[META_WAYLAND_DMA_BUF_MAX_FDS];
};

G_DEFINE_TYPE (MetaWaylandDmaBufBuffer, meta_wayland_dma_buf_buffer, G_TYPE_OBJECT);

G_DEFINE_TYPE (MetaWaylandDmaBufManager, meta_wayland_dma_buf_manager,
               G_TYPE_OBJECT)

static GQuark quark_dma_buf_surface_feedback;

static gboolean
should_send_modifiers (MetaBackend *backend)
{
  MetaRendererNative *renderer_native;
  MetaGpuKms *gpu_kms;

  if (!META_IS_BACKEND_NATIVE (backend))
    return FALSE;

  renderer_native = META_RENDERER_NATIVE (meta_backend_get_renderer (backend));
  gpu_kms = meta_renderer_native_get_primary_gpu (renderer_native);
  if (!gpu_kms)
    return TRUE;

  return meta_renderer_native_send_modifiers (renderer_native);
}

static gint
compare_tranches (gconstpointer a,
                  gconstpointer b)
{
  const MetaWaylandDmaBufTranche *tranche_a = a;
  const MetaWaylandDmaBufTranche *tranche_b = b;

  if (tranche_a->priority > tranche_b->priority)
    return 1;
  if (tranche_a->priority < tranche_b->priority)
    return -1;
  else
    return 0;
}

static MetaWaylandDmaBufTranche *
meta_wayland_dma_buf_tranche_new (dev_t                             device_id,
                                  GArray                           *formats,
                                  MetaWaylandDmaBufTranchePriority  priority,
                                  MetaWaylandDmaBufTrancheFlags     flags)
{
  MetaWaylandDmaBufTranche *tranche;

  tranche = g_new0 (MetaWaylandDmaBufTranche, 1);
  tranche->target_device_id = device_id;
  tranche->formats = g_array_copy (formats);
  tranche->priority = priority;
  tranche->flags = flags;

  return tranche;
}

static void
meta_wayland_dma_buf_tranche_free (MetaWaylandDmaBufTranche *tranche)
{
  g_clear_pointer (&tranche->formats, g_array_unref);
  g_free (tranche);
}

static MetaWaylandDmaBufTranche *
meta_wayland_dma_buf_tranche_copy (MetaWaylandDmaBufTranche *tranche)
{
  return meta_wayland_dma_buf_tranche_new (tranche->target_device_id,
                                           tranche->formats,
                                           tranche->priority,
                                           tranche->flags);
}

static void
meta_wayland_dma_buf_tranche_send (MetaWaylandDmaBufTranche *tranche,
                                   struct wl_resource       *resource)
{
  struct wl_array target_device_buf;
  dev_t *device_id_ptr;
  struct wl_array formats_array;
  unsigned int i;

  wl_array_init (&target_device_buf);
  device_id_ptr = wl_array_add (&target_device_buf, sizeof (*device_id_ptr));
  *device_id_ptr = tranche->target_device_id;
  zwp_linux_dmabuf_feedback_v1_send_tranche_target_device (resource,
                                                           &target_device_buf);
  wl_array_release (&target_device_buf);
  zwp_linux_dmabuf_feedback_v1_send_tranche_flags (resource, tranche->flags);

  wl_array_init (&formats_array);
  for (i = 0; i < tranche->formats->len; i++)
    {
      MetaWaylandDmaBufFormat *format =
        &g_array_index (tranche->formats,
                        MetaWaylandDmaBufFormat,
                        i);
      uint16_t *format_index_ptr;

      format_index_ptr = wl_array_add (&formats_array,
                                       sizeof (*format_index_ptr));
      *format_index_ptr = format->table_index;
    }
  zwp_linux_dmabuf_feedback_v1_send_tranche_formats (resource, &formats_array);
  wl_array_release (&formats_array);

  zwp_linux_dmabuf_feedback_v1_send_tranche_done (resource);
}

static void
meta_wayland_dma_buf_feedback_send (MetaWaylandDmaBufFeedback *feedback,
                                    MetaWaylandDmaBufManager  *dma_buf_manager,
                                    struct wl_resource        *resource)
{
  size_t size;
  int fd;
  struct wl_array main_device_buf;
  dev_t *device_id_ptr;

  fd = meta_anonymous_file_open_fd (dma_buf_manager->format_table_file,
                                    META_ANONYMOUS_FILE_MAPMODE_PRIVATE);
  size = meta_anonymous_file_size (dma_buf_manager->format_table_file);
  zwp_linux_dmabuf_feedback_v1_send_format_table (resource, fd, size);
  meta_anonymous_file_close_fd (fd);

  wl_array_init (&main_device_buf);
  device_id_ptr = wl_array_add (&main_device_buf, sizeof (*device_id_ptr));
  *device_id_ptr = feedback->main_device_id;
  zwp_linux_dmabuf_feedback_v1_send_main_device (resource, &main_device_buf);
  wl_array_release (&main_device_buf);

  g_list_foreach (feedback->tranches,
                  (GFunc) meta_wayland_dma_buf_tranche_send,
                  resource);

  zwp_linux_dmabuf_feedback_v1_send_done (resource);
}

static void
meta_wayland_dma_buf_feedback_add_tranche (MetaWaylandDmaBufFeedback *feedback,
                                           MetaWaylandDmaBufTranche  *tranche)
{
  feedback->tranches = g_list_insert_sorted (feedback->tranches, tranche,
                                             compare_tranches);
}

static MetaWaylandDmaBufFeedback *
meta_wayland_dma_buf_feedback_new (dev_t device_id)
{
  MetaWaylandDmaBufFeedback *feedback;

  feedback = g_new0 (MetaWaylandDmaBufFeedback, 1);
  feedback->main_device_id = device_id;

  return feedback;
}

static void
meta_wayland_dma_buf_feedback_free (MetaWaylandDmaBufFeedback *feedback)
{
  g_clear_list (&feedback->tranches,
                (GDestroyNotify) meta_wayland_dma_buf_tranche_free);
  g_free (feedback);
}

static MetaWaylandDmaBufFeedback *
meta_wayland_dma_buf_feedback_copy (MetaWaylandDmaBufFeedback *feedback)
{
  MetaWaylandDmaBufFeedback *new_feedback;

  new_feedback = meta_wayland_dma_buf_feedback_new (feedback->main_device_id);
  new_feedback->tranches =
    g_list_copy_deep (feedback->tranches,
                      (GCopyFunc) meta_wayland_dma_buf_tranche_copy,
                      NULL);

  return new_feedback;
}

static gboolean
meta_wayland_dma_buf_realize_texture (MetaWaylandBuffer  *buffer,
                                      GError            **error)
{
  MetaContext *context =
    meta_wayland_compositor_get_context (buffer->compositor);
  MetaBackend *backend = meta_context_get_backend (context);
  MetaEgl *egl = meta_backend_get_egl (backend);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  EGLDisplay egl_display = cogl_egl_context_get_egl_display (cogl_context);
  MetaWaylandDmaBufBuffer *dma_buf = buffer->dma_buf.dma_buf;
  uint32_t n_planes;
  uint64_t modifiers[META_WAYLAND_DMA_BUF_MAX_FDS];
  CoglPixelFormat cogl_format;
  EGLImageKHR egl_image;
  CoglEglImageFlags flags;
  CoglTexture2D *texture;
#ifdef HAVE_NATIVE_BACKEND
  MetaDrmFormatBuf format_buf;
#endif

  if (buffer->dma_buf.texture)
    return TRUE;

  switch (dma_buf->drm_format)
    {
    /*
     * NOTE: The cogl_format here is only used for texture color channel
     * swizzling as compared to COGL_PIXEL_FORMAT_ARGB. It is *not* used
     * for accessing the buffer memory. EGL will access the buffer
     * memory according to the DRM fourcc code. Cogl will not mmap
     * and access the buffer memory at all.
     */
    case DRM_FORMAT_XRGB8888:
      cogl_format = COGL_PIXEL_FORMAT_RGB_888;
      break;
    case DRM_FORMAT_XBGR8888:
      cogl_format = COGL_PIXEL_FORMAT_BGR_888;
      break;
    case DRM_FORMAT_ARGB8888:
      cogl_format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
      break;
    case DRM_FORMAT_ABGR8888:
      cogl_format = COGL_PIXEL_FORMAT_ABGR_8888_PRE;
      break;
    case DRM_FORMAT_XRGB2101010:
      cogl_format = COGL_PIXEL_FORMAT_XRGB_2101010;
      break;
    case DRM_FORMAT_ARGB2101010:
      cogl_format = COGL_PIXEL_FORMAT_ARGB_2101010_PRE;
      break;
    case DRM_FORMAT_XBGR2101010:
      cogl_format = COGL_PIXEL_FORMAT_XBGR_2101010;
      break;
    case DRM_FORMAT_ABGR2101010:
      cogl_format = COGL_PIXEL_FORMAT_ABGR_2101010_PRE;
      break;
    case DRM_FORMAT_RGB565:
      cogl_format = COGL_PIXEL_FORMAT_RGB_565;
      break;
    case DRM_FORMAT_XBGR16161616F:
      cogl_format = COGL_PIXEL_FORMAT_XBGR_FP_16161616;
      break;
    case DRM_FORMAT_ABGR16161616F:
      cogl_format = COGL_PIXEL_FORMAT_ABGR_FP_16161616_PRE;
      break;
    case DRM_FORMAT_XRGB16161616F:
      cogl_format = COGL_PIXEL_FORMAT_XRGB_FP_16161616;
      break;
    case DRM_FORMAT_ARGB16161616F:
      cogl_format = COGL_PIXEL_FORMAT_ARGB_FP_16161616_PRE;
      break;
    default:
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Unsupported buffer format %d", dma_buf->drm_format);
      return FALSE;
    }

#ifdef HAVE_NATIVE_BACKEND
  meta_topic (META_DEBUG_WAYLAND,
              "[dma-buf] wl_buffer@%u DRM format %s -> CoglPixelFormat %s",
              wl_resource_get_id (meta_wayland_buffer_get_resource (buffer)),
              meta_drm_format_to_string (&format_buf, dma_buf->drm_format),
              cogl_pixel_format_to_string (cogl_format));
#endif

  for (n_planes = 0; n_planes < META_WAYLAND_DMA_BUF_MAX_FDS; n_planes++)
    {
      if (dma_buf->fds[n_planes] < 0)
        break;

      modifiers[n_planes] = dma_buf->drm_modifier;
    }

  egl_image = meta_egl_create_dmabuf_image (egl,
                                            egl_display,
                                            dma_buf->width,
                                            dma_buf->height,
                                            dma_buf->drm_format,
                                            n_planes,
                                            dma_buf->fds,
                                            dma_buf->strides,
                                            dma_buf->offsets,
                                            modifiers,
                                            error);
  if (egl_image == EGL_NO_IMAGE_KHR)
    return FALSE;

  flags = COGL_EGL_IMAGE_FLAG_NO_GET_DATA;
  texture = cogl_egl_texture_2d_new_from_image (cogl_context,
                                                dma_buf->width,
                                                dma_buf->height,
                                                cogl_format,
                                                egl_image,
                                                flags,
                                                error);

  meta_egl_destroy_image (egl, egl_display, egl_image, NULL);

  if (!texture)
    return FALSE;

  buffer->dma_buf.texture = COGL_TEXTURE (texture);
  buffer->is_y_inverted = dma_buf->is_y_inverted;

  return TRUE;
}

gboolean
meta_wayland_dma_buf_buffer_attach (MetaWaylandBuffer  *buffer,
                                    CoglTexture       **texture,
                                    GError            **error)
{
  if (!meta_wayland_dma_buf_realize_texture (buffer, error))
    return FALSE;

  cogl_clear_object (texture);
  *texture = cogl_object_ref (buffer->dma_buf.texture);
  return TRUE;
}

#ifdef HAVE_NATIVE_BACKEND
static struct gbm_bo *
import_scanout_gbm_bo (MetaWaylandDmaBufBuffer  *dma_buf,
                       MetaGpuKms               *gpu_kms,
                       int                       n_planes,
                       gboolean                 *use_modifier,
                       GError                  **error)
{
  struct gbm_device *gbm_device;
  struct gbm_bo *gbm_bo;

  gbm_device = meta_gbm_device_from_gpu (gpu_kms);
  if (!gbm_device)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "No gbm_device available");
      return NULL;
    }

  if (dma_buf->drm_modifier != DRM_FORMAT_MOD_INVALID ||
      n_planes > 1 ||
      dma_buf->offsets[0] > 0)
    {
      struct gbm_import_fd_modifier_data import_with_modifier;

      import_with_modifier = (struct gbm_import_fd_modifier_data) {
        .width = dma_buf->width,
        .height = dma_buf->height,
        .format = dma_buf->drm_format,
        .num_fds = n_planes,
        .modifier = dma_buf->drm_modifier,
      };
      memcpy (import_with_modifier.fds,
              dma_buf->fds,
              sizeof (dma_buf->fds));
      memcpy (import_with_modifier.strides,
              dma_buf->strides,
              sizeof (import_with_modifier.strides));
      memcpy (import_with_modifier.offsets,
              dma_buf->offsets,
              sizeof (import_with_modifier.offsets));

      *use_modifier = TRUE;

      gbm_bo = gbm_bo_import (gbm_device, GBM_BO_IMPORT_FD_MODIFIER,
                              &import_with_modifier,
                              GBM_BO_USE_SCANOUT);
    }
  else
    {
      struct gbm_import_fd_data import_legacy;

      import_legacy = (struct gbm_import_fd_data) {
        .width = dma_buf->width,
        .height = dma_buf->height,
        .format = dma_buf->drm_format,
        .stride = dma_buf->strides[0],
        .fd = dma_buf->fds[0],
      };

      *use_modifier = FALSE;
      gbm_bo = gbm_bo_import (gbm_device, GBM_BO_IMPORT_FD,
                              &import_legacy,
                              GBM_BO_USE_SCANOUT);
    }

  if (!gbm_bo)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                   "gbm_bo_import failed: %s", g_strerror (errno));
      return NULL;
    }

  return gbm_bo;
}
#endif

CoglScanout *
meta_wayland_dma_buf_try_acquire_scanout (MetaWaylandDmaBufBuffer *dma_buf,
                                          CoglOnscreen            *onscreen)
{
#ifdef HAVE_NATIVE_BACKEND
  MetaContext *context =
    meta_wayland_compositor_get_context (dma_buf->manager->compositor);
  MetaBackend *backend = meta_context_get_backend (context);
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  int n_planes;
  MetaDeviceFile *device_file;
  MetaGpuKms *gpu_kms;
  struct gbm_bo *gbm_bo;
  gboolean use_modifier;
  g_autoptr (GError) error = NULL;
  MetaDrmBufferFlags flags;
  g_autoptr (MetaDrmBufferGbm) fb = NULL;

  for (n_planes = 0; n_planes < META_WAYLAND_DMA_BUF_MAX_FDS; n_planes++)
    {
      if (dma_buf->fds[n_planes] < 0)
        break;
    }

  device_file = meta_renderer_native_get_primary_device_file (renderer_native);
  gpu_kms = meta_renderer_native_get_primary_gpu (renderer_native);
  gbm_bo = import_scanout_gbm_bo (dma_buf, gpu_kms, n_planes, &use_modifier,
                                  &error);
  if (!gbm_bo)
    {
      meta_topic (META_DEBUG_WAYLAND,
                  "Failed to import scanout gbm_bo: %s", error->message);
      return NULL;
    }

  flags = META_DRM_BUFFER_FLAG_NONE;
  if (!use_modifier)
    flags |= META_DRM_BUFFER_FLAG_DISABLE_MODIFIERS;

  fb = meta_drm_buffer_gbm_new_take (device_file, gbm_bo, flags, &error);
  if (!fb)
    {
      meta_topic (META_DEBUG_WAYLAND,
                  "Failed to create scanout buffer: %s", error->message);
      gbm_bo_destroy (gbm_bo);
      return NULL;
    }

  if (!meta_onscreen_native_is_buffer_scanout_compatible (onscreen,
                                                          META_DRM_BUFFER (fb)))
    return NULL;

  return COGL_SCANOUT (g_steal_pointer (&fb));
#else
  return NULL;
#endif
}

static void
buffer_params_add (struct wl_client   *client,
                   struct wl_resource *resource,
                   int32_t             fd,
                   uint32_t            plane_idx,
                   uint32_t            offset,
                   uint32_t            stride,
                   uint32_t            drm_modifier_hi,
                   uint32_t            drm_modifier_lo)
{
  MetaWaylandDmaBufBuffer *dma_buf;
  uint64_t drm_modifier;

  drm_modifier = ((uint64_t) drm_modifier_hi) << 32;
  drm_modifier |= ((uint64_t) drm_modifier_lo) & 0xffffffff;

  dma_buf = wl_resource_get_user_data (resource);
  if (!dma_buf)
    {
      wl_resource_post_error (resource,
                              ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                              "params already used");
      return;
    }

  if (plane_idx >= META_WAYLAND_DMA_BUF_MAX_FDS)
    {
      wl_resource_post_error (resource,
                              ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                              "out-of-bounds plane index %d",
                              plane_idx);
      return;
    }

  if (dma_buf->fds[plane_idx] != -1)
    {
      wl_resource_post_error (resource,
                              ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                              "plane index %d already set",
                              plane_idx);
      return;
    }

  if (dma_buf->drm_modifier != DRM_FORMAT_MOD_INVALID &&
      dma_buf->drm_modifier != drm_modifier)
    {
      wl_resource_post_error (resource,
                              ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                              "mismatching modifier between planes");
      return;
    }

  dma_buf->drm_modifier = drm_modifier;
  dma_buf->fds[plane_idx] = fd;
  dma_buf->offsets[plane_idx] = offset;
  dma_buf->strides[plane_idx] = stride;
}

static void
buffer_params_destroy (struct wl_client   *client,
                       struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
buffer_params_destructor (struct wl_resource *resource)
{
  MetaWaylandDmaBufBuffer *dma_buf;

  /* The user-data for our MetaWaylandBuffer is only valid in between adding
   * FDs and creating the buffer; once it is created, we free it out into
   * the wild, where the ref is considered transferred to the wl_buffer. */
  dma_buf = wl_resource_get_user_data (resource);
  if (dma_buf)
    g_object_unref (dma_buf);
}

static void
buffer_destroy (struct wl_client   *client,
                struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct wl_buffer_interface dma_buf_buffer_impl =
{
  buffer_destroy,
};

/**
 * meta_wayland_dma_buf_from_buffer:
 * @buffer: A #MetaWaylandBuffer object
 *
 * Fetches the associated #MetaWaylandDmaBufBuffer from the wayland buffer.
 * This does not *create* a new object, as this happens in the create_params
 * request of linux_dmabuf_unstable_v1.
 *
 * Returns: (transfer none): The corresponding #MetaWaylandDmaBufBuffer (or
 * %NULL if it wasn't a dma_buf-based wayland buffer)
 */
MetaWaylandDmaBufBuffer *
meta_wayland_dma_buf_from_buffer (MetaWaylandBuffer *buffer)
{
  if (!buffer->resource)
    return NULL;

  if (wl_resource_instance_of (buffer->resource, &wl_buffer_interface,
                               &dma_buf_buffer_impl))
    return wl_resource_get_user_data (buffer->resource);

  return NULL;
}

static void
buffer_params_create_common (struct wl_client   *client,
                             struct wl_resource *params_resource,
                             uint32_t            buffer_id,
                             int32_t             width,
                             int32_t             height,
                             uint32_t            drm_format,
                             uint32_t            flags)
{
  MetaWaylandDmaBufBuffer *dma_buf;
  MetaWaylandBuffer *buffer;
  struct wl_resource *buffer_resource;
  GError *error = NULL;

  dma_buf = wl_resource_get_user_data (params_resource);
  if (!dma_buf)
    {
      wl_resource_post_error (params_resource,
                              ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                              "params already used");
      return;
    }

  /* Calling the 'create' method is the point of no return: after that point,
   * the params object cannot be used. This method must either transfer the
   * ownership of the MetaWaylandDmaBufBuffer to a MetaWaylandBuffer, or
   * destroy it. */
  wl_resource_set_user_data (params_resource, NULL);

  if (dma_buf->fds[0] == -1)
    {
      wl_resource_post_error (params_resource,
                              ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                              "no planes added to params");
      g_object_unref (dma_buf);
      return;
    }

  if ((dma_buf->fds[3] >= 0 || dma_buf->fds[2] >= 0) &&
      (dma_buf->fds[2] == -1 || dma_buf->fds[1] == -1))
    {
      wl_resource_post_error (params_resource,
                              ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                              "gap in planes added to params");
      g_object_unref (dma_buf);
      return;
    }

  dma_buf->width = width;
  dma_buf->height = height;
  dma_buf->drm_format = drm_format;
  dma_buf->is_y_inverted = !(flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT);

  if (flags & ~ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT)
    {
      wl_resource_post_error (params_resource,
                              ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                              "unknown flags 0x%x supplied", flags);
      g_object_unref (dma_buf);
      return;
    }

  /* Create a new MetaWaylandBuffer wrapping our dmabuf, and immediately try
   * to realize it, so we can give the client success/fail feedback for the
   * import. */
  buffer_resource =
    wl_resource_create (client, &wl_buffer_interface, 1, buffer_id);
  wl_resource_set_implementation (buffer_resource, &dma_buf_buffer_impl,
                                  dma_buf, NULL);
  buffer = meta_wayland_buffer_from_resource (dma_buf->manager->compositor,
                                              buffer_resource);

  meta_wayland_buffer_realize (buffer);
  if (!meta_wayland_dma_buf_realize_texture (buffer, &error))
    {
      if (buffer_id == 0)
        {
          zwp_linux_buffer_params_v1_send_failed (params_resource);
        }
      else
        {
          wl_resource_post_error (params_resource,
                                  ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                                  "failed to import supplied dmabufs: %s",
                                  error ? error->message : "unknown error");
        }

        /* will unref the MetaWaylandBuffer */
        wl_resource_destroy (buffer->resource);
        return;
    }

    /* If buffer_id is 0, we are using the non-immediate interface, so
     * need to send a success event with our buffer. */
    if (buffer_id == 0)
      zwp_linux_buffer_params_v1_send_created (params_resource,
                                               buffer->resource);
}

static void
buffer_params_create (struct wl_client   *client,
                      struct wl_resource *params_resource,
                      int32_t             width,
                      int32_t             height,
                      uint32_t            format,
                      uint32_t            flags)
{
  buffer_params_create_common (client, params_resource, 0, width, height,
                               format, flags);
}

static void
buffer_params_create_immed (struct wl_client   *client,
                            struct wl_resource *params_resource,
                            uint32_t            buffer_id,
                            int32_t             width,
                            int32_t             height,
                            uint32_t            format,
                            uint32_t            flags)
{
  buffer_params_create_common (client, params_resource, buffer_id, width,
                               height, format, flags);
}

static const struct zwp_linux_buffer_params_v1_interface buffer_params_implementation =
{
  buffer_params_destroy,
  buffer_params_add,
  buffer_params_create,
  buffer_params_create_immed,
};

static void
dma_buf_handle_destroy (struct wl_client   *client,
                        struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
dma_buf_handle_create_buffer_params (struct wl_client   *client,
                                     struct wl_resource *dma_buf_resource,
                                     uint32_t            params_id)
{
  MetaWaylandDmaBufManager *dma_buf_manager =
    wl_resource_get_user_data (dma_buf_resource);
  struct wl_resource *params_resource;
  MetaWaylandDmaBufBuffer *dma_buf;

  dma_buf = g_object_new (META_TYPE_WAYLAND_DMA_BUF_BUFFER, NULL);
  dma_buf->manager = dma_buf_manager;

  params_resource =
    wl_resource_create (client,
                        &zwp_linux_buffer_params_v1_interface,
                        wl_resource_get_version (dma_buf_resource),
                        params_id);
  wl_resource_set_implementation (params_resource,
                                  &buffer_params_implementation,
                                  dma_buf,
                                  buffer_params_destructor);
}

static void
feedback_destroy (struct wl_client   *client,
                  struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_linux_dmabuf_feedback_v1_interface feedback_implementation =
{
  feedback_destroy,
};

static void
feedback_destructor (struct wl_resource *resource)
{
}

static void
dma_buf_handle_get_default_feedback (struct wl_client   *client,
                                     struct wl_resource *dma_buf_resource,
                                     uint32_t            feedback_id)
{
  MetaWaylandDmaBufManager *dma_buf_manager =
    wl_resource_get_user_data (dma_buf_resource);
  struct wl_resource *feedback_resource;

  feedback_resource =
    wl_resource_create (client,
                        &zwp_linux_dmabuf_feedback_v1_interface,
                        wl_resource_get_version (dma_buf_resource),
                        feedback_id);

  wl_resource_set_implementation (feedback_resource,
                                  &feedback_implementation,
                                  NULL,
                                  feedback_destructor);

  meta_wayland_dma_buf_feedback_send (dma_buf_manager->default_feedback,
                                      dma_buf_manager,
                                      feedback_resource);
}

#ifdef HAVE_NATIVE_BACKEND
static int
find_scanout_tranche_func (gconstpointer a,
                           gconstpointer b)
{
  const MetaWaylandDmaBufTranche *tranche = a;

  if (tranche->scanout_crtc_id)
    return 0;
  else
    return -1;
}

static gboolean
has_modifier (GArray   *modifiers,
              uint64_t  drm_modifier)
{
  int i;

  for (i = 0; i < modifiers->len; i++)
    {
      if (drm_modifier == g_array_index (modifiers, uint64_t, i))
        return TRUE;
    }
  return FALSE;
}

static gboolean
crtc_supports_modifier (MetaCrtcKms *crtc_kms,
                        uint32_t     drm_format,
                        uint64_t     drm_modifier)
{
  GArray *crtc_modifiers;

  crtc_modifiers = meta_crtc_kms_get_modifiers (crtc_kms, drm_format);
  if (!crtc_modifiers)
    return FALSE;

  return has_modifier (crtc_modifiers, drm_modifier);
}

static void
ensure_scanout_tranche (MetaWaylandDmaBufSurfaceFeedback *surface_feedback,
                        MetaCrtc                         *crtc)
{
  MetaWaylandDmaBufManager *dma_buf_manager = surface_feedback->dma_buf_manager;
  MetaContext *context =
    meta_wayland_compositor_get_context (dma_buf_manager->compositor);
  MetaBackend *backend = meta_context_get_backend (context);
  MetaWaylandDmaBufFeedback *feedback = surface_feedback->feedback;
  MetaCrtcKms *crtc_kms;
  MetaWaylandDmaBufTranche *tranche;
  GList *el;
  int i;
  g_autoptr (GArray) formats = NULL;
  MetaWaylandDmaBufTranchePriority priority;
  MetaWaylandDmaBufTrancheFlags flags;

  g_return_if_fail (META_IS_CRTC_KMS (crtc));
  crtc_kms = META_CRTC_KMS (crtc);

  el = g_list_find_custom (feedback->tranches, NULL, find_scanout_tranche_func);
  if (el)
    {
      tranche = el->data;

      if (tranche->scanout_crtc_id == meta_crtc_get_id (crtc))
        return;

      meta_wayland_dma_buf_tranche_free (tranche);
      feedback->tranches = g_list_delete_link (feedback->tranches, el);
    }

  formats = g_array_new (FALSE, FALSE, sizeof (MetaWaylandDmaBufFormat));
  if (should_send_modifiers (backend))
    {
      for (i = 0; i < dma_buf_manager->formats->len; i++)
        {
          MetaWaylandDmaBufFormat format =
            g_array_index (dma_buf_manager->formats,
                           MetaWaylandDmaBufFormat,
                           i);

          if (!crtc_supports_modifier (crtc_kms,
                                       format.drm_format,
                                       format.drm_modifier))
            continue;

          g_array_append_val (formats, format);
        }

      if (formats->len == 0)
        return;
    }
  else
    {
      for (i = 0; i < dma_buf_manager->formats->len; i++)
        {
          MetaWaylandDmaBufFormat format =
            g_array_index (dma_buf_manager->formats,
                           MetaWaylandDmaBufFormat,
                           i);

          if (format.drm_modifier != DRM_FORMAT_MOD_INVALID)
            continue;

          if (!meta_crtc_kms_supports_format (crtc_kms, format.drm_format))
            continue;

          g_array_append_val (formats, format);
        }

      if (formats->len == 0)
        return;
    }

  priority = META_WAYLAND_DMA_BUF_TRANCHE_PRIORITY_HIGH;
  flags = META_WAYLAND_DMA_BUF_TRANCHE_FLAG_SCANOUT;
  tranche = meta_wayland_dma_buf_tranche_new (feedback->main_device_id,
                                              formats,
                                              priority,
                                              flags);
  tranche->scanout_crtc_id = meta_crtc_get_id (crtc);
  meta_wayland_dma_buf_feedback_add_tranche (feedback, tranche);
}

static void
clear_scanout_tranche (MetaWaylandDmaBufSurfaceFeedback *surface_feedback)
{
  MetaWaylandDmaBufFeedback *feedback = surface_feedback->feedback;
  MetaWaylandDmaBufTranche *tranche;
  GList *el;

  el = g_list_find_custom (feedback->tranches, NULL, find_scanout_tranche_func);
  if (!el)
    return;

  tranche = el->data;
  meta_wayland_dma_buf_tranche_free (tranche);
  feedback->tranches = g_list_delete_link (feedback->tranches, el);
}
#endif /* HAVE_NATIVE_BACKEND */

static void
update_surface_feedback_tranches (MetaWaylandDmaBufSurfaceFeedback *surface_feedback)
{
#ifdef HAVE_NATIVE_BACKEND
  MetaCrtc *crtc;

  crtc = meta_wayland_surface_get_scanout_candidate (surface_feedback->surface);
  if (crtc)
    ensure_scanout_tranche (surface_feedback, crtc);
  else
    clear_scanout_tranche (surface_feedback);
#endif /* HAVE_NATIVE_BACKEND */
}

static void
on_scanout_candidate_changed (MetaWaylandSurface               *surface,
                              GParamSpec                       *pspec,
                              MetaWaylandDmaBufSurfaceFeedback *surface_feedback)
{
  GList *l;

  update_surface_feedback_tranches (surface_feedback);

  for (l = surface_feedback->resources; l; l = l->next)
    {
      struct wl_resource *resource = l->data;

      meta_wayland_dma_buf_feedback_send (surface_feedback->feedback,
                                          surface_feedback->dma_buf_manager,
                                          resource);
    }
}

static void
surface_feedback_surface_destroyed_cb (gpointer user_data)
{
  MetaWaylandDmaBufSurfaceFeedback *surface_feedback = user_data;

  g_list_foreach (surface_feedback->resources,
                  (GFunc) wl_resource_set_user_data,
                  NULL);
  g_list_free (surface_feedback->resources);

  meta_wayland_dma_buf_feedback_free (surface_feedback->feedback);

  g_free (surface_feedback);
}

static MetaWaylandDmaBufSurfaceFeedback *
ensure_surface_feedback (MetaWaylandDmaBufManager *dma_buf_manager,
                         MetaWaylandSurface       *surface)
{
  MetaWaylandDmaBufSurfaceFeedback *surface_feedback;

  surface_feedback = g_object_get_qdata (G_OBJECT (surface),
                                         quark_dma_buf_surface_feedback);
  if (surface_feedback)
    return surface_feedback;

  surface_feedback = g_new0 (MetaWaylandDmaBufSurfaceFeedback, 1);
  surface_feedback->dma_buf_manager = dma_buf_manager;
  surface_feedback->surface = surface;
  surface_feedback->feedback =
    meta_wayland_dma_buf_feedback_copy (dma_buf_manager->default_feedback);

  surface_feedback->scanout_candidate_changed_id =
    g_signal_connect (surface, "notify::scanout-candidate",
                      G_CALLBACK (on_scanout_candidate_changed),
                      surface_feedback);

  g_object_set_qdata_full (G_OBJECT (surface),
                           quark_dma_buf_surface_feedback,
                           surface_feedback,
                           surface_feedback_surface_destroyed_cb);

  return surface_feedback;
}

static void
surface_feedback_destructor (struct wl_resource *resource)
{
  MetaWaylandDmaBufSurfaceFeedback *surface_feedback;

  surface_feedback = wl_resource_get_user_data (resource);
  if (!surface_feedback)
    return;

  surface_feedback->resources = g_list_remove (surface_feedback->resources,
                                               resource);
  if (!surface_feedback->resources)
    {
      g_clear_signal_handler (&surface_feedback->scanout_candidate_changed_id,
                              surface_feedback->surface);
      g_object_set_qdata (G_OBJECT (surface_feedback->surface),
                          quark_dma_buf_surface_feedback, NULL);
    }
}

static void
dma_buf_handle_get_surface_feedback (struct wl_client   *client,
                                     struct wl_resource *dma_buf_resource,
                                     uint32_t            feedback_id,
                                     struct wl_resource *surface_resource)
{
  MetaWaylandDmaBufManager *dma_buf_manager =
    wl_resource_get_user_data (dma_buf_resource);
  MetaWaylandSurface *surface =
    wl_resource_get_user_data (surface_resource);
  MetaWaylandDmaBufSurfaceFeedback *surface_feedback;
  struct wl_resource *feedback_resource;

  surface_feedback = ensure_surface_feedback (dma_buf_manager, surface);

  feedback_resource =
    wl_resource_create (client,
                        &zwp_linux_dmabuf_feedback_v1_interface,
                        wl_resource_get_version (dma_buf_resource),
                        feedback_id);

  wl_resource_set_implementation (feedback_resource,
                                  &feedback_implementation,
                                  surface_feedback,
                                  surface_feedback_destructor);
  surface_feedback->resources = g_list_prepend (surface_feedback->resources,
                                                feedback_resource);

  meta_wayland_dma_buf_feedback_send (surface_feedback->feedback,
                                      dma_buf_manager,
                                      feedback_resource);
}

static const struct zwp_linux_dmabuf_v1_interface dma_buf_implementation =
{
  dma_buf_handle_destroy,
  dma_buf_handle_create_buffer_params,
  dma_buf_handle_get_default_feedback,
  dma_buf_handle_get_surface_feedback,
};

static void
send_modifiers (struct wl_resource      *resource,
                MetaWaylandDmaBufFormat *format,
                GHashTable              *sent_formats)
{
  g_assert (wl_resource_get_version (resource) <
            ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION);

  if (!g_hash_table_contains (sent_formats,
                              GUINT_TO_POINTER (format->drm_format)))
    {
      g_hash_table_add (sent_formats, GUINT_TO_POINTER (format->drm_format));
      zwp_linux_dmabuf_v1_send_format (resource, format->drm_format);
    }

  if (wl_resource_get_version (resource) <
      ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION)
    return;

  zwp_linux_dmabuf_v1_send_modifier (resource,
                                     format->drm_format,
                                     format->drm_modifier >> 32,
                                     format->drm_modifier & 0xffffffff);
}

static void
dma_buf_bind (struct wl_client *client,
              void             *user_data,
              uint32_t          version,
              uint32_t          id)
{
  MetaWaylandDmaBufManager *dma_buf_manager = user_data;
  struct wl_resource *resource;

  resource = wl_resource_create (client, &zwp_linux_dmabuf_v1_interface,
                                 version, id);
  wl_resource_set_implementation (resource, &dma_buf_implementation,
                                  dma_buf_manager, NULL);

  if (version < ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION)
    {
      g_autoptr (GHashTable) sent_formats = NULL;
      unsigned int i;

      sent_formats = g_hash_table_new (NULL, NULL);

      for (i = 0; i < dma_buf_manager->formats->len; i++)
        {
          MetaWaylandDmaBufFormat *format =
            &g_array_index (dma_buf_manager->formats,
                            MetaWaylandDmaBufFormat,
                            i);

          send_modifiers (resource, format, sent_formats);
        }
    }
}

static void
add_format (MetaWaylandDmaBufManager *dma_buf_manager,
            EGLDisplay                egl_display,
            uint32_t                  drm_format)
{
  MetaContext *context = dma_buf_manager->compositor->context;
  MetaBackend *backend = meta_context_get_backend (context);
  MetaEgl *egl = meta_backend_get_egl (backend);
  EGLint num_modifiers;
  g_autofree EGLuint64KHR *modifiers = NULL;
  g_autoptr (GError) error = NULL;
  int i;
  MetaWaylandDmaBufFormat format;

  if (!should_send_modifiers (backend))
    goto add_fallback;

  /* First query the number of available modifiers, then allocate an array,
   * then fill the array. */
  if (!meta_egl_query_dma_buf_modifiers (egl, egl_display, drm_format, 0, NULL,
                                         NULL, &num_modifiers, NULL))
    goto add_fallback;

  if (num_modifiers == 0)
    goto add_fallback;

  modifiers = g_new0 (uint64_t, num_modifiers);
  if (!meta_egl_query_dma_buf_modifiers (egl, egl_display, drm_format,
                                         num_modifiers, modifiers, NULL,
                                         &num_modifiers, &error))
    {
      g_warning ("Failed to query modifiers for format 0x%" PRIu32 ": %s",
                 drm_format, error->message);
      goto add_fallback;
    }

  for (i = 0; i < num_modifiers; i++)
    {
      format = (MetaWaylandDmaBufFormat) {
        .drm_format = drm_format,
        .drm_modifier = modifiers[i],
        .table_index = dma_buf_manager->formats->len,
      };
      g_array_append_val (dma_buf_manager->formats, format);
    }

add_fallback:
  format = (MetaWaylandDmaBufFormat) {
    .drm_format = drm_format,
    .drm_modifier = DRM_FORMAT_MOD_INVALID,
    .table_index = dma_buf_manager->formats->len,
  };
  g_array_append_val (dma_buf_manager->formats, format);
}

/*
 * This is the structure the data is expected to have in the shared memory file
 * shared with clients, according to the Wayland Linux DMA buffer protocol.
 * It's structured as 16 bytes (128 bits) per entry, where each entry consists
 * of the following:
 *
 * [ 32 bit format  ][ 32 bit padding ][          64 bit modifier         ]
 */
typedef struct _MetaWaylandDmaBufFormatEntry
{
  uint32_t drm_format;
  uint32_t unused_padding;
  uint64_t drm_modifier;
} MetaWaylandDmaBufFormatEntry;

G_STATIC_ASSERT (sizeof (MetaWaylandDmaBufFormatEntry) == 16);
G_STATIC_ASSERT (offsetof (MetaWaylandDmaBufFormatEntry, drm_format) == 0);
G_STATIC_ASSERT (offsetof (MetaWaylandDmaBufFormatEntry, drm_modifier) == 8);

static void
init_format_table (MetaWaylandDmaBufManager *dma_buf_manager)
{
  g_autofree MetaWaylandDmaBufFormatEntry *format_table = NULL;
  size_t size;
  int i;

  size = sizeof (MetaWaylandDmaBufFormatEntry) * dma_buf_manager->formats->len;
  format_table = g_malloc0 (size);

  for (i = 0; i < dma_buf_manager->formats->len; i++)
    {
      MetaWaylandDmaBufFormat *format =
        &g_array_index (dma_buf_manager->formats, MetaWaylandDmaBufFormat, i);

      format_table[i].drm_format = format->drm_format;
      format_table[i].drm_modifier = format->drm_modifier;
    }

  dma_buf_manager->format_table_file =
    meta_anonymous_file_new (size, (uint8_t *) format_table);
}

static EGLint supported_formats[] = {
  DRM_FORMAT_ARGB8888,
  DRM_FORMAT_ABGR8888,
  DRM_FORMAT_XRGB8888,
  DRM_FORMAT_XBGR8888,
  DRM_FORMAT_ARGB2101010,
  DRM_FORMAT_ABGR2101010,
  DRM_FORMAT_XRGB2101010,
  DRM_FORMAT_XBGR2101010,
  DRM_FORMAT_RGB565,
  DRM_FORMAT_ABGR16161616F,
  DRM_FORMAT_XBGR16161616F,
  DRM_FORMAT_XRGB16161616F,
  DRM_FORMAT_ARGB16161616F
};

static gboolean
init_formats (MetaWaylandDmaBufManager  *dma_buf_manager,
              EGLDisplay                 egl_display,
              GError                   **error)
{
  MetaContext *context = dma_buf_manager->compositor->context;
  MetaBackend *backend = meta_context_get_backend (context);
  MetaEgl *egl = meta_backend_get_egl (backend);
  EGLint num_formats;
  g_autofree EGLint *driver_formats = NULL;
  int i, j;

  dma_buf_manager->formats = g_array_new (FALSE, FALSE,
                                          sizeof (MetaWaylandDmaBufFormat));

  if (!meta_egl_query_dma_buf_formats (egl, egl_display, 0, NULL, &num_formats,
                                       error))
    return FALSE;

  if (num_formats == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "EGL doesn't support any DRM formats");
      return FALSE;
    }

  driver_formats = g_new0 (EGLint, num_formats);
  if (!meta_egl_query_dma_buf_formats (egl, egl_display, num_formats,
                                       driver_formats, &num_formats, error))
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      for (j = 0; j < num_formats; j++)
        {
          if (supported_formats[i] == driver_formats[j])
            add_format (dma_buf_manager, egl_display, supported_formats[i]);
        }
    }

  if (dma_buf_manager->formats->len == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "EGL doesn't support any DRM formats supported by the "
                   "compositor");
      return FALSE;
    }

  init_format_table (dma_buf_manager);
  return TRUE;
}

static void
init_default_feedback (MetaWaylandDmaBufManager *dma_buf_manager)
{
  MetaWaylandDmaBufTranchePriority priority;
  MetaWaylandDmaBufTrancheFlags flags;
  MetaWaylandDmaBufTranche *tranche;

  dma_buf_manager->default_feedback =
    meta_wayland_dma_buf_feedback_new (dma_buf_manager->main_device_id);

  priority = META_WAYLAND_DMA_BUF_TRANCHE_PRIORITY_DEFAULT;
  flags = META_WAYLAND_DMA_BUF_TRANCHE_FLAG_NONE;
  tranche = meta_wayland_dma_buf_tranche_new (dma_buf_manager->main_device_id,
                                              dma_buf_manager->formats,
                                              priority,
                                              flags);
  meta_wayland_dma_buf_feedback_add_tranche (dma_buf_manager->default_feedback,
                                             tranche);
}

/**
 * meta_wayland_dma_buf_manager_new:
 * @compositor: The #MetaWaylandCompositor
 *
 * Creates the global Wayland object that exposes the linux-dmabuf protocol.
 *
 * Returns: (transfer full): The MetaWaylandDmaBufManager instance.
 */
MetaWaylandDmaBufManager *
meta_wayland_dma_buf_manager_new (MetaWaylandCompositor  *compositor,
                                  GError                **error)
{
  MetaContext *context =
    meta_wayland_compositor_get_context (compositor);
  MetaBackend *backend = meta_context_get_backend (context);
  MetaEgl *egl = meta_backend_get_egl (backend);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  EGLDisplay egl_display = cogl_egl_context_get_egl_display (cogl_context);
  dev_t device_id = 0;
  int protocol_version;
  EGLDeviceEXT egl_device;
  EGLAttrib attrib;
  g_autoptr (GError) local_error = NULL;
  g_autoptr (MetaWaylandDmaBufManager) dma_buf_manager = NULL;
  const char *device_path = NULL;
  struct stat device_stat;

  g_assert (backend && egl && clutter_backend && cogl_context && egl_display);

  if (!meta_egl_has_extensions (egl, egl_display, NULL,
                                "EGL_EXT_image_dma_buf_import_modifiers",
                                NULL))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "Missing 'EGL_EXT_image_dma_buf_import_modifiers'");
      return NULL;
    }

  if (!meta_egl_query_display_attrib (egl, egl_display,
                                      EGL_DEVICE_EXT, &attrib,
                                      &local_error))
    {
      g_warning ("Failed to query EGL device from primary EGL display: %s",
                 local_error->message);
      protocol_version = 3;
      goto initialize;
    }
  egl_device = (EGLDeviceEXT) attrib;

  if (meta_egl_egl_device_has_extensions (egl, egl_device, NULL,
                                          "EGL_EXT_device_drm_render_node",
                                          NULL))
    {
      device_path = meta_egl_query_device_string (egl, egl_device,
                                                  EGL_DRM_RENDER_NODE_FILE_EXT,
                                                  &local_error);
      if (local_error)
        {
          g_warning ("Failed to query EGL render node path: %s",
                     local_error->message);
          g_clear_error (&local_error);
        }
    }

  if (!device_path &&
      meta_egl_egl_device_has_extensions (egl, egl_device, NULL,
                                          "EGL_EXT_device_drm",
                                          NULL))
    {
      device_path = meta_egl_query_device_string (egl, egl_device,
                                                  EGL_DRM_DEVICE_FILE_EXT,
                                                  &local_error);
      if (local_error)
        {
          g_warning ("Failed to query EGL render node path: %s",
                     local_error->message);
        }
    }

  if (!device_path)
    {
      meta_topic (META_DEBUG_WAYLAND,
                  "Only advertising zwp_linux_dmabuf_v1 interface version 3 "
                  "support, no suitable device path could be found");
      protocol_version = 3;
      goto initialize;
    }

  if (stat (device_path, &device_stat) != 0)
    {
      g_warning ("Failed to fetch device file ID for '%s': %s",
                 device_path,
                 g_strerror (errno));
      protocol_version = 3;
      goto initialize;
    }

  device_id = device_stat.st_rdev;

  protocol_version = 4;

initialize:

  dma_buf_manager = g_object_new (META_TYPE_WAYLAND_DMA_BUF_MANAGER, NULL);

  dma_buf_manager->compositor = compositor;
  dma_buf_manager->main_device_id = device_id;

  if (!init_formats (dma_buf_manager, egl_display, &local_error))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "No supported formats detected: %s", local_error->message);
      return NULL;
    }

  if (!wl_global_create (compositor->wayland_display,
                         &zwp_linux_dmabuf_v1_interface,
                         protocol_version,
                         dma_buf_manager,
                         dma_buf_bind))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create zwp_linux_dmabuf_v1 global");
      return NULL;
    }

  init_default_feedback (dma_buf_manager);

  return g_steal_pointer (&dma_buf_manager);
}

static void
meta_wayland_dma_buf_buffer_finalize (GObject *object)
{
  MetaWaylandDmaBufBuffer *dma_buf = META_WAYLAND_DMA_BUF_BUFFER (object);
  int i;

  for (i = 0; i < META_WAYLAND_DMA_BUF_MAX_FDS; i++)
    {
      if (dma_buf->fds[i] != -1)
        close (dma_buf->fds[i]);
    }

  G_OBJECT_CLASS (meta_wayland_dma_buf_buffer_parent_class)->finalize (object);
}

static void
meta_wayland_dma_buf_buffer_init (MetaWaylandDmaBufBuffer *dma_buf)
{
  int i;

  dma_buf->drm_modifier = DRM_FORMAT_MOD_INVALID;

  for (i = 0; i < META_WAYLAND_DMA_BUF_MAX_FDS; i++)
    dma_buf->fds[i] = -1;
}

static void
meta_wayland_dma_buf_buffer_class_init (MetaWaylandDmaBufBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_wayland_dma_buf_buffer_finalize;
}

static void
meta_wayland_dma_buf_manager_finalize (GObject *object)
{
  MetaWaylandDmaBufManager *dma_buf_manager =
    META_WAYLAND_DMA_BUF_MANAGER (object);

  g_clear_pointer (&dma_buf_manager->format_table_file,
                   meta_anonymous_file_free);
  g_clear_pointer (&dma_buf_manager->formats, g_array_unref);
  g_clear_pointer (&dma_buf_manager->default_feedback,
                   meta_wayland_dma_buf_feedback_free);

  G_OBJECT_CLASS (meta_wayland_dma_buf_manager_parent_class)->finalize (object);
}

static void
meta_wayland_dma_buf_manager_class_init (MetaWaylandDmaBufManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_wayland_dma_buf_manager_finalize;

  quark_dma_buf_surface_feedback =
    g_quark_from_static_string ("-meta-wayland-dma-buf-surface-feedback");
}

static void
meta_wayland_dma_buf_manager_init (MetaWaylandDmaBufManager *dma_buf)
{
}
