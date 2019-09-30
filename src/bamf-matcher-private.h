/*
 * Copyright (C) 2012 Canonical Ltd
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
 * Authored by: Marco Trevisan (Treviño) <marco.trevisan@canonical.com>
 *
 */

#ifndef __BAMF_MATCHER_PRIVATE_H__
#define __BAMF_MATCHER_PRIVATE_H__

#include "bamf-view.h"
#include "bamf-matcher.h"
#include "bamf-application.h"
#include "bamf-window.h"
#include "bamf-legacy-window.h"

struct _BamfMatcherPrivate
{
  GArray          * bad_prefixes;
  GArray          * good_prefixes;
  GHashTable      * desktop_id_table;
  GHashTable      * desktop_file_table;
  GHashTable      * desktop_class_table;
  GHashTable      * registered_pids;
  GHashTable      * opened_closed_paths_table;
  GList           * known_pids;
  GList           * views;
  GList           * monitors;
  GList           * favorites;
  GList           * no_display_desktop;
  BamfView        * active_app;
  BamfView        * active_win;
  guint             dispatch_changes_id;
};

BamfApplication * bamf_matcher_get_application_by_desktop_file (BamfMatcher *self, const char *desktop_file);
BamfApplication * bamf_matcher_get_application_by_xid (BamfMatcher *self, guint xid);
char * get_exec_overridden_desktop_file (const char *exec);

gboolean is_autostart_desktop_file (const gchar *desktop_file);

#endif
