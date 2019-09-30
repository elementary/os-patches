/*
 * Copyright (C) 2009 Canonical Ltd
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
 * Authored by Jason Smith <jason.smith@canonical.com>
 *
 */

#include <glib.h>
#include <stdlib.h>
#include "bamf-window.h"
#include "bamf-legacy-window.h"
#include "bamf-legacy-window-test.h"

static void test_allocation    (void);
static void test_xid           (void);
static void test_hints         (void);
static void test_active        (void);
static void test_urgent        (void);
static void test_user_visible  (void);
static void test_maximized     (void);
static void test_vmaximized    (void);
static void test_hmaximized    (void);
static void test_hmaximized    (void);

static gboolean signal_seen = FALSE;
static gboolean signal_result = FALSE;

void
test_window_create_suite (void)
{
#define DOMAIN "/Window"

  g_test_add_func (DOMAIN"/Allocation", test_allocation);
  g_test_add_func (DOMAIN"/Xid", test_xid);
  g_test_add_func (DOMAIN"/Hints", test_hints);
  g_test_add_func (DOMAIN"/Events/Active", test_active);
  g_test_add_func (DOMAIN"/Events/Urgent", test_urgent);
  g_test_add_func (DOMAIN"/Events/UserVisible", test_user_visible);
  g_test_add_func (DOMAIN"/Events/Maximized", test_maximized);
  g_test_add_func (DOMAIN"/Events/VerticallyMaximized", test_vmaximized);
  g_test_add_func (DOMAIN"/Events/HorizontallyMaximized", test_hmaximized);
}

void
test_allocation (void)
{
  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Window X", "class", "exec");

  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));

  g_assert (BAMF_IS_WINDOW (window));

  g_object_unref (window);


  g_object_unref (test);
}

void
test_hints (void)
{
  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Window X", "class", "exec");
  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));

  bamf_legacy_window_set_hint (BAMF_LEGACY_WINDOW (test), "HINT_NAME", "HINT_VALUE");

  g_assert_cmpstr (bamf_window_get_string_hint (window, "HINT_NAME"), ==, "HINT_VALUE");
  g_assert_cmpstr (bamf_window_get_string_hint (window, "INVALID_HINT_NAME"), ==, NULL);

  g_object_unref (window);
  g_object_unref (test);
}

void
test_xid (void)
{
  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Window X", "class", "exec");

  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));

  g_assert (bamf_window_get_xid (window) == 20);

  g_object_unref (window);
  g_object_unref (test);
}

void
on_urgent_changed (BamfWindow *window, gboolean result, gpointer pointer)
{
  signal_seen = TRUE;
  signal_result = result;
}

void
test_urgent (void)
{
  signal_seen = FALSE;

  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Window X", "class", "exec");

  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));
  g_signal_connect (G_OBJECT (window), "urgent-changed", (GCallback) on_urgent_changed, NULL);

  g_assert (!bamf_view_is_urgent (BAMF_VIEW (window)));
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_attention (test, TRUE);
  g_assert (bamf_view_is_urgent (BAMF_VIEW (window)));
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_legacy_window_test_set_attention (test, FALSE);
  g_assert (!bamf_view_is_urgent (BAMF_VIEW (window)));
  g_assert (signal_seen);
  g_assert (!signal_result);

  g_object_unref (window);
  g_object_unref (test);
}

void
on_active_changed (BamfWindow *window, gboolean result, gpointer pointer)
{
  signal_seen = TRUE;
  signal_result = result;
}

void
test_active (void)
{
  signal_seen = FALSE;

  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Window X", "class", "exec");

  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));
  g_signal_connect (G_OBJECT (window), "active-changed", (GCallback) on_active_changed, NULL);

  g_assert (!bamf_view_is_active (BAMF_VIEW (window)));
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_active (test, TRUE);
  g_assert (bamf_view_is_active (BAMF_VIEW (window)));
  g_assert (!signal_seen);
  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_legacy_window_test_set_active (test, FALSE);
  g_assert (!bamf_view_is_active (BAMF_VIEW (window)));
  g_assert (!signal_seen);
  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (signal_seen);
  g_assert (!signal_result);

  g_object_unref (window);
  g_object_unref (test);
}

