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
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#include "config.h"

#include "bamf-matcher.h"
#include "bamf-matcher-private.h"
#include "bamf-application.h"
#include "bamf-tab.h"
#include "bamf-window.h"
#include "bamf-legacy-screen.h"

#include <strings.h>

#define BAMF_INDEX_NAME "bamf-2.index"

G_DEFINE_TYPE (BamfMatcher, bamf_matcher, BAMF_DBUS_TYPE_MATCHER_SKELETON);
#define BAMF_MATCHER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE(obj, \
                                       BAMF_TYPE_MATCHER, BamfMatcherPrivate))

enum
{
  FAVORITES_CHANGED,

  LAST_SIGNAL,
};

typedef enum
{
  VIEW_ADDED = 0,
  VIEW_REMOVED
} ViewChangeType;

static BamfMatcher *static_matcher;
static guint matcher_signals[LAST_SIGNAL] = { 0 };

// Prefixes to be ignored in exec strings
const gchar* EXEC_BAD_PREFIXES[] =
{
  "^gksu(do)?$", "^sudo$", "^su-to-root$", "^amdxdg-su$", "^java(ws)?$", "^cli$",
  "^mono$", "^ruby$", "^padsp$", "^aoss$", "^python(\\d(\\.\\d)?)?$", "^(ba)?sh$",
  "^perl$", "^env$", "^xdg-open$", "^qmlscene$", "^qmlviewer$",
  "^unity-webapps-runner$", "^webapp-container$",
  /* javaws strings: */ "^net\\.sourceforge\\.jnlp\\.runtime\\.Boot$", "^rt\\.jar$",
                        "^com\\.sun\\.javaws\\.Main$", "^deploy\\.jar$"
};

// Sufixes to be ignored in exec strings
const gchar* EXEC_BAD_SUFIXES = "(\\.bin|\\.py|\\.pl|\\.qml)$";

// Prefixes that must be considered starting point of exec strings
const gchar* EXEC_GOOD_PREFIXES[] =
{
  "^gnome-control-center$", "^libreoffice$", "^ooffice$", "^wine$", "^steam$",
  "^sol$"
};

// These class names are ignored as matching values
const gchar * CLASS_BAD_VALUES[] =
{
  "sun-awt-X11-XFramePeer", "net-sourceforge-jnlp-runtime-Boot",
  "com-sun-javaws-Main", "VCLSalFrame"
};

const gchar * EXEC_DESKTOP_FILE_OVERRIDE = "--desktop_file_hint";

static void
on_view_active_changed (BamfView *view, gboolean active, BamfMatcher *matcher)
{
  BamfMatcherPrivate *priv;
  BamfView *last;

  g_return_if_fail (BAMF_IS_MATCHER (matcher));
  g_return_if_fail (BAMF_IS_VIEW (view));

  priv = matcher->priv;

  if (BAMF_IS_APPLICATION (view))
    {
      /* Do some handy short circuiting so we can assume a signal
       * will be generated at the end of this
       */
      if (!active && priv->active_app != view)
        return;

      if (active && priv->active_app == view)
        return;

      last = priv->active_app;

      if (active)
        priv->active_app = view;
      else
        priv->active_app = NULL;

      g_signal_emit_by_name (matcher, "active-application-changed",
                             BAMF_IS_VIEW (last) ? bamf_view_get_path (BAMF_VIEW (last)) : "",
                             BAMF_IS_VIEW (priv->active_app) ? bamf_view_get_path (BAMF_VIEW (priv->active_app)) : "");
    }
  else if (BAMF_IS_WINDOW (view))
    {
      /* Do some handy short circuiting so we can assume a signal
       * will be generated at the end of this
       */
      if (!active && priv->active_win != view)
        return;

      if (active && priv->active_win == view)
        return;

      last = priv->active_win;

      if (active)
        priv->active_win = view;
      else
        priv->active_win = NULL;

      g_signal_emit_by_name (matcher, "active-window-changed",
                             BAMF_IS_VIEW (last) ? bamf_view_get_path (BAMF_VIEW (last)) : "",
                             BAMF_IS_VIEW (priv->active_win) ? bamf_view_get_path (BAMF_VIEW (priv->active_win)) : "");
    }
}

static void bamf_matcher_unregister_view (BamfMatcher *self, BamfView *view);

BamfApplication *
bamf_matcher_get_application_by_desktop_file (BamfMatcher *self, const char *desktop_file)
{
  GList *l;
  BamfView *view;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);

  if (!desktop_file)
    return NULL;

  for (l = self->priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_APPLICATION (view))
        continue;

      BamfApplication *app = BAMF_APPLICATION (view);
      const gchar *app_desktop;
      app_desktop = bamf_application_get_desktop_file (app);

      if (g_strcmp0 (desktop_file, app_desktop) == 0)
        {
          return app;
        }
    }

  return NULL;
}

BamfApplication *
bamf_matcher_get_application_by_xid (BamfMatcher *self, guint xid)
{
  GList *l;
  BamfView *view;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);

  for (l = self->priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_APPLICATION (view))
        continue;

      if (bamf_application_manages_xid (BAMF_APPLICATION (view), xid))
        {
          return BAMF_APPLICATION (view);
        }
    }

  return NULL;
}


BamfView *
bamf_matcher_get_view_by_path (BamfMatcher *self, const char *view_path)
{
  GList *l;
  BamfView *view;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);

  for (l = self->priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_VIEW (view))
        continue;

      if (g_strcmp0 (bamf_view_get_path (view), view_path) == 0)
        {
          return view;
        }
    }

  return NULL;
}

static gboolean
emit_paths_changed (gpointer user_data)
{
  BamfMatcher *matcher;
  BamfMatcherPrivate *priv;
  GHashTableIter iter;
  guint ht_size;
  ViewChangeType change_type;
  gpointer key, value;
  gchar **opened_apps, **closed_apps;
  gint i, j;

  g_return_val_if_fail (BAMF_IS_MATCHER (user_data), FALSE);

  matcher = (BamfMatcher*) user_data;
  priv = matcher->priv;

  ht_size = g_hash_table_size (priv->opened_closed_paths_table);
  /* these will end with NULL pointer */
  opened_apps = g_new0 (gchar*, ht_size+1);
  closed_apps = g_new0 (gchar*, ht_size+1);
  i = 0;
  j = 0;

  g_hash_table_iter_init (&iter, priv->opened_closed_paths_table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      change_type = (ViewChangeType) GPOINTER_TO_UINT (value);
      if (change_type == VIEW_ADDED)
        opened_apps[i++] = (gchar*) key;
      else
        closed_apps[j++] = (gchar*) key;
    }

  /* the strings are owned by the hashtable, so emit the signal and clear
   * the hashtable then */
  g_signal_emit_by_name (matcher, "running-applications-changed",
                         opened_apps, closed_apps);

  g_hash_table_remove_all (priv->opened_closed_paths_table);

  g_free (closed_apps);
  g_free (opened_apps);

  priv->dispatch_changes_id = 0;

  return FALSE;
}

static void
bamf_matcher_prepare_path_change (BamfMatcher *self, const gchar *desktop_file, ViewChangeType change_type)
{
  BamfMatcherPrivate *priv;
  BamfApplication *app;

  if (desktop_file == NULL) return;

  g_return_if_fail (BAMF_IS_MATCHER (self));

  priv = self->priv;

  /* the app was already running (ADDED) / had more instances which are still
   * there (REMOVED) */
  app = bamf_matcher_get_application_by_desktop_file (self, desktop_file);

  if (BAMF_IS_APPLICATION (app) && bamf_view_is_running (BAMF_VIEW (app)))
    {
      return;
    }

  if (!priv->opened_closed_paths_table)
    {
      priv->opened_closed_paths_table = g_hash_table_new_full (g_str_hash,
                                                               g_str_equal,
                                                               g_free, NULL);
    }

  g_hash_table_insert (priv->opened_closed_paths_table,
                       g_strdup (desktop_file), GUINT_TO_POINTER (change_type));

  if (priv->dispatch_changes_id == 0)
    {
      priv->dispatch_changes_id = g_timeout_add (500, emit_paths_changed, self);
    }
}

static gboolean
bamf_matcher_is_view_registered (BamfMatcher *self, BamfView *view)
{
  g_return_val_if_fail (BAMF_IS_MATCHER (self), FALSE);
  g_return_val_if_fail (BAMF_IS_VIEW (view), FALSE);

  /* Instead of looping over the self->priv->views list, we can safely assume
   * that if a view is not registered then its path is NULL */
  return (bamf_view_get_path (view) != NULL);
}

static void
bamf_matcher_register_view_stealing_ref (BamfMatcher *self, BamfView *view)
{
  const char *path, *type;
  GDBusConnection *connection;
  GDBusInterfaceSkeleton *dbus_interface = G_DBUS_INTERFACE_SKELETON (self);

  connection = g_dbus_interface_skeleton_get_connection (dbus_interface);
  path = bamf_view_export_on_bus (view, connection);
  type = bamf_view_get_view_type (view);

  g_signal_connect_swapped (G_OBJECT (view), "closed-internal",
                            (GCallback) bamf_matcher_unregister_view, self);
  g_signal_connect (G_OBJECT (view), "active-changed",
                    (GCallback) on_view_active_changed, self);

  if (BAMF_IS_APPLICATION (view))
    {
      bamf_matcher_prepare_path_change (self,
        bamf_application_get_desktop_file (BAMF_APPLICATION (view)), VIEW_ADDED);
    }

  // This steals the reference of the view
  self->priv->views = g_list_prepend (self->priv->views, view);

  g_signal_emit_by_name (self, "view-opened", path, type);

  // trigger manually since this is already active
  if (bamf_view_is_active (view))
    on_view_active_changed (view, TRUE, self);
}

static void
bamf_matcher_unregister_view (BamfMatcher *self, BamfView *view)
{
  const char * path;
  const char * type;

  path = bamf_view_get_path (view);
  type = bamf_view_get_view_type (view);

  g_signal_emit_by_name (self, "view-closed", path, type);

  g_signal_handlers_disconnect_by_data (G_OBJECT (view), self);

  if (BAMF_IS_APPLICATION (view))
    {
      bamf_matcher_prepare_path_change (self,
          bamf_application_get_desktop_file (BAMF_APPLICATION (view)),
          VIEW_REMOVED);
    }

  if (self->priv->active_app == view)
    self->priv->active_app = NULL;

  if (self->priv->active_win == view)
    self->priv->active_win = NULL;

  GList *listed_view = g_list_find (self->priv->views, view);
  if (listed_view)
    {
      self->priv->views = g_list_delete_link (self->priv->views, listed_view);
      g_object_unref (view);
    }
}

