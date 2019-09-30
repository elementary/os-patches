/*
 * Copyright 2010-2011 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of either or both of the following licenses:
 *
 * 1) the GNU Lesser General Public License version 3, as published by the
 * Free Software Foundation; and/or
 * 2) the GNU Lesser General Public License version 2.1, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the applicable version of the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License version 3 and version 2.1 along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 *
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Neil Jagdish Patel <neil.patel@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#ifndef _BAMF_WINDOW_H_
#define _BAMF_WINDOW_H_

#include <time.h>
#include <glib-object.h>
#include <libbamf/bamf-view.h>

G_BEGIN_DECLS

#define BAMF_TYPE_WINDOW (bamf_window_get_type ())

#define BAMF_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
        BAMF_TYPE_WINDOW, BamfWindow))

#define BAMF_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),\
        BAMF_TYPE_WINDOW, BamfWindowClass))

#define BAMF_IS_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
        BAMF_TYPE_WINDOW))

#define BAMF_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
        BAMF_TYPE_WINDOW))

#define BAMF_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
        BAMF_TYPE_WINDOW, BamfWindowClass))

#define BAMF_WINDOW_SIGNAL_MONITOR_CHANGED   "monitor-changed"
#define BAMF_WINDOW_SIGNAL_MAXIMIZED_CHANGED "maximized-changed"

typedef struct _BamfWindow        BamfWindow;
typedef struct _BamfWindowClass   BamfWindowClass;
typedef struct _BamfWindowPrivate BamfWindowPrivate;

struct _BamfWindow
{
  BamfView parent;

  BamfWindowPrivate *priv;
};

typedef enum
{
  BAMF_WINDOW_NORMAL,       /* document/app window */
  BAMF_WINDOW_DESKTOP,      /* desktop background */
  BAMF_WINDOW_DOCK,         /* panel */
  BAMF_WINDOW_DIALOG,       /* dialog */
  BAMF_WINDOW_TOOLBAR,      /* tearoff toolbar */
  BAMF_WINDOW_MENU,         /* tearoff menu */
  BAMF_WINDOW_UTILITY,      /* palette/toolbox window */
  BAMF_WINDOW_SPLASHSCREEN, /* splash screen */
  BAMF_WINDOW_UNKNOWN
} BamfWindowType;

typedef enum
{
  BAMF_WINDOW_FLOATING,              /* Floating window */
  BAMF_WINDOW_HORIZONTAL_MAXIMIZED,  /* Horizontally maximized window */
  BAMF_WINDOW_VERTICAL_MAXIMIZED,    /* Vertically maximized window */
  BAMF_WINDOW_MAXIMIZED              /* Maximized window */
} BamfWindowMaximizationType;

struct _BamfWindowClass
{
  BamfViewClass parent_class;

  BamfWindow               * (*get_transient)      (BamfWindow *self);
  BamfWindowType             (*get_window_type)    (BamfWindow *self);
  guint32                    (*get_xid)            (BamfWindow *self);
  guint32                    (*get_pid)            (BamfWindow *self);
  gint                       (*get_monitor)        (BamfWindow *self);
  gchar                    * (*get_utf8_prop)      (BamfWindow *self, const char* prop);
  BamfWindowMaximizationType (*maximized)          (BamfWindow *self);
  time_t                     (*last_active)        (BamfWindow *self);

  /*< signals >*/
  void (*monitor_changed)   (BamfWindow *window, gint old_value, gint new_value);
  void (*maximized_changed) (BamfWindow *window, gint old_value, gint new_value);

  /*< private >*/
  void (*_window_padding1) (void);
  void (*_window_padding2) (void);
  void (*_window_padding3) (void);
  void (*_window_padding4) (void);
};

GType             bamf_window_get_type                  (void) G_GNUC_CONST;

BamfWindow      * bamf_window_get_transient             (BamfWindow *self);

BamfWindowType    bamf_window_get_window_type           (BamfWindow *self);

guint32           bamf_window_get_xid                   (BamfWindow *self);

guint32           bamf_window_get_pid                   (BamfWindow *self);

gint              bamf_window_get_monitor               (BamfWindow *self);

gchar           * bamf_window_get_utf8_prop             (BamfWindow *self, const char* prop);
BamfWindowMaximizationType bamf_window_maximized        (BamfWindow *self);

time_t            bamf_window_last_active               (BamfWindow *self);

G_END_DECLS

#endif
