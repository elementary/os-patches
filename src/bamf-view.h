/*
 * Copyright (C) 2010-2011 Canonical Ltd
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
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#ifndef __BAMFVIEW_H__
#define __BAMFVIEW_H__

#include <glib.h>
#include <glib-object.h>
#include <libbamf-private/bamf-private.h>

#define BAMF_TYPE_VIEW                  (bamf_view_get_type ())
#define BAMF_VIEW(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_VIEW, BamfView))
#define BAMF_IS_VIEW(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_VIEW))
#define BAMF_VIEW_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_VIEW, BamfViewClass))
#define BAMF_IS_VIEW_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_VIEW))
#define BAMF_VIEW_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_VIEW, BamfViewClass))

typedef struct _BamfView BamfView;
typedef struct _BamfViewClass BamfViewClass;
typedef struct _BamfViewPrivate BamfViewPrivate;

struct _BamfViewClass
{
  BamfDBusItemObjectSkeletonClass parent;
  GList *names;

  /*< methods >*/
  const char * (*view_type)         (BamfView *view);
  char *       (*stable_bus_name)   (BamfView *view);

  /*< random stuff >*/
  gboolean (* urgent_changed)       (BamfView *view, gboolean urgent);
  gboolean (* running_changed)      (BamfView *view, gboolean running);
  gboolean (* active_changed)       (BamfView *view, gboolean active);
  gboolean (* user_visible_changed) (BamfView *view, gboolean visible);
  gboolean (* closed)               (BamfView *view);
  void     (* child_added)          (BamfView *view, BamfView *child);
  void     (* child_removed)        (BamfView *view, BamfView *child);

  /*< signals >*/
  void (* closed_internal)          (BamfView *view);
  void (* child_added_internal)     (BamfView *view, BamfView *child);
  void (* child_removed_internal)   (BamfView *view, BamfView *child);
  void (* exported)                 (BamfView *view);
};

struct _BamfView
{
  BamfDBusItemObjectSkeleton parent;

  /* private */
  BamfViewPrivate *priv;
};

GType         bamf_view_get_type           (void) G_GNUC_CONST;

void          bamf_view_close              (BamfView *view);

GVariant    * bamf_view_get_children_paths (BamfView *view);

GList       * bamf_view_get_children       (BamfView *view);

GVariant    * bamf_view_get_parent_paths   (BamfView *view);

GList       * bamf_view_get_parents        (BamfView *view);

const char  * bamf_view_get_path           (BamfView *view);

void          bamf_view_add_child          (BamfView *view, BamfView *child);
void          bamf_view_remove_child       (BamfView *view, BamfView *child);

gboolean      bamf_view_is_active          (BamfView *view);
void          bamf_view_set_active         (BamfView *view, gboolean active);

gboolean      bamf_view_is_running         (BamfView *view);
void          bamf_view_set_running        (BamfView *view, gboolean running);

gboolean      bamf_view_is_user_visible    (BamfView *view);
void          bamf_view_set_user_visible   (BamfView *view, gboolean user_visible);

gboolean      bamf_view_is_urgent          (BamfView *view);
void          bamf_view_set_urgent         (BamfView *view, gboolean urgent);

void          bamf_view_set_icon           (BamfView *view, const char *icon);
const char  * bamf_view_get_icon           (BamfView *view);

const char  * bamf_view_get_name           (BamfView *view);
void          bamf_view_set_name           (BamfView *view, const char *name);

const char  * bamf_view_get_parent_path    (BamfView *view);

BamfView    * bamf_view_get_parent         (BamfView *view);
void          bamf_view_set_parent         (BamfView *view, BamfView *parent);

const char  * bamf_view_get_view_type      (BamfView *view);

gboolean      bamf_view_is_on_bus          (BamfView *view);
const char  * bamf_view_export_on_bus      (BamfView *view,
                                            GDBusConnection *connection);

#endif