gboolean
bamf_matcher_is_valid_process_prefix (BamfMatcher *self, const char *process_name)
{
  GRegex *regex;
  gint i;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), TRUE);

  if (!process_name || *process_name == '\0')
    return FALSE;

  for (i = 0; i < self->priv->bad_prefixes->len; ++i)
    {
      regex = g_array_index (self->priv->bad_prefixes, GRegex *, i);

      if (g_regex_match (regex, process_name, 0, NULL))
        {
          return FALSE;
        }
    }

  return TRUE;
}

/* Attempts to return the binary name for a particular execution string */
char *
bamf_matcher_get_trimmed_exec (BamfMatcher * self, const char * exec_string)
{
  gchar *result = NULL, *part, *tmp;
  gchar **parts;
  gint i, j, parts_size;
  gboolean bad_prefix;
  gboolean good_prefix = FALSE;
  gboolean double_parsed = FALSE;
  GRegex *regex;

  if (!exec_string || exec_string[0] == '\0')
    return NULL;

  if (!g_shell_parse_argv (exec_string, &parts_size, &parts, NULL))
    return g_strdup (exec_string);

  for (i = 0; i < parts_size; ++i)
    {
      part = parts[i];
      if (*part == '%' || *part == '$' || g_utf8_strrchr (part, -1, '='))
        continue;

      if (i+1 < parts_size && g_strcmp0 (parts[i], EXEC_DESKTOP_FILE_OVERRIDE) == 0)
        {
          /* Skip if the .desktop file is overridden using the exec parameter */
          ++i;
          continue;
        }

      if (*part != '-' || good_prefix)
        {
          if (!result)
            {
              tmp = g_utf8_strrchr (part, -1, G_DIR_SEPARATOR);
              if (tmp)
                part = tmp + 1;
            }

          if (good_prefix)
            {
              tmp = g_strconcat (result, " ", part, NULL);
              g_free (result);
              result = tmp;
            }
          else
            {
              for (j = 0; j < self->priv->good_prefixes->len; j++)
                {
                  regex = g_array_index (self->priv->good_prefixes, GRegex *, j);
                  if (g_regex_match (regex, part, 0, NULL))
                    {
                      good_prefix = TRUE;
                      result = g_ascii_strdown (part, -1);
                      break;
                    }
                }

              if (good_prefix)
                continue;

              bad_prefix = !bamf_matcher_is_valid_process_prefix (self, part);

              if (!bad_prefix)
                {
                  if (!double_parsed && g_utf8_strrchr (part, -1, ' '))
                  {
                    /* If the current exec_string has an empty char,
                     * we double check it again to parse scripts:
                     * For example strings like 'sh -c "foo || bar"' */
                    gchar **old_parts = parts;

                    if (g_shell_parse_argv (part, &parts_size, &parts, NULL))
                      {
                        // Make the loop to restart!
                        g_strfreev (old_parts);
                        i = -1;
                        continue;
                      }

                    double_parsed = TRUE;
                  }

                  result = g_ascii_strdown (part, -1);
                  break;
                }
            }
        }
    }

  if (!result)
    {
      if (parts_size > 0)
        {
          tmp = g_utf8_strrchr (parts[0], -1, G_DIR_SEPARATOR);
          if (tmp)
            exec_string = tmp + 1;
        }

      result = g_strdup (exec_string);
    }
  else
    {
      tmp = result;

      regex = g_regex_new (EXEC_BAD_SUFIXES, 0, 0, NULL);
      result = g_regex_replace_literal (regex, result, -1, 0, "", 0, NULL);

      g_free (tmp);
      g_regex_unref (regex);
    }

  g_strfreev (parts);

  return result;
}

char *
get_exec_overridden_desktop_file (const char *exec_string)
{
  gchar *result = NULL;
  gchar **parts;
  gint i, parts_size;

  if (!exec_string || exec_string[0] == '\0')
    return result;

  if (!g_shell_parse_argv (exec_string, &parts_size, &parts, NULL))
    return result;

  for (i = 0; i < parts_size; ++i)
    {
      if (i+1 < parts_size && g_strcmp0 (parts[i], EXEC_DESKTOP_FILE_OVERRIDE) == 0)
        {
          if (g_str_has_suffix (parts[i+1], ".desktop") &&
              g_file_test (parts[i+1], G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR))
            {
              result = g_strdup (parts[i+1]);
              break;
            }
        }
    }

  g_strfreev (parts);

  return result;
}

static GList *
pid_parent_tree (BamfMatcher *self, guint pid)
{
  BamfMatcherPrivate *priv;
  GList *tree, *l;
  guint known_pid;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);

  priv = self->priv;
  tree = g_list_append (NULL, GUINT_TO_POINTER (pid));

  glibtop_proc_uid buf;
  glibtop_get_proc_uid (&buf, pid);

  pid = buf.ppid;

  while (pid > 1)
    {
      for (l = priv->known_pids; l; l = l->next)
        {
          /* ensure we dont match onto a terminal by mistake */
          known_pid = GPOINTER_TO_UINT (l->data);

          if (known_pid == pid)
            return tree;
        }

      tree = g_list_prepend (tree, GUINT_TO_POINTER (pid));

      glibtop_proc_uid buf;
      glibtop_get_proc_uid (&buf, pid);

      pid = buf.ppid;
    }

  return g_list_reverse (tree);
}

static gboolean
is_desktop_folder_item (const char *desktop_file_path, gssize max_len)
{
  gsize len;
  const char *desktop_folder;

  g_return_val_if_fail (desktop_file_path, FALSE);

  if (max_len > 0)
    {
      len = max_len;
    }
  else
    {
      char *tmp;
      tmp = strrchr (desktop_file_path, G_DIR_SEPARATOR);
      g_return_val_if_fail (tmp, FALSE);
      len = tmp - desktop_file_path;
    }

  desktop_folder = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);

  if (strncmp (desktop_folder, desktop_file_path, len) == 0)
    return TRUE;

  return FALSE;
}

static gboolean
is_no_display_desktop (BamfMatcher *self, const gchar *desktop_path)
{
  g_return_val_if_fail (BAMF_IS_MATCHER (self), FALSE);

  if (!desktop_path || desktop_path[0] == '\0')
    return FALSE;

  GList *list = self->priv->no_display_desktop;

  return g_list_find_custom (list, desktop_path, (GCompareFunc) g_strcmp0) != NULL;
}

static GList *
get_last_no_display_desktop_in_list (BamfMatcher *self, GList *desktop_list)
{
  GList *last, *l;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);

  last = NULL;

  for (l = desktop_list; l; l = l->next)
    {
      char *desktop_path = l->data;

      if (is_no_display_desktop (self, desktop_path))
        {
          last = l;
          break;
        }
    }

  return last;
}

static void
insert_data_into_tables (BamfMatcher *self,
                         const char *data,
                         const char *exec,
                         const char *desktop_id,
                         gboolean no_display,
                         GHashTable *desktop_file_table,
                         GHashTable *desktop_id_table)
{
  GList *file_list, *id_list, *last, *l;
  char *datadup;

  g_return_if_fail (exec);
  g_return_if_fail (desktop_id);

  file_list = g_hash_table_lookup (desktop_file_table, exec);
  id_list   = g_hash_table_lookup (desktop_id_table, desktop_id);

  if (g_list_find_custom (file_list, data, (GCompareFunc) g_strcmp0) &&
      g_list_find_custom (id_list, data, (GCompareFunc) g_strcmp0))
    {
      return;
    }

  datadup = g_strdup (data);

  if (no_display)
    {
      self->priv->no_display_desktop = g_list_prepend (self->priv->no_display_desktop, datadup);
    }

  /* order so that items whose desktop_id == exec string are first in the list */

  if (g_strcmp0 (exec, desktop_id) == 0 || is_desktop_folder_item (datadup, -1))
    {
      last = NULL;

      for (l = file_list; l; l = l->next)
        {
          char *dpath;
          char *dname_start, *dname_end;
          size_t len;

          dpath = l->data;
          dname_start = strrchr (dpath, G_DIR_SEPARATOR);
          if (!dname_start)
            {
              continue;
            }

          dname_start++;
          dname_end = strrchr (dname_start, '.');
          len = dname_end - dname_start;

          if (!dname_end || len < 1)
            {
              continue;
            }

          if ((strncmp (desktop_id, dname_start, len) != 0 || is_no_display_desktop (self, dpath)) &&
              !is_desktop_folder_item (dpath, (dname_start - dpath - 1)))
            {
              last = l;
              break;
            }
        }

      file_list = g_list_insert_before (file_list, last, datadup);
    }
  else
    {
      last = NULL;

      if (!no_display)
        last = get_last_no_display_desktop_in_list (self, file_list);

      file_list = g_list_insert_before (file_list, last, datadup);
    }

  last = NULL;

  if (!no_display)
    last = get_last_no_display_desktop_in_list (self, id_list);

  id_list = g_list_insert_before (id_list, last, datadup);

  g_hash_table_insert (desktop_file_table, g_strdup (exec),       file_list);
  g_hash_table_insert (desktop_id_table,   g_strdup (desktop_id), id_list);
}

static void
insert_desktop_file_class_into_table (BamfMatcher *self,
                                      const char *desktop_file,
                                      GHashTable *desktop_class_table)
{
  GKeyFile *desktop_keyfile;
  char *class;

  g_return_if_fail (desktop_file);

  desktop_keyfile = g_key_file_new ();

  if (g_key_file_load_from_file (desktop_keyfile, desktop_file, G_KEY_FILE_NONE,
                                 NULL))
    {
      class = g_key_file_get_string (desktop_keyfile,
                                     G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS,
                                     NULL);
      if (class)
        g_hash_table_insert (desktop_class_table, g_strdup (desktop_file), class);

      g_key_file_free (desktop_keyfile);
    }
}

