/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Endless Mobile
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
 * SECTION:meta-wayland-buffer
 * @title: MetaWaylandBuffer
 * @short_description: A wrapper for wayland buffers
 *
 * #MetaWaylandBuffer is a general wrapper around wl_buffer, the basic way of
 * passing rendered data from Wayland clients to the compositor. Note that a
 * buffer can be backed by several types of memory, as specified by
 * #MetaWaylandBufferType.
 */

/**
 * MetaWaylandBufferType:
 * @META_WAYLAND_BUFFER_TYPE_UNKNOWN: Unknown type.
 * @META_WAYLAND_BUFFER_TYPE_SHM: wl_buffer backed by shared memory
 * @META_WAYLAND_BUFFER_TYPE_EGL_IMAGE: wl_buffer backed by an EGLImage
 * @META_WAYLAND_BUFFER_TYPE_EGL_STREAM: wl_buffer backed by an EGLStream (NVIDIA-specific)
 * @META_WAYLAND_BUFFER_TYPE_DMA_BUF: wl_buffer backed by a Linux DMA-BUF
 *
 * Specifies the backing memory for a #MetaWaylandBuffer. Depending on the type
 * of buffer, this will lead to different handling for the compositor.  For
 * example, a shared-memory buffer will still need to be uploaded to the GPU.
 */

#include "config.h"

#include "wayland/meta-wayland-buffer.h"

#include <drm_fourcc.h>

#include "backends/meta-backend-private.h"
#include "clutter/clutter.h"
#include "cogl/cogl-egl.h"
#include "meta/util.h"
#include "wayland/meta-wayland-dma-buf.h"
#include "wayland/meta-wayland-private.h"

#ifdef HAVE_NATIVE_BACKEND
#include "backends/native/meta-drm-buffer-gbm.h"
#include "backends/native/meta-kms-utils.h"
#include "backends/native/meta-onscreen-native.h"
#include "backends/native/meta-renderer-native.h"
#endif

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif

enum
{
  RESOURCE_DESTROYED,

  LAST_SIGNAL
};

guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (MetaWaylandBuffer, meta_wayland_buffer, G_TYPE_OBJECT);

static void
meta_wayland_buffer_destroy_handler (struct wl_listener *listener,
                                     void *data)
{
  MetaWaylandBuffer *buffer =
    wl_container_of (listener, buffer, destroy_listener);

  buffer->resource = NULL;
  g_signal_emit (buffer, signals[RESOURCE_DESTROYED], 0);
  g_object_unref (buffer);
}

MetaWaylandBuffer *
meta_wayland_buffer_from_resource (MetaWaylandCompositor *compositor,
                                   struct wl_resource    *resource)
{
  MetaWaylandBuffer *buffer;
  struct wl_listener *listener;

  listener =
    wl_resource_get_destroy_listener (resource,
                                      meta_wayland_buffer_destroy_handler);

  if (listener)
    {
      buffer = wl_container_of (listener, buffer, destroy_listener);
    }
  else
    {
      buffer = g_object_new (META_TYPE_WAYLAND_BUFFER, NULL);

      buffer->resource = resource;
      buffer->compositor = compositor;
      buffer->destroy_listener.notify = meta_wayland_buffer_destroy_handler;
      wl_resource_add_destroy_listener (resource, &buffer->destroy_listener);
    }

  return buffer;
}

struct wl_resource *
meta_wayland_buffer_get_resource (MetaWaylandBuffer *buffer)
{
  return buffer->resource;
}

gboolean
meta_wayland_buffer_is_realized (MetaWaylandBuffer *buffer)
{
  return buffer->type != META_WAYLAND_BUFFER_TYPE_UNKNOWN;
}

