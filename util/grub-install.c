/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2006,2007,2008,2009,2010,2011,2012,2013 Free Software Foundation, Inc.
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

#include <config.h>
#include <grub/types.h>
#include <grub/emu/misc.h>
#include <grub/util/misc.h>
#include <grub/misc.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/fs.h>
#include <grub/env.h>
#include <grub/term.h>
#include <grub/mm.h>
#include <grub/lib/hexdump.h>
#include <grub/crypto.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/zfs/zfs.h>
#include <grub/util/install.h>
#include <grub/emu/getroot.h>
#include <grub/diskfilter.h>
#include <grub/cryptodisk.h>
#include <grub/legacy_parse.h>
#include <grub/gpt_partition.h>
#include <grub/emu/config.h>
#include <grub/util/ofpath.h>
#include <grub/hfsplus.h>
#include <grub/emu/hostfile.h>

#include <string.h>

#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#include <argp.h>
#pragma GCC diagnostic error "-Wmissing-prototypes"
#pragma GCC diagnostic error "-Wmissing-declarations"

#include "progname.h"

static char *target;
static int removable = 0;
static int no_extra_removable = 0;
static int recheck = 0;
static int update_nvram = 1;
static char *install_device = NULL;
static char *debug_image = NULL;
static char *rootdir = NULL;
static char *bootdir = NULL;
static int allow_floppy = 0;
static int force_file_id = 0;
static char *disk_module = NULL;
static char *efidir = NULL;
static char *macppcdir = NULL;
static int force = 0;
static int have_abstractions = 0;
static int have_cryptodisk = 0;
static char * bootloader_id;
static int have_load_cfg = 0;
static FILE * load_cfg_f = NULL;
static char *load_cfg;
static int install_bootsector = 1;
static char *label_font;
static char *label_color;
static char *label_bgcolor;
static char *product_version;
static int add_rs_codes = 1;
static int uefi_secure_boot = 1;

enum
  {
    OPTION_BOOT_DIRECTORY = 0x301,
    OPTION_ROOT_DIRECTORY,
    OPTION_TARGET,
    OPTION_SETUP,
    OPTION_MKRELPATH,
    OPTION_MKDEVICEMAP,
    OPTION_PROBE,
    OPTION_EDITENV,
    OPTION_ALLOW_FLOPPY,
    OPTION_RECHECK,
    OPTION_FORCE,
    OPTION_FORCE_FILE_ID,
    OPTION_NO_NVRAM,
    OPTION_REMOVABLE,
    OPTION_BOOTLOADER_ID,
    OPTION_EFI_DIRECTORY,
    OPTION_FONT,
    OPTION_DEBUG,
    OPTION_DEBUG_IMAGE,
    OPTION_NO_FLOPPY,
    OPTION_DISK_MODULE,
    OPTION_NO_BOOTSECTOR,
    OPTION_NO_RS_CODES,
    OPTION_MACPPC_DIRECTORY,
    OPTION_LABEL_FONT,
    OPTION_LABEL_COLOR,
    OPTION_LABEL_BGCOLOR,
    OPTION_PRODUCT_VERSION,
    OPTION_UEFI_SECURE_BOOT,
    OPTION_NO_UEFI_SECURE_BOOT,
    OPTION_NO_EXTRA_REMOVABLE
  };

static int fs_probe = 1;

static error_t
argp_parser (int key, char *arg, struct argp_state *state)
{
  if (grub_install_parse (key, arg))
    return 0;
  switch (key)
    {
    case OPTION_FORCE_FILE_ID:
      force_file_id = 1;
      return 0;
    case 's':
      fs_probe = 0;
      return 0;

    case OPTION_SETUP:
      if (!grub_strstr (arg, "setup"))
	install_bootsector = 0;
      return 0;

    case OPTION_PRODUCT_VERSION:
      free (product_version);
      product_version = xstrdup (arg);
      return 0;
    case OPTION_LABEL_FONT:
      free (label_font);
      label_font = xstrdup (arg);
      return 0;

    case OPTION_LABEL_COLOR:
      free (label_color);
      label_color = xstrdup (arg);
      return 0;

    case OPTION_LABEL_BGCOLOR:
      free (label_bgcolor);
      label_bgcolor = xstrdup (arg);
      return 0;

      /* Accept and ignore for compatibility.  */
    case OPTION_FONT:
    case OPTION_MKRELPATH:
    case OPTION_PROBE:
    case OPTION_EDITENV:
    case OPTION_MKDEVICEMAP:
    case OPTION_NO_FLOPPY:
      return 0;
    case OPTION_ROOT_DIRECTORY:
      /* Accept for compatibility.  */
      free (rootdir);
      rootdir = xstrdup (arg);
      return 0;

    case OPTION_BOOT_DIRECTORY:
      free (bootdir);
      bootdir = xstrdup (arg);
      return 0;

    case OPTION_MACPPC_DIRECTORY:
      free (macppcdir);
      macppcdir = xstrdup (arg);
      return 0;

    case OPTION_EFI_DIRECTORY:
      free (efidir);
      efidir = xstrdup (arg);
      return 0;

    case OPTION_DISK_MODULE:
      free (disk_module);
      disk_module = xstrdup (arg);
      return 0;

    case OPTION_TARGET:
      free (target);
      target = xstrdup (arg);
      return 0;

    case OPTION_DEBUG_IMAGE:
      free (debug_image);
      debug_image = xstrdup (arg);
      return 0;

    case OPTION_NO_NVRAM:
      update_nvram = 0;
      return 0;

    case OPTION_FORCE:
      force = 1;
      return 0;

    case OPTION_RECHECK:
      recheck = 1;
      return 0;

    case OPTION_REMOVABLE:
      removable = 1;
      return 0;

    case OPTION_NO_EXTRA_REMOVABLE:
      no_extra_removable = 1;
      return 0;

    case OPTION_ALLOW_FLOPPY:
      allow_floppy = 1;
      return 0;

    case OPTION_NO_BOOTSECTOR:
      install_bootsector = 0;
      return 0;

    case OPTION_NO_RS_CODES:
      add_rs_codes = 0;
      return 0;

    case OPTION_DEBUG:
      verbosity++;
      return 0;

    case OPTION_BOOTLOADER_ID:
      free (bootloader_id);
      bootloader_id = xstrdup (arg);
      return 0;

    case OPTION_UEFI_SECURE_BOOT:
      uefi_secure_boot = 1;
      return 0;

    case OPTION_NO_UEFI_SECURE_BOOT:
      uefi_secure_boot = 0;
      return 0;

    case ARGP_KEY_ARG:
      if (install_device)
	grub_util_error ("%s", _("More than one install device?"));
      install_device = xstrdup (arg);
      return 0;

    default:
      return ARGP_ERR_UNKNOWN;
    }
}


static struct argp_option options[] = {
  GRUB_INSTALL_OPTIONS,
  {"boot-directory", OPTION_BOOT_DIRECTORY, N_("DIR"),
   0, N_("install GRUB images under the directory DIR/%s instead of the %s directory"), 2},
  {"root-directory", OPTION_ROOT_DIRECTORY, N_("DIR"),
   OPTION_HIDDEN, 0, 2},
  {"font", OPTION_FONT, N_("FILE"),
   OPTION_HIDDEN, 0, 2},
  {"target", OPTION_TARGET, N_("TARGET"),
   /* TRANSLATORS: "TARGET" as in "target platform".  */
   0, N_("install GRUB for TARGET platform [default=%s]; available targets: %s"), 2},
  {"grub-setup", OPTION_SETUP, "FILE", OPTION_HIDDEN, 0, 2},
  {"grub-mkrelpath", OPTION_MKRELPATH, "FILE", OPTION_HIDDEN, 0, 2},
  {"grub-mkdevicemap", OPTION_MKDEVICEMAP, "FILE", OPTION_HIDDEN, 0, 2},
  {"grub-probe", OPTION_PROBE, "FILE", OPTION_HIDDEN, 0, 2},
  {"grub-editenv", OPTION_EDITENV, "FILE", OPTION_HIDDEN, 0, 2},
  {"allow-floppy", OPTION_ALLOW_FLOPPY, 0, 0,
   /* TRANSLATORS: "may break" doesn't just mean that option wouldn't have any
      effect but that it will make the resulting install unbootable from HDD. */
   N_("make the drive also bootable as floppy (default for fdX devices)."
      " May break on some BIOSes."), 2},
  {"recheck", OPTION_RECHECK, 0, 0,
   N_("delete device map if it already exists"), 2},
  {"force", OPTION_FORCE, 0, 0,
   N_("install even if problems are detected"), 2},
  {"force-file-id", OPTION_FORCE_FILE_ID, 0, 0,
   N_("use identifier file even if UUID is available"), 2},
  {"disk-module", OPTION_DISK_MODULE, N_("MODULE"), 0,
   N_("disk module to use (biosdisk or native). "
      "This option is only available on BIOS target."), 2},
  {"no-nvram", OPTION_NO_NVRAM, 0, 0,
   N_("don't update the `boot-device'/`Boot*' NVRAM variables. "
      "This option is only available on EFI and IEEE1275 targets."), 2},
  {"skip-fs-probe",'s',0,      0,
   N_("do not probe for filesystems in DEVICE"), 0},
  {"no-bootsector", OPTION_NO_BOOTSECTOR, 0, 0,
   N_("do not install bootsector"), 0},
  {"no-rs-codes", OPTION_NO_RS_CODES, 0, 0,
   N_("Do not apply any reed-solomon codes when embedding core.img. "
      "This option is only available on x86 BIOS targets."), 0},