static void
load_desktop_file_to_table (BamfMatcher * self,
                            const char *file,
                            GHashTable *desktop_file_table,
                            GHashTable *desktop_id_table,
                            GHashTable *desktop_class_table)
{
  GDesktopAppInfo *desktop_file;
  gboolean no_display;
  const char *current_desktop;
  char *exec;
  char *path;
  GString *desktop_id; /* is ok... really */

  g_return_if_fail (BAMF_IS_MATCHER (self));

  desktop_file = g_desktop_app_info_new_from_filename (file);

  if (!G_IS_APP_INFO (desktop_file))
    {
      return;
    }

  current_desktop = g_getenv ("XDG_CURRENT_DESKTOP");

  if (current_desktop && !g_desktop_app_info_get_show_in (desktop_file, current_desktop))
    {
      g_object_unref (desktop_file);
      return;
    }

  exec = g_strdup (g_app_info_get_commandline (G_APP_INFO (desktop_file)));

  if (!exec || exec[0] == '\0')
    {
      g_object_unref (desktop_file);

      if (exec)
        {
          g_free (exec);
        }

      return;
    }

  /**
   * Set of nasty hacks which should be removed some day. We wish to keep the full exec
   * strings so we can do nasty matching hacks later. A very very evil thing indeed. However this
   * helps hack around applications that run in the same process cross radically different instances.
   * A better solution needs to be thought up, however at this time it is not known.
   **/
  char *tmp = bamf_matcher_get_trimmed_exec (self, exec);
  g_free (exec);
  exec = tmp;

  path = g_path_get_basename (file);
  desktop_id = g_string_new (path);
  g_free (path);

  desktop_id = g_string_truncate (desktop_id, desktop_id->len - 8); /* remove last 8 characters for .desktop */
  no_display = g_desktop_app_info_get_nodisplay (desktop_file);

  insert_data_into_tables (self, file, exec, desktop_id->str, no_display, desktop_file_table, desktop_id_table);
  insert_desktop_file_class_into_table (self, file, desktop_class_table);

  g_free (exec);
  g_string_free (desktop_id, TRUE);
  g_object_unref (desktop_file);
}

static void
load_directory_to_table (BamfMatcher * self,
                         const char *directory,
                         GHashTable *desktop_file_table,
                         GHashTable *desktop_id_table,
                         GHashTable *desktop_class_table)
{
  GFile *dir;
  GFileEnumerator *enumerator;
  GFileInfo *info;
  const char *name;
  char *path;

  dir = g_file_new_for_path (directory);

  enumerator = g_file_enumerate_children (dir,
                                          "standard::*",
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          NULL,
                                          NULL);

  if (!enumerator)
    return;

  info = g_file_enumerator_next_file (enumerator, NULL, NULL);
  for (; info; info = g_file_enumerator_next_file (enumerator, NULL, NULL))
    {
      name = g_file_info_get_name (info);
      path = g_build_filename (directory, name, NULL);

      if (g_str_has_suffix (name, ".desktop"))
        load_desktop_file_to_table (self,
                                    path,
                                    desktop_file_table,
                                    desktop_id_table,
                                    desktop_class_table);

      g_free (path);
      g_object_unref (info);
    }

  g_object_unref (enumerator);
  g_object_unref (dir);
}

static void
load_index_file_to_table (BamfMatcher * self,
                          const char *index_file,
                          GHashTable *desktop_file_table,
                          GHashTable *desktop_id_table,
                          GHashTable *desktop_class_table)
{
  GFile *file;
  GFileInputStream *stream;
  GDataInputStream *input;
  char *line;
  char *directory;
  const char *current_desktop;
  gsize length;

  file = g_file_new_for_path (index_file);

  g_return_if_fail (file);

  stream = g_file_read (file, NULL, NULL);

  if (!stream)
    {
      g_object_unref (file);
      return;
    }

  length = 0;
  current_desktop = g_getenv ("XDG_CURRENT_DESKTOP");
  directory = g_path_get_dirname (index_file);
  input = g_data_input_stream_new (G_INPUT_STREAM (stream));

  if (current_desktop && current_desktop[0] == '\0')
    current_desktop = NULL;

  while ((line = g_data_input_stream_read_line (input, &length, NULL, NULL)))
    {
      char *exec;
      char *filename;
      const char *class;
      const char *show_in;
      GString *desktop_id;
      gboolean no_display;

      /* Order is: 0 Desktop-Id, 1 Exec, 2 class, 3 ShowIn, 4 NoDisplay */
      gchar **parts = g_strsplit (line, "\t", 5);

      show_in = parts[3];

      if (current_desktop && show_in && show_in[0] != '\0')
        {
          gchar **sub_parts = g_strsplit (show_in, ";", -1);
          gboolean found_current = FALSE;
          int i = 0;

          for (i = 0; sub_parts[i]; ++i)
            {
              if (g_ascii_strcasecmp (sub_parts[i], current_desktop) == 0)
                {
                  found_current = TRUE;
                  break;
                }
            }

          g_strfreev (sub_parts);

          if (!found_current)
            {
              length = 0;
              g_strfreev (parts);
              continue;
            }
        }

      char *tmp = bamf_matcher_get_trimmed_exec (self, parts[1]);
      g_free (parts[1]);
      parts[1] = tmp;
      exec = parts[1];

      filename = g_build_filename (directory, parts[0], NULL);

      desktop_id = g_string_new (parts[0]);
      g_string_truncate (desktop_id, desktop_id->len - 8);

      no_display = FALSE;
      if (parts[4] && g_ascii_strcasecmp (parts[4], "true") == 0)
        {
          no_display = TRUE;
        }

      insert_data_into_tables (self, filename, exec, desktop_id->str, no_display, desktop_file_table, desktop_id_table);

      class = parts[2];
      if (class && class[0] != '\0')
        {
          g_hash_table_insert (desktop_class_table, g_strdup (filename), g_strdup (class));
        }

      g_string_free (desktop_id, TRUE);
      g_free (line);
      g_free (filename);
      g_strfreev (parts);
      length = 0;
    }

  g_object_unref (input);
  g_object_unref (stream);
  g_object_unref (file);
  g_free (directory);
}

static GList * get_directory_tree_list (GList *) G_GNUC_WARN_UNUSED_RESULT;

static GList *
get_directory_tree_list (GList *dirs)
{
  GList *l;
  GFile *file;
  GFileEnumerator *enumerator;
  GFileInfo *info;
  gchar *path, *subpath;

  for (l = dirs; l; l = l->next)
    {
      path = l->data;

      file = g_file_new_for_path (path);

      if (!g_file_query_exists (file, NULL))
        {
          g_object_unref (file);
          continue;
        }

      enumerator = g_file_enumerate_children (file,
                                              "standard::*",
                                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                              NULL,
                                              NULL);

      if (!enumerator)
        continue;

      info = g_file_enumerator_next_file (enumerator, NULL, NULL);

      while (info)
        {
          if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
            {
              /* append after the current list item for non-recursive recursion love
               * and to keep the priorities (hierarchy) of the .desktop directories.
               */
              subpath = g_build_filename (path, g_file_info_get_name (info), NULL);
              dirs = g_list_insert_before (dirs, l->next, subpath);
            }

          g_object_unref (info);
          info = g_file_enumerator_next_file (enumerator, NULL, NULL);
        }

      g_object_unref (enumerator);
      g_object_unref (file);
    }

  return dirs;
}

static GList * list_prepend_desktop_file_env_directories (GList *, const gchar *) G_GNUC_WARN_UNUSED_RESULT;

static GList *
list_prepend_desktop_file_env_directories (GList *dirs, const gchar *varname)
{
  g_return_val_if_fail (varname, dirs);

  const gchar *env;
  char *path;
  char **data_dirs = NULL;
  char **data;

  env = g_getenv (varname);

  if (env)
    {
      data_dirs = g_strsplit (env, ":", 0);

      for (data = data_dirs; data && *data; data++)
        {
          path = g_build_filename (*data, "applications", NULL);
          if (g_file_test (path, G_FILE_TEST_IS_DIR) &&
              !g_list_find_custom (dirs, path, (GCompareFunc) g_strcmp0))
            {
              dirs = g_list_prepend (dirs, path);
            }
          else
            {
              g_free (path);
            }
        }

      if (data_dirs)
        g_strfreev (data_dirs);
    }

  return dirs;
}

static GList *
get_desktop_file_directories (BamfMatcher *self)
{
  GList *dirs = NULL;
  char *path;

  dirs = list_prepend_desktop_file_env_directories (dirs, "XDG_DATA_DIRS");

  if (!g_list_find_custom (dirs, "/usr/share/applications", (GCompareFunc) g_strcmp0))
    dirs = g_list_prepend (dirs, g_strdup ("/usr/share/applications"));

  if (!g_list_find_custom (dirs, "/usr/local/share/applications", (GCompareFunc) g_strcmp0))
    dirs = g_list_prepend (dirs, g_strdup ("/usr/local/share/applications"));

  dirs = list_prepend_desktop_file_env_directories (dirs, "XDG_DATA_HOME");

  /* If XDG_DATA_HOME doesn't exist, we need to track .local or the home itself! */
  path = g_build_filename (g_get_home_dir (), ".local/share/applications", NULL);

  if (!g_list_find_custom (dirs, path, (GCompareFunc) g_strcmp0))
    dirs = g_list_prepend (dirs, path);
  else
    g_free (path);

  /* include subdirs */
  dirs = get_directory_tree_list (dirs);

  /* Include also the user desktop folder, but without its subfolders */
  dirs = g_list_prepend (dirs, g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP)));

  return dirs;
}

static gint
compare_sub_values (gconstpointer desktop_path, gconstpointer desktop_file)
{
  return !g_str_has_prefix (desktop_file, desktop_path);
}

static void
hash_table_remove_sub_values (GHashTable *htable, GCompareFunc compare_func,
                              GFreeFunc free_func, gpointer target, gboolean search_all)
{
  g_return_if_fail (htable);
  g_return_if_fail (compare_func);

  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_hash_table_iter_init (&iter, htable);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GList *list, *l;
      gboolean found;

      list = value;
      found = FALSE;

      l = list;
      while (l)
        {
          GList *next = l->next;

          if (compare_func (target, l->data) == 0)
            {
              found = TRUE;

              if (!l->prev && !l->next)
              {
                if (free_func)
                  g_list_free_full (list, free_func);
                else
                  g_list_free (list);

                g_hash_table_iter_remove (&iter);

                next = NULL;
                break;
              }
            else
              {
                if (free_func)
                  free_func (l->data);

                /* If the target is the first element of the list (and thanks to
                 * the previous check we're also sure that it's not the only one),
                 * simply switch it with its follower, not to change the first
                 * pointer and the hash table value for key
                 */
                if (l == list)
                  {
                    l->data = next->data;
                    l = next;
                    next = list;
                  }

                list = g_list_delete_link (list, l);
              }

              if (!search_all)
                break;
            }
            l = next;
        }

      if (found && !search_all)
         break;
    }
}

static gboolean
hash_table_compare_sub_values (gpointer desktop_path, gpointer desktop_class, gpointer target_path)
{
  return !compare_sub_values (target_path, desktop_path);
}

static void fill_desktop_file_table (BamfMatcher *, GList *, GHashTable *, GHashTable *, GHashTable *);

