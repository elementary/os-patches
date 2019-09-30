/*
 * Copyright (C) 2010 Canonical Ltd
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
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *
 */

#include "bamf-legacy-window-test.h"
#include "bamf-legacy-screen-private.h"

G_DEFINE_TYPE (BamfLegacyWindowTest, bamf_legacy_window_test, BAMF_TYPE_LEGACY_WINDOW);

guint
bamf_legacy_window_test_get_pid (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return  self->pid;
}

guint32
bamf_legacy_window_test_get_xid (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->xid;
}

void
bamf_legacy_window_test_set_attention (BamfLegacyWindowTest *self, gboolean val)
{
  if (self->needs_attention == val)
    return;

  self->needs_attention = val;

  g_signal_emit_by_name (self, "state-changed");
}

gboolean
bamf_legacy_window_test_needs_attention (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->needs_attention;
}

void
bamf_legacy_window_test_set_active (BamfLegacyWindowTest *self, gboolean val)
{
  if (self->is_active == val)
    return;

  self->is_active = val;

  g_signal_emit_by_name (self, "state-changed");
}

gboolean
bamf_legacy_window_test_is_active (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->is_active;
}

void
bamf_legacy_window_test_set_desktop (BamfLegacyWindowTest *self, gboolean val)
{
  if (self->is_desktop == val)
    return;

  self->is_desktop = val;

  g_signal_emit_by_name (self, "state-changed");
}

gboolean
bamf_legacy_window_test_is_desktop (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->is_desktop;
}

void
bamf_legacy_window_test_set_skip (BamfLegacyWindowTest *self, gboolean val)
{
  if (self->is_skip == val)
    return;

  self->is_skip = val;

  g_signal_emit_by_name (self, "state-changed");
}

gboolean
bamf_legacy_window_test_is_skip_tasklist (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->is_skip;
}

void
bamf_legacy_window_test_set_name (BamfLegacyWindowTest *self, const char *val)
{
  if (g_strcmp0 (self->name, val) == 0)
    return;

  g_free (self->name);
  self->name = g_strdup (val);

  g_signal_emit_by_name (self, "name-changed");
}

void
bamf_legacy_window_test_set_icon (BamfLegacyWindowTest *self, const char *val)
{
  if (g_strcmp0 (self->icon, val) == 0)
    return;

  g_free (self->icon);
  self->icon = g_strdup (val);
}

void
bamf_legacy_window_test_set_role (BamfLegacyWindowTest *self, const char *val)
{
  if (g_strcmp0 (self->role, val) == 0)
    return;

  g_free (self->role);
  self->role = g_strdup (val);

  g_signal_emit_by_name (self, "role-changed");
}

void
bamf_legacy_window_test_set_wmclass (BamfLegacyWindowTest *self, const char *class_name, const char *instance_name)
{
  gboolean changed = FALSE;

  if (g_strcmp0 (self->wm_class_name, class_name) != 0)
    {
      g_free (self->wm_class_name);
      self->wm_class_name = g_strdup (class_name);
      changed = TRUE;
    }

  if (g_strcmp0 (self->wm_class_instance, instance_name) != 0)
    {
      g_free (self->wm_class_instance);
      self->wm_class_instance = g_strdup (instance_name);
      changed = TRUE;
    }

  if (changed)
    g_signal_emit_by_name (self, "class-changed");
}

static const char *
bamf_legacy_window_test_get_name (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->name;
}

static const char *
bamf_legacy_window_test_get_role (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->role;
}

static BamfLegacyWindow *
bamf_legacy_window_test_get_transient (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->transient_window;
}

static const char *
bamf_legacy_window_test_get_class_name (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->wm_class_name;
}

static const char *
bamf_legacy_window_test_get_class_instance_name (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->wm_class_instance;
}

const char *
bamf_legacy_window_test_get_exec_string (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->exec;
}

const char *
bamf_legacy_window_test_get_working_dir (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->working_dir;
}

char *
bamf_legacy_window_test_get_process_name (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return g_strdup (self->process_name);
}

char *
bamf_legacy_window_test_get_app_id (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (legacy_window), NULL);

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return g_strdup (self->application_id);
}

char *
bamf_legacy_window_test_get_unique_bus_name (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (legacy_window), NULL);

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return g_strdup (self->unique_bus_name);
}

char *
bamf_legacy_window_test_get_menu_object_path (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (legacy_window), NULL);

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return g_strdup (self->dbus_menu_object_path);
}

void
bamf_legacy_window_test_get_geometry (BamfLegacyWindow *legacy_window,
                                      gint *x, gint *y,
                                      gint *width, gint *height)
{
  BamfLegacyWindowTest *self;
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (legacy_window));

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  *x = self->geometry.x;
  *y = self->geometry.y;
  *width = self->geometry.width;
  *height = self->geometry.height;
}

