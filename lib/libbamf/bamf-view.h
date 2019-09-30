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

#ifndef _BAMF_VIEW_H_
#define _BAMF_VIEW_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define BAMF_TYPE_VIEW (bamf_view_get_type ())

#define BAMF_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
        BAMF_TYPE_VIEW, BamfView))

#define BAMF_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),\
        BAMF_TYPE_VIEW, BamfViewClass))

#define BAMF_IS_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
        BAMF_TYPE_VIEW))

#define BAMF_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
        BAMF_TYPE_VIEW))

#define BAMF_VIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
        BAMF_TYPE_VIEW, BamfViewClass))

#define BAMF_VIEW_SIGNAL_ACTIVE_CHANGED       "active-changed"
#define BAMF_VIEW_SIGNAL_RUNNING_CHANGED      "running-changed"
#define BAMF_VIEW_SIGNAL_URGENT_CHANGED       "urgent-changed"
#define BAMF_VIEW_SIGNAL_USER_VISIBLE_CHANGED "user-visible-changed"
#define BAMF_VIEW_SIGNAL_NAME_CHANGED         "name-changed"
#define BAMF_VIEW_SIGNAL_ICON_CHANGED         "icon-changed"
#define BAMF_VIEW_SIGNAL_CHILD_ADDED          "child-added"
#define BAMF_VIEW_SIGNAL_CHILD_REMOVED        "child-removed"
#define BAMF_VIEW_SIGNAL_CHILD_MOVED          "child-moved"
#define BAMF_VIEW_SIGNAL_CLOSED               "closed"

typedef enum
{
  BAMF_CLICK_BEHAVIOR_NONE,
  BAMF_CLICK_BEHAVIOR_OPEN,
  BAMF_CLICK_BEHAVIOR_FOCUS,
  BAMF_CLICK_BEHAVIOR_FOCUS_ALL,
  BAMF_CLICK_BEHAVIOR_MINIMIZE,
  BAMF_CLICK_BEHAVIOR_RESTORE,
  BAMF_CLICK_BEHAVIOR_RESTORE_ALL,
  BAMF_CLICK_BEHAVIOR_PICKER,
} BamfClickBehavior;

typedef struct _BamfView        BamfView;
typedef struct _BamfViewClass   BamfViewClass;
typedef struct _BamfViewPrivate BamfViewPrivate;

struct _BamfView
{
  GInitiallyUnowned parent;

  BamfViewPrivate *priv;
};

struct _BamfViewClass
{
  GInitiallyUnownedClass parent_class;

  GList            * (*get_children)        (BamfView *view);
  gboolean           (*is_active)           (BamfView *view);
  gboolean           (*is_running)          (BamfView *view);
  gboolean           (*is_urgent)           (BamfView *view);
  gboolean           (*is_user_visible)     (BamfView *view);
  gchar            * (*get_name)            (BamfView *view);
  gchar            * (*get_icon)            (BamfView *view);
  const gchar      * (*view_type)           (BamfView *view);
  void               (*set_path)            (BamfView *view, const gchar *path);
  void               (*set_sticky)          (BamfView *view, gboolean value);
  BamfClickBehavior  (*click_behavior)      (BamfView *view);

  /*< signals >*/
  void (*active_changed)              (BamfView *view, gboolean active);
  void (*closed)                      (BamfView *view);
  void (*child_added)                 (BamfView *view, BamfView *child);
  void (*child_removed)               (BamfView *view, BamfView *child);
  void (*running_changed)             (BamfView *view, gboolean running);
  void (*urgent_changed)              (BamfView *view, gboolean urgent);
  void (*user_visible_changed)        (BamfView *view, gboolean user_visible);
  void (*name_changed)                (BamfView *view, gchar* old_name, gchar* new_name);
  void (*icon_changed)                (BamfView *view, gchar* icon);
  void (*child_moved)                 (BamfView *view, BamfView *child);

  /*< private >*/
  void (*_view_padding1) (void);
  void (*_view_padding2) (void);
  void (*_view_padding3) (void);
};

GType      bamf_view_get_type             (void) G_GNUC_CONST;

GList    * bamf_view_get_children  (BamfView *view);

gboolean   bamf_view_is_closed     (BamfView *view);

gboolean   bamf_view_is_active     (BamfView *view);

gboolean   bamf_view_is_running    (BamfView *view);

gboolean   bamf_view_is_urgent     (BamfView *view);

gboolean   bamf_view_is_user_visible  (BamfView *view);

gchar    * bamf_view_get_name      (BamfView *view);

gchar    * bamf_view_get_icon      (BamfView *view);

const gchar    * bamf_view_get_view_type (BamfView *view);

void bamf_view_set_sticky (BamfView *view, gboolean value);

gboolean bamf_view_is_sticky (BamfView *view);

/* Deprecated symbols */
G_GNUC_DEPRECATED
BamfClickBehavior bamf_view_get_click_suggestion (BamfView *view);

G_GNUC_DEPRECATED_FOR (bamf_view_user_visible)
gboolean bamf_view_user_visible (BamfView *view);

G_END_DECLS

#endif