  {"debug", OPTION_DEBUG, 0, OPTION_HIDDEN, 0, 2},
  {"no-floppy", OPTION_NO_FLOPPY, 0, OPTION_HIDDEN, 0, 2},
  {"debug-image", OPTION_DEBUG_IMAGE, N_("STRING"), OPTION_HIDDEN, 0, 2},
  {"removable", OPTION_REMOVABLE, 0, 0,
   N_("the installation device is removable. "
      "This option is only available on EFI."), 2},
  {"bootloader-id", OPTION_BOOTLOADER_ID, N_("ID"), 0,
   N_("the ID of bootloader. This option is only available on EFI and Macs."), 2},
  {"efi-directory", OPTION_EFI_DIRECTORY, N_("DIR"), 0,
   N_("use DIR as the EFI System Partition root."), 2},
  {"macppc-directory", OPTION_MACPPC_DIRECTORY, N_("DIR"), 0,
   N_("use DIR for PPC MAC install."), 2},
  {"label-font", OPTION_LABEL_FONT, N_("FILE"), 0, N_("use FILE as font for label"), 2},
  {"label-color", OPTION_LABEL_COLOR, N_("COLOR"), 0, N_("use COLOR for label"), 2},
  {"label-bgcolor", OPTION_LABEL_BGCOLOR, N_("COLOR"), 0, N_("use COLOR for label background"), 2},
  {"product-version", OPTION_PRODUCT_VERSION, N_("STRING"), 0, N_("use STRING as product version"), 2},
  {"uefi-secure-boot", OPTION_UEFI_SECURE_BOOT, 0, 0,
   N_("install an image usable with UEFI Secure Boot. "
      "This option is only available on EFI and if the grub-efi-amd64-signed "
      "package is installed."), 2},
  {"no-uefi-secure-boot", OPTION_NO_UEFI_SECURE_BOOT, 0, 0,
   N_("do not install an image usable with UEFI Secure Boot, even if the "
      "system was currently started using it. "
      "This option is only available on EFI."), 2},
  {"no-extra-removable", OPTION_NO_EXTRA_REMOVABLE, 0, 0,
   N_("Do not install bootloader code to the removable media path. "
      "This option is only available on EFI."), 2},
  {0, 0, 0, 0, 0, 0}
};

