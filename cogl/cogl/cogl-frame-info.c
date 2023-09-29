/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#include "cogl-config.h"

#include "cogl-frame-info-private.h"
#include "cogl-gtype-private.h"
#include "cogl-context-private.h"

static void _cogl_frame_info_free (CoglFrameInfo *info);

COGL_OBJECT_DEFINE (FrameInfo, frame_info);
COGL_GTYPE_DEFINE_CLASS (FrameInfo, frame_info);

CoglFrameInfo *
cogl_frame_info_new (CoglContext *context,
                     int64_t      global_frame_counter)
{
  CoglFrameInfo *info;

  info = g_new0 (CoglFrameInfo, 1);
  info->context = context;
  info->global_frame_counter = global_frame_counter;

  return _cogl_frame_info_object_new (info);
}

static void
_cogl_frame_info_free (CoglFrameInfo *info)
{
  if (info->timestamp_query)
    cogl_context_free_timestamp_query (info->context, info->timestamp_query);

  g_free (info);
}

int64_t
cogl_frame_info_get_frame_counter (CoglFrameInfo *info)
{
  return info->frame_counter;
}

int64_t
cogl_frame_info_get_presentation_time_us (CoglFrameInfo *info)
{
  g_warn_if_fail (!(info->flags & COGL_FRAME_INFO_FLAG_SYMBOLIC));

  return info->presentation_time_us;
}

float
cogl_frame_info_get_refresh_rate (CoglFrameInfo *info)
{
  g_warn_if_fail (!(info->flags & COGL_FRAME_INFO_FLAG_SYMBOLIC));

  return info->refresh_rate;
}

int64_t
cogl_frame_info_get_global_frame_counter (CoglFrameInfo *info)
{
  return info->global_frame_counter;
}

gboolean
cogl_frame_info_get_is_symbolic (CoglFrameInfo *info)
{
  return !!(info->flags & COGL_FRAME_INFO_FLAG_SYMBOLIC);
}

gboolean
cogl_frame_info_is_hw_clock (CoglFrameInfo *info)
{
  return !!(info->flags & COGL_FRAME_INFO_FLAG_HW_CLOCK);
}

gboolean
cogl_frame_info_is_zero_copy (CoglFrameInfo *info)
{
  return !!(info->flags & COGL_FRAME_INFO_FLAG_ZERO_COPY);
}

gboolean
cogl_frame_info_is_vsync (CoglFrameInfo *info)
{
  return !!(info->flags & COGL_FRAME_INFO_FLAG_VSYNC);
}

unsigned int
cogl_frame_info_get_sequence (CoglFrameInfo *info)
{
  g_warn_if_fail (!(info->flags & COGL_FRAME_INFO_FLAG_SYMBOLIC));

  return info->sequence;
}

int64_t
cogl_frame_info_get_rendering_duration_ns (CoglFrameInfo *info)
{
  int64_t gpu_time_rendering_done_ns;

  if (!info->timestamp_query ||
      info->gpu_time_before_buffer_swap_ns == 0)
    return 0;

  gpu_time_rendering_done_ns =
    cogl_context_timestamp_query_get_time_ns (info->context,
                                              info->timestamp_query);

  return gpu_time_rendering_done_ns - info->gpu_time_before_buffer_swap_ns;
}

int64_t
cogl_frame_info_get_time_before_buffer_swap_us (CoglFrameInfo *info)
{
  return info->cpu_time_before_buffer_swap_us;
}