gboolean
meta_wayland_buffer_realize (MetaWaylandBuffer *buffer)
{
#ifdef HAVE_WAYLAND_EGLSTREAM
  MetaWaylandEglStream *stream;
#endif
  MetaWaylandDmaBufBuffer *dma_buf;

  if (wl_shm_buffer_get (buffer->resource) != NULL)
    {
      buffer->type = META_WAYLAND_BUFFER_TYPE_SHM;
      return TRUE;
    }

#ifdef HAVE_WAYLAND_EGLSTREAM
  stream = meta_wayland_egl_stream_new (buffer, NULL);
  if (stream)
    {
      CoglTexture2D *texture;

      texture = meta_wayland_egl_stream_create_texture (stream, NULL);
      if (!texture)
        return FALSE;

      buffer->egl_stream.stream = stream;
      buffer->type = META_WAYLAND_BUFFER_TYPE_EGL_STREAM;
      buffer->egl_stream.texture = COGL_TEXTURE (texture);
      buffer->is_y_inverted = meta_wayland_egl_stream_is_y_inverted (stream);

      return TRUE;
    }
#endif /* HAVE_WAYLAND_EGLSTREAM */

  if (meta_wayland_compositor_is_egl_display_bound (buffer->compositor))
    {
      EGLint format;
      MetaBackend *backend = meta_get_backend ();
      MetaEgl *egl = meta_backend_get_egl (backend);
      ClutterBackend *clutter_backend =
        meta_backend_get_clutter_backend (backend);
      CoglContext *cogl_context =
        clutter_backend_get_cogl_context (clutter_backend);
      EGLDisplay egl_display = cogl_egl_context_get_egl_display (cogl_context);

      if (meta_egl_query_wayland_buffer (egl, egl_display, buffer->resource,
                                         EGL_TEXTURE_FORMAT, &format,
                                         NULL))
        {
          buffer->type = META_WAYLAND_BUFFER_TYPE_EGL_IMAGE;
          return TRUE;
        }
    }

  dma_buf = meta_wayland_dma_buf_from_buffer (buffer);
  if (dma_buf)
    {
      buffer->dma_buf.dma_buf = dma_buf;
      buffer->type = META_WAYLAND_BUFFER_TYPE_DMA_BUF;
      return TRUE;
    }

  return FALSE;
}

static gboolean
shm_format_to_cogl_pixel_format (enum wl_shm_format     shm_format,
                                 CoglPixelFormat       *format_out,
                                 CoglTextureComponents *components_out)
{
  CoglPixelFormat format;
  CoglTextureComponents components = COGL_TEXTURE_COMPONENTS_RGBA;

  switch (shm_format)
    {
#if G_BYTE_ORDER == G_BIG_ENDIAN
    case WL_SHM_FORMAT_ARGB8888:
      format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
      break;
    case WL_SHM_FORMAT_XRGB8888:
      format = COGL_PIXEL_FORMAT_ARGB_8888;
      components = COGL_TEXTURE_COMPONENTS_RGB;
      break;
    case WL_SHM_FORMAT_XBGR8888:
      components = COGL_TEXTURE_COMPONENTS_RGB;
      G_GNUC_FALLTHROUGH;
    case WL_SHM_FORMAT_ABGR8888:
      format = COGL_PIXEL_FORMAT_ABGR_8888_PRE;
      break;
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
    case WL_SHM_FORMAT_RGB565:
      format = COGL_PIXEL_FORMAT_RGB_565;
      components = COGL_TEXTURE_COMPONENTS_RGB;
      break;
    case WL_SHM_FORMAT_ARGB8888:
      format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
      break;
    case WL_SHM_FORMAT_XRGB8888:
      format = COGL_PIXEL_FORMAT_BGRA_8888;
      components = COGL_TEXTURE_COMPONENTS_RGB;
      break;
    case WL_SHM_FORMAT_XBGR8888:
      components = COGL_TEXTURE_COMPONENTS_RGB;
      G_GNUC_FALLTHROUGH;
    case WL_SHM_FORMAT_ABGR8888:
      format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;
      break;
    case WL_SHM_FORMAT_XRGB2101010:
      components = COGL_TEXTURE_COMPONENTS_RGB;
      G_GNUC_FALLTHROUGH;
    case WL_SHM_FORMAT_ARGB2101010:
      format = COGL_PIXEL_FORMAT_ARGB_2101010_PRE;
      break;
    case WL_SHM_FORMAT_XBGR2101010:
      components = COGL_TEXTURE_COMPONENTS_RGB;
      G_GNUC_FALLTHROUGH;
    case WL_SHM_FORMAT_ABGR2101010:
      format = COGL_PIXEL_FORMAT_ABGR_2101010_PRE;
      break;
    case WL_SHM_FORMAT_XRGB16161616F:
      components = COGL_TEXTURE_COMPONENTS_RGB;
      G_GNUC_FALLTHROUGH;
    case WL_SHM_FORMAT_ARGB16161616F:
      format = COGL_PIXEL_FORMAT_BGRA_FP_16161616_PRE;
      break;
    case WL_SHM_FORMAT_XBGR16161616F:
      components = COGL_TEXTURE_COMPONENTS_RGB;
      G_GNUC_FALLTHROUGH;
    case WL_SHM_FORMAT_ABGR16161616F:
      format = COGL_PIXEL_FORMAT_RGBA_FP_16161616_PRE;
      break;
#endif
    default:
      return FALSE;
    }

  if (format_out)
    *format_out = format;
  if (components_out)
    *components_out = components;

  return TRUE;
}

