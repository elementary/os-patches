/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2025  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
GRUB_MOD_LICENSE ("GPLv3+");

#include <grub/crypto.h>
#include <grub/hwfeatures-gcry.h>
#include "hwf-common.h"

unsigned int
_gcry_get_hw_features (void)
{
  static bool detected = false;
  static unsigned int hw_features = 0;

  if (grub_gcry_hwf_enabled () == false)
    return 0;

  if (detected == true)
    return hw_features;

#if defined (__x86_64__) && defined (GRUB_MACHINE_EFI)
  hw_features = _gcry_hwf_detect_x86 ();
#endif

  grub_dprintf ("hwfeatures", "Detected features: 0x%x\n", hw_features);

  detected = true;

  return hw_features;
}
