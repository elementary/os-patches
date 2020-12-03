/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2019 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#include <dlfcn.h>
#include <glib.h>
#include <locale.h>
#include <stdlib.h>
#include "config.h"

/* Test that the `pam_malcontent.so` module can be loaded using dlopen() and
 * that it exports the appropriate symbols for PAM to be able to use it. */
static void
test_pam_malcontent_dlopen (void)
{
  g_autofree gchar *module_path = NULL;
  void *handle;
  int retval;
  void *fn;

  module_path = g_test_build_filename (G_TEST_BUILT, "..", "pam_malcontent.so", NULL);

  /* Installed tests version. */
  if (!g_file_test (module_path, G_FILE_TEST_EXISTS))
    {
      g_free (module_path);
      module_path = g_build_filename (PAMLIBDIR, "pam_malcontent.so", NULL);
    }

  /* Check the module can be loaded. */
  handle = dlopen (module_path, RTLD_NOW);
  g_assert_nonnull (handle);

  /* Check the appropriate symbols exist. */
  fn = dlsym (handle, "pam_sm_acct_mgmt");
  g_assert_nonnull (fn);

  retval = dlclose (handle);
  g_assert_cmpint (retval, ==, 0);
}

int
main (int    argc,
      char **argv)
{
  setlocale (LC_ALL, "");
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/pam_malcontent/dlopen", test_pam_malcontent_dlopen);

  return g_test_run ();
}
