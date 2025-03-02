/* SPDX-License-Identifier: GPL-3.0+ */

/* Distinguishing our loaded image handles from the firmware's */
#define GRUB_PEIMAGE_MARKER_GUID \
  { 0xda24567a, 0xf899, 0x4566, \
    { 0xb8, 0x27, 0x9f, 0x66, 0x00, 0xc2, 0x14, 0x39 } \
  }

/* Associates an image handle with the device path it was loaded from */
#define GRUB_EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID \
  { 0xbc62157e, 0x3e33, 0x4fec, \
    { 0x99, 0x20, 0x2d, 0x3b, 0x36, 0xd7, 0x50, 0xdf } \
  }

/* Revision defined for the EFI_LOADED_IMAGE_PROTOCOL */
#define GRUB_EFI_LOADED_IMAGE_REVISION  0x1000

/* Value of the signature field of a PE image header */
#define GRUB_PE32_SIGNATURE "PE\0"
