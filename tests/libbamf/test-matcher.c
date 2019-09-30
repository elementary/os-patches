/*
 * Copyright (C) 2009-2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Marco Trevisan (Treviño) <marco.trevisan@canonical.com>
 *              Neil Jagdish Patel <neil.patel@canonical.com>
 *
 */

#include <glib.h>
#include <stdlib.h>
#include "libbamf.h"

void ignore_fatal_errors (void);

static void
test_allocation (void)
{
  BamfMatcher *matcher;

  ignore_fatal_errors();
  matcher = bamf_matcher_get_default ();
  g_assert (BAMF_IS_MATCHER (matcher));

  g_object_unref (matcher);
}

static void
test_singleton (void)
{
  BamfMatcher *matcher_1, *matcher_2;

  ignore_fatal_errors();
  matcher_1 = bamf_matcher_get_default ();
  g_assert (BAMF_IS_MATCHER (matcher_1));

  matcher_2 = bamf_matcher_get_default ();
  g_assert (matcher_1 == matcher_2);
  g_assert_cmpuint (G_OBJECT (matcher_1)->ref_count, ==, 2);

  g_object_unref (matcher_2);
  g_object_unref (matcher_1);
}

static void
test_singleton_after_unref (void)
{
  BamfMatcher *matcher_old, *matcher_new;

  ignore_fatal_errors();
  matcher_old = bamf_matcher_get_default ();
  g_object_add_weak_pointer (G_OBJECT (matcher_old), (gpointer*) &matcher_old);
  g_object_unref (matcher_old);
  g_assert (matcher_old == NULL);

  matcher_new = bamf_matcher_get_default ();
  g_assert (matcher_old != matcher_new);
  g_object_unref (matcher_new);
}

void
test_matcher_create_suite (void)
{
#define DOMAIN "/Matcher"

  g_test_add_func (DOMAIN"/Allocation", test_allocation);
  g_test_add_func (DOMAIN"/Singleton", test_singleton);
  g_test_add_func (DOMAIN"/SingletonUnref", test_singleton_after_unref);
}