static const char *
get_default_platform (void)
{
#ifdef __powerpc__
   return "powerpc-ieee1275";
#elif defined (__sparc__) || defined (__sparc64__)
   return "sparc64-ieee1275";
#elif defined (__MIPSEL__)
   return "mipsel-loongson";
#elif defined (__MIPSEB__)
   return "mips-arc";
#elif defined (__ia64__)
   return "ia64-efi";
#elif defined (__arm__)
   return grub_install_get_default_arm_platform ();
#elif defined (__aarch64__)
   return "arm64-efi";
#elif defined (__amd64__) || defined (__x86_64__) || defined (__i386__)
   return grub_install_get_default_x86_platform ();
#elif defined (__loongarch_lp64)
   return "loongarch64-efi";
#elif defined (__riscv)
#if __riscv_xlen == 32
   return "riscv32-efi";
#elif __riscv_xlen == 64
   return "riscv64-efi";
#else
   return NULL;
#endif
#else
   return NULL;
#endif
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static char *
help_filter (int key, const char *text, void *input __attribute__ ((unused)))
{
  switch (key)
    {
    case OPTION_BOOT_DIRECTORY:
      return xasprintf (text, GRUB_DIR_NAME, GRUB_BOOT_DIR_NAME "/" GRUB_DIR_NAME);
    case OPTION_TARGET:
      {
	char *plats = grub_install_get_platforms_string ();
	char *ret;
	ret = xasprintf (text, get_default_platform (), plats);
	free (plats);
	return ret;
      }
    case ARGP_KEY_HELP_POST_DOC:
      return xasprintf (text, program_name, GRUB_BOOT_DIR_NAME "/" GRUB_DIR_NAME);
    default:
      return grub_install_help_filter (key, text, input);
    }
}

#pragma GCC diagnostic error "-Wformat-nonliteral"

/* TRANSLATORS: INSTALL_DEVICE isn't an identifier and is the DEVICE you
   install to.  */
struct argp argp = {
  options, argp_parser, N_("[OPTION] [INSTALL_DEVICE]"),
  N_("Install GRUB on your drive.")"\v"
  N_("INSTALL_DEVICE must be system device filename.\n"
     "%s copies GRUB images into %s.  On some platforms, it"
     " may also install GRUB into the boot sector."),
  NULL, help_filter, NULL
};

static int
probe_raid_level (grub_disk_t disk)
{
  /* disk might be NULL in the case of a LVM physical volume with no LVM
     signature.  Ignore such cases here.  */
  if (!disk)
    return -1;

  if (disk->dev->id != GRUB_DISK_DEVICE_DISKFILTER_ID)
    return -1;

  if (disk->name[0] != 'm' || disk->name[1] != 'd')
    return -1;

  if (!((struct grub_diskfilter_lv *) disk->data)->segments)
    return -1;
  return ((struct grub_diskfilter_lv *) disk->data)->segments->type;
}

static void
push_partmap_module (const char *map, void *data __attribute__ ((unused)))
{
  char buf[50];

  if (strcmp (map, "openbsd") == 0 || strcmp (map, "netbsd") == 0)
    {
      grub_install_push_module ("part_bsd");
      return;
    }

  snprintf (buf, sizeof (buf), "part_%s", map);
  grub_install_push_module (buf);
}

static void
push_cryptodisk_module (const char *mod, void *data __attribute__ ((unused)))
{
  grub_install_push_module (mod);
}

static void
probe_mods (grub_disk_t disk)
{
  grub_partition_t part;
  grub_disk_memberlist_t list = NULL, tmp;
  int raid_level;

  if (disk->partition == NULL)
    grub_util_info ("no partition map found for %s", disk->name);

  for (part = disk->partition; part; part = part->parent)
    push_partmap_module (part->partmap->name, NULL);

  if (disk->dev->id == GRUB_DISK_DEVICE_DISKFILTER_ID)
    {
      grub_diskfilter_get_partmap (disk, push_partmap_module, NULL);
      have_abstractions = 1;
    }

  if (disk->dev->id == GRUB_DISK_DEVICE_DISKFILTER_ID
      && (grub_memcmp (disk->name, "lvm/", sizeof ("lvm/") - 1) == 0 ||
	  grub_memcmp (disk->name, "lvmid/", sizeof ("lvmid/") - 1) == 0))
    grub_install_push_module ("lvm");

  if (disk->dev->id == GRUB_DISK_DEVICE_DISKFILTER_ID
      && grub_memcmp (disk->name, "ldm/", sizeof ("ldm/") - 1) == 0)
    grub_install_push_module ("ldm");

  if (disk->dev->id == GRUB_DISK_DEVICE_CRYPTODISK_ID)
    {
      grub_util_cryptodisk_get_abstraction (disk,
					    push_cryptodisk_module, NULL);
      have_abstractions = 1;
      have_cryptodisk = 1;
    }

  raid_level = probe_raid_level (disk);
  if (raid_level >= 0)
    {
      grub_install_push_module ("diskfilter");
      if (disk->dev->disk_raidname)
	grub_install_push_module (disk->dev->disk_raidname (disk));
    }
  if (raid_level == 5)
    grub_install_push_module ("raid5rec");
  if (raid_level == 6)
    grub_install_push_module ("raid6rec");

  /* In case of LVM/RAID, check the member devices as well.  */
  if (disk->dev->disk_memberlist)
    list = disk->dev->disk_memberlist (disk);
  while (list)
    {
      probe_mods (list->disk);
      tmp = list->next;
      free (list);
      list = tmp;
    }
}

static int
have_bootdev (enum grub_install_plat pl)
{
  switch (pl)
    {
    case GRUB_INSTALL_PLATFORM_I386_PC:
    case GRUB_INSTALL_PLATFORM_I386_EFI:
    case GRUB_INSTALL_PLATFORM_X86_64_EFI:
    case GRUB_INSTALL_PLATFORM_IA64_EFI:
    case GRUB_INSTALL_PLATFORM_ARM_EFI:
    case GRUB_INSTALL_PLATFORM_ARM64_EFI:
    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
    case GRUB_INSTALL_PLATFORM_I386_IEEE1275:
    case GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275:
    case GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275:
    case GRUB_INSTALL_PLATFORM_MIPSEL_ARC:
    case GRUB_INSTALL_PLATFORM_MIPS_ARC:
      return 1;

    case GRUB_INSTALL_PLATFORM_I386_QEMU:
    case GRUB_INSTALL_PLATFORM_I386_COREBOOT:
    case GRUB_INSTALL_PLATFORM_ARM_COREBOOT:
    case GRUB_INSTALL_PLATFORM_I386_MULTIBOOT:
    case GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS:

    case GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON:
    case GRUB_INSTALL_PLATFORM_ARM_UBOOT:

    case GRUB_INSTALL_PLATFORM_I386_XEN:
    case GRUB_INSTALL_PLATFORM_X86_64_XEN:
    case GRUB_INSTALL_PLATFORM_I386_XEN_PVH:
      return 0;

      /* pacify warning.  */
    case GRUB_INSTALL_PLATFORM_MAX:
      return 0;
    }
  return 0;
}

static void
probe_cryptodisk_uuid (grub_disk_t disk)
{
  grub_disk_memberlist_t list = NULL, tmp;

  /* In case of LVM/RAID, check the member devices as well.  */
  if (disk->dev->disk_memberlist)
    {
      list = disk->dev->disk_memberlist (disk);
    }
  while (list)
    {
      probe_cryptodisk_uuid (list->disk);
      tmp = list->next;
      free (list);
      list = tmp;
    }
  if (disk->dev->id == GRUB_DISK_DEVICE_CRYPTODISK_ID)
    {
      const char *uuid = grub_util_cryptodisk_get_uuid (disk);
      if (!load_cfg_f)
	load_cfg_f = grub_util_fopen (load_cfg, "wb");
      have_load_cfg = 1;

      fprintf (load_cfg_f, "cryptomount -u %s\n",
	      uuid);
    }
}

static int
is_same_disk (const char *a, const char *b)
{
  while (1)
    {
      if ((*a == ',' || *a == '\0') && (*b == ',' || *b == '\0'))
	return 1;
      if (*a != *b)
	return 0;
      if (*a == '\\')
	{
	  if (a[1] != b[1])
	    return 0;
	  a += 2;
	  b += 2;
	  continue;
	}
      a++;
      b++;
    }
}

static char *
get_rndstr (void)
{
  grub_uint8_t rnd[15];
  const size_t sz = sizeof (rnd) * GRUB_CHAR_BIT / 5;
  char * ret = xmalloc (sz + 1);
  size_t i;
  if (grub_get_random (rnd, sizeof (rnd)))
    grub_util_error ("%s", _("couldn't retrieve random data"));
  for (i = 0; i < sz; i++)
    {
      grub_size_t b = i * 5;
      grub_uint8_t r;
      grub_size_t f1 = GRUB_CHAR_BIT - b % GRUB_CHAR_BIT;
      grub_size_t f2;
      if (f1 > 5)
	f1 = 5;
      f2 = 5 - f1;
      r = (rnd[b / GRUB_CHAR_BIT] >> (b % GRUB_CHAR_BIT)) & ((1 << f1) - 1);
      if (f2)
	r |= (rnd[b / GRUB_CHAR_BIT + 1] & ((1 << f2) - 1)) << f1;
      if (r < 10)
	ret[i] = '0' + r;
      else
	ret[i] = 'a' + (r - 10);
    }
  ret[sz] = '\0';
  return ret;
}

static char *
escape (const char *in)
{
  char *ptr;
  char *ret;
  int overhead = 0;

  for (ptr = (char*)in; *ptr; ptr++)
    if (*ptr == '\'')
      overhead += 3;
  ret = grub_malloc (ptr - in + overhead + 1);
  if (!ret)
    return NULL;

  grub_strchrsub (ret, in, '\'', "'\\''");
  return ret;
}

static struct grub_util_config config;

static void
device_map_check_duplicates (const char *dev_map)
{
  FILE *fp;
  char buf[1024];	/* XXX */
  size_t alloced = 8;
  size_t filled = 0;
  char **d;
  size_t i;

  if (dev_map[0] == '\0')
    return;

  fp = grub_util_fopen (dev_map, "r");
  if (! fp)
    return;

  d = xcalloc (alloced, sizeof (d[0]));

  while (fgets (buf, sizeof (buf), fp))
    {
      char *p = buf;
      char *e;

      /* Skip leading spaces.  */
      while (*p && grub_isspace (*p))
	p++;

      /* If the first character is `#' or NUL, skip this line.  */
      if (*p == '\0' || *p == '#')
	continue;

      if (*p != '(')
	continue;

      p++;

      e = p;
      p = strchr (p, ')');
      if (! p)
	continue;

      if (filled >= alloced)
	{
	  alloced *= 2;
	  d = xrealloc (d, alloced * sizeof (d[0]));
	}

      *p = '\0';

      d[filled++] = xstrdup (e);
    }

  fclose (fp);

  qsort (d, filled, sizeof (d[0]), grub_qsort_strcmp);

  for (i = 0; i + 1 < filled; i++)
    if (strcmp (d[i], d[i+1]) == 0)
      {
	grub_util_error (_("the drive %s is defined multiple times in the device map %s"),
			 d[i], dev_map);
      }

  for (i = 0; i < filled; i++)
    free (d[i]);

  free (d);
}

static grub_err_t
write_to_disk (grub_device_t dev, const char *fn)
{
  char *core_img;
  size_t core_size;
  grub_err_t err;

  core_size = grub_util_get_image_size (fn);

  core_img = grub_util_read_image (fn);

  grub_util_info ("writing `%s' to `%s'", fn, dev->disk->name);
  err = grub_disk_write (dev->disk, 0, 0,
			 core_size, core_img);
  free (core_img);
  return err;
}

static int
is_prep_partition (grub_device_t dev)
{
  if (!dev->disk)
    return 0;
  if (!dev->disk->partition)
    return 0;
  if (strcmp(dev->disk->partition->partmap->name, "msdos") == 0)
    return (dev->disk->partition->msdostype == 0x41);

  if (strcmp (dev->disk->partition->partmap->name, "gpt") == 0)
    {
      struct grub_gpt_partentry gptdata;
      grub_partition_t p = dev->disk->partition;
      int ret = 0;
      dev->disk->partition = dev->disk->partition->parent;

      if (grub_disk_read (dev->disk, p->offset, p->index,
			  sizeof (gptdata), &gptdata) == 0)
	{
	  const grub_guid_t template = {
	    grub_cpu_to_le32_compile_time (0x9e1a2d38),
	    grub_cpu_to_le16_compile_time (0xc612),
	    grub_cpu_to_le16_compile_time (0x4316),
	    { 0xaa, 0x26, 0x8b, 0x49, 0x52, 0x1e, 0x5a, 0x8b }
	  };

	  ret = grub_memcmp (&template, &gptdata.type,
			     sizeof (template)) == 0;
	}
      dev->disk->partition = p;
      return ret;
    }

  return 0;
}

static int
is_prep_empty (grub_device_t dev)
{
  grub_disk_addr_t dsize, addr;
  grub_uint32_t buffer[32768];

  dsize = grub_disk_native_sectors (dev->disk);
  for (addr = 0; addr < dsize;
       addr += sizeof (buffer) / GRUB_DISK_SECTOR_SIZE)
    {
      grub_size_t sz = sizeof (buffer);
      grub_uint32_t *ptr;

      if (sizeof (buffer) / GRUB_DISK_SECTOR_SIZE > dsize - addr)
	sz = (dsize - addr) * GRUB_DISK_SECTOR_SIZE;
      grub_disk_read (dev->disk, addr, 0, sz, buffer);

      if (addr == 0 && grub_memcmp (buffer, ELFMAG, SELFMAG) == 0)
	return 1;

      for (ptr = buffer; ptr < buffer + sz / sizeof (*buffer); ptr++)
	if (*ptr)
	  return 0;
    }

  return 1;
}

static void
bless (grub_device_t dev, const char *path, int x86)
{
  struct stat st;
  grub_err_t err;

  grub_util_info ("blessing %s", path);

  if (stat (path, &st) < 0)
    grub_util_error (N_("cannot stat `%s': %s"),
		     path, strerror (errno));

  err = grub_mac_bless_inode (dev, st.st_ino, S_ISDIR (st.st_mode), x86);
  if (err)
    grub_util_error ("%s", grub_errmsg);
  grub_util_info ("blessed");
}

static void
fill_core_services (const char *core_services)
{
  char *label;
  FILE *f;
  char *label_text;
  char *label_string = xasprintf ("%s %s", bootloader_id, product_version);
  char *sysv_plist;

  label = grub_util_path_concat (2, core_services, ".disk_label");
  grub_util_info ("rendering label %s", label_string);
  grub_util_render_label (label_font, label_bgcolor ? : "white",
			  label_color ? : "black", label_string, label);
  grub_util_info ("label rendered");
  free (label);
  label_text = grub_util_path_concat (2, core_services, ".disk_label.contentDetails");
  f = grub_util_fopen (label_text, "wb");
  fprintf (f, "%s\n", label_string);
  fclose (f);
  free (label_string);
  free (label_text);

  sysv_plist = grub_util_path_concat (2, core_services, "SystemVersion.plist");
  f = grub_util_fopen (sysv_plist, "wb");
  fprintf (f,
	   "<plist version=\"1.0\">\n"
	   "<dict>\n"
	   "        <key>ProductBuildVersion</key>\n"
	   "        <string></string>\n"
	   "        <key>ProductName</key>\n"
	   "        <string>%s</string>\n"
	   "        <key>ProductVersion</key>\n"
	   "        <string>%s</string>\n"
	   "</dict>\n"
	   "</plist>\n", bootloader_id, product_version);
  fclose (f);
  free (sysv_plist);
}

#ifdef __linux__
static void
try_open (const char *path)
{
  FILE *f;

  f = grub_util_fopen (path, "r+");
  if (f == NULL)
    grub_util_error (_("Unable to open %s: %s"), path, strerror (errno));
  fclose (f);
}
#endif

/* Helper routine for also_install_removable() below. Walk through the
   specified dir, looking to see if there is a file/dir that matches
   the search string exactly, but in a case-insensitive manner. If so,
   return a copy of the exact file/dir that *does* exist. If not,
   return NULL */
static char *
check_component_exists(const char *dir,
		       const char *search)
{
  grub_util_fd_dir_t d;
  grub_util_fd_dirent_t de;
  char *found = NULL;

  d = grub_util_fd_opendir (dir);
  if (!d)
    grub_util_error (_("cannot open directory `%s': %s"),
		     dir, grub_util_fd_strerror ());

  while ((de = grub_util_fd_readdir (d)))
    {
      if (strcasecmp (de->d_name, search) == 0)
	{
	  found = xstrdup (de->d_name);
	  break;
	}
    }
  grub_util_fd_closedir (d);
  return found;
}

/* Some complex directory-handling stuff in here, to cope with
 * case-insensitive FAT/VFAT filesystem semantics. Ugh. */
static void
also_install_removable(const char *src,
		       const char *base_efidir,
		       const char *efi_suffix,
		       const char *efi_suffix_upper)
{
  char *efi_file = NULL;
  char *dst = NULL;
  char *cur = NULL;
  char *found = NULL;
  char *fb_file = NULL;
  char *mm_file = NULL;
  char *generic_efidir = NULL;

  if (!efi_suffix)
    grub_util_error ("%s", _("efi_suffix not set"));
  if (!efi_suffix_upper)
    grub_util_error ("%s", _("efi_suffix_upper not set"));

  efi_file = xasprintf ("BOOT%s.EFI", efi_suffix_upper);
  fb_file = xasprintf ("fb%s.efi", efi_suffix);
  mm_file = xasprintf ("mm%s.efi", efi_suffix);

  /* We need to install in $base_efidir/EFI/BOOT/$efi_file, but we
   * need to cope with case-insensitive stuff here. Build the path one
   * component at a time, checking for existing matches each time. */

  /* Look for "EFI" in base_efidir. Make it if it does not exist in
   * some form. */
  found = check_component_exists(base_efidir, "EFI");
  if (found == NULL)
    found = xstrdup("EFI");
  dst = grub_util_path_concat (2, base_efidir, found);
  cur = xstrdup (dst);
  free (dst);
  free (found);
  grub_install_mkdir_p (cur);

  /* Now BOOT */
  found = check_component_exists(cur, "BOOT");
  if (found == NULL)
    found = xstrdup("BOOT");
  dst = grub_util_path_concat (2, cur, found);
  free (cur);
  free (found);
  grub_install_mkdir_p (dst);
  generic_efidir = xstrdup (dst);
  free (dst);

  /* Now $efi_file */
  found = check_component_exists(generic_efidir, efi_file);
  if (found == NULL)
    found = xstrdup(efi_file);
  dst = grub_util_path_concat (2, generic_efidir, found);
  free (found);
  grub_install_copy_file (src, dst, 1);
  free (efi_file);
  free (dst);

  /* Now try to also install fallback */
  efi_file = grub_util_path_concat (2, "/usr/lib/shim/", fb_file);
  dst = grub_util_path_concat (2, generic_efidir, fb_file);
  grub_install_copy_file (efi_file, dst, 0);
  free (efi_file);
  free (dst);

  /* Also install MokManager to the removable path */
  efi_file = grub_util_path_concat (2, "/usr/lib/shim/", mm_file);
  dst = grub_util_path_concat (2, generic_efidir, mm_file);
  grub_install_copy_file (efi_file, dst, 0);
  free (efi_file);
  free (dst);

  free (generic_efidir);
  free (fb_file);
  free (mm_file);
}

int
main (int argc, char *argv[])
{
  int is_efi = 0;
  const char *efi_distributor = NULL, *efi_title = NULL;
  const char *efi_suffix = NULL, *efi_suffix_upper = NULL;
  char *efi_file = NULL;
  char **grub_devices;
  grub_fs_t grub_fs;
  grub_device_t grub_dev = NULL;
  enum grub_install_plat platform;
  char *grubdir, *device_map;
  char **curdev, **curdrive;
  char **grub_drives;
  char *relative_grubdir;
  char **efidir_device_names = NULL;
  grub_device_t efidir_grub_dev = NULL;
  char *base_efidir = NULL;
  char *efidir_grub_devname;
  int efidir_is_mac = 0;
  int is_prep = 0;
  const char *pkgdatadir;

  grub_util_host_init (&argc, &argv);
  product_version = xstrdup (PACKAGE_VERSION);
  pkgdatadir = grub_util_get_pkgdatadir ();
  label_font = grub_util_path_concat (2, pkgdatadir, "unicode.pf2");

  argp_parse (&argp, argc, argv, 0, 0, 0);

  if (verbosity > 1)
    grub_env_set ("debug", "all");

  grub_util_load_config (&config);

  if (!bootloader_id && config.grub_distributor)
    {
      char *ptr;
      bootloader_id = xstrdup (config.grub_distributor);
      for (ptr = bootloader_id; *ptr && *ptr != ' '; ptr++)
	if (*ptr >= 'A' && *ptr <= 'Z')
	  *ptr = *ptr - 'A' + 'a';
      *ptr = '\0';
    }
  if (!bootloader_id || bootloader_id[0] == '\0')
    {
      free (bootloader_id);
      bootloader_id = xstrdup ("grub");
    }

  if (removable && no_extra_removable)
    grub_util_error (_("Invalid to use both --removable and --no_extra_removable"));

  if (!grub_install_source_directory)
    {
      if (!target)
	{
	  const char * t;
	  t = get_default_platform ();
	  if (!t)
	    grub_util_error ("%s",
			     _("Unable to determine your platform."
			       " Use --target.")
			     );
	  target = xstrdup (t);
	}
      grub_install_source_directory
	= grub_util_path_concat (2, grub_util_get_pkglibdir (), target);
    }

  platform = grub_install_get_target (grub_install_source_directory);

  {
    char *platname = grub_install_get_platform_name (platform);
    fprintf (stderr, _("Installing for %s platform.\n"), platname);
    free (platname);
  }

  switch (platform)
    {
    case GRUB_INSTALL_PLATFORM_I386_PC:
      if (!disk_module)
	disk_module = xstrdup ("biosdisk");
      break;
    case GRUB_INSTALL_PLATFORM_I386_EFI:
    case GRUB_INSTALL_PLATFORM_X86_64_EFI:
    case GRUB_INSTALL_PLATFORM_ARM_EFI:
    case GRUB_INSTALL_PLATFORM_ARM64_EFI:
    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
    case GRUB_INSTALL_PLATFORM_IA64_EFI:
    case GRUB_INSTALL_PLATFORM_I386_IEEE1275:
    case GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275:
    case GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275:
    case GRUB_INSTALL_PLATFORM_MIPSEL_ARC:
    case GRUB_INSTALL_PLATFORM_MIPS_ARC:
    case GRUB_INSTALL_PLATFORM_ARM_UBOOT:
    case GRUB_INSTALL_PLATFORM_I386_XEN:
    case GRUB_INSTALL_PLATFORM_X86_64_XEN:
    case GRUB_INSTALL_PLATFORM_I386_XEN_PVH:
      break;

    case GRUB_INSTALL_PLATFORM_I386_QEMU:
    case GRUB_INSTALL_PLATFORM_I386_COREBOOT:
    case GRUB_INSTALL_PLATFORM_ARM_COREBOOT:
    case GRUB_INSTALL_PLATFORM_I386_MULTIBOOT:
    case GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON:
    case GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS:
      disk_module = xstrdup ("native");
      break;

      /* pacify warning.  */
    case GRUB_INSTALL_PLATFORM_MAX:
      break;
    }

  switch (platform)
    {
    case GRUB_INSTALL_PLATFORM_I386_PC:
    case GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275:
      if (!install_device)
	grub_util_error ("%s", _("install device isn't specified"));
      break;
    case GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275:
      if (install_device)
	is_prep = 1;
      break;
    case GRUB_INSTALL_PLATFORM_MIPS_ARC:
    case GRUB_INSTALL_PLATFORM_MIPSEL_ARC:
      break;
    case GRUB_INSTALL_PLATFORM_I386_EFI:
    case GRUB_INSTALL_PLATFORM_X86_64_EFI:
    case GRUB_INSTALL_PLATFORM_ARM_EFI:
    case GRUB_INSTALL_PLATFORM_ARM64_EFI:
    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
    case GRUB_INSTALL_PLATFORM_IA64_EFI:
    case GRUB_INSTALL_PLATFORM_I386_IEEE1275:
    case GRUB_INSTALL_PLATFORM_ARM_UBOOT:
    case GRUB_INSTALL_PLATFORM_I386_QEMU:
    case GRUB_INSTALL_PLATFORM_I386_COREBOOT:
    case GRUB_INSTALL_PLATFORM_ARM_COREBOOT:
    case GRUB_INSTALL_PLATFORM_I386_MULTIBOOT:
    case GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON:
    case GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_I386_XEN:
    case GRUB_INSTALL_PLATFORM_X86_64_XEN:
    case GRUB_INSTALL_PLATFORM_I386_XEN_PVH:
      free (install_device);
      install_device = NULL;
      break;

      /* pacify warning.  */
    case GRUB_INSTALL_PLATFORM_MAX:
      break;
    }

  if (!bootdir)
    bootdir = grub_util_path_concat (3, "/", rootdir, GRUB_BOOT_DIR_NAME);

  {
    char * t = grub_util_path_concat (2, bootdir, GRUB_DIR_NAME);
    grub_install_mkdir_p (t);
    grubdir = grub_canonicalize_file_name (t);
    if (!grubdir)
      grub_util_error (_("failed to get canonical path of `%s'"), t);
    free (t);
  }
  device_map = grub_util_path_concat (2, grubdir, "device.map");

  if (recheck)
    grub_util_unlink (device_map);

  device_map_check_duplicates (device_map);
  grub_util_biosdisk_init (device_map);

  /* Initialize all modules. */
  grub_init_all ();
  grub_gcry_init_all ();
  grub_hostfs_init ();
  grub_host_init ();

  switch (platform)
    {
    case GRUB_INSTALL_PLATFORM_I386_EFI:
    case GRUB_INSTALL_PLATFORM_X86_64_EFI:
    case GRUB_INSTALL_PLATFORM_ARM_EFI:
    case GRUB_INSTALL_PLATFORM_ARM64_EFI:
    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
    case GRUB_INSTALL_PLATFORM_IA64_EFI:
      is_efi = 1;
      break;
    default:
      is_efi = 0;
      break;

      /* pacify warning.  */
    case GRUB_INSTALL_PLATFORM_MAX:
      break;
    }

  switch (platform)
    {
    case GRUB_INSTALL_PLATFORM_I386_IEEE1275:
    case GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275:
#ifdef __linux__
      /* On Linux, ensure /dev/nvram is _functional_. */
      if (update_nvram)
        try_open ("/dev/nvram");
#endif
      break;
    default:
      break;
    }

  /* Find the EFI System Partition.  */

  if (is_efi)
    {
      grub_fs_t fs;
      free (install_device);
      install_device = NULL;
      if (!efidir)
	{
	  char *d = grub_util_path_concat (2, bootdir, "efi");
	  char *dr = NULL;
	  if (!grub_util_is_directory (d))
	    {
	      free (d);
	      d = grub_util_path_concat (2, bootdir, "EFI");
	    }
	  /*
	    The EFI System Partition may have been given directly using
	    --root-directory.
	  */
	  if (!grub_util_is_directory (d)
	      && rootdir && grub_strcmp (rootdir, "/") != 0)
	    {
	      free (d);
	      d = xstrdup (rootdir);
	    }
	  if (grub_util_is_directory (d))
	    dr = grub_make_system_path_relative_to_its_root (d);
	  /* Is it a mount point? */
	  if (dr && dr[0] == '\0')
	    efidir = d;
	  else
	    free (d);
	  free (dr);
	}
      if (!efidir)
	grub_util_error ("%s", _("cannot find EFI directory"));
      efidir_device_names = grub_guess_root_devices (efidir);
      if (!efidir_device_names || !efidir_device_names[0])
	grub_util_error (_("cannot find a device for %s (is /dev mounted?)"),
			     efidir);
      install_device = efidir_device_names[0];

      for (curdev = efidir_device_names; *curdev; curdev++)
	  grub_util_pull_device (*curdev);

      efidir_grub_devname = grub_util_get_grub_dev (efidir_device_names[0]);
      if (!efidir_grub_devname)
	grub_util_error (_("cannot find a GRUB drive for %s.  Check your device.map"),
			 efidir_device_names[0]);

      efidir_grub_dev = grub_device_open (efidir_grub_devname);
      if (! efidir_grub_dev)
	grub_util_error ("%s", grub_errmsg);

      fs = grub_fs_probe (efidir_grub_dev);
      if (! fs)
	grub_util_error ("%s", grub_errmsg);

      efidir_is_mac = 0;

      if (grub_strcmp (fs->name, "hfs") == 0
	  || grub_strcmp (fs->name, "hfsplus") == 0)
	efidir_is_mac = 1;

      if (!efidir_is_mac && grub_strcmp (fs->name, "fat") != 0)
	{
	  if (force)
	    grub_util_warn (_("%s doesn't look like an EFI partition, system may not boot"), efidir);
	  else
	    grub_util_error (_("%s doesn't look like an EFI partition"), efidir);
	}

      base_efidir = xstrdup(efidir);

      /* The EFI specification requires that an EFI System Partition must
	 contain an "EFI" subdirectory, and that OS loaders are stored in
	 subdirectories below EFI.  Vendors are expected to pick names that do
	 not collide with other vendors.  To minimise collisions, we use the
	 name of our distributor if possible.
      */
      char *t;
      efi_distributor = bootloader_id;
      if (strcmp (efi_distributor, "kubuntu") == 0)
	efi_distributor = "ubuntu";
      else if (strcmp (efi_distributor, "devuan") == 0)
	efi_distributor = "debian";
      efi_title = config.grub_distributor;
      if (!efi_title || efi_title[0] == '\0')
	efi_title = "GRUB";
      else if (strcasecmp (efi_title, "kubuntu") == 0)
	efi_title = "Ubuntu";
      else if (strcasecmp (efi_title, "devuan") == 0)
	efi_title = "Debian";
      switch (platform)
	{
	case GRUB_INSTALL_PLATFORM_I386_EFI:
	  efi_suffix = "ia32";
	  efi_suffix_upper = "IA32";
	  break;
	case GRUB_INSTALL_PLATFORM_X86_64_EFI:
	  efi_suffix = "x64";
	  efi_suffix_upper = "X64";
	  break;
	case GRUB_INSTALL_PLATFORM_IA64_EFI:
	  efi_suffix = "ia64";
	  efi_suffix_upper = "IA64";
	  break;
	case GRUB_INSTALL_PLATFORM_ARM_EFI:
	  efi_suffix = "arm";
	  efi_suffix_upper = "ARM";
	  break;
	case GRUB_INSTALL_PLATFORM_ARM64_EFI:
	  efi_suffix = "aa64";
	  efi_suffix_upper = "AA64";
	  break;
	case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
	  efi_suffix = "riscv32";
	  efi_suffix_upper = "RISCV32";
	  break;
	case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
	  efi_suffix = "riscv64";
	  efi_suffix_upper = "RISCV64";
	  break;
	default:
	  break;
	}
      if (removable)
	{
	  /* The specification makes stricter requirements of removable
	     devices, in order that only one image can be automatically loaded
	     from them.  The image must always reside under /EFI/BOOT, and it
	     must have a specific file name depending on the architecture.
	  */
	  efi_distributor = "BOOT";
	  if (!efi_suffix)
	    grub_util_error ("%s", _("You've found a bug"));
	  efi_file = xasprintf ("BOOT%s.EFI", efi_suffix_upper);
	}
      else
	{
	  /* It is convenient for each architecture to have a different
	     efi_file, so that different versions can be installed in parallel.
	  */
	  efi_file = xasprintf ("grub%s.efi", efi_suffix);
	}
      t = grub_util_path_concat (3, efidir, "EFI", efi_distributor);
      free (efidir);
      efidir = t;
      grub_install_mkdir_p (efidir);
    }

  if (platform == GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275)
    {
      const char *machtype = grub_install_get_default_powerpc_machtype ();
      int is_guess = 0;

      if (strcmp (machtype, "pmac_oldworld") == 0)
	update_nvram = 0;
      else if (strcmp (machtype, "cell") == 0)
	update_nvram = 0;
      else if (strcmp (machtype, "generic") == 0)
	update_nvram = 0;
      else if (strcmp (machtype, "chrp_ibm_qemu") == 0)
	update_nvram = 0;

      if (!macppcdir)
	{
	  char *d;

	  is_guess = 1;
	  d = grub_util_path_concat (2, bootdir, "macppc");
	  if (!grub_util_is_directory (d))
	    {
	      free (d);
	      d = grub_util_path_concat (2, bootdir, "efi");
	    }
	  /* Find the Mac HFS(+) System Partition.  */
	  if (!grub_util_is_directory (d))
	    {
	      free (d);
	      d = grub_util_path_concat (2, bootdir, "EFI");
	    }
	  if (!grub_util_is_directory (d))
	    {
	      free (d);
	      d = 0;
	    }
	  if (d)
	    macppcdir = d;
	}
      if (macppcdir)
	{
	  char **macppcdir_device_names = NULL;
	  grub_device_t macppcdir_grub_dev = NULL;
	  char *macppcdir_grub_devname;
	  grub_fs_t fs;

	  macppcdir_device_names = grub_guess_root_devices (macppcdir);
	  if (!macppcdir_device_names || !macppcdir_device_names[0])
	    grub_util_error (_("cannot find a device for %s (is /dev mounted?)"),
			     macppcdir);

	  for (curdev = macppcdir_device_names; *curdev; curdev++)
	    grub_util_pull_device (*curdev);

	  macppcdir_grub_devname = grub_util_get_grub_dev (macppcdir_device_names[0]);
	  if (!macppcdir_grub_devname)
	    grub_util_error (_("cannot find a GRUB drive for %s.  Check your device.map"),
			     macppcdir_device_names[0]);

	  macppcdir_grub_dev = grub_device_open (macppcdir_grub_devname);
	  if (! macppcdir_grub_dev)
	    grub_util_error ("%s", grub_errmsg);

	  fs = grub_fs_probe (macppcdir_grub_dev);
	  if (! fs)
	    grub_util_error ("%s", grub_errmsg);

	  if (grub_strcmp (fs->name, "hfs") != 0
	      && grub_strcmp (fs->name, "hfsplus") != 0
	      && !is_guess)
	    grub_util_error (_("filesystem on %s is neither HFS nor HFS+"),
			     macppcdir);
	  if (grub_strcmp (fs->name, "hfs") == 0
	      || grub_strcmp (fs->name, "hfsplus") == 0)
	    {
	      install_device = macppcdir_device_names[0];
	      is_prep = 0;
	    }
	}
    }

  size_t ndev = 0;

  /* Write device to a variable so we don't have to traverse /dev every time.  */
  grub_devices = grub_guess_root_devices (grubdir);
  if (!grub_devices || !grub_devices[0])
    grub_util_error (_("cannot find a device for %s (is /dev mounted?)"),
		     grubdir);

  for (curdev = grub_devices; *curdev; curdev++)
    {
      grub_util_pull_device (*curdev);
      ndev++;
    }

  grub_drives = xcalloc (ndev + 1, sizeof (grub_drives[0]));

  for (curdev = grub_devices, curdrive = grub_drives; *curdev; curdev++,
       curdrive++)
    {
      *curdrive = grub_util_get_grub_dev (*curdev);
      if (! *curdrive)
	grub_util_error (_("cannot find a GRUB drive for %s.  Check your device.map"),
			 *curdev);
    }
  *curdrive = 0;

  grub_dev = grub_device_open (grub_drives[0]);
  if (! grub_dev)
    grub_util_error ("%s", grub_errmsg);

  grub_fs = grub_fs_probe (grub_dev);
  if (! grub_fs)
    grub_util_error ("%s", grub_errmsg);

  grub_install_push_module (grub_fs->name);

  if (grub_dev->disk)
    probe_mods (grub_dev->disk);

  for (curdrive = grub_drives + 1; *curdrive; curdrive++)
    {
      grub_device_t dev = grub_device_open (*curdrive);
      if (!dev)
	continue;
      if (dev->disk)
	probe_mods (dev->disk);
      grub_device_close (dev);
    }

  if (!config.is_cryptodisk_enabled && have_cryptodisk)
    grub_util_error (_("attempt to install to encrypted disk without cryptodisk enabled. "
		       "Set `%s' in file `%s'"), "GRUB_ENABLE_CRYPTODISK=y",
		     grub_util_get_config_filename ());

  if (disk_module && grub_strcmp (disk_module, "ata") == 0)
    grub_install_push_module ("pata");
  else if (disk_module && grub_strcmp (disk_module, "native") == 0)
    {
      grub_install_push_module ("pata");
      grub_install_push_module ("ahci");
      grub_install_push_module ("ohci");
      grub_install_push_module ("uhci");
      grub_install_push_module ("ehci");
      grub_install_push_module ("usbms");
    }
  else if (disk_module && disk_module[0])
    grub_install_push_module (disk_module);

  relative_grubdir = grub_make_system_path_relative_to_its_root (grubdir);
  if (relative_grubdir[0] == '\0')
    {
      free (relative_grubdir);
      relative_grubdir = xstrdup ("/");
    }

  char *prefix_drive = NULL;
  char *install_drive = NULL;

  if (install_device)
    {
      if (install_device[0] == '('
	  && install_device[grub_strlen (install_device) - 1] == ')')
        {
	  size_t len = grub_strlen (install_device) - 2;
	  install_drive = xmalloc (len + 1);
	  memcpy (install_drive, install_device + 1, len);
	  install_drive[len] = '\0';
        }
      else
	{
	  grub_util_pull_device (install_device);
	  install_drive = grub_util_get_grub_dev (install_device);
	  if (!install_drive)
	    grub_util_error (_("cannot find a GRUB drive for %s.  Check your device.map"),
			     install_device);
	}
    }

  grub_install_copy_files (grub_install_source_directory,
                           grubdir, platform);

  char *envfile = grub_util_path_concat (2, grubdir, "grubenv");
  if (!grub_util_is_regular (envfile))
    grub_util_create_envblk_file (envfile);

  char *platname =  grub_install_get_platform_name (platform);
  char *platdir;
  {
    char *t = grub_util_path_concat (2, grubdir,
                                   platname);
    platdir = grub_canonicalize_file_name (t);
    if (!platdir)
      grub_util_error (_("failed to get canonical path of `%s'"),
                       t);
    free (t);
  }

  load_cfg = grub_util_path_concat (2, platdir,
                                  "load.cfg");

  grub_util_unlink (load_cfg);

  if (debug_image && debug_image[0])
    {
      load_cfg_f = grub_util_fopen (load_cfg, "wb");
      have_load_cfg = 1;
      fprintf (load_cfg_f, "set debug='%s'\n",
              debug_image);
    }

  char *efi_signed = NULL;
  switch (platform)
    {
    case GRUB_INSTALL_PLATFORM_I386_EFI:
    case GRUB_INSTALL_PLATFORM_X86_64_EFI:
    case GRUB_INSTALL_PLATFORM_ARM_EFI:
    case GRUB_INSTALL_PLATFORM_ARM64_EFI:
    case GRUB_INSTALL_PLATFORM_IA64_EFI:
      {
	char *dir = xasprintf ("%s-signed", grub_install_source_directory);
	char *signed_image;
	if (removable)
	  signed_image = xasprintf ("gcd%s.efi.signed", efi_suffix);
	else
	  signed_image = xasprintf ("grub%s.efi.signed", efi_suffix);
	efi_signed = grub_util_path_concat (2, dir, signed_image);
	break;
      }

    default:
      break;
    }

  if (!efi_signed || !grub_util_is_regular (efi_signed))
    uefi_secure_boot = 0;

  if (!have_abstractions || uefi_secure_boot)
    {
      if ((disk_module && grub_strcmp (disk_module, "biosdisk") != 0)
	  || grub_drives[1]
	  || (!install_drive
	      && platform != GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275)
	  || (install_drive && !is_same_disk (grub_drives[0], install_drive))
	  || !have_bootdev (platform)
	  || uefi_secure_boot)
	{
	  char *uuid = NULL;

	  if (uefi_secure_boot && config.is_cryptodisk_enabled)
	    {
	      if (grub_dev->disk)
		probe_cryptodisk_uuid (grub_dev->disk);

	      for (curdrive = grub_drives + 1; *curdrive; curdrive++)
		{
		  grub_device_t dev = grub_device_open (*curdrive);
		  if (!dev)
		    continue;
		  if (dev->disk)
		    probe_cryptodisk_uuid (dev->disk);
		  grub_device_close (dev);
		}
	    }

	  /*  generic method (used on coreboot and ata mod).  */
	  if (!force_file_id
	      && grub_fs->fs_uuid && grub_fs->fs_uuid (grub_dev, &uuid))
	    {
	      grub_print_error ();
	      grub_errno = 0;
	      uuid = NULL;
	    }

	  if (!load_cfg_f)
	    load_cfg_f = grub_util_fopen (load_cfg, "wb");
	  have_load_cfg = 1;
	  if (uuid)
	    {
	      fprintf (load_cfg_f, "search.fs_uuid %s root ",
		      uuid);
	      grub_install_push_module ("search_fs_uuid");
	    }
	  else
	    {
	      char *rndstr = get_rndstr ();
	      char *fl = grub_util_path_concat (3, grubdir,
						     "uuid", rndstr);
	      char *fldir = grub_util_path_concat (2, grubdir,
							"uuid");
	      char *relfl;
	      FILE *flf;
	      grub_install_mkdir_p (fldir);
	      flf = grub_util_fopen (fl, "w");
	      if (!flf)
		grub_util_error (_("Can't create file: %s"), strerror (errno));
	      fclose (flf);
	      relfl = grub_make_system_path_relative_to_its_root (fl);
	      fprintf (load_cfg_f, "search.file %s root ",
		       relfl);
	      grub_install_push_module ("search_fs_file");
	    }
	  for (curdev = grub_devices, curdrive = grub_drives; *curdev; curdev++,
		 curdrive++)
	    {
	      const char *map;
	      char *g = NULL;
	      grub_device_t dev;
	      if (curdrive == grub_drives)
		dev = grub_dev;
	      else
		dev = grub_device_open (*curdrive);
	      if (!dev)
		continue;

	      if (dev->disk->dev->id != GRUB_DISK_DEVICE_HOSTDISK_ID)
		{
		  grub_util_fprint_full_disk_name (load_cfg_f,
						   dev->disk->name,
						   dev);
		  fprintf (load_cfg_f, " ");
		  if (dev != grub_dev)
		    grub_device_close (dev);
		  continue;
		}

	      map = grub_util_biosdisk_get_compatibility_hint (dev->disk);

	      if (map)
		{
		  grub_util_fprint_full_disk_name (load_cfg_f, map, dev);
		  fprintf (load_cfg_f, " ");
		}


	      if (disk_module && disk_module[0]
		  && grub_strcmp (disk_module, "biosdisk") != 0)
		  g = grub_util_guess_baremetal_drive (*curdev);
	      else
		switch (platform)
		  {
		  case GRUB_INSTALL_PLATFORM_I386_PC:
		    g = grub_util_guess_bios_drive (*curdev);
		    break;
		  case GRUB_INSTALL_PLATFORM_I386_EFI:
		  case GRUB_INSTALL_PLATFORM_X86_64_EFI:
		  case GRUB_INSTALL_PLATFORM_ARM_EFI:
		  case GRUB_INSTALL_PLATFORM_ARM64_EFI:
		  case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
		  case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
		  case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
		  case GRUB_INSTALL_PLATFORM_IA64_EFI:
		    g = grub_util_guess_efi_drive (*curdev);
		    break;
		  case GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275:
		  case GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275:
		  case GRUB_INSTALL_PLATFORM_I386_IEEE1275:
		    {
		      const char * ofpath = grub_util_devname_to_ofpath (*curdev);
		      g = xasprintf ("ieee1275/%s", ofpath);
		      break;
		    }
		  case GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON:
		  case GRUB_INSTALL_PLATFORM_I386_QEMU:
		  case GRUB_INSTALL_PLATFORM_I386_COREBOOT:
		  case GRUB_INSTALL_PLATFORM_ARM_COREBOOT:
		  case GRUB_INSTALL_PLATFORM_I386_MULTIBOOT:
		  case GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS:
		  case GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS:
		    g = grub_util_guess_baremetal_drive (*curdev);
		    break;
		  case GRUB_INSTALL_PLATFORM_MIPS_ARC:
		  case GRUB_INSTALL_PLATFORM_MIPSEL_ARC:
		  case GRUB_INSTALL_PLATFORM_ARM_UBOOT:
		  case GRUB_INSTALL_PLATFORM_I386_XEN:
		  case GRUB_INSTALL_PLATFORM_X86_64_XEN:
		  case GRUB_INSTALL_PLATFORM_I386_XEN_PVH:
		    grub_util_warn ("%s", _("no hints available for your platform. Expect reduced performance"));
		    break;
		    /* pacify warning.  */
		  case GRUB_INSTALL_PLATFORM_MAX:
		    break;
		  }
	      if (g)
		{
		  grub_util_fprint_full_disk_name (load_cfg_f, g, dev);
		  fprintf (load_cfg_f, " ");
		  free (g);
		}
	      if (dev != grub_dev)
		grub_device_close (dev);
	    }
	  fprintf (load_cfg_f, "\n");
	  char *escaped_relpath = escape (relative_grubdir);
	  fprintf (load_cfg_f, "set prefix=($root)'%s'\n",
		   escaped_relpath);
	}
      else
	{
	  /* We need to hardcode the partition number in the core image's prefix.  */
	  char *p;
	  for (p = grub_drives[0]; *p; )
	    {
	      if (*p == '\\' && p[1])
		{
		  p += 2;
		  continue;
		}
	      if (*p == ',' || *p == '\0')
		break;
	      p++;
	    }
	  prefix_drive = xasprintf ("(%s)", p);
	}
    }
  else
    {
      if (config.is_cryptodisk_enabled)
	{
	  if (grub_dev->disk)
	    probe_cryptodisk_uuid (grub_dev->disk);

	  for (curdrive = grub_drives + 1; *curdrive; curdrive++)
	    {
	      grub_device_t dev = grub_device_open (*curdrive);
	      if (!dev)
		continue;
	      if (dev->disk)
		probe_cryptodisk_uuid (dev->disk);
	      grub_device_close (dev);
	    }
	}
      prefix_drive = xasprintf ("(%s)", grub_drives[0]);
    }

  char mkimage_target[200];
  const char *core_name = NULL;

  switch (platform)
    {
    case GRUB_INSTALL_PLATFORM_I386_EFI:
    case GRUB_INSTALL_PLATFORM_X86_64_EFI:
    case GRUB_INSTALL_PLATFORM_ARM_EFI:
    case GRUB_INSTALL_PLATFORM_ARM64_EFI:
    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
    case GRUB_INSTALL_PLATFORM_IA64_EFI:
      core_name = "core.efi";
      snprintf (mkimage_target, sizeof (mkimage_target),
		"%s-%s",
		grub_install_get_platform_cpu (platform),
		grub_install_get_platform_platform (platform));
      break;
    case GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON:
    case GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS:
      core_name = "core.elf";
      snprintf (mkimage_target, sizeof (mkimage_target),
		"%s-%s-elf",
		grub_install_get_platform_cpu (platform),
		grub_install_get_platform_platform (platform));
      break;

    case GRUB_INSTALL_PLATFORM_I386_COREBOOT:
    case GRUB_INSTALL_PLATFORM_ARM_COREBOOT:
    case GRUB_INSTALL_PLATFORM_I386_MULTIBOOT:
    case GRUB_INSTALL_PLATFORM_I386_IEEE1275:
    case GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275:
    case GRUB_INSTALL_PLATFORM_I386_XEN:
    case GRUB_INSTALL_PLATFORM_X86_64_XEN:
    case GRUB_INSTALL_PLATFORM_I386_XEN_PVH:
      core_name = "core.elf";
      snprintf (mkimage_target, sizeof (mkimage_target),
		"%s-%s",
		grub_install_get_platform_cpu (platform),
		grub_install_get_platform_platform (platform));
      break;


    case GRUB_INSTALL_PLATFORM_I386_PC:
    case GRUB_INSTALL_PLATFORM_MIPSEL_ARC:
    case GRUB_INSTALL_PLATFORM_MIPS_ARC:
    case GRUB_INSTALL_PLATFORM_ARM_UBOOT:
    case GRUB_INSTALL_PLATFORM_I386_QEMU:
      snprintf (mkimage_target, sizeof (mkimage_target),
		"%s-%s",
		grub_install_get_platform_cpu (platform),
		grub_install_get_platform_platform (platform));
      core_name = "core.img";
      break;
    case GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275:
      strcpy (mkimage_target, "sparc64-ieee1275-raw");
      core_name = "core.img";
      break;
      /* pacify warning.  */
    case GRUB_INSTALL_PLATFORM_MAX:
      break;
    }

  if (!core_name)
    grub_util_error ("%s", _("You've found a bug"));

  if (load_cfg_f)
    fclose (load_cfg_f);

  char *imgfile = grub_util_path_concat (2, platdir,
				       core_name);
  char *prefix = xasprintf ("%s%s", prefix_drive ? : "",
			    relative_grubdir);
  grub_install_make_image_wrap (/* source dir  */ grub_install_source_directory,
				/*prefix */ prefix,
				/* output */ imgfile,
				/* memdisk */ NULL,
				have_load_cfg ? load_cfg : NULL,
				/* image target */ mkimage_target, 0);
  /* Backward-compatibility kludges.  */
  switch (platform)
    {
    case GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON:
      {
	char *dst = grub_util_path_concat (2, bootdir, "grub.elf");
	grub_install_copy_file (imgfile, dst, 1);
	free (dst);
      }
      break;

    case GRUB_INSTALL_PLATFORM_I386_IEEE1275:
    case GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275:
      {
	char *dst = grub_util_path_concat (2, grubdir, "grub");
	grub_install_copy_file (imgfile, dst, 1);
	free (dst);
      }
      break;

    case GRUB_INSTALL_PLATFORM_I386_EFI:
    case GRUB_INSTALL_PLATFORM_X86_64_EFI:
      {
	char *dst = grub_util_path_concat (2, platdir, "grub.efi");
	grub_install_make_image_wrap (/* source dir  */ grub_install_source_directory,
				      /* prefix */ "",
				       /* output */ dst,
				       /* memdisk */ NULL,
				      have_load_cfg ? load_cfg : NULL,
				       /* image target */ mkimage_target, 0);
      }
      break;
    case GRUB_INSTALL_PLATFORM_ARM_EFI:
    case GRUB_INSTALL_PLATFORM_ARM64_EFI:
    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
    case GRUB_INSTALL_PLATFORM_IA64_EFI:
    case GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_I386_COREBOOT:
    case GRUB_INSTALL_PLATFORM_ARM_COREBOOT:
    case GRUB_INSTALL_PLATFORM_I386_MULTIBOOT:
    case GRUB_INSTALL_PLATFORM_I386_PC:
    case GRUB_INSTALL_PLATFORM_MIPSEL_ARC:
    case GRUB_INSTALL_PLATFORM_MIPS_ARC:
    case GRUB_INSTALL_PLATFORM_ARM_UBOOT:
    case GRUB_INSTALL_PLATFORM_I386_QEMU:
    case GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275:
    case GRUB_INSTALL_PLATFORM_I386_XEN:
    case GRUB_INSTALL_PLATFORM_X86_64_XEN:
    case GRUB_INSTALL_PLATFORM_I386_XEN_PVH:
      break;
      /* pacify warning.  */
    case GRUB_INSTALL_PLATFORM_MAX:
      break;
    }

  /* Perform the platform-dependent install */

  switch (platform)
    {
    case GRUB_INSTALL_PLATFORM_I386_PC:
      {
	char *boot_img_src = grub_util_path_concat (2,
						  grub_install_source_directory,
						  "boot.img");
	char *boot_img = grub_util_path_concat (2, platdir,
					      "boot.img");
	grub_install_copy_file (boot_img_src, boot_img, 1);

	grub_util_info ("%sgrub-bios-setup %s %s %s %s %s --directory='%s' --device-map='%s' '%s'",
			/* TRANSLATORS: This is a prefix in the log to indicate that usually
			   a command would be executed but due to an option was skipped.  */
			install_bootsector ? "" : _("NOT RUNNING: "),
			allow_floppy ? "--allow-floppy " : "",
			verbosity ? "--verbose " : "",
			force ? "--force " : "",
			!fs_probe ? "--skip-fs-probe" : "",
			!add_rs_codes ? "--no-rs-codes" : "",
			platdir,
			device_map,
			install_device);

	/*  Now perform the installation.  */
	if (install_bootsector)
	  {
	    grub_util_bios_setup (platdir, "boot.img", "core.img",
				  install_drive, force,
				  fs_probe, allow_floppy, add_rs_codes,
				  !grub_install_is_short_mbrgap_supported ());

	    grub_set_install_backup_ponr ();
	  }

	/* If vestiges of GRUB Legacy still exist, tell the Debian packaging
	   that they can ignore them.  */
	if (!rootdir && grub_util_is_regular ("/boot/grub/stage2") &&
	    grub_util_is_regular ("/boot/grub/menu.lst"))
	  {
	    grub_util_fd_t fd;

	    fd = grub_util_fd_open ("/boot/grub/grub2-installed",
				    GRUB_UTIL_FD_O_WRONLY);
	    grub_util_fd_close (fd);
	  }

	break;
      }
    case GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275:
      {
	char *boot_img_src = grub_util_path_concat (2,
						  grub_install_source_directory,
						  "boot.img");
	char *boot_img = grub_util_path_concat (2, platdir,
					      "boot.img");
	grub_install_copy_file (boot_img_src, boot_img, 1);

	grub_util_info ("%sgrub-sparc64-setup %s %s %s %s --directory='%s' --device-map='%s' '%s'",
			install_bootsector ? "" : "NOT RUNNING: ",
			allow_floppy ? "--allow-floppy " : "",
			verbosity ? "--verbose " : "",
			force ? "--force " : "",
			!fs_probe ? "--skip-fs-probe" : "",
			platdir,
			device_map,
			install_drive);

	/*  Now perform the installation.  */
	if (install_bootsector)
	  {
	    grub_util_sparc_setup (platdir, "boot.img", "core.img",
				   install_drive, force,
				   fs_probe, allow_floppy,
				   0 /* unused */, 0 /* unused */ );

	    grub_set_install_backup_ponr ();
	  }
	break;
      }

    case GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275:
      if (macppcdir)
	{
	  char *core_services = grub_util_path_concat (4, macppcdir,
						       "System", "Library",
						       "CoreServices");
	  char *mach_kernel = grub_util_path_concat (2, macppcdir,
						     "mach_kernel");
	  char *grub_elf, *bootx;
	  FILE *f;
	  grub_device_t ins_dev;
	  char *grub_chrp = grub_util_path_concat (2,
						   grub_install_source_directory,
						   "grub.chrp");

	  grub_install_mkdir_p (core_services);

	  bootx = grub_util_path_concat (2, core_services, "BootX");
	  grub_install_copy_file (grub_chrp, bootx, 1);

	  grub_elf = grub_util_path_concat (2, core_services, "grub.elf");
	  grub_install_copy_file (imgfile, grub_elf, 1);

	  grub_set_install_backup_ponr ();

	  f = grub_util_fopen (mach_kernel, "a+");
	  if (!f)
	    grub_util_error (_("Can't create file: %s"), strerror (errno));
	  fclose (f);

	  fill_core_services (core_services);

	  ins_dev = grub_device_open (install_drive);
	  if (ins_dev == NULL)
	    grub_util_error ("%s", grub_errmsg);

	  bless (ins_dev, core_services, 0);

	  if (update_nvram)
	    {
	      const char *dev;
	      int partno;

	      partno = ins_dev->disk->partition
		? ins_dev->disk->partition->number + 1 : 0;
	      dev = grub_util_get_os_disk (install_device);
	      grub_install_register_ieee1275 (0, dev, partno,
					      "\\\\BootX");
	    }
	  grub_device_close (ins_dev);
  	  free (grub_elf);
	  free (bootx);
	  free (mach_kernel);
	  free (grub_chrp);
	  break;
	}
      /* If a install device is defined, copy the core.elf to PReP partition.  */
      if (is_prep && install_device && install_device[0])
	{
	  grub_device_t ins_dev;
	  ins_dev = grub_device_open (install_drive);
	  if (!ins_dev || !is_prep_partition (ins_dev))
	    {
	      grub_util_error ("%s", _("the chosen partition is not a PReP partition"));
	    }
	  if (is_prep_empty (ins_dev))
	    {
	      if (write_to_disk (ins_dev, imgfile))
		grub_util_error ("%s", _("failed to copy Grub to the PReP partition"));
	      grub_set_install_backup_ponr ();
	    }
	  else
	    {
	      char *s = xasprintf ("dd if=/dev/zero of=%s", install_device);
	      grub_util_error (_("the PReP partition is not empty. If you are sure you want to use it, run dd to clear it: `%s'"),
			       s);
	    }
	  grub_device_close (ins_dev);
	  if (update_nvram)
	    grub_install_register_ieee1275 (1, grub_util_get_os_disk (install_device),
					    0, NULL);
	  break;
      }
      /* fallthrough.  */
    case GRUB_INSTALL_PLATFORM_I386_IEEE1275:
      if (update_nvram)
	{
	  const char *dev;
	  char *relpath;
	  int partno;
	  relpath = grub_make_system_path_relative_to_its_root (imgfile);
	  partno = grub_dev->disk->partition
	    ? grub_dev->disk->partition->number + 1 : 0;
	  dev = grub_util_get_os_disk (grub_devices[0]);
	  grub_install_register_ieee1275 (0, dev,
					  partno, relpath);
	}
      break;
    case GRUB_INSTALL_PLATFORM_MIPS_ARC:
      grub_install_sgi_setup (install_device, imgfile, "grub");
      break;

    case GRUB_INSTALL_PLATFORM_I386_EFI:
      if (!efidir_is_mac)
	{
	  char *dst = grub_util_path_concat (2, efidir, "grub.efi");
	  /* For old macs. Suggested by Peter Jones.  */
	  grub_install_copy_file (imgfile, dst, 1);
	  free (dst);
	}
      /* Fallthrough.  */
    case GRUB_INSTALL_PLATFORM_X86_64_EFI:
      if (efidir_is_mac)
	{
	  char *boot_efi;
	  char *core_services = grub_util_path_concat (4, efidir,
						       "System", "Library",
						       "CoreServices");
	  char *mach_kernel = grub_util_path_concat (2, efidir,
						     "mach_kernel");
	  FILE *f;
	  grub_device_t ins_dev;

	  grub_install_mkdir_p (core_services);

	  boot_efi = grub_util_path_concat (2, core_services, "boot.efi");
	  grub_install_copy_file (imgfile, boot_efi, 1);

	  grub_set_install_backup_ponr ();

	  f = grub_util_fopen (mach_kernel, "r+");
	  if (!f)
	    grub_util_error (_("Can't create file: %s"), strerror (errno));
	  fclose (f);

	  fill_core_services(core_services);

	  ins_dev = grub_device_open (install_drive);
	  if (ins_dev == NULL)
	    grub_util_error ("%s", grub_errmsg);

	  bless (ins_dev, boot_efi, 1);
	  if (!removable && update_nvram)
	    {
	      /* Try to make this image bootable using the EFI Boot Manager, if available.  */
	      int ret;
	      ret = grub_install_register_efi (
		  efidir_grub_dev, efidir, "\\System\\Library\\CoreServices",
		  efi_distributor, efi_title);
	      if (ret)
	        grub_util_error (_("failed to register the EFI boot entry: %s"),
				 strerror (ret));
	    }

	  grub_device_close (ins_dev);
  	  free (boot_efi);
	  free (mach_kernel);
	  break;
	}
      /* FALLTHROUGH */
    case GRUB_INSTALL_PLATFORM_ARM_EFI:
    case GRUB_INSTALL_PLATFORM_ARM64_EFI:
    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV32_EFI:
    case GRUB_INSTALL_PLATFORM_RISCV64_EFI:
    case GRUB_INSTALL_PLATFORM_IA64_EFI:
      {
	char *dst = grub_util_path_concat (2, efidir, efi_file);
	if (uefi_secure_boot)
	  {
	    char *shim_signed = NULL;
	    char *mok_file = NULL;
	    char *bootcsv = NULL;
	    char *config_dst;
	    FILE *config_dst_f;

	    shim_signed = xasprintf ("/usr/lib/shim/shim%s.efi.signed", efi_suffix);
	    mok_file = xasprintf ("mm%s.efi", efi_suffix);
	    bootcsv = xasprintf ("BOOT%s.CSV", efi_suffix_upper);

	    if (grub_util_is_regular (shim_signed))
	      {
		char *chained_base, *chained_dst;
		char *mok_src, *mok_dst, *bootcsv_dst, *bootcsv_utf8;
		grub_ssize_t bootcsv_utf16_len;
		grub_uint16_t *bootcsv_utf16;
		FILE *bootcsv_dst_f;

		/* Install grub as our chained bootloader */
		chained_base = xasprintf ("grub%s.efi", efi_suffix);
		chained_dst = grub_util_path_concat (2, efidir, chained_base);
		grub_install_copy_file (efi_signed, chained_dst, 1);
		free (chained_dst);
		free (chained_base);

		/* Now handle shim, and make this our new "default" loader. */
		if (!removable)
		  {
		    free (efi_file);
		    efi_file = xasprintf ("shim%s.efi", efi_suffix);
		    free (dst);
		    dst = grub_util_path_concat (2, efidir, efi_file);
		  }
		grub_install_copy_file (shim_signed, dst, 1);
		free (efi_signed);
		efi_signed = xstrdup (shim_signed);

		/* Not critical, so not an error if it is not present (as it
		   won't be for older releases); but if we have MokManager,
		   make sure it gets installed.  */
		mok_src = grub_util_path_concat (2, "/usr/lib/shim/",
						    mok_file);
		mok_dst = grub_util_path_concat (2, efidir,
						    mok_file);
		grub_install_copy_file (mok_src,
					mok_dst, 0);
		free (mok_src);
		free (mok_dst);

		/* Also try to install boot.csv for fallback */
		bootcsv_dst = grub_util_path_concat (2, efidir, bootcsv);
		bootcsv_utf8 = xasprintf("shim%s.efi,%s,,This is the boot entry for %s\n",
			 		 efi_suffix, efi_title, efi_title);
		bootcsv_utf16_len = grub_utf8_to_utf16_alloc (bootcsv_utf8, &bootcsv_utf16, NULL);
		bootcsv_dst_f = grub_util_fopen (bootcsv_dst, "wb");
		fwrite (bootcsv_utf16, bootcsv_utf16_len, sizeof *bootcsv_utf16, bootcsv_dst_f);
		fclose (bootcsv_dst_f);
		free (bootcsv_utf16);
		free (bootcsv_utf8);
		free (bootcsv_dst);
	      }
	    else
	      grub_install_copy_file (efi_signed, dst, 1);

	    config_dst = grub_util_path_concat (2, efidir, "grub.cfg");
	    grub_install_copy_file (load_cfg, config_dst, 1);
	    config_dst_f = grub_util_fopen (config_dst, "ab");
	    fprintf (config_dst_f, "configfile $prefix/grub.cfg\n");
	    fclose (config_dst_f);
	    free (config_dst);
	    if (!removable && !no_extra_removable)
	      also_install_removable(efi_signed, base_efidir, efi_suffix, efi_suffix_upper);
	  }
	else
	  {
	    grub_install_copy_file (imgfile, dst, 1);
	    if (!removable && !no_extra_removable)
	      also_install_removable(imgfile, base_efidir, efi_suffix, efi_suffix_upper);
	  }

	grub_set_install_backup_ponr ();

	free (dst);
      }
      if (!removable && update_nvram)
	{
	  char * efifile_path;
	  char * part;
	  int ret;

	  /* Try to make this image bootable using the EFI Boot Manager, if available.  */
	  if (!efi_distributor || efi_distributor[0] == '\0')
	    grub_util_error ("%s", _("EFI bootloader id isn't specified."));
	  efifile_path = xasprintf ("\\EFI\\%s\\%s",
				    efi_distributor,
				    efi_file);
	  part = (efidir_grub_dev->disk->partition
		  ? grub_partition_get_name (efidir_grub_dev->disk->partition)
		  : 0);
	  grub_util_info ("Registering with EFI: distributor = `%s',"
			  " path = `%s', ESP at %s%s%s",
			  efi_distributor, efifile_path,
			  efidir_grub_dev->disk->name,
			  (part ? ",": ""), (part ? : ""));
	  grub_free (part);
	  ret = grub_install_register_efi (efidir_grub_dev, efidir,
					   efifile_path, efi_distributor,
					   efi_title);
	  if (ret)
	    grub_util_error (_("failed to register the EFI boot entry: %s"),
			     strerror (ret));
	}
      break;

    case GRUB_INSTALL_PLATFORM_I386_XEN:
      {
	char *path = grub_util_path_concat (2, bootdir, "xen");
	char *dst = grub_util_path_concat (2, path, "pvboot-i386.elf");
	grub_install_mkdir_p (path);
	grub_install_copy_file (imgfile, dst, 1);
	free (dst);
	free (path);
      }
      break;

    case GRUB_INSTALL_PLATFORM_X86_64_XEN:
      {
	char *path = grub_util_path_concat (2, bootdir, "xen");
	char *dst = grub_util_path_concat (2, path, "pvboot-x86_64.elf");
	grub_install_mkdir_p (path);
	grub_install_copy_file (imgfile, dst, 1);
	free (dst);
	free (path);
      }
      break;

    case GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON:
    case GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS:
    case GRUB_INSTALL_PLATFORM_I386_COREBOOT:
    case GRUB_INSTALL_PLATFORM_ARM_COREBOOT:
    case GRUB_INSTALL_PLATFORM_I386_MULTIBOOT:
    case GRUB_INSTALL_PLATFORM_MIPSEL_ARC:
    case GRUB_INSTALL_PLATFORM_ARM_UBOOT:
    case GRUB_INSTALL_PLATFORM_I386_QEMU:
    case GRUB_INSTALL_PLATFORM_I386_XEN_PVH:
      grub_util_warn ("%s",
		      _("WARNING: no platform-specific install was performed"));
      break;
      /* pacify warning.  */
    case GRUB_INSTALL_PLATFORM_MAX:
      break;
    }

  /*
   * Either there are no platform specific code, or it didn't raise
   * ponr. Raise it here, because usually this is already past point
   * of no return. If we leave this flag false, at exit all the modules
   * will be removed from the prefix which would be very confusing.
   */
  grub_set_install_backup_ponr ();

  fprintf (stderr, "%s\n", _("Installation finished. No error reported."));

  /* Free resources.  */
  grub_gcry_fini_all ();
  grub_fini_all ();

  return 0;
}
