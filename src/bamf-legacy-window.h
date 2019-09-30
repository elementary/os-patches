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


#ifndef __BAMFLEGACY_WINDOW_H__
#define __BAMFLEGACY_WINDOW_H__

#include "config.h"

#include "bamf-view.h"
#include <sys/types.h>
#include <glib.h>
#include <glib-object.h>
#include <libwnck/libwnck.h>

#define BAMF_TYPE_LEGACY_WINDOW                 (bamf_legacy_window_get_type ())
#define BAMF_LEGACY_WINDOW(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_LEGACY_WINDOW, BamfLegacyWindow))
#define BAMF_IS_LEGACY_WINDOW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_LEGACY_WINDOW))
#define BAMF_LEGACY_WINDOW_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_LEGACY_WINDOW, BamfLegacyWindowClass))
#define BAMF_IS_LEGACY_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_LEGACY_WINDOW))
#define BAMF_LEGACY_WINDOW_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_LEGACY_WINDOW, BamfLegacyWindowClass))

#define BAMF_LEGACY_WINDOW_SIGNAL_NAME_CHANGED     "name-changed"
#define BAMF_LEGACY_WINDOW_SIGNAL_ROLE_CHANGED     "role-changed"
#define BAMF_LEGACY_WINDOW_SIGNAL_CLASS_CHANGED    "class-changed"
#define BAMF_LEGACY_WINDOW_SIGNAL_STATE_CHANGED    "state-changed"
#define BAMF_LEGACY_WINDOW_SIGNAL_GEOMETRY_CHANGED "geometry-changed"
#define BAMF_LEGACY_WINDOW_SIGNAL_CLOSED           "closed"

typedef struct _BamfLegacyWindow BamfLegacyWindow;
typedef struct _BamfLegacyWindowClass BamfLegacyWindowClass;
typedef struct _BamfLegacyWindowPrivate BamfLegacyWindowPrivate;

typedef enum
{
  BAMF_WINDOW_NORMAL,       /* document/app window */
  BAMF_WINDOW_DESKTOP,      /* desktop background */
  BAMF_WINDOW_DOCK,         /* panel */
  BAMF_WINDOW_DIALOG,       /* dialog */
  BAMF_WINDOW_TOOLBAR,      /* tearoff toolbar */
  BAMF_WINDOW_MENU,         /* tearoff menu */
  BAMF_WINDOW_UTILITY,      /* palette/toolbox window */
  BAMF_WINDOW_SPLASHSCREEN  /* splash screen */
} BamfWindowType;

typedef enum
{
  BAMF_WINDOW_FLOATING,
  BAMF_WINDOW_HORIZONTAL_MAXIMIZED,
  BAMF_WINDOW_VERTICAL_MAXIMIZED,
  BAMF_WINDOW_MAXIMIZED
} BamfWindowMaximizationType;

struct _BamfLegacyWindowClass
{
  GObjectClass parent;

  BamfLegacyWindow * (*get_transient)     (BamfLegacyWindow *legacy_window);
  const char * (*get_name)                (BamfLegacyWindow *legacy_window);
  const char * (*get_role)                (BamfLegacyWindow *legacy_window);
  const char * (*get_class_name)          (BamfLegacyWindow *legacy_window);
  const char * (*get_class_instance_name) (BamfLegacyWindow *legacy_window);
  const char * (*get_exec_string)         (BamfLegacyWindow *legacy_window);
  const char * (*get_working_dir)         (BamfLegacyWindow *legacy_window);
  char       * (*save_mini_icon)          (BamfLegacyWindow *legacy_window);
  char       * (*get_process_name)        (BamfLegacyWindow *legacy_window);
  char       * (*get_app_id)              (BamfLegacyWindow *legacy_window);
  char       * (*get_unique_bus_name)     (BamfLegacyWindow *legacy_window);
  char       * (*get_menu_object_path)    (BamfLegacyWindow *legacy_window);
  char       * (*get_hint)                (BamfLegacyWindow *legacy_window,
                                           const gchar *name);
  guint        (*get_pid)                 (BamfLegacyWindow *legacy_window);
  guint32      (*get_xid)                 (BamfLegacyWindow *legacy_window);
  gboolean     (*needs_attention)         (BamfLegacyWindow *legacy_window);
  gboolean     (*is_active)               (BamfLegacyWindow *legacy_window);
  gboolean     (*is_skip_tasklist)        (BamfLegacyWindow *legacy_window);
  gboolean     (*is_desktop)              (BamfLegacyWindow *legacy_window);
  gboolean     (*is_dialog)               (BamfLegacyWindow *legacy_window);
  gboolean     (*is_closed)               (BamfLegacyWindow *legacy_window);
  BamfWindowMaximizationType (*maximized) (BamfLegacyWindow *legacy_window);
  BamfWindowType (*get_window_type)       (BamfLegacyWindow *legacy_window);
  void         (*get_geometry)            (BamfLegacyWindow *legacy_window,
                                           gint *x, gint *y, gint *w, gint *h);
  void         (*set_hint)                (BamfLegacyWindow *legacy_window,
                                           const gchar *name, const gchar *value);
  void         (*show_action_menu)        (BamfLegacyWindow *legacy_window,
                                           guint32 time, guint button, gint x, gint y);
  void         (*reopen)                  (BamfLegacyWindow *legacy_window);

