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

#ifndef __BAMFLEGACY_SCREEN_H__
#define __BAMFLEGACY_SCREEN_H__

#include "bamf-view.h"
#include "bamf-legacy-window.h"
#include <glib.h>
#include <glib-object.h>

#define BAMF_TYPE_LEGACY_SCREEN                 (bamf_legacy_screen_get_type ())
#define BAMF_LEGACY_SCREEN(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_LEGACY_SCREEN, BamfLegacyScreen))
#define BAMF_IS_LEGACY_SCREEN(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_LEGACY_SCREEN))
#define BAMF_LEGACY_SCREEN_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_LEGACY_SCREEN, BamfLegacyScreenClass))
#define BAMF_IS_LEGACY_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_LEGACY_SCREEN))
#define BAMF_LEGACY_SCREEN_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_LEGACY_SCREEN, BamfLegacyScreenClass))

#define BAMF_LEGACY_SCREEN_SIGNAL_WINDOW_OPENED         "window-opened"
#define BAMF_LEGACY_SCREEN_SIGNAL_WINDOW_CLOSED         "window-closed"
#define BAMF_LEGACY_SCREEN_SIGNAL_STACKING_CHANGED      "stacking-changed"
#define BAMF_LEGACY_SCREEN_SIGNAL_ACTIVE_WINDOW_CHANGED "active-window-changed"

typedef struct _BamfLegacyScreen BamfLegacyScreen;
typedef struct _BamfLegacyScreenClass BamfLegacyScreenClass;
typedef struct _BamfLegacyScreenPrivate BamfLegacyScreenPrivate;

struct _BamfLegacyScreenClass
{
  GObjectClass parent;

  GList    (*get_windows)      (BamfLegacyScreen *legacy_screen);

  /*< signals >*/
  void     (*window_opened)          (BamfLegacyScreen *legacy_screen, BamfLegacyWindow *legacy_window);
  void     (*window_closed)          (BamfLegacyScreen *legacy_screen, BamfLegacyWindow *legacy_window);
  void     (*stacking_changed)       (BamfLegacyScreen *legacy_screen);
  void     (*active_window_changed)  (BamfLegacyScreen *legacy_screen);
};

struct _BamfLegacyScreen
{
  GObject parent;

  /* private */
  BamfLegacyScreenPrivate *priv;
};

GType              bamf_legacy_screen_get_type           (void) G_GNUC_CONST;

void               bamf_legacy_screen_set_state_file     (BamfLegacyScreen *screen, const char *file);

GList            * bamf_legacy_screen_get_windows        (BamfLegacyScreen *screen);

BamfLegacyWindow * bamf_legacy_screen_get_active_window  (BamfLegacyScreen *screen);

BamfLegacyScreen * bamf_legacy_screen_get_default        (void);

void               bamf_legacy_screen_inject_window      (BamfLegacyScreen *screen, guint xid);

#endif