static void
on_monitor_changed (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent type, BamfMatcher *self)
{
  char *path;
  const char *monitored_dir;
  GFileType filetype;

  g_return_if_fail (G_IS_FILE_MONITOR (monitor));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (BAMF_IS_MATCHER (self));

  if (type != G_FILE_MONITOR_EVENT_CREATED &&
      type != G_FILE_MONITOR_EVENT_DELETED &&
      type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    return;

  path = g_file_get_path (file);
  filetype = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);
  monitored_dir = g_object_get_data (G_OBJECT (monitor), "root");

  if (!g_str_has_suffix (path, ".desktop") &&
      filetype != G_FILE_TYPE_DIRECTORY &&
      type != G_FILE_MONITOR_EVENT_DELETED)
    {
      g_free(path);
      return;
    }

  if (type == G_FILE_MONITOR_EVENT_DELETED ||
      type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
      if (g_str_has_suffix (path, ".desktop"))
        {
          /* Remove all the .desktop file references from the hash tables.
           * Free the string itself only on the 2nd pass (tables share the same
           * string instance)
           */
          hash_table_remove_sub_values (self->priv->desktop_id_table,
                                       (GCompareFunc) g_strcmp0, NULL, path, FALSE);
          hash_table_remove_sub_values (self->priv->desktop_file_table,
                                       (GCompareFunc) g_strcmp0, g_free, path, FALSE);
          g_hash_table_remove (self->priv->desktop_class_table, path);
        }
      else if (g_strcmp0 (monitored_dir, path) == 0)
        {
          /* Remove all the references to the .desktop files placed in subfolders
           * of the current path. Free the strings itself only on the 2nd pass
           * (as before, the tables share the same string instance)
           */
          char *prefix = g_strconcat (path, G_DIR_SEPARATOR_S, NULL);

          hash_table_remove_sub_values (self->priv->desktop_id_table,
                                        compare_sub_values, NULL, prefix, TRUE);
          hash_table_remove_sub_values (self->priv->desktop_file_table,
                                        compare_sub_values, g_free, prefix, TRUE);
          g_hash_table_foreach_remove (self->priv->desktop_class_table,
                                       hash_table_compare_sub_values, prefix);

          g_signal_handlers_disconnect_by_func (monitor, on_monitor_changed, self);
          self->priv->monitors = g_list_remove (self->priv->monitors, monitor);
          g_object_unref (monitor);
          g_free (prefix);
        }
    }

  if (type == G_FILE_MONITOR_EVENT_CREATED ||
      type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
      if (filetype == G_FILE_TYPE_DIRECTORY)
        {
          const char *desktop_dir;
          desktop_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);

          if (g_strcmp0 (monitored_dir, desktop_dir) != 0)
            {
              GList *dirs = NULL;
              dirs = g_list_prepend (dirs, g_strdup (path));
              dirs = get_directory_tree_list (dirs);
              fill_desktop_file_table (self, dirs,
                                       self->priv->desktop_file_table,
                                       self->priv->desktop_id_table,
                                       self->priv->desktop_class_table);

              g_list_free_full (dirs, g_free);
            }
        }
      else if (filetype != G_FILE_TYPE_UNKNOWN)
        {
          bamf_matcher_load_desktop_file (self, path);
        }
    }

  g_free (path);
}

static void
bamf_matcher_add_new_monitored_directory (BamfMatcher * self, const gchar *directory)
{
  g_return_if_fail (BAMF_IS_MATCHER (self));

  GFile *file;
  GFileMonitor *monitor;
  GError *error = NULL;

  file = g_file_new_for_path (directory);
  monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &error);

  if (error)
    {
      g_message ("Error monitoring %s: %s\n", directory, error->message);
      g_error_free (error);
      g_object_unref (file);
      return;
    }

  g_file_monitor_set_rate_limit (monitor, 1000);
  g_object_set_data_full (G_OBJECT (monitor), "root", g_strdup (directory), g_free);
  g_signal_connect (monitor, "changed", (GCallback) on_monitor_changed, self);
  self->priv->monitors = g_list_prepend (self->priv->monitors, monitor);

  g_object_unref (file);
}

static void
fill_desktop_file_table (BamfMatcher * self,
                         GList *directories,
                         GHashTable *desktop_file_table,
                         GHashTable *desktop_id_table,
                         GHashTable *desktop_class_table)
{
  g_return_if_fail (BAMF_IS_MATCHER (self));

  GList *l;
  char *directory;
  char *bamf_file;

  for (l = directories; l; l = l->next)
    {
      directory = l->data;

      if (!g_file_test (directory, G_FILE_TEST_IS_DIR))
        continue;

      bamf_matcher_add_new_monitored_directory (self, directory);

      bamf_file = g_build_filename (directory, BAMF_INDEX_NAME, NULL);

      if (g_file_test (bamf_file, G_FILE_TEST_EXISTS))
        {
          load_index_file_to_table (self, bamf_file, desktop_file_table,
                                    desktop_id_table, desktop_class_table);
        }
      else
        {
          load_directory_to_table (self, directory, desktop_file_table,
                                   desktop_id_table, desktop_class_table);
        }

      g_free (bamf_file);
    }
}

static void
create_desktop_file_table (BamfMatcher * self,
                           GHashTable **desktop_file_table,
                           GHashTable **desktop_id_table,
                           GHashTable **desktop_class_table)
{
  g_return_if_fail (BAMF_IS_MATCHER (self));

  GList *directories;

  *desktop_file_table =
    g_hash_table_new_full ((GHashFunc) g_str_hash,
                           (GEqualFunc) g_str_equal,
                           (GDestroyNotify) g_free,
                           NULL);

  *desktop_id_table =
    g_hash_table_new_full ((GHashFunc) g_str_hash,
                           (GEqualFunc) g_str_equal,
                           (GDestroyNotify) g_free,
                           NULL);

  *desktop_class_table =
    g_hash_table_new_full ((GHashFunc) g_str_hash,
                           (GEqualFunc) g_str_equal,
                           (GDestroyNotify) g_free,
                           (GDestroyNotify) g_free);

  directories = get_desktop_file_directories (self);

  fill_desktop_file_table (self, directories, *desktop_file_table,
                           *desktop_id_table, *desktop_class_table);

  g_list_free_full (directories, g_free);
}

static GList *
bamf_matcher_possible_applications_for_window_process (BamfMatcher *self, BamfLegacyWindow *window)
{
  BamfMatcherPrivate *priv;
  GList *result = NULL, *table_list, *l;
  const char *exec_string;
  char *trimmed;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW (window), NULL);

  priv = self->priv;
  exec_string = bamf_legacy_window_get_exec_string (window);

  if (exec_string)
    {
      trimmed = bamf_matcher_get_trimmed_exec (self, exec_string);

      if (trimmed)
        {
          if (trimmed[0] != '\0')
            {
              table_list = g_hash_table_lookup (priv->desktop_file_table, trimmed);

              for (l = table_list; l; l = l->next)
                {
                  result = g_list_prepend (result, g_strdup (l->data));
                }
            }
          g_free (trimmed);
        }
    }

  if (result)
    {
      result = g_list_reverse (result);
      return result;
    }

  gchar *proc_name = bamf_legacy_window_get_process_name (window);

  if (bamf_matcher_is_valid_process_prefix (self, proc_name))
    {
      table_list = g_hash_table_lookup (priv->desktop_file_table, proc_name);

      for (l = table_list; l; l = l->next)
        {
          result = g_list_prepend (result, g_strdup (l->data));
        }
    }
  g_free (proc_name);

  result = g_list_reverse (result);
  return result;
}

static gboolean
is_libreoffice_window (BamfLegacyWindow * window)
{
  const char *class_name = bamf_legacy_window_get_class_name (window);

  if (!class_name)
    return FALSE;

  return (g_ascii_strncasecmp (class_name, "libreoffice", 11) == 0 ||
          g_ascii_strncasecmp (class_name, "openoffice", 10) == 0);
}

static gboolean
is_web_app_window (BamfLegacyWindow *window)
{
  const char *window_class = bamf_legacy_window_get_class_name (window);
  const char *instance_name = bamf_legacy_window_get_class_instance_name (window);

  // Chrome/Chromium uses url wm_class strings to represent its web apps (--app mode).
  // These apps will still have the same parent pid and hints as the main chrome
  // window, so we skip the hint check.
  // We can tell a window is a chrome web app window if its instance name is
  // not google-chrome but its window class is Google-chrome
  // We can tell a window is chromium web app window if its instance name is
  // not chromium-browser but its window class is Chromium Browser

  gboolean valid_app = FALSE;

  if (instance_name && window_class)
    {
      if (g_strcmp0 (window_class, "Google-chrome") == 0 &&
          g_strcmp0 (instance_name, "google-chrome") != 0 &&
          !g_str_has_prefix (instance_name, "Google-chrome"))
        {
          valid_app = TRUE;
        }
      else if (g_strcmp0 (window_class, "Chromium-browser") == 0 &&
               g_strcmp0 (instance_name, "chromium-browser") != 0 &&
               !g_str_has_prefix (instance_name, "Chromium-browser"))
        {
          valid_app = TRUE;
        }
    }

  return valid_app;
}

static gboolean
is_javaws_window (BamfLegacyWindow *window)
{
  const char *window_class = bamf_legacy_window_get_class_name (window);

  if (g_strcmp0 (window_class, "net-sourceforge-jnlp-runtime-Boot") == 0 ||
      g_strcmp0 (window_class, "com-sun-javaws-Main") == 0)
    {
      return TRUE;
    }

  return FALSE;
}

static gboolean
bamf_matcher_window_skips_hint_set (BamfMatcher *self, BamfLegacyWindow *window)
{
  gboolean skip_hint_set;
  g_return_val_if_fail (BAMF_IS_MATCHER (self), TRUE);

  skip_hint_set = is_libreoffice_window (window) || is_web_app_window (window) || is_javaws_window (window);

  return skip_hint_set;
}

static GList *
bamf_matcher_get_class_matching_desktop_files (BamfMatcher *self, const gchar *class_name)
{
  GList* desktop_files = NULL;
  gpointer key;
  gpointer value;
  GHashTableIter iter;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);
  g_hash_table_iter_init (&iter, self->priv->desktop_class_table);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      gchar* desktop_file = g_strdup (key);
      gchar* desktop_class = value;

      if (g_strcmp0 (desktop_class, class_name) == 0)
        {
          desktop_files = g_list_prepend (desktop_files, desktop_file);
        }
    }

  return desktop_files;
}

static gboolean
bamf_matcher_has_instance_class_desktop_file (BamfMatcher *self, const gchar *class_name)
{
  gpointer key;
  gpointer value;
  GHashTableIter iter;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), FALSE);
  g_hash_table_iter_init (&iter, self->priv->desktop_class_table);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      gchar* desktop_class = value;

      if (g_strcmp0 (desktop_class, class_name) == 0)
        {
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
bamf_matcher_is_valid_class_name (BamfMatcher *self, const char *class_name)
{
  int i;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), FALSE);

  if (!class_name)
    return TRUE;

  for (i = 0; i < G_N_ELEMENTS (CLASS_BAD_VALUES); ++i)
    {
      if (g_strcmp0 (class_name, CLASS_BAD_VALUES[i]) == 0)
        return FALSE;
    }

  return TRUE;
}