static gboolean
shm_buffer_get_cogl_pixel_format (struct wl_shm_buffer  *shm_buffer,
                                  CoglPixelFormat       *format_out,
                                  CoglTextureComponents *components_out)
{
  MetaBackend *backend = meta_get_backend ();
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  CoglPixelFormat cogl_format;
  CoglTextureComponents cogl_components;

  if (!shm_format_to_cogl_pixel_format (wl_shm_buffer_get_format (shm_buffer),
                                        &cogl_format,
                                        &cogl_components))
    return FALSE;

  if (!cogl_context_format_supports_upload (cogl_context, cogl_format))
    return FALSE;

  if (format_out)
    *format_out = cogl_format;
  if (components_out)
    *components_out = cogl_components;

  return TRUE;
}

static const char *
shm_format_to_string (MetaDrmFormatBuf *format_buf,
                      uint32_t          shm_format)
{
  const char *result;

  switch (shm_format)
    {
    case WL_SHM_FORMAT_ARGB8888:
      result = "ARGB8888";
      break;
    case WL_SHM_FORMAT_XRGB8888:
      result = "XRGB8888";
      break;
    default:
      result = meta_drm_format_to_string (format_buf, shm_format);
      break;
    }

  return result;
}

static gboolean
shm_buffer_attach (MetaWaylandBuffer  *buffer,
                   CoglTexture       **texture,
                   GError            **error)
{
  MetaBackend *backend = meta_get_backend ();
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  struct wl_shm_buffer *shm_buffer;
  int stride, width, height;
  CoglPixelFormat format;
  CoglTextureComponents components;
  CoglBitmap *bitmap;
  CoglTexture *new_texture;
  MetaDrmFormatBuf format_buf;

  shm_buffer = wl_shm_buffer_get (buffer->resource);
  stride = wl_shm_buffer_get_stride (shm_buffer);
  width = wl_shm_buffer_get_width (shm_buffer);
  height = wl_shm_buffer_get_height (shm_buffer);
  if (!shm_buffer_get_cogl_pixel_format (shm_buffer, &format, &components))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid shm pixel format");
      return FALSE;
    }

  meta_topic (META_DEBUG_WAYLAND,
              "[wl-shm] wl_buffer@%u wl_shm_format %s -> CoglPixelFormat %s",
              wl_resource_get_id (meta_wayland_buffer_get_resource (buffer)),
              shm_format_to_string (&format_buf,
                                    wl_shm_buffer_get_format (shm_buffer)),
              cogl_pixel_format_to_string (format));

  if (*texture &&
      cogl_texture_get_width (*texture) == width &&
      cogl_texture_get_height (*texture) == height &&
      cogl_texture_get_components (*texture) == components &&
      _cogl_texture_get_format (*texture) == format)
    {
      buffer->is_y_inverted = TRUE;
      return TRUE;
    }

  cogl_clear_object (texture);

  wl_shm_buffer_begin_access (shm_buffer);

  bitmap = cogl_bitmap_new_for_data (cogl_context,
                                     width, height,
                                     format,
                                     stride,
                                     wl_shm_buffer_get_data (shm_buffer));

  new_texture = COGL_TEXTURE (cogl_texture_2d_new_from_bitmap (bitmap));
  cogl_texture_set_components (new_texture, components);

  if (!cogl_texture_allocate (new_texture, error))
    {
      g_clear_pointer (&new_texture, cogl_object_unref);
      if (g_error_matches (*error, COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_SIZE))
        {
          CoglTexture2DSliced *texture_sliced;

          g_clear_error (error);

          texture_sliced =
            cogl_texture_2d_sliced_new_from_bitmap (bitmap,
                                                    COGL_TEXTURE_MAX_WASTE);
          new_texture = COGL_TEXTURE (texture_sliced);
          cogl_texture_set_components (new_texture, components);

          if (!cogl_texture_allocate (new_texture, error))
            g_clear_pointer (&new_texture, cogl_object_unref);
        }
    }

  cogl_object_unref (bitmap);

  wl_shm_buffer_end_access (shm_buffer);

  if (!new_texture)
    return FALSE;

  *texture = new_texture;
  buffer->is_y_inverted = TRUE;

  return TRUE;
}

