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

#ifndef __BAMFWINDOW_H__
#define __BAMFWINDOW_H__

#include "bamf-view.h"
#include "bamf-legacy-window.h"
#include <glib.h>
#include <glib-object.h>
#include <time.h>

#define BAMF_TYPE_WINDOW                        (bamf_window_get_type ())
#define BAMF_WINDOW(obj)                        (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_WINDOW, BamfWindow))
#define BAMF_IS_WINDOW(obj)                     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_WINDOW))
#define BAMF_WINDOW_CLASS(klass)                (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_WINDOW, BamfWindowClass))
#define BAMF_IS_WINDOW_CLASS(klass)             (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_WINDOW))
#define BAMF_WINDOW_GET_CLASS(obj)              (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_WINDOW, BamfWindowClass))

typedef struct _BamfWindow BamfWindow;
typedef struct _BamfWindowClass BamfWindowClass;
typedef struct _BamfWindowPrivate BamfWindowPrivate;

struct _BamfWindowClass
{
  BamfViewClass parent;

  gboolean           (*user_visible) (BamfWindow *window);
  gboolean           (*is_urgent)    (BamfWindow *window);
  guint32            (*get_xid)      (BamfWindow *window);
  BamfLegacyWindow * (*get_window)   (BamfWindow *window);
};

struct _BamfWindow
{
  BamfView parent;

  /* private */
  BamfWindowPrivate *priv;
};

GType bamf_window_get_type (void) G_GNUC_CONST;

BamfLegacyWindow * bamf_window_get_window (BamfWindow *self);

BamfWindow       * bamf_window_get_transient (BamfWindow *self);

const char       * bamf_window_get_transient_path (BamfWindow *self);

guint32            bamf_window_get_window_type (BamfWindow *self);

guint32            bamf_window_get_xid (BamfWindow *window);

guint32            bamf_window_get_pid (BamfWindow *window);

time_t             bamf_window_opened (BamfWindow *window);

gint               bamf_window_get_stack_position (BamfWindow *window);

char             * bamf_window_get_string_hint (BamfWindow *self, const char* prop);

BamfWindowMaximizationType bamf_window_maximized (BamfWindow *self);

gint               bamf_window_get_monitor (BamfWindow *self);

BamfWindow       * bamf_window_new (BamfLegacyWindow *window);

#endif
