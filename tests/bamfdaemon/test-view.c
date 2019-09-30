/*
 * Copyright (C) 2009-2011 Canonical Ltd
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
 * Authored by Neil Jagdish Patel <neil.patel@canonical.com>
 *             Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#include <glib.h>
#include <stdlib.h>
#include "bamf-view.h"

static GDBusConnection *gdbus_connection = NULL;

static void
test_allocation (void)
{
  BamfView    *view;

  view = g_object_new (BAMF_TYPE_VIEW, NULL);
  g_assert (BAMF_IS_VIEW (view));

  g_object_unref (G_OBJECT (view));
}

#define test_string_property(prop) test_##prop
#define declare_test_string_property(prop)                         \
  static void                                                      \
  test_string_property (prop) (void)                               \
  {                                                                \
    BamfView *view;                                                \
                                                                   \
    view = g_object_new (BAMF_TYPE_VIEW, NULL);                    \
    g_assert (!bamf_view_get_##prop (view));                       \
                                                                   \
    const gchar *new_##prop = "Some" #prop;                        \
    bamf_view_set_##prop (view, new_##prop);                       \
    g_assert_cmpstr (bamf_view_get_##prop (view), ==, new_##prop); \
                                                                   \
    new_##prop = "Another" #prop;                                  \
    bamf_view_set_##prop (view, new_##prop);                       \
    g_assert_cmpstr (bamf_view_get_##prop (view), ==, new_##prop); \
                                                                   \
    g_object_unref (view);                                         \
  }

declare_test_string_property (name);
declare_test_string_property (icon);

#define test_string_property_exported(prop) test_##prop##_exported
#define declare_test_string_property_exported(prop)                \
  static void                                                      \
  test_string_property_exported (prop) (void)                      \
  {                                                                \
    BamfView *view;                                                \
                                                                   \
    view = g_object_new (BAMF_TYPE_VIEW, NULL);                    \
    g_assert (!bamf_view_get_##prop (view));                       \
                                                                   \
    const gchar *new_##prop = "Some" #prop;                        \
    bamf_view_set_##prop (view, new_##prop);                       \
    bamf_view_export_on_bus (view, gdbus_connection);              \
    g_assert_cmpstr (bamf_view_get_##prop (view), ==, new_##prop); \
                                                                   \
    new_##prop = "Another" #prop;                                  \
    bamf_view_set_##prop (view, new_##prop);                       \
    g_assert_cmpstr (bamf_view_get_##prop (view), ==, new_##prop); \
                                                                   \
    g_object_unref (view);                                         \
  }

declare_test_string_property_exported (name);
declare_test_string_property_exported (icon);

#define test_boolean_property(prop) test_##prop
#define declare_test_boolean_property(prop)     \
  static void                                   \
  test_boolean_property (prop) (void)           \
  {                                             \
    BamfView *view;                             \
                                                \
    view = g_object_new (BAMF_TYPE_VIEW, NULL); \
    g_assert (!bamf_view_is_##prop (view));     \
                                                \
    bamf_view_set_##prop (view, TRUE);          \
    g_assert (bamf_view_is_##prop (view));      \
                                                \
    bamf_view_set_##prop (view, FALSE);         \
    g_assert (!bamf_view_is_##prop (view));     \
                                                \
    g_object_unref (view);                      \
  }

declare_test_boolean_property (active);
declare_test_boolean_property (running);
declare_test_boolean_property (urgent);
declare_test_boolean_property (user_visible);

#define test_boolean_property_exported(prop) test_##prop##_exported
#define declare_test_boolean_property_exported(prop)  \
  static void                                         \
  test_boolean_property_exported(prop) (void)         \
  {                                                   \
    BamfView *view;                                   \
                                                      \
    view = g_object_new (BAMF_TYPE_VIEW, NULL);       \
    bamf_view_set_active (view, TRUE);                \
                                                      \
    bamf_view_export_on_bus (view, gdbus_connection); \
    g_assert (bamf_view_is_active (view));            \
                                                      \
    bamf_view_set_active (view, FALSE);               \
    g_assert (!bamf_view_is_active (view));           \
                                                      \
    g_object_unref (view);                            \
  }

declare_test_boolean_property_exported (active);
declare_test_boolean_property_exported (running);
declare_test_boolean_property_exported (urgent);
declare_test_boolean_property_exported (user_visible);

static void
test_path (void)
{
  BamfView *view;
  const char *path;

  view = g_object_new (BAMF_TYPE_VIEW, NULL);
  g_assert (bamf_view_get_path (view) == NULL);

  path = bamf_view_export_on_bus (view, gdbus_connection);
  g_assert (path);
  g_assert (g_strcmp0 (path, bamf_view_get_path (view)) == 0);

  g_object_unref (view);
}

static void
test_path_collision (void)
{
  int i, j;

  for (i = 0; i < 20; i++)
    {
      GList *views = NULL;

      for (j = 0; j < 2000; j++)
        {
          BamfView * view = g_object_new (BAMF_TYPE_VIEW, NULL);
          g_assert (BAMF_IS_VIEW (view));

          views = g_list_prepend (views, view);

          bamf_view_export_on_bus (view, gdbus_connection);
        }

      g_list_free_full (views, g_object_unref);
    }
}

static void
test_children (void)
{
  BamfView *parent;
  BamfView *child1, *child2, *child3;

  parent = g_object_new (BAMF_TYPE_VIEW, NULL);
  child1 = g_object_new (BAMF_TYPE_VIEW, NULL);
  child2 = g_object_new (BAMF_TYPE_VIEW, NULL);
  child3 = g_object_new (BAMF_TYPE_VIEW, NULL);

  g_assert (bamf_view_get_children (parent) == NULL);

  bamf_view_add_child (parent, child1);
  g_assert (g_list_length (bamf_view_get_children (parent)) == 1);
  g_assert (g_list_nth_data (bamf_view_get_children (parent), 0) == child1);

  bamf_view_add_child (parent, child2);
  bamf_view_add_child (parent, child3);
  g_assert (g_list_length (bamf_view_get_children (parent)) == 3);

  bamf_view_close (child1);
  g_object_unref (child1);
  g_assert (g_list_length (bamf_view_get_children (parent)) == 2);

  bamf_view_close (child2);
  g_object_unref (child2);
  g_assert (g_list_length (bamf_view_get_children (parent)) == 1);

  bamf_view_close (child3);
  g_object_unref (child3);
  g_assert (g_list_length (bamf_view_get_children (parent)) == 0);

  bamf_view_close (parent);
  g_object_unref (parent);
}

static void
test_children_paths (void)
{
  BamfView *parent;
  BamfView *child1, *child2, *child3;
  GVariant *container;
  GVariantIter *paths;
  const char *path;
  gboolean found;

  parent = g_object_new (BAMF_TYPE_VIEW, NULL);
  child1 = g_object_new (BAMF_TYPE_VIEW, NULL);
  child2 = g_object_new (BAMF_TYPE_VIEW, NULL);
  child3 = g_object_new (BAMF_TYPE_VIEW, NULL);

  bamf_view_export_on_bus (parent, gdbus_connection);
  bamf_view_export_on_bus (child1, gdbus_connection);
  bamf_view_export_on_bus (child2, gdbus_connection);

  g_assert (bamf_view_get_children (parent) == NULL);

  bamf_view_add_child (parent, child1);
  bamf_view_add_child (parent, child2);
  bamf_view_add_child (parent, child3);
  g_assert (g_list_length (bamf_view_get_children (parent)) == 3);

  container = bamf_view_get_children_paths (parent);
  g_assert (g_variant_type_equal (g_variant_get_type (container),
                                  G_VARIANT_TYPE ("(as)")));
  g_assert (g_variant_n_children (container) == 1);
  g_variant_get (container, "(as)", &paths);
  g_assert (g_variant_iter_n_children (paths) == 2);
  g_variant_iter_free (paths);
  g_variant_unref (container);

  bamf_view_export_on_bus (child3, gdbus_connection);

  container = bamf_view_get_children_paths (parent);
  g_variant_get (container, "(as)", &paths);
  g_assert (g_variant_iter_n_children (paths) == 3);

  found = FALSE;
  while (g_variant_iter_loop (paths, "s", &path))
    {
      if (g_strcmp0 (path, bamf_view_get_path (child1)) == 0)
        {
          found = TRUE;
          break;
        }
    }

  g_assert (found);

  found = FALSE;
  g_variant_get (container, "(as)", &paths);
  while (g_variant_iter_loop (paths, "s", &path))
    {
      if (g_strcmp0 (path, bamf_view_get_path (child2)) == 0)
        {
          found = TRUE;
          break;
        }
    }
  g_variant_iter_free (paths);

  g_assert (found);

  found = FALSE;
  g_variant_get (container, "(as)", &paths);
  while (g_variant_iter_loop (paths, "s", &path))
    {
      if (g_strcmp0 (path, bamf_view_get_path (child3)) == 0)
        {
          found = TRUE;
          break;
        }
    }
  g_variant_iter_free (paths);

  g_assert (found);

  g_variant_unref (container);

  g_object_unref (child1);
  g_object_unref (child2);
  g_object_unref (child3);
  g_object_unref (parent);
}

static gboolean boolean_event_fired = FALSE;
static gboolean boolean_event_result = FALSE;
static guint boolean_event_calls = 0;

static void
on_boolean_event (BamfView *view, gboolean event, gpointer pointer)
{
  boolean_event_fired = TRUE;
  boolean_event_result = event;
}

#define test_boolean_property_event(prop) test_##prop##_event
#define declare_test_boolean_property_event(prop)                                \
  static void                                                                    \
  test_boolean_property_event (prop) (void)                                      \
  {                                                                              \
    BamfView *view;                                                              \
                                                                                 \
    view = g_object_new (BAMF_TYPE_VIEW, NULL);                                  \
    g_assert (!bamf_view_is_##prop (view));                                      \
                                                                                 \
    g_signal_connect (G_OBJECT (view), #prop "_changed",                         \
          (GCallback) on_boolean_event, NULL);                                   \
                                                                                 \
    boolean_event_fired = FALSE;                                                 \
    boolean_event_result = FALSE;                                                \
                                                                                 \
    bamf_view_set_##prop (view, TRUE);                                           \
    g_assert (bamf_view_is_##prop (view));                                       \
                                                                                 \
    while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE); \
    g_assert (boolean_event_fired);                                              \
    g_assert (boolean_event_result);                                             \
                                                                                 \
    boolean_event_fired = FALSE;                                                 \
    bamf_view_set_##prop (view, FALSE);                                          \
    g_assert (!bamf_view_is_##prop (view));                                      \
                                                                                 \
    while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE); \
    g_assert (boolean_event_fired);                                              \
    g_assert (!boolean_event_result);                                            \
                                                                                 \
    g_object_unref (view);                                                       \
  }

declare_test_boolean_property_event (active);
declare_test_boolean_property_event (running);
declare_test_boolean_property_event (urgent);
declare_test_boolean_property_event (user_visible);

static gboolean string_event_fired = FALSE;
static char * string_event_result = NULL;

static void
on_string_event (BamfView *view, const gchar *oval, const gchar *nval, gpointer pointer)
{
  string_event_fired = TRUE;
  g_free (string_event_result);
  string_event_result = g_strdup (nval);
}

static void
test_name_event (void)
{
  BamfView *view;

  view = g_object_new (BAMF_TYPE_VIEW, NULL);
  g_assert (!bamf_view_get_name (view));

  g_signal_connect (G_OBJECT (view), "name_changed",
        (GCallback) on_string_event, NULL);

  string_event_fired = FALSE;
  string_event_result = NULL;

  bamf_view_set_name (view, "NewName");
  g_assert_cmpstr (bamf_view_get_name (view), ==, "NewName");

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (string_event_fired);
  g_assert_cmpstr (string_event_result, ==, "NewName");

  string_event_fired = FALSE;
  bamf_view_set_name (view, "AnotherName");
  g_assert_cmpstr (bamf_view_get_name (view), ==, "AnotherName");

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (string_event_fired);
  g_assert_cmpstr (string_event_result, ==, "AnotherName");

  g_object_unref (view);
}

static gboolean property_event_fired = FALSE;
static char * property_event_name = NULL;

static void
on_property_changed (BamfView *view, GParamSpec *param, BamfView *self)
{
  g_free (property_event_name);
  property_event_fired = TRUE;
  property_event_name = g_strdup (param->name);
}

static void
test_icon_event (void)
{
  BamfView *view;

  view = g_object_new (BAMF_TYPE_VIEW, NULL);
  g_assert (!bamf_view_get_icon (view));

  g_signal_connect (G_OBJECT (view), "notify::icon",
        (GCallback) on_property_changed, NULL);

  property_event_fired = FALSE;
  property_event_name = NULL;

  bamf_view_set_icon (view, "NewIcon");

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (property_event_fired);
  g_assert_cmpstr (property_event_name, ==, "icon");

  property_event_fired = FALSE;
  bamf_view_set_icon (view, "AnotherIcon");
  g_assert_cmpstr (bamf_view_get_icon (view), ==, "AnotherIcon");

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (property_event_fired);
  g_assert_cmpstr (property_event_name, ==, "icon");

  g_object_unref (view);
}


#define test_boolean_property_event_exported(prop) test_##prop##_event_exported
#define declare_test_boolean_property_event_exported(prop)                       \
  static void                                                                    \
  test_boolean_property_event_exported (prop) (void)                             \
  {                                                                              \
    BamfView *view;                                                              \
                                                                                 \
    view = g_object_new (BAMF_TYPE_VIEW, NULL);                                  \
    g_assert (!bamf_view_is_##prop (view));                                      \
                                                                                 \
    g_signal_connect (G_OBJECT (view), #prop "_changed",                         \
          (GCallback) on_boolean_event, NULL);                                   \
                                                                                 \
    boolean_event_fired = FALSE;                                                 \
    boolean_event_result = FALSE;                                                \
                                                                                 \
    bamf_view_set_##prop (view, TRUE);                                           \
                                                                                 \
    while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE); \
    g_assert (boolean_event_fired);                                              \
    g_assert (boolean_event_result);                                             \
    boolean_event_fired = FALSE;                                                 \
    boolean_event_result = FALSE;                                                \
                                                                                 \
    bamf_view_export_on_bus (view, gdbus_connection);                            \
    while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE); \
    g_assert (boolean_event_fired);                                              \
    g_assert (boolean_event_result);                                             \
                                                                                 \
    boolean_event_fired = FALSE;                                                 \
    bamf_view_set_##prop (view, FALSE);                                          \
    g_assert (!bamf_view_is_##prop (view));                                      \
                                                                                 \
    while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE); \
    g_assert (boolean_event_fired);                                              \
    g_assert (!boolean_event_result);                                            \
                                                                                 \
    g_object_unref (view);                                                       \
  }

declare_test_boolean_property_event_exported (active);
declare_test_boolean_property_event_exported (running);
declare_test_boolean_property_event_exported (urgent);
declare_test_boolean_property_event_exported (user_visible);

static void
test_name_event_exported (void)
{
  BamfView *view;

  view = g_object_new (BAMF_TYPE_VIEW, NULL);
  g_assert (!bamf_view_get_name (view));

  g_signal_connect (G_OBJECT (view), "name_changed",
        (GCallback) on_string_event, NULL);

  string_event_fired = FALSE;
  string_event_result = NULL;

  bamf_view_set_name (view, "NewName");

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (string_event_fired);
  g_assert_cmpstr (string_event_result, ==, "NewName");
  string_event_fired = FALSE;
  g_free (string_event_result);
  string_event_result = NULL;

  bamf_view_export_on_bus (view, gdbus_connection);
  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (string_event_fired);
  g_assert_cmpstr (string_event_result, ==, "NewName");

  string_event_fired = FALSE;
  bamf_view_set_name (view, "AnotherName");
  g_assert_cmpstr (bamf_view_get_name (view), ==, "AnotherName");

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (string_event_fired);
  g_assert_cmpstr (string_event_result, ==, "AnotherName");

  g_object_unref (view);
}

static void
test_icon_event_exported (void)
{
  BamfView *view;

  view = g_object_new (BAMF_TYPE_VIEW, NULL);
  g_assert (!bamf_view_get_icon (view));


  g_signal_connect (G_OBJECT (view), "notify::icon",
        (GCallback) on_property_changed, NULL);

  property_event_fired = FALSE;
  property_event_name = NULL;

  bamf_view_set_icon (view, "NewIcon");

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (property_event_fired);
  g_assert_cmpstr (property_event_name, ==, "icon");
  property_event_fired = FALSE;
  g_free (property_event_name);
  property_event_name = NULL;

  bamf_view_export_on_bus (view, gdbus_connection);
  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (property_event_fired);
  g_assert_cmpstr (property_event_name, ==, "icon");

  property_event_fired = FALSE;
  bamf_view_set_icon (view, "AnotherIcon");
  g_assert_cmpstr (bamf_view_get_icon (view), ==, "AnotherIcon");

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert (property_event_fired);
  g_assert_cmpstr (property_event_name, ==, "icon");
  g_object_unref (view);
}

static void
on_boolean_event_count (BamfView *view, gboolean event, gpointer pointer)
{
  boolean_event_calls++;
  boolean_event_result = event;
}

static void
test_active_event_count (void)
{
  BamfView *view;

  view = g_object_new (BAMF_TYPE_VIEW, NULL);
  g_assert (!bamf_view_is_active (view));

  g_signal_connect (G_OBJECT (view), "active-changed",
        (GCallback) on_boolean_event_count, NULL);

  boolean_event_calls = 0;
  bamf_view_set_active (view, TRUE);
  g_assert (bamf_view_is_active (view));
  g_assert_cmpuint (boolean_event_calls, ==, 0);

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert_cmpuint (boolean_event_calls, ==, 1);
  g_assert (boolean_event_result);

  boolean_event_calls = 0;
  bamf_view_set_active (view, FALSE);
  bamf_view_set_active (view, TRUE);
  bamf_view_set_active (view, FALSE);

  while (g_main_context_pending (NULL)) g_main_context_iteration (NULL, TRUE);
  g_assert_cmpuint (boolean_event_calls, ==, 1);
  g_assert (!boolean_event_result);

  g_object_unref (view);
}

static gboolean child_added_event_fired;
static char * child_added_event_result;

static void
on_child_added (BamfView *view, char *path, gpointer pointer)
{
  child_added_event_fired = TRUE;
  child_added_event_result = g_strdup (path);
}

static void
test_child_added_event (void)
{
  BamfView *parent;
  BamfView *child;

  parent = g_object_new (BAMF_TYPE_VIEW, NULL);
  child = g_object_new (BAMF_TYPE_VIEW, NULL);

  bamf_view_export_on_bus (parent, gdbus_connection);
  bamf_view_export_on_bus (child, gdbus_connection);

  g_signal_connect (G_OBJECT (parent), "child-added",
                    (GCallback) on_child_added, NULL);

  child_added_event_fired = FALSE;
  bamf_view_add_child (parent, child);

  g_assert (child_added_event_fired);
  g_assert (g_strcmp0 (bamf_view_get_path (child), child_added_event_result) == 0);

  bamf_view_close (child);
  bamf_view_close (parent);

  g_object_unref (child);
  g_object_unref (parent);
}

static gboolean child_removed_event_fired;
static char * child_removed_event_result;

static void
on_child_removed (BamfView *view, char *path, gpointer pointer)
{
  child_removed_event_fired = TRUE;
  child_removed_event_result = g_strdup (path);
}

static void
test_child_removed_event (void)
{
  BamfView *parent;
  BamfView *child;

  parent = g_object_new (BAMF_TYPE_VIEW, NULL);
  child = g_object_new (BAMF_TYPE_VIEW, NULL);

  bamf_view_export_on_bus (parent, gdbus_connection);
  bamf_view_export_on_bus (child, gdbus_connection);
  bamf_view_add_child (parent, child);

  g_signal_connect (G_OBJECT (parent), "child-removed",
                    (GCallback) on_child_removed, NULL);

  child_removed_event_fired = FALSE;
  bamf_view_remove_child (parent, child);

  g_assert (child_removed_event_fired);
  g_assert (g_strcmp0 (bamf_view_get_path (child), child_removed_event_result) == 0);

  g_object_unref (child);
  g_object_unref (parent);
}

static gboolean closed_event_fired;

static void
on_closed (BamfView *view, gpointer pointer)
{
  closed_event_fired = TRUE;
}

static void
test_closed_event (void)
{
  BamfView *view;

  view = g_object_new (BAMF_TYPE_VIEW, NULL);
  bamf_view_export_on_bus (view, gdbus_connection);

  g_signal_connect (G_OBJECT (view), "closed",
                    (GCallback) on_closed, NULL);

  closed_event_fired = FALSE;

  bamf_view_close (view);
  g_assert (closed_event_fired);

  g_object_unref (view);
}

static void
test_parent_child_out_of_order_unref (void)
{
  BamfView *parent, *child;

  parent = g_object_new (BAMF_TYPE_VIEW, NULL);
  child = g_object_new (BAMF_TYPE_VIEW, NULL);

  bamf_view_export_on_bus (parent, gdbus_connection);
  bamf_view_export_on_bus (child, gdbus_connection);

  bamf_view_add_child (parent, child);

  g_object_unref (parent);
  g_object_unref (child);
}

/* Test Suite */
void
test_view_create_suite (GDBusConnection *connection)
{
#define DOMAIN "/View"

  gdbus_connection = connection;

  g_test_add_func (DOMAIN"/Allocation", test_allocation);
  g_test_add_func (DOMAIN"/Name", test_string_property (name));
  g_test_add_func (DOMAIN"/Name/Exported", test_string_property_exported (name));
  g_test_add_func (DOMAIN"/Icon", test_string_property (icon));
  g_test_add_func (DOMAIN"/Icon/Exported", test_string_property_exported (icon));
  g_test_add_func (DOMAIN"/Active", test_boolean_property (active));
  g_test_add_func (DOMAIN"/Active/Exported", test_boolean_property_exported (active));
  g_test_add_func (DOMAIN"/Running", test_boolean_property (running));
  g_test_add_func (DOMAIN"/Running/Exported", test_boolean_property_exported (running));
  g_test_add_func (DOMAIN"/Urgent", test_boolean_property (urgent));
  g_test_add_func (DOMAIN"/Urgent/Exported", test_boolean_property_exported (urgent));
  g_test_add_func (DOMAIN"/UserVisible", test_boolean_property (user_visible));
  g_test_add_func (DOMAIN"/UserVisible/Exported", test_boolean_property_exported (user_visible));
  g_test_add_func (DOMAIN"/Path", test_path);
  g_test_add_func (DOMAIN"/Path/Collision", test_path_collision);
  g_test_add_func (DOMAIN"/Events/Close", test_closed_event);
  g_test_add_func (DOMAIN"/Events/Active", test_boolean_property_event (active));
  g_test_add_func (DOMAIN"/Events/Name", test_name_event);
  g_test_add_func (DOMAIN"/Events/Name/Exported", test_name_event_exported);
  g_test_add_func (DOMAIN"/Events/Icon", test_icon_event);
  g_test_add_func (DOMAIN"/Events/Icon/Exported", test_icon_event_exported);
  g_test_add_func (DOMAIN"/Events/Active/Count", test_active_event_count);
  g_test_add_func (DOMAIN"/Events/Active/Exported", test_boolean_property_event_exported (active));
  g_test_add_func (DOMAIN"/Events/Running", test_boolean_property_event (running));
  g_test_add_func (DOMAIN"/Events/Running/Exported", test_boolean_property_event_exported (running));
  g_test_add_func (DOMAIN"/Events/Urgent", test_boolean_property_event (urgent));
  g_test_add_func (DOMAIN"/Events/Urgent/Exported", test_boolean_property_event_exported (urgent));
  g_test_add_func (DOMAIN"/Events/UserVisible", test_boolean_property_event (user_visible));
  g_test_add_func (DOMAIN"/Events/UserVisible/Exported", test_boolean_property_event_exported (user_visible));
  g_test_add_func (DOMAIN"/Events/ChildAdded", test_child_added_event);
  g_test_add_func (DOMAIN"/Events/ChildRemoved", test_child_removed_event);
  g_test_add_func (DOMAIN"/Children", test_children);
  g_test_add_func (DOMAIN"/Children/Paths", test_children_paths);
  g_test_add_func (DOMAIN"/Children/UnrefOrder", test_parent_child_out_of_order_unref);
}