static gboolean
egl_image_buffer_attach (MetaWaylandBuffer  *buffer,
                         CoglTexture       **texture,
                         GError            **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaEgl *egl = meta_backend_get_egl (backend);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  EGLDisplay egl_display = cogl_egl_context_get_egl_display (cogl_context);
  int format, width, height, y_inverted;
  CoglPixelFormat cogl_format;
  EGLImageKHR egl_image;
  CoglEglImageFlags flags;
  CoglTexture2D *texture_2d;

  if (buffer->egl_image.texture)
    {
      cogl_clear_object (texture);
      *texture = cogl_object_ref (buffer->egl_image.texture);
      return TRUE;
    }

  if (!meta_egl_query_wayland_buffer (egl, egl_display, buffer->resource,
                                      EGL_TEXTURE_FORMAT, &format,
                                      error))
    return FALSE;

  if (!meta_egl_query_wayland_buffer (egl, egl_display, buffer->resource,
                                      EGL_WIDTH, &width,
                                      error))
    return FALSE;

  if (!meta_egl_query_wayland_buffer (egl, egl_display, buffer->resource,
                                      EGL_HEIGHT, &height,
                                      error))
    return FALSE;

  if (!meta_egl_query_wayland_buffer (egl, egl_display, buffer->resource,
                                      EGL_WAYLAND_Y_INVERTED_WL, &y_inverted,
                                      NULL))
    y_inverted = EGL_TRUE;

  switch (format)
    {
    case EGL_TEXTURE_RGB:
      cogl_format = COGL_PIXEL_FORMAT_RGB_888;
      break;
    case EGL_TEXTURE_RGBA:
      cogl_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;
      break;
    default:
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Unsupported buffer format %d", format);
      return FALSE;
    }

  /* The WL_bind_wayland_display spec states that EGL_NO_CONTEXT is to be used
   * in conjunction with the EGL_WAYLAND_BUFFER_WL target. */
  egl_image = meta_egl_create_image (egl, egl_display, EGL_NO_CONTEXT,
                                     EGL_WAYLAND_BUFFER_WL, buffer->resource,
                                     NULL,
                                     error);
  if (egl_image == EGL_NO_IMAGE_KHR)
    return FALSE;

  flags = COGL_EGL_IMAGE_FLAG_NONE;
  texture_2d = cogl_egl_texture_2d_new_from_image (cogl_context,
                                                   width, height,
                                                   cogl_format,
                                                   egl_image,
                                                   flags,
                                                   error);

  meta_egl_destroy_image (egl, egl_display, egl_image, NULL);

  if (!texture_2d)
    return FALSE;

  buffer->egl_image.texture = COGL_TEXTURE (texture_2d);
  buffer->is_y_inverted = !!y_inverted;

  cogl_clear_object (texture);
  *texture = cogl_object_ref (buffer->egl_image.texture);

  return TRUE;
}