static GList *
bamf_matcher_possible_applications_for_window (BamfMatcher *self,
                                               BamfWindow *bamf_window,
                                               const char **target_class_out)
{
  BamfMatcherPrivate *priv;
  BamfLegacyWindow *window;
  GList *desktop_files = NULL, *l;
  char *desktop_file = NULL;
  const char *desktop_class = NULL;
  const char *class_name = NULL;
  const char *instance_name = NULL;
  const char *target_class = NULL;
  gboolean filter_by_wmclass = FALSE;

  g_return_val_if_fail (BAMF_IS_WINDOW (bamf_window), NULL);
  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);

  priv = self->priv;
  window = bamf_window_get_window (bamf_window);
  desktop_file = bamf_legacy_window_get_hint (window, _NET_WM_DESKTOP_FILE);
  class_name = bamf_legacy_window_get_class_name (window);
  instance_name = bamf_legacy_window_get_class_instance_name (window);

  if (!bamf_matcher_is_valid_class_name (self, class_name))
    class_name = NULL;

  if (!bamf_matcher_is_valid_class_name (self, instance_name))
    instance_name = NULL;

  target_class = instance_name;
  filter_by_wmclass = bamf_matcher_has_instance_class_desktop_file (self, target_class);

  if (!filter_by_wmclass)
    {
      if (is_web_app_window (window))
        {
          // This ensures that a new application is created even for unknown webapps
          filter_by_wmclass = TRUE;
        }
      else
        {
          target_class = class_name;
          filter_by_wmclass = bamf_matcher_has_instance_class_desktop_file (self, target_class);
        }
    }

  if (desktop_file)
    {
      desktop_class = bamf_matcher_get_desktop_file_class (self, desktop_file);

      if ((!filter_by_wmclass && !desktop_class) || g_strcmp0 (desktop_class, target_class) == 0)
        {
          desktop_files = g_list_prepend (desktop_files, desktop_file);
        }
      else
        {
          g_free (desktop_file);
        }
    }
  else
    {
      const char *exec_string = bamf_legacy_window_get_exec_string (window);
      desktop_file = get_exec_overridden_desktop_file (exec_string);

      if (desktop_file)
        {
          desktop_files = g_list_prepend (desktop_files, desktop_file);
        }
    }

  desktop_file = NULL;

  if (!desktop_files)
    {
      if (class_name)
        {
          char *window_class_down = g_ascii_strdown (class_name, -1);
          l = g_hash_table_lookup (priv->desktop_id_table, window_class_down);
          g_free (window_class_down);

          for (; l; l = l->next)
            {
              desktop_file = l->data;

              if (desktop_file)
                {
                  desktop_class = bamf_matcher_get_desktop_file_class (self, desktop_file);

                  if ((!filter_by_wmclass && !desktop_class) || g_strcmp0 (desktop_class, target_class) == 0)
                    {
                      if (!g_list_find_custom (desktop_files, desktop_file,
                                               (GCompareFunc) g_strcmp0))
                        {
                          desktop_files = g_list_prepend (desktop_files, g_strdup (desktop_file));
                        }
                    }
                }
            }

          desktop_files = g_list_reverse (desktop_files);
        }

      GList *pid_list = bamf_matcher_possible_applications_for_window_process (self, window);

      /* Append these files to the end to give preference to class_name style picking.
         This style of matching is prefered and used by GNOME Shell however does not work
         very well in practice, thus requiring the fallback here */
      for (l = pid_list; l; l = l->next)
        {
          desktop_file = l->data;
          if (g_list_find_custom (desktop_files, desktop_file, (GCompareFunc) g_strcmp0))
            {
              g_free (desktop_file);
            }
          else
            {
              gboolean append = FALSE;

              if (target_class)
                {
                  desktop_class = bamf_matcher_get_desktop_file_class (self, desktop_file);
                  if ((!filter_by_wmclass && !desktop_class) || g_strcmp0 (desktop_class, target_class) == 0)
                    {
                      append = TRUE;
                    }
                }
              else
                {
                  append = TRUE;
                }

              if (append)
                {
                  /* If we're adding a .desktop file stored in the desktop folder,
                     give it the priority it should have. */
                  GList *last = NULL;

                  if (is_desktop_folder_item (desktop_file, -1))
                    {
                      GList *ll;

                      for (ll = desktop_files; ll; ll = ll->next)
                        {
                          if (!is_desktop_folder_item (ll->data, -1))
                            {
                              last = ll;
                              break;
                            }
                        }
                    }

                  desktop_files = g_list_insert_before (desktop_files, last, desktop_file);
                }
              else
                {
                  g_free (desktop_file);
                }
            }
        }

      g_list_free (pid_list);
    }

  if (!desktop_files && filter_by_wmclass)
    {
      desktop_files = bamf_matcher_get_class_matching_desktop_files (self, target_class);
    }

  if (target_class_out)
    {
      *target_class_out = target_class;
    }

  return desktop_files;
}

static BamfApplication *
bamf_matcher_get_application_for_window (BamfMatcher *self,
                                         BamfWindow *bamf_window)
{
  GList *possible_apps, *l;
  BamfLegacyWindow *window;
  const gchar *win_class_name;
  const gchar *target_class = NULL;
  const gchar *app_class = NULL;
  const gchar *app_desktop = NULL;
  BamfApplication *app = NULL, *best = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);
  g_return_val_if_fail (BAMF_IS_WINDOW (bamf_window), NULL);

  window = bamf_window_get_window (bamf_window);

  if (bamf_legacy_window_get_window_type (window) != BAMF_WINDOW_NORMAL)
    {
      BamfLegacyWindow *transient = bamf_legacy_window_get_transient (window);

      if (transient)
        {
          Window xid = bamf_legacy_window_get_xid (transient);
          app = bamf_matcher_get_application_by_xid (self, xid);

          if (BAMF_IS_APPLICATION (app))
            return app;
        }
    }

  win_class_name = bamf_legacy_window_get_class_name (window);

  possible_apps = bamf_matcher_possible_applications_for_window (self, bamf_window, &target_class);
  app_class = target_class;

  /* Loop over every possible desktop file that could match the window, and try
   * to reuse an already-opened window that uses it.
   * Desktop files are ordered by priority, so we try to use the first possible,
   * wm_class matching applications have the priority, btw. */
  if (possible_apps)
    {
      /* primary matching */
      for (l = possible_apps; l; l = l->next)
        {
          const gchar *desktop_file = l->data;
          app = bamf_matcher_get_application_by_desktop_file (self, desktop_file);

          if (BAMF_IS_APPLICATION (app))
            {
              const gchar *app_desktop_class;
              app_desktop_class = bamf_application_get_wmclass (app);

              if (target_class && app_desktop_class && strcasecmp (target_class, app_desktop_class) == 0)
                {
                  best = app;
                  break;
                }
              else if (!best)
                {
                  best = app;
                }
            }
        }

      /* If a "best" application has been found, we should check again if the
       * desktop file that is going to be used is really the best one we have.
       * To do this, we compare the window class name with the desktop class
       * of both candidates to ensure that really is the best one.
       * This is important to avoid that very-similar (which differ only by
       * StartupWMClass) running desktop files, would be wrongly used to match
       * an incompatible window. */
      if (BAMF_IS_APPLICATION (best) && possible_apps)
        {
          const gchar *best_app_desktop = bamf_application_get_desktop_file (best);
          const gchar *best_desktop = possible_apps->data;

          if (win_class_name && g_strcmp0 (best_app_desktop, best_desktop) != 0)
            {
              const gchar *best_app_class;
              const gchar *best_desktop_class;

              best_app_class = bamf_application_get_wmclass (best);
              best_desktop_class = bamf_matcher_get_desktop_file_class (self, best_desktop);

              /* We compare the two classes using their "distance" from the
               * desidered class value */
              if (best_app_class && best_desktop_class)
                {
                  int max_chars = strlen (win_class_name);
                  int app_diff = strncasecmp (win_class_name, best_app_class, max_chars);
                  int desktop_diff = strncasecmp (win_class_name, best_desktop_class, max_chars);

                  if (abs (desktop_diff) < abs (app_diff))
                    {
                      best = bamf_matcher_get_application_by_desktop_file (self, best_desktop);
                      app_desktop = best_desktop;
                    }
                }
            }
        }
    }
  else
    {
      /* secondary matching */
      GList *a;
      BamfView *view;

      const gchar *app_desktop_class;

      const char *win_instance_name = bamf_legacy_window_get_class_instance_name (window);
      const char *exec_string = bamf_legacy_window_get_exec_string (window);
      char *trimmed_exec = bamf_matcher_get_trimmed_exec (self, exec_string);

      if (trimmed_exec || win_class_name || win_instance_name)
        {
          for (a = self->priv->views; a; a = a->next)
            {
              view = a->data;

              if (!BAMF_IS_APPLICATION (view))
                continue;

              app = BAMF_APPLICATION (view);

              if (bamf_application_contains_similar_to_window (app, bamf_window))
                {
                  GList *ll;
                  gboolean found_exec = FALSE;
                  for (ll = bamf_view_get_children (BAMF_VIEW (app)); ll && !found_exec; ll = ll->next)
                    {
                      if (!BAMF_IS_WINDOW (ll->data))
                        continue;

                      BamfLegacyWindow *w = bamf_window_get_window (BAMF_WINDOW (ll->data));
                      const char *wexec = bamf_legacy_window_get_exec_string (w);
                      char *wtrimmed = bamf_matcher_get_trimmed_exec (self, wexec);

                      if (g_strcmp0 (trimmed_exec, wtrimmed) == 0)
                        {
                          best = BAMF_APPLICATION (view);
                          found_exec = TRUE;
                        }

                      g_free (wtrimmed);
                    }

                  if (!found_exec)
                    continue;

                  app_desktop_class = bamf_application_get_wmclass (app);

                  if (target_class && g_strcmp0 (target_class, app_desktop_class) == 0)
                    {
                      best = app;
                      break;
                    }
                  else if (!best)
                    {
                      best = app;
                    }
                }
            }
        }

      g_free (trimmed_exec);
    }

  if (!best)
    {
      if (app_desktop)
        {
          best = bamf_application_new_from_desktop_file (app_desktop);
        }
      else if (possible_apps)
        {
          best = bamf_application_new_from_desktop_files (possible_apps);
        }
      else
        {
          best = bamf_application_new ();
        }

      bamf_application_set_wmclass (best, app_class);
    }

  g_list_free_full (possible_apps, g_free);

  return best;
}

