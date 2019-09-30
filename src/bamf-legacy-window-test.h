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


#ifndef __BAMFMOCKWINDOW_H__
#define __BAMFMOCKWINDOW_H__

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include "bamf-application.h"
#include "bamf-window.h"
#include "bamf-legacy-window.h"

#define BAMF_TYPE_LEGACY_WINDOW_TEST (bamf_legacy_window_test_get_type ())

#define BAMF_LEGACY_WINDOW_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
        BAMF_TYPE_LEGACY_WINDOW_TEST, BamfLegacyWindowTest))

#define BAMF_LEGACY_WINDOW_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),\
        BAMF_TYPE_LEGACY_WINDOW_TEST, BamfLegacyWindowTestClass))

#define BAMF_IS_LEGACY_WINDOW_TEST(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
        BAMF_TYPE_LEGACY_WINDOW_TEST))

#define BAMF_IS_LEGACY_WINDOW_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
        BAMF_TYPE_LEGACY_WINDOW_TEST))

#define BAMF_LEGACY_WINDOW_TEST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
        BAMF_TYPE_LEGACY_WINDOW_TEST, BamfLegacyWindowTestClass))

typedef struct _BamfLegacyWindowTest        BamfLegacyWindowTest;
typedef struct _BamfLegacyWindowTestClass   BamfLegacyWindowTestClass;

struct _BamfLegacyWindowTest
{
  BamfLegacyWindow parent;
  guint32 xid;
  guint   pid;
  char * name;
  char * icon;
  char * role;
  char * wm_class_name;
  char * wm_class_instance;
  char * exec;
  char * working_dir;
  char * process_name;
  char * application_id;
  char * unique_bus_name;
  char * dbus_menu_object_path;
  BamfLegacyWindow * transient_window;
  gboolean needs_attention;
  gboolean is_desktop;
  gboolean is_skip;
  gboolean is_active;
  gboolean is_closed;
  GdkRectangle geometry;
  BamfWindowMaximizationType maximized;
  BamfWindowType window_type;
  GHashTable * hints;
};

struct _BamfLegacyWindowTestClass
{
  /*< private >*/
  BamfLegacyWindowClass parent_class;

  void (*_test_padding1) (void);
  void (*_test_padding2) (void);
  void (*_test_padding3) (void);
  void (*_test_padding4) (void);
  void (*_test_padding5) (void);
  void (*_test_padding6) (void);
};

GType       bamf_legacy_window_test_get_type (void) G_GNUC_CONST;

guint
bamf_legacy_window_test_get_pid (BamfLegacyWindow *legacy_window);

guint32
bamf_legacy_window_test_get_xid (BamfLegacyWindow *legacy_window);

void
bamf_legacy_window_test_set_attention (BamfLegacyWindowTest *self, gboolean val);

gboolean
bamf_legacy_window_test_needs_attention (BamfLegacyWindow *legacy_window);

void
bamf_legacy_window_test_set_active (BamfLegacyWindowTest *self, gboolean val);

gboolean
bamf_legacy_window_test_is_active (BamfLegacyWindow *legacy_window);

void
bamf_legacy_window_test_set_desktop (BamfLegacyWindowTest *self, gboolean val);

gboolean
bamf_legacy_window_test_is_desktop (BamfLegacyWindow *legacy_window);

void
bamf_legacy_window_test_set_skip (BamfLegacyWindowTest *self, gboolean val);

gboolean
bamf_legacy_window_test_is_skip_tasklist (BamfLegacyWindow *legacy_window);

void
bamf_legacy_window_test_set_name (BamfLegacyWindowTest *self, const char *val);

void
bamf_legacy_window_test_set_icon (BamfLegacyWindowTest *self, const char *val);

void
bamf_legacy_window_test_set_role (BamfLegacyWindowTest *self, const char *val);

void
bamf_legacy_window_test_set_geometry (BamfLegacyWindowTest *self, int x, int y,
                                                             int width, int height);

void
bamf_legacy_window_test_set_maximized (BamfLegacyWindowTest *self,
                                       BamfWindowMaximizationType maximized);

void
bamf_legacy_window_test_set_application_id (BamfLegacyWindowTest *self, const char *id);

void
bamf_legacy_window_test_set_unique_bus_name (BamfLegacyWindowTest *self, const char *bus_name);

void
bamf_legacy_window_test_set_dbus_menu_object_path (BamfLegacyWindowTest *self, const char *object_path);

void
bamf_legacy_window_test_set_wmclass (BamfLegacyWindowTest *self, const char *class_name, const char *instance_name);

void
bamf_legacy_window_test_close (BamfLegacyWindowTest *self);

BamfLegacyWindowTest *
bamf_legacy_window_test_new (guint32 xid, const gchar *name, const gchar *wmclass_name, const gchar *exec);

BamfLegacyWindowTest *
bamf_legacy_window_copy (BamfLegacyWindowTest *self);

#endif