#ifdef HAVE_WAYLAND_EGLSTREAM
static gboolean
egl_stream_buffer_attach (MetaWaylandBuffer  *buffer,
                          CoglTexture       **texture,
                          GError            **error)
{
  MetaWaylandEglStream *stream = buffer->egl_stream.stream;

  g_assert (stream);

  if (!meta_wayland_egl_stream_attach (stream, error))
    return FALSE;

  cogl_clear_object (texture);
  *texture = cogl_object_ref (buffer->egl_stream.texture);

  return TRUE;
}
#endif /* HAVE_WAYLAND_EGLSTREAM */

/**
 * meta_wayland_buffer_attach:
 * @buffer: a pointer to a #MetaWaylandBuffer
 * @texture: (inout) (transfer full): a #CoglTexture representing the surface
 *                                    content
 * @error: return location for error or %NULL
 *
 * This function should be passed a pointer to the texture used to draw the
 * surface content. The texture will either be replaced by a new texture, or
 * stay the same, in which case, it may later be updated with new content when
 * processing damage. The new texture might be newly created, or it may be a
 * reference to an already existing one.
 *
 * If replaced, the old texture will have its reference count decreased by one,
 * potentially freeing it. When a new texture is set, the caller (i.e. the
 * surface) will be the owner of one reference count. It must free it, either
 * using g_object_unref() or have it updated again using
 * meta_wayland_buffer_attach(), which also might free it, as described above.
 */
gboolean
meta_wayland_buffer_attach (MetaWaylandBuffer  *buffer,
                            CoglTexture       **texture,
                            GError            **error)
{
  g_return_val_if_fail (buffer->resource, FALSE);

  COGL_TRACE_BEGIN_SCOPED (MetaWaylandBufferAttach, "WaylandBuffer (attach)");

  if (!meta_wayland_buffer_is_realized (buffer))
    {
      /* The buffer should have been realized at surface commit time */
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Unknown buffer type");
      return FALSE;
    }

  switch (buffer->type)
    {
    case META_WAYLAND_BUFFER_TYPE_SHM:
      return shm_buffer_attach (buffer, texture, error);
    case META_WAYLAND_BUFFER_TYPE_EGL_IMAGE:
      return egl_image_buffer_attach (buffer, texture, error);
#ifdef HAVE_WAYLAND_EGLSTREAM
    case META_WAYLAND_BUFFER_TYPE_EGL_STREAM:
      return egl_stream_buffer_attach (buffer, texture, error);
#endif
    case META_WAYLAND_BUFFER_TYPE_DMA_BUF:
      return meta_wayland_dma_buf_buffer_attach (buffer,
                                                 texture,
                                                 error);
    case META_WAYLAND_BUFFER_TYPE_UNKNOWN:
      g_assert_not_reached ();
      return FALSE;
    }

  g_assert_not_reached ();
  return FALSE;
}

/**
 * meta_wayland_buffer_create_snippet:
 * @buffer: A #MetaWaylandBuffer object
 *
 * If needed, this method creates a #CoglSnippet to make sure the buffer can be
 * dealt with appropriately in a #CoglPipeline that renders it.
 *
 * Returns: (transfer full) (nullable): A new #CoglSnippet, or %NULL.
 */
CoglSnippet *
meta_wayland_buffer_create_snippet (MetaWaylandBuffer *buffer)
{
#ifdef HAVE_WAYLAND_EGLSTREAM
  if (!buffer->egl_stream.stream)
    return NULL;

  return meta_wayland_egl_stream_create_snippet (buffer->egl_stream.stream);
#else
  return NULL;
#endif /* HAVE_WAYLAND_EGLSTREAM */
}

gboolean
meta_wayland_buffer_is_y_inverted (MetaWaylandBuffer *buffer)
{
  return buffer->is_y_inverted;
}

