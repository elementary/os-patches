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
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */


#ifndef __BAMFMATCHER_H__
#define __BAMFMATCHER_H__

#include "bamf-view.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <libgtop-2.0/glibtop.h>
#include <glibtop/procargs.h>
#include <glibtop/procuid.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#define BAMF_TYPE_MATCHER                       (bamf_matcher_get_type ())
#define BAMF_MATCHER(obj)                       (G_TYPE_CHECK_INSTANCE_CAST ((obj), BAMF_TYPE_MATCHER, BamfMatcher))
#define BAMF_IS_MATCHER(obj)                    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BAMF_TYPE_MATCHER))
#define BAMF_MATCHER_CLASS(klass)               (G_TYPE_CHECK_CLASS_CAST ((klass), BAMF_TYPE_MATCHER, BamfMatcherClass))
#define BAMF_IS_MATCHER_CLASS(klass)            (G_TYPE_CHECK_CLASS_TYPE ((klass), BAMF_TYPE_MATCHER))
#define BAMF_MATCHER_GET_CLASS(obj)             (G_TYPE_INSTANCE_GET_CLASS ((obj), BAMF_TYPE_MATCHER, BamfMatcherClass))

#define _NET_WM_DESKTOP_FILE "_NET_WM_DESKTOP_FILE"

typedef struct _BamfMatcher BamfMatcher;
typedef struct _BamfMatcherClass BamfMatcherClass;
typedef struct _BamfMatcherPrivate BamfMatcherPrivate;

struct _BamfMatcherClass
{
  BamfDBusMatcherSkeletonClass parent;
};

struct _BamfMatcher
{
  BamfDBusMatcherSkeleton parent;

  /* private */
  BamfMatcherPrivate *priv;
};

GType         bamf_matcher_get_type                      (void) G_GNUC_CONST;

void          bamf_matcher_load_desktop_file             (BamfMatcher * self,
                                                          const char * desktop_file);

void          bamf_matcher_register_desktop_file_for_pid (BamfMatcher * self,
                                                          const char *application,
                                                          guint64 pid);

const char  * bamf_matcher_get_desktop_file_class        (BamfMatcher * self,
                                                          const char * desktop_file);

const char  * bamf_matcher_get_active_application        (BamfMatcher *matcher);

const char  * bamf_matcher_get_active_window             (BamfMatcher *matcher);

const char  * bamf_matcher_application_for_xid           (BamfMatcher *matcher,
                                                          guint32 xid);

gboolean      bamf_matcher_application_is_running        (BamfMatcher *matcher,
                                                          const char *application);

GVariant    * bamf_matcher_application_dbus_paths        (BamfMatcher *matcher);

GVariant    * bamf_matcher_window_dbus_paths             (BamfMatcher *matcher);

const char  * bamf_matcher_dbus_path_for_application     (BamfMatcher *matcher,
                                                          const char *application);

void          bamf_matcher_register_favorites            (BamfMatcher *matcher,
                                                          const char **favorites);

GList       * bamf_matcher_get_favorites                 (BamfMatcher *matcher);

GVariant    * bamf_matcher_running_application_paths     (BamfMatcher *matcher);

GVariant    * bamf_matcher_tab_dbus_paths                (BamfMatcher *matcher);

GVariant    * bamf_matcher_xids_for_application          (BamfMatcher *matcher,
                                                          const char *application);

GVariant    * bamf_matcher_get_window_stack_for_monitor  (BamfMatcher *matcher,
                                                          gint monitor);

gboolean      bamf_matcher_is_valid_process_prefix       (BamfMatcher *matcher,
                                                          const char *process_name);

gboolean      bamf_matcher_is_valid_class_name           (BamfMatcher *matcher,
                                                          const char *class_name);

char        * bamf_matcher_get_trimmed_exec              (BamfMatcher *matcher,
                                                          const char *exec_string);

BamfView    * bamf_matcher_get_view_by_path              (BamfMatcher *matcher,
                                                          const char *view_path);

BamfMatcher * bamf_matcher_get_default                   (void);

#endif