void
on_user_visible_changed (BamfWindow *window, gboolean result, gpointer pointer)
{
  signal_seen = TRUE;
  signal_result = result;
}

void
test_user_visible (void)
{
  signal_seen = FALSE;

  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Window X", "class", "exec");

  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));
  g_signal_connect (G_OBJECT (window), "user-visible-changed", (GCallback) on_user_visible_changed, NULL);

  g_assert (bamf_view_is_user_visible (BAMF_VIEW (window)));
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_skip (test, TRUE);
  g_assert (!bamf_view_is_user_visible (BAMF_VIEW (window)));
  g_assert (signal_seen);
  g_assert (!signal_result);

  signal_seen = FALSE;

  bamf_legacy_window_test_set_skip (test, FALSE);
  g_assert (bamf_view_is_user_visible (BAMF_VIEW (window)));
  g_assert (signal_seen);
  g_assert (signal_result);

  g_object_unref (window);
  g_object_unref (test);
}

void
on_maximized_changed (BamfWindow *window, gint old, gint new, gpointer pointer)
{
  signal_seen = TRUE;
  signal_result = (old == BAMF_WINDOW_FLOATING && new == BAMF_WINDOW_MAXIMIZED);
}


void
test_maximized (void)
{
  signal_seen = FALSE;

  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Maximized Window X", "class", "exec");

  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));
  g_signal_connect (G_OBJECT (window), "maximized-changed", (GCallback) on_maximized_changed, NULL);

  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_FLOATING);
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_maximized (test, BAMF_WINDOW_MAXIMIZED);
  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_MAXIMIZED);
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_legacy_window_test_set_maximized (test, BAMF_WINDOW_FLOATING);
  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_FLOATING);
  g_assert (signal_seen);
  g_assert (!signal_result);

  g_object_unref (window);
  g_object_unref (test);
}

void
on_vmaximized_changed (BamfWindow *window, gint old, gint new, gpointer pointer)
{
  signal_seen = TRUE;
  signal_result = (old == BAMF_WINDOW_FLOATING && new == BAMF_WINDOW_VERTICAL_MAXIMIZED);
}

void
test_vmaximized (void)
{
  signal_seen = FALSE;

  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Maximized Window X", "class", "exec");

  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));
  g_signal_connect (G_OBJECT (window), "maximized-changed", (GCallback) on_vmaximized_changed, NULL);

  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_FLOATING);
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_maximized (test, BAMF_WINDOW_VERTICAL_MAXIMIZED);
  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_VERTICAL_MAXIMIZED);
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_legacy_window_test_set_maximized (test, BAMF_WINDOW_FLOATING);
  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_FLOATING);
  g_assert (signal_seen);
  g_assert (!signal_result);

  g_object_unref (window);
  g_object_unref (test);
}

void
on_hmaximized_changed (BamfWindow *window, gint old, gint new, gpointer pointer)
{
  signal_seen = TRUE;
  signal_result = (old == BAMF_WINDOW_FLOATING && new == BAMF_WINDOW_HORIZONTAL_MAXIMIZED);
}

void
test_hmaximized (void)
{
  signal_seen = FALSE;

  BamfWindow *window;
  BamfLegacyWindowTest *test;

  test = bamf_legacy_window_test_new (20,"Maximized Window X", "class", "exec");

  window = bamf_window_new (BAMF_LEGACY_WINDOW (test));
  g_signal_connect (G_OBJECT (window), "maximized-changed", (GCallback) on_hmaximized_changed, NULL);

  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_FLOATING);
  g_assert (!signal_seen);

  bamf_legacy_window_test_set_maximized (test, BAMF_WINDOW_HORIZONTAL_MAXIMIZED);
  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_HORIZONTAL_MAXIMIZED);
  g_assert (signal_seen);
  g_assert (signal_result);

  signal_seen = FALSE;

  bamf_legacy_window_test_set_maximized (test, BAMF_WINDOW_FLOATING);
  g_assert (bamf_window_maximized (window) == BAMF_WINDOW_FLOATING);
  g_assert (signal_seen);
  g_assert (!signal_result);

  g_object_unref (window);
  g_object_unref (test);
}