  /*< signals >*/
  void     (*name_changed)     (BamfLegacyWindow *legacy_window);
  void     (*class_changed)    (BamfLegacyWindow *legacy_window);
  void     (*role_changed)     (BamfLegacyWindow *legacy_window);
  void     (*state_changed)    (BamfLegacyWindow *legacy_window);
  void     (*geometry_changed) (BamfLegacyWindow *legacy_window);
  void     (*closed)           (BamfLegacyWindow *legacy_window);
};

struct _BamfLegacyWindow
{
  GObject parent;

  /* private */
  BamfLegacyWindowPrivate *priv;
};

GType              bamf_legacy_window_get_type             (void) G_GNUC_CONST;

guint32            bamf_legacy_window_get_xid              (BamfLegacyWindow *self);

guint              bamf_legacy_window_get_pid              (BamfLegacyWindow *self);

void               bamf_legacy_window_get_geometry         (BamfLegacyWindow *self,
                                                            gint *x, gint *y,
                                                            gint *width, gint *height);

gboolean           bamf_legacy_window_is_active            (BamfLegacyWindow *self);

gboolean           bamf_legacy_window_is_skip_tasklist     (BamfLegacyWindow *self);

gboolean           bamf_legacy_window_needs_attention      (BamfLegacyWindow *self);

gboolean           bamf_legacy_window_is_closed            (BamfLegacyWindow *self);

BamfWindowType     bamf_legacy_window_get_window_type      (BamfLegacyWindow *self);

BamfWindowMaximizationType bamf_legacy_window_maximized    (BamfLegacyWindow *self);

const char       * bamf_legacy_window_get_class_instance_name (BamfLegacyWindow *self);

const char       * bamf_legacy_window_get_class_name       (BamfLegacyWindow *self);

const char       * bamf_legacy_window_get_name             (BamfLegacyWindow *self);

const char       * bamf_legacy_window_get_role             (BamfLegacyWindow *self);

const char       * bamf_legacy_window_get_exec_string      (BamfLegacyWindow *self);

const char       * bamf_legacy_window_get_working_dir      (BamfLegacyWindow *self);

char             * bamf_legacy_window_save_mini_icon       (BamfLegacyWindow *self);

GFile            * bamf_legacy_window_get_saved_mini_icon  (BamfLegacyWindow *self);

char             * bamf_legacy_window_get_process_name     (BamfLegacyWindow *self);

BamfLegacyWindow * bamf_legacy_window_get_transient        (BamfLegacyWindow *self);

char             * bamf_legacy_window_get_hint             (BamfLegacyWindow *self,
                                                            const char *name);

void               bamf_legacy_window_set_hint             (BamfLegacyWindow *self,
                                                            const char *name,
                                                            const char *value);

gint               bamf_legacy_window_get_stacking_position (BamfLegacyWindow *self);

void               bamf_legacy_window_show_action_menu     (BamfLegacyWindow *self,
                                                            guint32 time, guint button,
                                                            gint x, gint y);

void               bamf_legacy_window_reopen               (BamfLegacyWindow *self);

BamfLegacyWindow * bamf_legacy_window_new                  (WnckWindow *legacy_window);

#endif