static gboolean
process_shm_buffer_damage (MetaWaylandBuffer *buffer,
                           CoglTexture       *texture,
                           cairo_region_t    *region,
                           GError           **error)
{
  struct wl_shm_buffer *shm_buffer;
  int i, n_rectangles;
  gboolean set_texture_failed = FALSE;
  CoglPixelFormat format;

  n_rectangles = cairo_region_num_rectangles (region);

  shm_buffer = wl_shm_buffer_get (buffer->resource);

  shm_buffer_get_cogl_pixel_format (shm_buffer, &format, NULL);
  g_return_val_if_fail (cogl_pixel_format_get_n_planes (format) == 1, FALSE);

  wl_shm_buffer_begin_access (shm_buffer);

  for (i = 0; i < n_rectangles; i++)
    {
      const uint8_t *data = wl_shm_buffer_get_data (shm_buffer);
      int32_t stride = wl_shm_buffer_get_stride (shm_buffer);
      cairo_rectangle_int_t rect;
      int bpp;

      bpp = cogl_pixel_format_get_bytes_per_pixel (format, 0);
      cairo_region_get_rectangle (region, i, &rect);

      if (!_cogl_texture_set_region (texture,
                                     rect.width, rect.height,
                                     format,
                                     stride,
                                     data + rect.x * bpp + rect.y * stride,
                                     rect.x, rect.y,
                                     0,
                                     error))
        {
          set_texture_failed = TRUE;
          break;
        }
    }

  wl_shm_buffer_end_access (shm_buffer);

  return !set_texture_failed;
}

