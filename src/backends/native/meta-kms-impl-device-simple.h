/*
 * Copyright (C) 2018-2020 Red Hat
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

#ifndef META_KMS_IMPL_DEVICE_SIMPLE_H
#define META_KMS_IMPL_DEVICE_SIMPLE_H

#include "backends/native/meta-kms-impl-device.h"

#define META_TYPE_KMS_IMPL_DEVICE_SIMPLE (meta_kms_impl_device_simple_get_type ())
G_DECLARE_FINAL_TYPE (MetaKmsImplDeviceSimple, meta_kms_impl_device_simple,
                      META, KMS_IMPL_DEVICE_SIMPLE, MetaKmsImplDevice)

#endif /* META_KMS_IMPL_DEVICE_SIMPLE_H */
