/*
 * Copyright (C) 2013-2019 Red Hat
 * Copyright (c) 2018 DisplayLink (UK) Ltd.
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

#include "backends/native/meta-kms-utils.h"

#include <drm_fourcc.h>
#include <glib.h>

/* added in libdrm 2.4.95 */
#ifndef DRM_FORMAT_INVALID
#define DRM_FORMAT_INVALID 0
#endif

float
meta_calculate_drm_mode_refresh_rate (const drmModeModeInfo *drm_mode)
{
  double numerator;
  double denominator;

  if (drm_mode->htotal <= 0 || drm_mode->vtotal <= 0)
    return 0.0;

  numerator = drm_mode->clock * 1000.0;
  denominator = (double) drm_mode->vtotal * drm_mode->htotal;
  if (drm_mode->vscan > 1)
    denominator *= drm_mode->vscan;

  return numerator / denominator;
}

int64_t
meta_calculate_drm_mode_vblank_duration_us (const drmModeModeInfo *drm_mode)
{
  int64_t value;

  if (drm_mode->htotal <= 0 || drm_mode->vtotal <= 0)
    return 0;

  /* Convert to int64_t early. */
  value = drm_mode->vtotal - drm_mode->vdisplay;
  value *= drm_mode->htotal;

  if (drm_mode->flags & DRM_MODE_FLAG_DBLSCAN)
    value *= 2;

  /* Round the duration up as it is used for buffer swap deadline computation. */
  value = (value * 1000 + drm_mode->clock - 1) / drm_mode->clock;

  return value;
}

/**
 * meta_drm_format_to_string:
 * @tmp: temporary buffer
 * @drm_format: DRM fourcc pixel format
 *
 * Returns a pointer to a string naming the given pixel format,
 * usually a pointer to the temporary buffer but not always.
 * Invalid formats may return nonsense names.
 *
 * When calling this, allocate one MetaDrmFormatBuf on the stack to
 * be used as the temporary buffer.
 */
const char *
meta_drm_format_to_string (MetaDrmFormatBuf *tmp,
                           uint32_t          drm_format)
{
  int i;

  if (drm_format == DRM_FORMAT_INVALID)
    return "INVALID";

  G_STATIC_ASSERT (sizeof (tmp->s) == 5);
  for (i = 0; i < 4; i++)
    {
      char c = (drm_format >> (i * 8)) & 0xff;
      tmp->s[i] = g_ascii_isgraph (c) ? c : '.';
    }

  tmp->s[i] = 0;

  return tmp->s;
}

