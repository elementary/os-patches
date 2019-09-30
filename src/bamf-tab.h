/*
 * Copyright (C) 2010-2012 Canonical Ltd
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
 * Authored by: Robert Carr <racarr@canonical.com>
 */

#ifndef __BAMFTAB_H__
#define __BAMFTAB_H__

#include "bamf-view.h"
#include <glib.h>
#include <glib-object.h>

#define BAMF_TYPE_TAB                   (bamf_tab_get_type ())
#define BAMF_TAB(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_TAB, BamfTab))
#define BAMF_IS_TAB(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_TAB))
#define BAMF_TAB_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_TAB, BamfTabClass))
#define BAMF_IS_TAB_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_TAB))
#define BAMF_TAB_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_TAB, BamfTabClass))

typedef struct _BamfTab BamfTab;
typedef struct _BamfTabClass BamfTabClass;
typedef struct _BamfTabPrivate BamfTabPrivate;

typedef void (*BamfTabPreviewReadyCallback) (BamfTab *, const gchar *, gpointer);

struct _BamfTabClass
{
  BamfViewClass parent;

  void (*raise) (BamfTab *self);
  void (*close) (BamfTab *self);
  void (*request_preview) (BamfTab *self, BamfTabPreviewReadyCallback callback, gpointer user_data);
};

struct _BamfTab
{
  BamfView parent;

  /* private */
  BamfTabPrivate *priv;
};

GType       bamf_tab_get_type    (void) G_GNUC_CONST;

const gchar *bamf_tab_get_desktop_id (BamfTab *self);
const gchar *bamf_tab_get_location (BamfTab *self);
guint64 bamf_tab_get_xid (BamfTab *self);
gboolean bamf_tab_get_is_foreground_tab (BamfTab *self);


void bamf_tab_raise (BamfTab *self);
void bamf_tab_close (BamfTab *self);

void bamf_tab_request_preview (BamfTab *self, BamfTabPreviewReadyCallback callback, gpointer user_data);



#endif