/* Ensures that the window hint is set if a registered pid matches, and that set window hints
   are already known to bamfdaemon */
static void
ensure_window_hint_set (BamfMatcher *self,
                        BamfLegacyWindow *window)
{
  GList *pids, *l;
  GHashTable *registered_pids;
  char *desktop_file_hint = NULL;
  gpointer key;
  guint pid;

  g_return_if_fail (BAMF_IS_MATCHER (self));
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW (window));

  registered_pids = self->priv->registered_pids;

  /* Some windows such as web applications shares the pid with their parent
   * browser so, we have to ignore them */
  if (bamf_matcher_window_skips_hint_set (self, window))
    {
      return;
    }

  desktop_file_hint = bamf_legacy_window_get_hint (window, _NET_WM_DESKTOP_FILE);

  if (desktop_file_hint)
    {
      /* already set, make sure we know about this
       * fact for future windows of this applications */
      pid = bamf_legacy_window_get_pid (window);

      if (pid > 0)
        {
          key = GUINT_TO_POINTER (pid);

          if (!g_hash_table_lookup (registered_pids, key))
            {
              g_hash_table_insert (registered_pids, key, g_strdup (desktop_file_hint));
            }
        }

      g_free (desktop_file_hint);
      return;
    }

  pids = pid_parent_tree (self, bamf_legacy_window_get_pid (window));

  for (l = pids; l; l = l->next)
    {
      key = l->data;
      desktop_file_hint = g_hash_table_lookup (registered_pids, key);

      if (desktop_file_hint != NULL && desktop_file_hint[0] != '\0')
        break;
    }

  g_list_free (pids);

  if (desktop_file_hint)
    bamf_legacy_window_set_hint (window, _NET_WM_DESKTOP_FILE, desktop_file_hint);
}

static void
on_raw_window_class_changed (BamfLegacyWindow *window, BamfMatcher* self)
{
  BamfWindow *bamf_win;
  BamfApplication *old_app, *new_app;

  g_return_if_fail (BAMF_IS_MATCHER (self));
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW (window));

  guint xid = bamf_legacy_window_get_xid (window);
  old_app = bamf_matcher_get_application_by_xid (self, xid);

  if (!old_app)
    return;

  bamf_win = bamf_application_get_window (old_app, xid);

  if (bamf_win)
    {
      new_app = bamf_matcher_get_application_for_window (self, bamf_win);

      if (new_app)
        {
          if (old_app != new_app)
            {
              g_object_ref (bamf_win);
              bamf_view_remove_child (BAMF_VIEW (old_app), BAMF_VIEW (bamf_win));

              if (!bamf_matcher_is_view_registered (self, BAMF_VIEW (new_app)))
                {
                  bamf_matcher_register_view_stealing_ref (self, BAMF_VIEW (new_app));
                }

              bamf_view_add_child (BAMF_VIEW (new_app), BAMF_VIEW (bamf_win));
              g_object_unref (bamf_win);
            }
          else
            {
              if (!bamf_matcher_is_view_registered (self, BAMF_VIEW (new_app)))
                {
                  /* This is probably never happening, but just in case... */
                  g_object_unref (new_app);
                }
            }
        }
    }
}

static void
on_raw_window_closed (BamfLegacyWindow *window, BamfMatcher* self)
{
  g_signal_handlers_disconnect_by_data (window, self);
}

static void
handle_raw_window (BamfMatcher *self, BamfLegacyWindow *window)
{
  BamfWindow *bamf_win;
  BamfApplication *bamf_app;

  g_return_if_fail (BAMF_IS_MATCHER (self));
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW (window));

  g_signal_connect (window, "class-changed", G_CALLBACK (on_raw_window_class_changed), self);
  g_signal_connect (window, "closed", G_CALLBACK (on_raw_window_closed), self);

  guint pid = bamf_legacy_window_get_pid (window);
  if (pid > 1 && !g_list_find (self->priv->known_pids, GUINT_TO_POINTER (pid)))
    self->priv->known_pids = g_list_prepend (self->priv->known_pids, GUINT_TO_POINTER (pid));

  ensure_window_hint_set (self, window);

  /* We need to make our objects for bus export, the quickest way to do this, though not
   * always the best is to simply go window by window creating new applications as needed.
   */

  bamf_win = bamf_window_new (window);
  bamf_matcher_register_view_stealing_ref (self, BAMF_VIEW (bamf_win));

  bamf_app = bamf_matcher_get_application_for_window (self, bamf_win);

  if (!bamf_matcher_is_view_registered (self, BAMF_VIEW (bamf_app)))
    {
      bamf_matcher_register_view_stealing_ref (self, BAMF_VIEW (bamf_app));
    }

  bamf_view_add_child (BAMF_VIEW (bamf_app), BAMF_VIEW (bamf_win));
}

static char *
get_gnome_control_center_window_hint (BamfMatcher * self, BamfLegacyWindow * window)
{
  const gchar *role;
  GList *list;

  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);
  g_return_val_if_fail (BAMF_IS_LEGACY_WINDOW (window), NULL);

  role = bamf_legacy_window_get_role (window);

  if (role)
    {
      gchar *exec = g_strconcat ("gnome-control-center ", role, NULL);
      list = g_hash_table_lookup (self->priv->desktop_file_table, exec);
      g_free (exec);
    }

  if (!role || !list)
    {
      list = g_hash_table_lookup (self->priv->desktop_id_table, "gnome-control-center");
    }

  return (list ? (char *) list->data : NULL);
}

static void
on_gnome_control_center_window_role_changed (BamfLegacyWindow *window, BamfMatcher* self)
{
  g_return_if_fail (BAMF_IS_MATCHER (self));
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW (window));

  char *old_hint;
  const char *new_hint;

  old_hint = bamf_legacy_window_get_hint (window, _NET_WM_DESKTOP_FILE);
  new_hint = get_gnome_control_center_window_hint (self, window);

  if (new_hint && g_strcmp0 (new_hint, old_hint) != 0)
    {
      bamf_legacy_window_reopen (window);
    }

  g_free (old_hint);
}

static void
handle_window_opened (BamfLegacyScreen * screen, BamfLegacyWindow * window, BamfMatcher *self)
{
  g_return_if_fail (BAMF_IS_MATCHER (self));
  g_return_if_fail (BAMF_IS_LEGACY_WINDOW (window));
  BamfWindowType win_type = bamf_legacy_window_get_window_type (window);

  if (win_type == BAMF_WINDOW_DESKTOP)
    {
      BamfWindow *bamfwindow = bamf_window_new (window);
      bamf_matcher_register_view_stealing_ref (self, BAMF_VIEW (bamfwindow));

      return;
    }

  if (is_libreoffice_window (window))
    {
      if (win_type == BAMF_WINDOW_SPLASHSCREEN || win_type == BAMF_WINDOW_TOOLBAR)
        {
          return;
        }
    }
  else if (g_strcmp0 (bamf_legacy_window_get_class_name (window), "Gnome-control-center") == 0)
    {
      char *old_hint = bamf_legacy_window_get_hint (window, _NET_WM_DESKTOP_FILE);
      const char *new_hint = get_gnome_control_center_window_hint (self, window);

      if (new_hint && g_strcmp0 (old_hint, new_hint) != 0)
        {
          bamf_legacy_window_set_hint (window, _NET_WM_DESKTOP_FILE, new_hint);
        }

      g_signal_connect (window, "role-changed", (GCallback) on_gnome_control_center_window_role_changed, self);
      g_free (old_hint);
    }

  /* we have a window who is ready to be matched */
  handle_raw_window (self, window);
}

static void
handle_stacking_changed (BamfLegacyScreen * screen, BamfMatcher *self)
{
  g_signal_emit_by_name (self, "stacking-order-changed");
}

void
bamf_matcher_load_desktop_file (BamfMatcher * self,
                                const char * desktop_file)
{
  GList *vl, *wl;

  g_return_if_fail (BAMF_IS_MATCHER (self));

  if (is_autostart_desktop_file (desktop_file))
    return;

  load_desktop_file_to_table (self,
                              desktop_file,
                              self->priv->desktop_file_table,
                              self->priv->desktop_id_table,
                              self->priv->desktop_class_table);

  /* If an application with no .desktop file has windows that matches
   * the new added .desktop file, then we try to re-match them.
   * We use another list to save the windows that should be re-matched to avoid
   * that the list that we're iterating is changed, since reopening a window
   * makes it to be removed from the views. */
  GList *to_rematch = NULL;

  for (vl = self->priv->views; vl; vl = vl->next)
    {
      if (!BAMF_IS_APPLICATION (vl->data))
        continue;

      BamfApplication *app = BAMF_APPLICATION (vl->data);

      if (!bamf_application_get_desktop_file (app))
        {
          GList *children = bamf_view_get_children (BAMF_VIEW (app));

          for (wl = children; wl; wl = wl->next)
            {
              if (!BAMF_IS_WINDOW (wl->data))
                continue;

              BamfWindow *win = BAMF_WINDOW (wl->data);
              GList *desktops = bamf_matcher_possible_applications_for_window (self, win, NULL);

              if (g_list_find_custom (desktops, desktop_file, (GCompareFunc) g_strcmp0))
                {
                  BamfLegacyWindow *legacy_window = bamf_window_get_window (win);
                  to_rematch = g_list_prepend (to_rematch, legacy_window);
                }

              g_list_free_full (desktops, g_free);
            }
        }
    }

  for (wl = to_rematch; wl; wl = wl->next)
    {
      BamfLegacyWindow *legacy_window = BAMF_LEGACY_WINDOW (wl->data);
      bamf_legacy_window_reopen (legacy_window);
    }

  g_list_free (to_rematch);
}

gboolean
is_autostart_desktop_file (const gchar *desktop_file)
{
  gchar *data_dir;
  gchar *dirname;
  gboolean autostart;
  gint i;

  g_return_val_if_fail (desktop_file, FALSE);

  autostart = FALSE;
  dirname = g_path_get_dirname (desktop_file);
  data_dir = g_build_filename (g_get_user_config_dir (), "autostart", NULL);

  if (g_strcmp0 (dirname, data_dir) == 0)
    {
      autostart = TRUE;
    }

  g_free (data_dir);

  if (!autostart)
    {
      const gchar * const * data_dirs = g_get_system_config_dirs ();

      for (i = 0; data_dirs[i]; ++i)
        {
          data_dir = g_build_filename (data_dirs[i], "autostart", NULL);

          if (g_strcmp0 (dirname, data_dir) == 0)
            {
              autostart = TRUE;
              g_free (data_dir);
              break;
            }

          g_free (data_dir);
        }
    }

  g_free (dirname);

  return autostart;
}