void
meta_wayland_buffer_process_damage (MetaWaylandBuffer *buffer,
                                    CoglTexture       *texture,
                                    cairo_region_t    *region)
{
  gboolean res = FALSE;
  GError *error = NULL;

  g_return_if_fail (buffer->resource);

  switch (buffer->type)
    {
    case META_WAYLAND_BUFFER_TYPE_SHM:
      res = process_shm_buffer_damage (buffer, texture, region, &error);
      break;
    case META_WAYLAND_BUFFER_TYPE_EGL_IMAGE:
#ifdef HAVE_WAYLAND_EGLSTREAM
    case META_WAYLAND_BUFFER_TYPE_EGL_STREAM:
#endif
    case META_WAYLAND_BUFFER_TYPE_DMA_BUF:
      res = TRUE;
      break;
    case META_WAYLAND_BUFFER_TYPE_UNKNOWN:
      g_set_error (&error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Unknown buffer type");
      res = FALSE;
      break;
    }

  if (!res)
    {
      g_warning ("Failed to process Wayland buffer damage: %s", error->message);
      g_error_free (error);
    }
}

static CoglScanout *
try_acquire_egl_image_scanout (MetaWaylandBuffer *buffer,
                               CoglOnscreen      *onscreen)
{
#ifdef HAVE_NATIVE_BACKEND
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  MetaGpuKms *gpu_kms;
  MetaDeviceFile *device_file;
  struct gbm_device *gbm_device;
  struct gbm_bo *gbm_bo;
  MetaDrmBufferFlags flags;
  g_autoptr (MetaDrmBufferGbm) fb = NULL;
  g_autoptr (GError) error = NULL;

  gpu_kms = meta_renderer_native_get_primary_gpu (renderer_native);
  device_file = meta_renderer_native_get_primary_device_file (renderer_native);
  gbm_device = meta_gbm_device_from_gpu (gpu_kms);

  gbm_bo = gbm_bo_import (gbm_device,
                          GBM_BO_IMPORT_WL_BUFFER, buffer->resource,
                          GBM_BO_USE_SCANOUT);
  if (!gbm_bo)
    return NULL;

  flags = META_DRM_BUFFER_FLAG_NONE;
  if (gbm_bo_get_modifier (gbm_bo) == DRM_FORMAT_MOD_INVALID)
    flags |= META_DRM_BUFFER_FLAG_DISABLE_MODIFIERS;

  fb = meta_drm_buffer_gbm_new_take (device_file, gbm_bo, flags, &error);
  if (!fb)
    {
      g_debug ("Failed to create scanout buffer: %s", error->message);
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

CoglScanout *
meta_wayland_buffer_try_acquire_scanout (MetaWaylandBuffer *buffer,
                                         CoglOnscreen      *onscreen)
{
  COGL_TRACE_BEGIN_SCOPED (MetaWaylandBufferTryScanout,
                           "WaylandBuffer (try scanout)");

  switch (buffer->type)
    {
    case META_WAYLAND_BUFFER_TYPE_SHM:
      return NULL;
    case META_WAYLAND_BUFFER_TYPE_EGL_IMAGE:
      return try_acquire_egl_image_scanout (buffer, onscreen);
#ifdef HAVE_WAYLAND_EGLSTREAM
    case META_WAYLAND_BUFFER_TYPE_EGL_STREAM:
      return NULL;
#endif
    case META_WAYLAND_BUFFER_TYPE_DMA_BUF:
      {
        MetaWaylandDmaBufBuffer *dma_buf;

        dma_buf = meta_wayland_dma_buf_from_buffer (buffer);
        if (!dma_buf)
          return NULL;

        return meta_wayland_dma_buf_try_acquire_scanout (dma_buf, onscreen);
      }
    case META_WAYLAND_BUFFER_TYPE_UNKNOWN:
      g_warn_if_reached ();
      return NULL;
    }

  g_assert_not_reached ();
  return NULL;
}

static void
meta_wayland_buffer_finalize (GObject *object)
{
  MetaWaylandBuffer *buffer = META_WAYLAND_BUFFER (object);

  g_clear_pointer (&buffer->egl_image.texture, cogl_object_unref);
#ifdef HAVE_WAYLAND_EGLSTREAM
  g_clear_pointer (&buffer->egl_stream.texture, cogl_object_unref);
  g_clear_object (&buffer->egl_stream.stream);
#endif
  g_clear_pointer (&buffer->dma_buf.texture, cogl_object_unref);
  g_clear_object (&buffer->dma_buf.dma_buf);

  G_OBJECT_CLASS (meta_wayland_buffer_parent_class)->finalize (object);
}

static void
meta_wayland_buffer_init (MetaWaylandBuffer *buffer)
{
}

static void
meta_wayland_buffer_class_init (MetaWaylandBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_wayland_buffer_finalize;

  /**
   * MetaWaylandBuffer::resource-destroyed:
   *
   * Called when the underlying wl_resource was destroyed.
   */
  signals[RESOURCE_DESTROYED] = g_signal_new ("resource-destroyed",
                                              G_TYPE_FROM_CLASS (object_class),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL, NULL, NULL,
                                              G_TYPE_NONE, 0);
}

void
meta_wayland_init_shm (MetaWaylandCompositor *compositor)
{
  MetaBackend *backend = meta_get_backend ();
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);

  static const enum wl_shm_format shm_formats[] = {
    WL_SHM_FORMAT_ABGR8888,
    WL_SHM_FORMAT_XBGR8888,
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    WL_SHM_FORMAT_RGB565,
    WL_SHM_FORMAT_ARGB2101010,
    WL_SHM_FORMAT_XRGB2101010,
    WL_SHM_FORMAT_ABGR2101010,
    WL_SHM_FORMAT_XBGR2101010,
    WL_SHM_FORMAT_ARGB16161616F,
    WL_SHM_FORMAT_XRGB16161616F,
    WL_SHM_FORMAT_ABGR16161616F,
    WL_SHM_FORMAT_XBGR16161616F,
#endif
  };
  int i;

  wl_display_init_shm (compositor->wayland_display);

  for (i = 0; i < G_N_ELEMENTS (shm_formats); i++)
    {
      CoglPixelFormat cogl_format;

      if (!shm_format_to_cogl_pixel_format (shm_formats[i],
                                            &cogl_format,
                                            NULL))
        continue;

      if (!cogl_context_format_supports_upload (cogl_context, cogl_format))
        continue;

      wl_display_add_shm_format (compositor->wayland_display, shm_formats[i]);
    }
}
