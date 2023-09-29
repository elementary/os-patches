/*
 * Copyright (C) 2021 Red Hat Inc.
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
 */

#ifndef META_RENDER_DEVICE_PRIVATE_H
#define META_RENDER_DEVICE_PRIVATE_H

#include <gio/gio.h>
#include <glib-object.h>

#include "backends/meta-egl.h"
#include "backends/native/meta-device-pool.h"
#include "backends/native/meta-drm-buffer.h"
#include "backends/native/meta-render-device.h"

struct _MetaRenderDeviceClass
{
  GObjectClass parent_class;

  EGLDisplay (* create_egl_display) (MetaRenderDevice  *render_device,
                                     GError           **error);

  MetaDrmBuffer * (* allocate_dma_buf) (MetaRenderDevice    *render_device,
                                        int                  width,
                                        int                  height,
                                        uint32_t             format,
                                        MetaDrmBufferFlags   flags,
                                        GError             **error);
  MetaDrmBuffer * (* import_dma_buf) (MetaRenderDevice  *render_device,
                                      MetaDrmBuffer     *buffer,
                                      GError           **error);
};

#endif /* META_RENDER_DEVICE_PRIVATE_H */