BamfWindowMaximizationType
bamf_legacy_window_test_maximized (BamfLegacyWindow *legacy_window)
{
  BamfLegacyWindowTest *self;
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (legacy_window), BAMF_WINDOW_FLOATING);

  self = BAMF_LEGACY_WINDOW_TEST (legacy_window);

  return self->maximized;
}

void
bamf_legacy_window_test_close (BamfLegacyWindowTest *self)
{
  self->is_closed = TRUE;
  g_signal_emit_by_name (self, "closed");
}

void
bamf_legacy_window_test_set_geometry (BamfLegacyWindowTest *self, int x, int y,
                                 int width, int height)
{
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (self));

  self->geometry.x = x;
  self->geometry.y = y;
  self->geometry.width = width;
  self->geometry.height = height;
  g_signal_emit_by_name (self, BAMF_LEGACY_WINDOW_SIGNAL_GEOMETRY_CHANGED);
}

void
bamf_legacy_window_test_set_maximized (BamfLegacyWindowTest *self,
                                  BamfWindowMaximizationType maximized)
{
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (self));

  self->maximized = maximized;
  g_signal_emit_by_name (self, BAMF_LEGACY_WINDOW_SIGNAL_GEOMETRY_CHANGED);
  g_signal_emit_by_name (self, BAMF_LEGACY_WINDOW_SIGNAL_STATE_CHANGED);
}

void
bamf_legacy_window_test_set_application_id (BamfLegacyWindowTest *self, const char *id)
{
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (self));

  self->application_id = g_strdup (id);
}

void
bamf_legacy_window_test_set_unique_bus_name (BamfLegacyWindowTest *self, const char *bus_name)
{
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (self));

  self->unique_bus_name = g_strdup (bus_name);
}

void
bamf_legacy_window_test_set_dbus_menu_object_path (BamfLegacyWindowTest *self, const char *object_path)
{
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (self));

  self->dbus_menu_object_path = g_strdup (object_path);
}

gboolean
bamf_legacy_window_test_is_closed (BamfLegacyWindow *window)
{
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (window), TRUE);

  BamfLegacyWindowTest *self = BAMF_LEGACY_WINDOW_TEST (window);
  return self->is_closed;
}

static void
handle_destroy_notify (BamfLegacyWindowTest *copy, BamfLegacyWindowTest *self_was_here)
{
  BamfLegacyScreen *screen = bamf_legacy_screen_get_default ();
  _bamf_legacy_screen_open_test_window (screen, copy);
}

void
bamf_legacy_window_test_reopen (BamfLegacyWindow *window)
{
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (window));

  BamfLegacyWindowTest *self = BAMF_LEGACY_WINDOW_TEST (window);
  BamfLegacyWindowTest *copy = bamf_legacy_window_copy (self);
  g_object_weak_ref (G_OBJECT (self), (GWeakNotify) handle_destroy_notify, copy);
  bamf_legacy_window_test_close (self);
}

BamfWindowType
bamf_legacy_window_test_get_window_type (BamfLegacyWindow *window)
{
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (window), TRUE);

  BamfLegacyWindowTest *self = BAMF_LEGACY_WINDOW_TEST (window);
  return self->window_type;
}

char *
bamf_legacy_window_test_get_hint (BamfLegacyWindow *window, const char *name)
{
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (window), NULL);
  BamfLegacyWindowTest *self = BAMF_LEGACY_WINDOW_TEST (window);

  return g_strdup (g_hash_table_lookup (self->hints, name));
}

void
bamf_legacy_window_test_set_hint (BamfLegacyWindow *window, const char *name, const char *value)
{
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW_TEST (window));
  BamfLegacyWindowTest *self = BAMF_LEGACY_WINDOW_TEST (window);

  g_hash_table_insert (self->hints, g_strdup (name), g_strdup (value));
}

static char *
bamf_legacy_window_test_save_mini_icon (BamfLegacyWindow *window)
{
  return g_strdup (BAMF_LEGACY_WINDOW_TEST (window)->icon);
}

void
bamf_legacy_window_test_show_action_menu (BamfLegacyWindow *window, guint32 time, guint button, gint x, gint y)
{}

static void
bamf_legacy_window_test_finalize (GObject *object)
{
  BamfLegacyWindowTest *self = BAMF_LEGACY_WINDOW_TEST (object);

  g_free (self->name);
  g_free (self->icon);
  g_free (self->role);
  g_free (self->wm_class_name);
  g_free (self->wm_class_instance);
  g_free (self->exec);
  g_free (self->working_dir);
  g_free (self->process_name);
  g_free (self->application_id);
  g_free (self->unique_bus_name);
  g_free (self->dbus_menu_object_path);
  g_hash_table_unref (self->hints);

  G_OBJECT_CLASS (bamf_legacy_window_test_parent_class)->finalize (object);
}