void
bamf_matcher_register_desktop_file_for_pid (BamfMatcher * self,
                                            const gchar * desktop_file,
                                            guint64 pid)
{
  gpointer key;
  BamfLegacyScreen *screen;
  GList *windows, *l;

  g_return_if_fail (BAMF_IS_MATCHER (self));
  g_return_if_fail (desktop_file);

  if (is_autostart_desktop_file (desktop_file))
    return;

  key = GUINT_TO_POINTER (pid);
  g_hash_table_insert (self->priv->registered_pids, key, g_strdup (desktop_file));

  /* fixme, this is a bit heavy */

  screen = bamf_legacy_screen_get_default ();

  g_return_if_fail (BAMF_IS_LEGACY_SCREEN (screen));

  windows = bamf_legacy_screen_get_windows (screen);

  for (l = windows; l; l = l->next)
    {
      ensure_window_hint_set (self, l->data);
    }
}

const char *
bamf_matcher_get_desktop_file_class (BamfMatcher * self, const char * desktop_file)
{
  g_return_val_if_fail (BAMF_IS_MATCHER (self), NULL);
  g_return_val_if_fail (desktop_file, NULL);

  return g_hash_table_lookup (self->priv->desktop_class_table, desktop_file);
}

static int
x_error_handler (Display *display, XErrorEvent *event)
{
  /* We hardly care, this means one of our get or set property calls didn't work
   * while this kind of sucks, it wont effect the running of the program except to
   * perhaps reduce the efficiency of the application
   */
  return 0;
}

const char *
bamf_matcher_get_active_application (BamfMatcher *matcher)
{
  GList *l;
  BamfView *view;
  BamfMatcherPrivate *priv;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  priv = matcher->priv;

  for (l = priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_APPLICATION (view))
        continue;

      if (bamf_view_is_active (view))
        {
          return bamf_view_get_path (view);
        }
    }

  return "";
}

const char *
bamf_matcher_get_active_window (BamfMatcher *matcher)
{
  GList *l;
  BamfView *view;
  BamfMatcherPrivate *priv;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  priv = matcher->priv;

  for (l = priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_WINDOW (view))
        continue;

      if (bamf_view_is_active (view))
        {
          return bamf_view_get_path (view);
        }
    }

  return "";
}

const char *
bamf_matcher_application_for_xid (BamfMatcher *matcher,
                                  guint32 xid)
{
  BamfApplication *app;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  app = bamf_matcher_get_application_by_xid (matcher, xid);

  if (BAMF_IS_APPLICATION (app))
    return bamf_view_get_path (BAMF_VIEW (app));

  return "";
}

static gint
compare_windows_by_stack_order (gconstpointer a, gconstpointer b)
{
  g_return_val_if_fail (BAMF_IS_WINDOW (a), -1);
  g_return_val_if_fail (BAMF_IS_WINDOW (b), 1);

  gint idx_a = bamf_window_get_stack_position (BAMF_WINDOW (a));
  gint idx_b = bamf_window_get_stack_position (BAMF_WINDOW (b));

  return (idx_a < idx_b) ? -1 : 1;
}

GVariant *
bamf_matcher_get_window_stack_for_monitor (BamfMatcher *matcher, gint monitor)
{
  GList *l;
  GList *windows;
  BamfView *view;
  GVariantBuilder b;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  windows = NULL;
  for (l = matcher->priv->views; l; l = l->next)
    {
      if (BAMF_IS_WINDOW (l->data))
        {
          windows = g_list_insert_sorted (windows, l->data,
                                          compare_windows_by_stack_order);
        }
    }

  g_variant_builder_init (&b, G_VARIANT_TYPE ("(as)"));
  g_variant_builder_open (&b, G_VARIANT_TYPE ("as"));

  for (l = windows; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_WINDOW (view))
        continue;

      if ((monitor >= 0 && bamf_window_get_monitor (BAMF_WINDOW (view)) == monitor) ||
          monitor < 0)
      {
        g_variant_builder_add (&b, "s", bamf_view_get_path (view));
      }
    }

  g_list_free (windows);
  g_variant_builder_close (&b);

  return g_variant_builder_end (&b);
}

gboolean
bamf_matcher_application_is_running (BamfMatcher *matcher,
                                     const char *application)
{
  BamfApplication *app;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), FALSE);

  app = bamf_matcher_get_application_by_desktop_file (matcher, application);

  if (BAMF_IS_APPLICATION (app))
    {
      return bamf_view_is_running (BAMF_VIEW (app));
    }

  return FALSE;
}

GVariant *
bamf_matcher_window_dbus_paths (BamfMatcher *matcher)
{
  GList *l;
  BamfView *view;
  BamfMatcherPrivate *priv;
  GVariantBuilder b;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  g_variant_builder_init (&b, G_VARIANT_TYPE ("(as)"));
  g_variant_builder_open (&b, G_VARIANT_TYPE ("as"));

  priv = matcher->priv;

  for (l = priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_WINDOW (view))
        continue;

      g_variant_builder_add (&b, "s", bamf_view_get_path (view));
    }

  g_variant_builder_close (&b);

  return g_variant_builder_end (&b);
}

GVariant *
bamf_matcher_application_dbus_paths (BamfMatcher *matcher)
{
  GList *l;
  BamfView *view;
  BamfMatcherPrivate *priv;
  GVariantBuilder b;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  g_variant_builder_init (&b, G_VARIANT_TYPE ("(as)"));
  g_variant_builder_open (&b, G_VARIANT_TYPE ("as"));

  priv = matcher->priv;

  for (l = priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_APPLICATION (view))
        continue;

      g_variant_builder_add (&b, "s", bamf_view_get_path (view));
    }

  g_variant_builder_close (&b);

  return g_variant_builder_end (&b);
}

const char *
bamf_matcher_dbus_path_for_application (BamfMatcher *matcher,
                                        const char *application)
{
  const char * path = "";
  BamfApplication *app;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  app = bamf_matcher_get_application_by_desktop_file (matcher, application);

  if (BAMF_IS_APPLICATION (app))
    {
      return bamf_view_get_path (BAMF_VIEW (app));
    }

  return path;
}

GList *
bamf_matcher_get_favorites (BamfMatcher *matcher)
{
  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  return matcher->priv->favorites;
}

void
bamf_matcher_register_favorites (BamfMatcher *matcher,
                                 const char **favorites)
{
  const char *fav;
  const char **favs;
  BamfMatcherPrivate *priv;

  g_return_if_fail (BAMF_IS_MATCHER (matcher));
  g_return_if_fail (favorites);
  priv = matcher->priv;

  for (favs = favorites; *favs; favs++)
    {
      fav = *favs;
      /* ignore things already in the list */
      if (g_list_find_custom (priv->favorites, fav, (GCompareFunc) g_strcmp0))
        continue;

      bamf_matcher_load_desktop_file (matcher, fav);
      priv->favorites = g_list_prepend (priv->favorites, g_strdup (fav));
    }

  g_signal_emit (matcher, matcher_signals[FAVORITES_CHANGED], 0);
}

GVariant *
bamf_matcher_running_application_paths (BamfMatcher *matcher)
{
  GList *l;
  BamfView *view;
  BamfMatcherPrivate *priv;
  GVariantBuilder b;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  g_variant_builder_init (&b, G_VARIANT_TYPE ("(as)"));
  g_variant_builder_open (&b, G_VARIANT_TYPE ("as"));

  priv = matcher->priv;

  for (l = priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_APPLICATION (view) || !bamf_view_is_running (view))
        continue;

      g_variant_builder_add (&b, "s", bamf_view_get_path (view));
    }

  g_variant_builder_close (&b);

  return g_variant_builder_end (&b);
}

GVariant *
bamf_matcher_running_applications_desktop_files (BamfMatcher *matcher)
{
  GList *l;
  BamfView *view;
  BamfMatcherPrivate *priv;
  GSequence *paths;
  GSequenceIter *iter;
  const char *desktop_file;
  GVariantBuilder b;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  g_variant_builder_init (&b, G_VARIANT_TYPE ("(as)"));
  g_variant_builder_open (&b, G_VARIANT_TYPE ("as"));

  paths = g_sequence_new (NULL);

  priv = matcher->priv;

  for (l = priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_APPLICATION (view) || !bamf_view_is_running (view))
        continue;

      desktop_file = bamf_application_get_desktop_file (BAMF_APPLICATION (view));
      if (!desktop_file) continue;

      if (g_sequence_lookup (paths, (gpointer) desktop_file,
                             (GCompareDataFunc) g_strcmp0, NULL) == NULL)
        {
          g_sequence_insert_sorted (paths, (gpointer) desktop_file,
                                    (GCompareDataFunc) g_strcmp0, NULL);
        }
    }

  iter = g_sequence_get_begin_iter (paths);
  while (!g_sequence_iter_is_end (iter))
    {
      g_variant_builder_add (&b, "s", g_sequence_get (iter));

      iter = g_sequence_iter_next (iter);
    }

  g_sequence_free (paths);

  g_variant_builder_close (&b);

  return g_variant_builder_end (&b);
}

GVariant *
bamf_matcher_tab_dbus_paths (BamfMatcher *matcher)
{
  GList *l;
  BamfView *view;
  BamfMatcherPrivate *priv;
  GVariantBuilder b;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  g_variant_builder_init (&b, G_VARIANT_TYPE ("(as)"));
  g_variant_builder_open (&b, G_VARIANT_TYPE ("as"));

  priv = matcher->priv;

  for (l = priv->views; l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_TAB (view))
        continue;

      g_variant_builder_add (&b, "s", bamf_view_get_path (view));
    }

  g_variant_builder_close (&b);

  return g_variant_builder_end (&b);
}

GVariant *
bamf_matcher_xids_for_application (BamfMatcher *matcher,
                                   const char *application)
{
  GVariantBuilder b;
  GVariant *xids;
  BamfApplication *app;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);

  xids = NULL;
  app = bamf_matcher_get_application_by_desktop_file (matcher, application);

  if (BAMF_IS_APPLICATION (app))
    {
      xids = bamf_application_get_xids (app);
    }

  if (!xids)
    {
      g_variant_builder_init (&b, G_VARIANT_TYPE ("(au)"));
      g_variant_builder_open (&b, G_VARIANT_TYPE ("au"));
      g_variant_builder_close (&b);
      xids = g_variant_builder_end (&b);
    }

  return xids;
}

static gboolean
on_dbus_handle_xids_for_application (BamfDBusMatcher *interface,
                                     GDBusMethodInvocation *invocation,
                                     const gchar *application,
                                     BamfMatcher *self)
{
  GVariant *xids = bamf_matcher_xids_for_application (self, application);
  g_dbus_method_invocation_return_value (invocation, xids);

  return TRUE;
}