void
bamf_legacy_window_test_class_init (BamfLegacyWindowTestClass *klass)
{
  BamfLegacyWindowClass *win_class = BAMF_LEGACY_WINDOW_CLASS (klass);
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->finalize         = bamf_legacy_window_test_finalize;
  win_class->get_transient    = bamf_legacy_window_test_get_transient;
  win_class->get_name         = bamf_legacy_window_test_get_name;
  win_class->save_mini_icon   = bamf_legacy_window_test_save_mini_icon;
  win_class->get_role         = bamf_legacy_window_test_get_role;
  win_class->get_class_name   = bamf_legacy_window_test_get_class_name;
  win_class->get_class_instance_name = bamf_legacy_window_test_get_class_instance_name;
  win_class->get_exec_string  = bamf_legacy_window_test_get_exec_string;
  win_class->get_working_dir  = bamf_legacy_window_test_get_working_dir;
  win_class->get_process_name = bamf_legacy_window_test_get_process_name;
  win_class->get_xid          = bamf_legacy_window_test_get_xid;
  win_class->get_pid          = bamf_legacy_window_test_get_pid;
  win_class->needs_attention  = bamf_legacy_window_test_needs_attention;
  win_class->is_skip_tasklist = bamf_legacy_window_test_is_skip_tasklist;
  win_class->is_desktop       = bamf_legacy_window_test_is_desktop;
  win_class->is_active        = bamf_legacy_window_test_is_active;
  win_class->get_app_id       = bamf_legacy_window_test_get_app_id;
  win_class->get_unique_bus_name = bamf_legacy_window_test_get_unique_bus_name;
  win_class->get_menu_object_path = bamf_legacy_window_test_get_menu_object_path;
  win_class->get_geometry     = bamf_legacy_window_test_get_geometry;
  win_class->get_window_type  = bamf_legacy_window_test_get_window_type;
  win_class->maximized        = bamf_legacy_window_test_maximized;
  win_class->is_closed        = bamf_legacy_window_test_is_closed;
  win_class->get_hint         = bamf_legacy_window_test_get_hint;
  win_class->set_hint         = bamf_legacy_window_test_set_hint;
  win_class->show_action_menu = bamf_legacy_window_test_show_action_menu;
  win_class->reopen           = bamf_legacy_window_test_reopen;
}


void
bamf_legacy_window_test_init (BamfLegacyWindowTest *self)
{
  self->pid = g_random_int_range (1, 100000);
  self->maximized = BAMF_WINDOW_FLOATING;
  self->is_closed = FALSE;
  self->hints = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}


BamfLegacyWindowTest *
bamf_legacy_window_copy (BamfLegacyWindowTest *self)
{
  BamfLegacyWindowTest *copy;

  copy = g_object_new (BAMF_TYPE_LEGACY_WINDOW_TEST, NULL);
  copy->xid = self->xid;
  copy->pid = self->pid;
  copy->name = g_strdup (self->name);
  copy->icon = g_strdup (self->icon);
  copy->role = g_strdup (self->role);
  copy->wm_class_name = g_strdup (self->wm_class_name);
  copy->wm_class_instance = g_strdup (self->wm_class_instance);
  copy->exec = g_strdup (self->exec);
  copy->working_dir = g_strdup (self->working_dir);
  copy->process_name = g_strdup (self->process_name);
  copy->application_id = g_strdup (self->application_id);
  copy->unique_bus_name = g_strdup (self->unique_bus_name);
  copy->dbus_menu_object_path = g_strdup (self->dbus_menu_object_path);
  copy->transient_window = self->transient_window;
  copy->needs_attention = self->needs_attention;
  copy->is_desktop = self->is_desktop;
  copy->is_skip = self->is_skip;
  copy->is_active = self->is_active;
  copy->is_closed = self->is_closed;
  copy->geometry = self->geometry;
  copy->maximized = self->maximized;
  copy->window_type = self->window_type;
  copy->hints = g_hash_table_ref (self->hints);

  return copy;
}

BamfLegacyWindowTest *
bamf_legacy_window_test_new (guint32 xid, const gchar *name, const gchar *wmclass_name, const gchar *exec)
{
  BamfLegacyWindowTest *self;

  self = g_object_new (BAMF_TYPE_LEGACY_WINDOW_TEST, NULL);
  self->window_type = BAMF_WINDOW_NORMAL;
  self->xid = xid;
  self->name = g_strdup (name);
  self->wm_class_name = g_strdup (wmclass_name);
  self->exec = g_strdup (exec);
  self->working_dir = g_get_current_dir ();

  if (self->exec)
    {
      gchar **splitted_exec = g_strsplit (exec, " ", 2);
      gchar *tmp = g_utf8_strrchr (splitted_exec[0], -1, G_DIR_SEPARATOR);
      self->process_name = g_strdup (tmp ? tmp + 1 : splitted_exec[0]);
      g_strfreev (splitted_exec);
    }

  return self;
}