static gboolean
on_dbus_handle_tab_paths (BamfDBusMatcher *interface,
                          GDBusMethodInvocation *invocation,
                          BamfMatcher *self)
{
  GVariant *tab_paths = bamf_matcher_tab_dbus_paths (self);
  g_dbus_method_invocation_return_value (invocation, tab_paths);

  return TRUE;
}

static gboolean
on_dbus_handle_application_paths (BamfDBusMatcher *interface,
                                  GDBusMethodInvocation *invocation,
                                  BamfMatcher *self)
{
  GVariant *app_paths = bamf_matcher_application_dbus_paths (self);
  g_dbus_method_invocation_return_value (invocation, app_paths);

  return TRUE;
}


static gboolean
on_dbus_handle_window_paths (BamfDBusMatcher *interface,
                             GDBusMethodInvocation *invocation,
                             BamfMatcher *self)
{
  GVariant *win_paths = bamf_matcher_window_dbus_paths (self);
  g_dbus_method_invocation_return_value (invocation, win_paths);

  return TRUE;
}

static gboolean
on_dbus_handle_running_applications (BamfDBusMatcher *interface,
                                     GDBusMethodInvocation *invocation,
                                     BamfMatcher *self)
{
  GVariant *running_apps = bamf_matcher_running_application_paths (self);
  g_dbus_method_invocation_return_value (invocation, running_apps);

  return TRUE;
}

static gboolean
on_dbus_handle_running_applications_desktop_files (BamfDBusMatcher *interface,
                                                   GDBusMethodInvocation *invocation,
                                                   BamfMatcher *self)
{
  GVariant *paths = bamf_matcher_running_applications_desktop_files (self);
  g_dbus_method_invocation_return_value (invocation, paths);

  return TRUE;
}

static gboolean
on_dbus_handle_active_application (BamfDBusMatcher *interface,
                                   GDBusMethodInvocation *invocation,
                                   BamfMatcher *self)
{
  const gchar *active_app = bamf_matcher_get_active_application (self);

  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s)", active_app));
  return TRUE;
}

static gboolean
on_dbus_handle_active_window (BamfDBusMatcher *interface,
                              GDBusMethodInvocation *invocation,
                              BamfMatcher *self)
{
  const gchar *active_win = bamf_matcher_get_active_window (self);

  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s)", active_win));
  return TRUE;
}

static gboolean
on_dbus_handle_application_is_running (BamfDBusMatcher *interface,
                                       GDBusMethodInvocation *invocation,
                                       const gchar *application,
                                       BamfMatcher *self)
{
  gboolean is_running = bamf_matcher_application_is_running (self, application);

  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(b)", is_running));
  return TRUE;
}

static gboolean
on_dbus_handle_register_favorites (BamfDBusMatcher *interface,
                                   GDBusMethodInvocation *invocation,
                                   const char **favorites,
                                   BamfMatcher *self)
{
  g_dbus_method_invocation_return_value (invocation, NULL);
  bamf_matcher_register_favorites (self, favorites);

  return TRUE;
}

static gboolean
on_dbus_handle_path_for_application (BamfDBusMatcher *interface,
                                     GDBusMethodInvocation *invocation,
                                     const gchar *application,
                                     BamfMatcher *self)
{
  const gchar *app_path = bamf_matcher_dbus_path_for_application (self, application);

  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s)", app_path));

  return TRUE;
}

static gboolean
on_dbus_handle_application_for_xid (BamfDBusMatcher *interface,
                                    GDBusMethodInvocation *invocation,
                                    guint xid,
                                    BamfMatcher *self)
{
  const gchar *app_path = bamf_matcher_application_for_xid (self, xid);

  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s)", app_path));

  return TRUE;
}

static gboolean
on_dbus_handle_window_stack_for_monitor (BamfDBusMatcher *interface,
                                         GDBusMethodInvocation *invocation,
                                         gint monitor,
                                         BamfMatcher *self)
{
  GVariant *windows = bamf_matcher_get_window_stack_for_monitor (self, monitor);

  g_dbus_method_invocation_return_value (invocation, windows);

  return TRUE;
}

static void
bamf_matcher_init (BamfMatcher * self)
{
  BamfMatcherPrivate *priv;
  BamfLegacyScreen *screen;
  int i;

  priv = self->priv = BAMF_MATCHER_GET_PRIVATE (self);

  priv->bad_prefixes = g_array_sized_new (FALSE, TRUE, sizeof (GRegex *),
                                          G_N_ELEMENTS (EXEC_BAD_PREFIXES));
  priv->good_prefixes = g_array_sized_new (FALSE, TRUE, sizeof (GRegex *),
                                           G_N_ELEMENTS (EXEC_GOOD_PREFIXES));
  priv->registered_pids = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                 NULL, g_free);

  for (i = 0; i < G_N_ELEMENTS (EXEC_BAD_PREFIXES); ++i)
    {
      GRegex *regex = g_regex_new (EXEC_BAD_PREFIXES[i], G_REGEX_OPTIMIZE, 0, NULL);
      g_array_append_val (priv->bad_prefixes, regex);
    }

  for (i = 0; i < G_N_ELEMENTS (EXEC_GOOD_PREFIXES); ++i)
    {
      GRegex *regex = g_regex_new (EXEC_GOOD_PREFIXES[i], G_REGEX_OPTIMIZE, 0, NULL);
      g_array_append_val (priv->good_prefixes, regex);
    }

  create_desktop_file_table (self, &(priv->desktop_file_table),
                             &(priv->desktop_id_table),
                             &(priv->desktop_class_table));

  screen = bamf_legacy_screen_get_default ();
  g_signal_connect (G_OBJECT (screen), BAMF_LEGACY_SCREEN_SIGNAL_WINDOW_OPENED,
                    (GCallback) handle_window_opened, self);

  g_signal_connect (G_OBJECT (screen), BAMF_LEGACY_SCREEN_SIGNAL_STACKING_CHANGED,
                    (GCallback) handle_stacking_changed, self);

  XSetErrorHandler (x_error_handler);

  /* Registering signal callbacks to reply to dbus method calls */
  g_signal_connect (self, "handle-xids-for-application",
                    G_CALLBACK (on_dbus_handle_xids_for_application), self);

  g_signal_connect (self, "handle-tab-paths",
                    G_CALLBACK (on_dbus_handle_tab_paths), self);

  g_signal_connect (self, "handle-application-paths",
                    G_CALLBACK (on_dbus_handle_application_paths), self);

  g_signal_connect (self, "handle-window-paths",
                    G_CALLBACK (on_dbus_handle_window_paths), self);

  g_signal_connect (self, "handle-running-applications",
                    G_CALLBACK (on_dbus_handle_running_applications), self);

  g_signal_connect (self, "handle-running-applications-desktop-files",
                    G_CALLBACK (on_dbus_handle_running_applications_desktop_files), self);

  g_signal_connect (self, "handle-active-window",
                    G_CALLBACK (on_dbus_handle_active_window), self);

  g_signal_connect (self, "handle-active-application",
                    G_CALLBACK (on_dbus_handle_active_application), self);

  g_signal_connect (self, "handle-application-is-running",
                    G_CALLBACK (on_dbus_handle_application_is_running), self);

  g_signal_connect (self, "handle-register-favorites",
                    G_CALLBACK (on_dbus_handle_register_favorites), self);

  g_signal_connect (self, "handle-path-for-application",
                    G_CALLBACK (on_dbus_handle_path_for_application), self);

  g_signal_connect (self, "handle-application-for-xid",
                    G_CALLBACK (on_dbus_handle_application_for_xid), self);

  g_signal_connect (self, "handle-window-stack-for-monitor",
                    G_CALLBACK (on_dbus_handle_window_stack_for_monitor), self);
}

static void
bamf_matcher_dispose (GObject *object)
{
  BamfMatcher *self = (BamfMatcher *) object;
  BamfMatcherPrivate *priv = self->priv;

  while (priv->views)
    {
      bamf_matcher_unregister_view (self, priv->views->data);
    }

  G_OBJECT_CLASS (bamf_matcher_parent_class)->dispose (object);
}

static void
bamf_matcher_finalize (GObject *object)
{
  BamfMatcher *self = (BamfMatcher *) object;
  BamfMatcherPrivate *priv = self->priv;
  BamfLegacyScreen *screen = bamf_legacy_screen_get_default ();
  GList *l;
  int i;

  for (i = 0; i < priv->bad_prefixes->len; i++)
    {
      GRegex *regex = g_array_index (priv->bad_prefixes, GRegex *, i);
      g_regex_unref (regex);
    }

  for (i = 0; i < priv->good_prefixes->len; i++)
    {
      GRegex *regex = g_array_index (priv->good_prefixes, GRegex *, i);
      g_regex_unref (regex);
    }

  g_array_free (priv->bad_prefixes, TRUE);
  g_array_free (priv->good_prefixes, TRUE);
  g_hash_table_destroy (priv->desktop_id_table);
  g_hash_table_destroy (priv->desktop_file_table);
  g_hash_table_destroy (priv->desktop_class_table);
  g_hash_table_destroy (priv->registered_pids);
  g_list_free (priv->no_display_desktop);

  if (priv->opened_closed_paths_table)
    {
      g_hash_table_destroy (priv->opened_closed_paths_table);
    }

  if (priv->dispatch_changes_id != 0)
    {
      g_source_remove (priv->dispatch_changes_id);
      priv->dispatch_changes_id = 0;
    }

  g_list_free (priv->known_pids);
  g_list_free_full (priv->views, g_object_unref);

  g_signal_handlers_disconnect_by_data (screen, self);

  for (l = priv->monitors; l; l = l->next)
    g_signal_handlers_disconnect_by_data (G_OBJECT (l->data), self);

  g_list_free_full (priv->monitors, g_object_unref);

  g_list_free_full (priv->favorites, g_free);

  priv->active_app = NULL;
  priv->active_win = NULL;

  static_matcher = NULL;

  G_OBJECT_CLASS (bamf_matcher_parent_class)->finalize (object);
}

static void
bamf_matcher_class_init (BamfMatcherClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (BamfMatcherPrivate));
  object_class->dispose = bamf_matcher_dispose;
  object_class->finalize = bamf_matcher_finalize;

  matcher_signals [FAVORITES_CHANGED] =
    g_signal_new ("favorites-changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  0, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

BamfMatcher *
bamf_matcher_get_default (void)
{
  if (!BAMF_IS_MATCHER (static_matcher))
    {
      static_matcher = g_object_new (BAMF_TYPE_MATCHER, NULL);
    }

  return static_matcher;
}
