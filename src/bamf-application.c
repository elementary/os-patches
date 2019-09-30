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

#include "bamf-application.h"
#include "bamf-window.h"
#include "bamf-matcher.h"
#include "bamf-legacy-window.h"
#include "bamf-legacy-screen.h"
#include "bamf-tab.h"
#include <string.h>
#include <gio/gdesktopappinfo.h>

#define BAMF_APPLICATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE(obj, \
BAMF_TYPE_APPLICATION, BamfApplicationPrivate))

static void bamf_application_dbus_application_iface_init (BamfDBusItemApplicationIface *iface);
G_DEFINE_TYPE_WITH_CODE (BamfApplication, bamf_application, BAMF_TYPE_VIEW,
                         G_IMPLEMENT_INTERFACE (BAMF_DBUS_ITEM_TYPE_APPLICATION,
                                                bamf_application_dbus_application_iface_init));

struct _BamfApplicationPrivate
{
  BamfDBusItemApplication *dbus_iface;
  BamfApplicationType app_type;
  BamfView * main_child;
  GCancellable * cancellable;
  char * desktop_file;
  GList * desktop_file_list;
  char * wmclass;
  char ** mimes;
  gboolean show_stubs;
};

enum
{
  SUPPORTED_MIMES_CHANGED,
  LAST_SIGNAL
};

static guint application_signals[LAST_SIGNAL] = { 0 };

#define STUB_KEY  "X-Ayatana-Appmenu-Show-Stubs"

static void on_main_child_name_changed (BamfView *, const gchar *, const gchar *, BamfApplication *);

void
bamf_application_supported_mime_types_changed (BamfApplication *application,
                                               const gchar **new_mimes)
{
  gchar **mimes = (gchar **) new_mimes;

  if (!new_mimes)
    {
      gchar *empty[] = {NULL};
      mimes = g_strdupv (empty);
    }

  g_signal_emit_by_name (application->priv->dbus_iface, "supported-mime-types-changed", mimes);

  if (!new_mimes)
  {
    g_strfreev (mimes);
    mimes = NULL;
  }

  if (application->priv->mimes)
    g_strfreev (application->priv->mimes);

  application->priv->mimes = mimes;
}

static gboolean
bamf_application_default_get_close_when_empty (BamfApplication *application)
{
  return TRUE;
}

static gchar **
bamf_application_default_get_supported_mime_types (BamfApplication *application)
{
  const char *desktop_file;
  char** mimes;

  desktop_file = bamf_application_get_desktop_file (application);

  if (!desktop_file)
    return NULL;

  GKeyFile* key_file = g_key_file_new ();

  if (!g_key_file_load_from_file (key_file, desktop_file, G_KEY_FILE_NONE, NULL))
    {
      g_key_file_free (key_file);
      return NULL;
    }

  mimes = g_key_file_get_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_MIME_TYPE, NULL, NULL);

  g_signal_emit (application, application_signals[SUPPORTED_MIMES_CHANGED], 0, mimes);

  g_key_file_free (key_file);

  return mimes;
}

char **
bamf_application_get_supported_mime_types (BamfApplication *application)
{
  gchar **mimes = NULL;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), NULL);

  if (application->priv->mimes)
    return g_strdupv (application->priv->mimes);

  if (BAMF_APPLICATION_GET_CLASS (application)->get_supported_mime_types)
    mimes = BAMF_APPLICATION_GET_CLASS (application)->get_supported_mime_types (application);

  application->priv->mimes = mimes;

  return g_strdupv (mimes);
}

BamfApplicationType
bamf_application_get_application_type (BamfApplication *application)
{
  g_return_val_if_fail (BAMF_IS_APPLICATION (application), BAMF_APPLICATION_UNKNOWN);

  return application->priv->app_type;
}

void
bamf_application_set_application_type (BamfApplication *application, BamfApplicationType type)
{
  g_return_if_fail (BAMF_IS_APPLICATION (application));
  g_return_if_fail (type >= 0 && type < BAMF_APPLICATION_UNKNOWN);

  application->priv->app_type = type;
}

const char *
bamf_application_get_desktop_file (BamfApplication *application)
{
  BamfApplicationPrivate *priv;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), NULL);
  priv = application->priv;

  return priv->desktop_file;
}

const char *
bamf_application_get_wmclass (BamfApplication *application)
{
  BamfApplicationPrivate *priv;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), NULL);
  priv = application->priv;

  return priv->wmclass;
}

static gboolean
icon_name_is_valid (const char *name)
{
  GtkIconTheme *icon_theme;

  if (!name || name[0] == '\0')
    return FALSE;

  if (g_file_test (name, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
    return TRUE;

  icon_theme = gtk_icon_theme_get_default ();
  return gtk_icon_theme_has_icon (icon_theme, name);
}

static gboolean
icon_name_is_generic (const char *name)
{
  BamfMatcher *matcher = bamf_matcher_get_default ();

  return !bamf_matcher_is_valid_process_prefix (matcher, name);
}

static void
bamf_application_setup_icon_and_name (BamfApplication *self, gboolean force)
{
  BamfWindow *window;
  BamfLegacyWindow *legacy_window;
  GDesktopAppInfo *desktop;
  GKeyFile * keyfile;
  GIcon *gicon;
  const char *class;
  char *icon = NULL, *generic_icon = NULL, *name = NULL;
  GError *error;

  g_return_if_fail (BAMF_IS_APPLICATION (self));

  if (!force)
    {
      if (bamf_view_get_icon (BAMF_VIEW (self)) && bamf_view_get_name (BAMF_VIEW (self)))
        return;
    }

  if (self->priv->desktop_file)
    {
      keyfile = g_key_file_new ();

      if (!g_key_file_load_from_file (keyfile, self->priv->desktop_file, G_KEY_FILE_NONE, NULL))
        {
          g_key_file_free (keyfile);
          return;
        }

      desktop = g_desktop_app_info_new_from_keyfile (keyfile);

      if (!G_IS_APP_INFO (desktop))
        {
          g_key_file_free (keyfile);
          return;
        }

      gicon = g_app_info_get_icon (G_APP_INFO (desktop));
      name = g_strdup (g_app_info_get_display_name (G_APP_INFO (desktop)));

      if (gicon)
        {
          icon = g_icon_to_string (gicon);

          if (!icon_name_is_valid (icon))
            {
              g_free (icon);
              icon = NULL;
            }
        }

      if (!icon)
        {
          icon = g_strdup (BAMF_APPLICATION_DEFAULT_ICON);
        }

      if (g_key_file_has_key (keyfile, G_KEY_FILE_DESKTOP_GROUP, STUB_KEY, NULL))
        {
          /* This will error to return false, which is okay as it seems
             unlikely anyone will want to set this flag except to turn
             off the stub menus. */
          self->priv->show_stubs = g_key_file_get_boolean (keyfile,
                                                           G_KEY_FILE_DESKTOP_GROUP,
                                                           STUB_KEY, NULL);
        }

      if (g_key_file_has_key (keyfile, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_FULLNAME, NULL))
        {
          /* Grab the better name if its available */
          gchar *fullname = NULL;
          error = NULL;
          fullname = g_key_file_get_locale_string (keyfile,
                                                   G_KEY_FILE_DESKTOP_GROUP,
                                                   G_KEY_FILE_DESKTOP_KEY_FULLNAME,
                                                   NULL, &error);
          if (error != NULL)
            {
              g_error_free (error);
              g_free (fullname);
            }
          else
            {
              g_free (name);
              name = fullname;
            }
        }

      g_object_unref (desktop);
      g_key_file_free (keyfile);
    }
  else if (BAMF_IS_WINDOW (self->priv->main_child))
    {
      name = g_strdup (bamf_view_get_name (self->priv->main_child));
      window = BAMF_WINDOW (self->priv->main_child);
      legacy_window = bamf_window_get_window (window);
      class = bamf_legacy_window_get_class_name (legacy_window);

      if (class)
        {
          icon = g_utf8_strdown (class, -1);

          if (icon_name_is_valid (icon))
            {
              if (icon_name_is_generic (icon))
                {
                  generic_icon = icon;
                  icon = NULL;
                }
            }
          else
            {
              g_free (icon);
              icon = NULL;
            }
        }

      if (!icon)
        {
          const char *exec = bamf_legacy_window_get_exec_string (legacy_window);
          icon = bamf_matcher_get_trimmed_exec (bamf_matcher_get_default (), exec);

          if (icon_name_is_valid (icon))
            {
              if (icon_name_is_generic (icon))
                {
                  g_free (generic_icon);
                  generic_icon = icon;
                  icon = NULL;
                }
            }
          else
            {
              g_free (icon);
              icon = NULL;
            }
        }

      if (!icon)
        {
          icon = bamf_legacy_window_save_mini_icon (legacy_window);

          if (!icon)
            {
              if (generic_icon)
                {
                  icon = generic_icon;
                  generic_icon = NULL;
                }
              else
                {
                  icon = g_strdup (BAMF_APPLICATION_DEFAULT_ICON);
                }
            }
        }

      g_free (generic_icon);
      generic_icon = NULL;
    }

  bamf_view_set_icon (BAMF_VIEW (self), icon);
  bamf_view_set_name (BAMF_VIEW (self), name);

  g_free (name);
  g_free (icon);
}

void
bamf_application_set_desktop_file (BamfApplication *application,
                                   const char * desktop_file)
{
  g_return_if_fail (BAMF_IS_APPLICATION (application));

  if (g_strcmp0 (application->priv->desktop_file, desktop_file) == 0)
    return;

  g_free (application->priv->desktop_file);
  application->priv->desktop_file = NULL;

  if (desktop_file && desktop_file[0] != '\0')
    application->priv->desktop_file = g_strdup (desktop_file);

  if (application->priv->main_child)
    {
      g_signal_handlers_disconnect_by_func (application->priv->main_child,
                                            on_main_child_name_changed, application);
    }

  g_signal_emit_by_name (application, "desktop-file-updated",
                         application->priv->desktop_file);

  bamf_application_setup_icon_and_name (application, TRUE);
}

gboolean
bamf_application_set_desktop_file_from_id (BamfApplication *application,
                                           const char *desktop_id)
{
  GDesktopAppInfo *info;
  const char *filename;

  info = g_desktop_app_info_new (desktop_id);

  if (info == NULL)
    {
      g_warning ("Failed to load desktop file from desktop ID: %s", desktop_id);
      return FALSE;
    }

  filename = g_desktop_app_info_get_filename (info);
  bamf_application_set_desktop_file (application, filename);

  g_object_unref (G_OBJECT (info));

  return TRUE;
}

static GFile *
try_create_subdir (GFile *parent, const gchar *child_name, GCancellable *cancellable)
{
  GFile *child;
  GError *error = NULL;

  child = g_file_get_child (parent, child_name);
  g_return_val_if_fail (G_IS_FILE (child), NULL);

  g_file_make_directory_with_parents (child, cancellable, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          g_error ("Impossible to create `%s` directory: %s", child_name, error->message);
          g_clear_object (&child);
        }

      g_error_free (error);
    }

  return child;
}

static GFile *
try_create_child (GFile *parent, const gchar *basename, const gchar *extension, GCancellable *cancellable)
{
  gchar *down, *child_name;

  down = g_ascii_strdown (basename, -1);
  g_strdelimit (down, "/\\&%\"'!?`*.;:^|()= <>[]{}", '_');

  child_name = g_strconcat (down, extension, NULL);

  GFile *child = g_file_get_child (parent, child_name);
  g_return_val_if_fail (G_IS_FILE (child), NULL);

  if (g_file_query_exists (child, cancellable))
    g_clear_object (&child);

  g_free (child_name);
  g_free (down);

  return child;
}

gboolean
try_create_local_desktop_data (GFile *apps_dir, GFile *icons_dir, const char *basename,
                               GFile **out_desktop_file, GFile **out_icon_file,
                               GCancellable *cancellable)
{
  g_return_val_if_fail (out_desktop_file, FALSE);

  if (!apps_dir)
    {
      *out_desktop_file = NULL;
      g_warn_if_reached ();
    }

  *out_desktop_file = try_create_child (apps_dir, basename, ".desktop", cancellable);

  if (G_IS_FILE (*out_desktop_file))
    {
      if (G_IS_FILE (icons_dir) && out_icon_file)
        *out_icon_file = try_create_child (icons_dir, basename, ".png", cancellable);

      return TRUE;
    }

  return FALSE;
}

gboolean
bamf_application_create_local_desktop_file (BamfApplication *self)
{
  BamfApplicationPrivate *priv;
  BamfLegacyWindow *window;
  BamfMatcher *matcher;
  GKeyFile *key_file;
  const gchar *name, *icon, *iclass, *nclass, *class, *exec, *path, *curdesktop;
  GFile *data_dir, *apps_dir, *icons_dir, *desktop_file, *icon_file, *mini_icon;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_APPLICATION (self), FALSE);
  priv = self->priv;

  if (priv->desktop_file || !BAMF_IS_WINDOW (priv->main_child))
    {
      return FALSE;
    }

  window = bamf_window_get_window (BAMF_WINDOW (priv->main_child));
  exec = bamf_legacy_window_get_exec_string (window);

  if (!exec)
    {
      return FALSE;
    }

  matcher = bamf_matcher_get_default ();
  data_dir = g_file_new_for_path (g_get_user_data_dir ());
  name = bamf_view_get_name (BAMF_VIEW (self));
  icon = bamf_view_get_icon (BAMF_VIEW (self));
  nclass = bamf_legacy_window_get_class_name (window);
  iclass = bamf_legacy_window_get_class_instance_name (window);
  path = bamf_legacy_window_get_working_dir (window);
  mini_icon = bamf_legacy_window_get_saved_mini_icon (window);
  curdesktop = g_getenv ("XDG_CURRENT_DESKTOP");

  if (!bamf_matcher_is_valid_class_name (matcher, iclass))
    iclass = NULL;

  if (!bamf_matcher_is_valid_class_name (matcher, nclass))
    nclass = NULL;

  apps_dir = try_create_subdir (data_dir, "applications", priv->cancellable);
  icons_dir = NULL;

  if (!G_IS_FILE (apps_dir))
    {
      g_object_unref (data_dir);
      return FALSE;
    }

  if (icon && G_IS_FILE (mini_icon))
    icons_dir = try_create_subdir (data_dir, "icons", priv->cancellable);

  g_clear_object (&data_dir);

  desktop_file = NULL;
  icon_file = NULL;
  class = (nclass) ? nclass : iclass;

  if (class)
    {
      try_create_local_desktop_data (apps_dir, icons_dir, class,
                                     &desktop_file, &icon_file, priv->cancellable);
    }

  if (!G_IS_FILE (desktop_file))
    {
      gchar *trimmed_exec = bamf_matcher_get_trimmed_exec (matcher, exec);
      try_create_local_desktop_data (apps_dir, icons_dir, trimmed_exec,
                                     &desktop_file, &icon_file, priv->cancellable);
      g_free (trimmed_exec);
    }

  if (!G_IS_FILE (desktop_file))
    {
      try_create_local_desktop_data (apps_dir, icons_dir, exec,
                                     &desktop_file, &icon_file, priv->cancellable);
    }

  g_object_unref (apps_dir);

  if (!G_IS_FILE (desktop_file))
    {
      g_critical ("Impossible to find a valid path where to save a .desktop file");
      g_clear_object (&icons_dir);
      g_clear_object (&icon_file);
      return FALSE;
    }

  if (G_IS_FILE (icons_dir) && !G_IS_FILE (icon_file))
    {
      gchar *basename = g_file_get_basename (mini_icon);
      icon_file = try_create_child (icons_dir, basename+1, ".png", priv->cancellable);
      g_free (basename);
    }

  g_clear_object (&icons_dir);

  if (G_IS_FILE (icon_file))
    {
      if (!g_file_copy (mini_icon, icon_file, G_FILE_COPY_NONE,
                        priv->cancellable, NULL, NULL, &error))
        {
          g_warning ("Impossible to copy icon to final destination: %s", error->message);
          g_clear_error (&error);
          g_clear_object (&icon_file);
        }
    }

  key_file = g_key_file_new ();

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                         "Encoding", "UTF-8");

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                         G_KEY_FILE_DESKTOP_KEY_VERSION, "1.0");

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                         G_KEY_FILE_DESKTOP_KEY_TYPE,
                         G_KEY_FILE_DESKTOP_TYPE_APPLICATION);

  if (name)
    {
      g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_NAME, name);
    }

  if (icon_file)
    {
      gchar *basename = g_file_get_basename (icon_file);
      g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_ICON, basename);
      bamf_view_set_icon (BAMF_VIEW (self), basename);
      g_free (basename);
      g_clear_object (&icon_file);
    }
  else if (icon)
    {
      g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_ICON, icon);
    }

  if (path && path[0] != '\0')
    {
      gchar *current_dir = g_get_current_dir ();

      if (g_strcmp0 (current_dir, path) != 0)
        {
          g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                 G_KEY_FILE_DESKTOP_KEY_PATH, path);
        }

      g_free (current_dir);
    }

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                         G_KEY_FILE_DESKTOP_KEY_EXEC, exec);

  /* It would be nice to know if the app support it from a win property */
  g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
                          G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, FALSE);

  if (class)
    {
      g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
                             G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS, class);
    }

  if (curdesktop)
    {
      const gchar* show_in_list[] = { curdesktop, NULL };
      g_key_file_set_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP,
                                  G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN,
                                  show_in_list, 1);
    }

  gchar *generator = g_strdup_printf ("X-%sGenerated", curdesktop ? curdesktop : "BAMF");
  g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, generator, TRUE);
  g_free (generator);

  gsize data_length = 0;
  gchar *data = g_key_file_to_data (key_file, &data_length, &error);
  g_key_file_free (key_file);

  if (error)
    {
      g_critical ("Impossible to generate local desktop file: %s", error->message);
      g_clear_error (&error);
      g_clear_pointer (&data, g_free);
    }

  if (data)
    {
      g_file_replace_contents (desktop_file, data, data_length, NULL, FALSE,
                               G_FILE_CREATE_NONE, NULL, priv->cancellable, &error);
      g_free (data);

      if (error)
        {
          g_critical ("Impossible to create local desktop file: %s", error->message);
          g_clear_error (&error);
          g_object_unref (desktop_file);

          return FALSE;
        }
    }

  gchar *desktop_path = g_file_get_path (desktop_file);
  g_object_unref (desktop_file);

  bamf_application_set_desktop_file (self, desktop_path);
  g_free (desktop_path);

  return TRUE;
}

void
bamf_application_set_wmclass (BamfApplication *application,
                              const char *wmclass)
{
  g_return_if_fail (BAMF_IS_APPLICATION (application));

  if (application->priv->wmclass)
    g_free (application->priv->wmclass);

  if (wmclass && wmclass[0] != '\0')
    application->priv->wmclass = g_strdup (wmclass);
  else
    application->priv->wmclass = NULL;
}

GVariant *
bamf_application_get_xids (BamfApplication *application)
{
  GList *l;
  GVariantBuilder b;
  BamfView *view;
  guint32 xid;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), NULL);

  g_variant_builder_init (&b, G_VARIANT_TYPE ("(au)"));
  g_variant_builder_open (&b, G_VARIANT_TYPE ("au"));

  for (l = bamf_view_get_children (BAMF_VIEW (application)); l; l = l->next)
    {
      view = l->data;

      if (BAMF_IS_WINDOW (view))
        xid = bamf_window_get_xid (BAMF_WINDOW (view));
      else if (BAMF_IS_TAB (view))
        xid = bamf_tab_get_xid (BAMF_TAB (view));
      else
        continue;
      g_variant_builder_add (&b, "u", xid);
    }

  g_variant_builder_close (&b);

  return g_variant_builder_end (&b);
}

gboolean
bamf_application_contains_similar_to_window (BamfApplication *self,
                                             BamfWindow *bamf_window)
{
  GList *children, *l;
  BamfView *child;

  g_return_val_if_fail (BAMF_IS_APPLICATION (self), FALSE);
  g_return_val_if_fail (BAMF_IS_WINDOW (bamf_window), FALSE);

  BamfLegacyWindow *window = bamf_window_get_window (bamf_window);
  const char *window_class = bamf_legacy_window_get_class_name (window);
  const char *instance_name = bamf_legacy_window_get_class_instance_name (window);

  children = bamf_view_get_children (BAMF_VIEW (self));
  for (l = children; l; l = l->next)
    {
      child = l->data;

      if (!BAMF_IS_WINDOW (child))
        continue;

      window = bamf_window_get_window (BAMF_WINDOW (child));
      const char *owned_win_class = bamf_legacy_window_get_class_name (window);
      const char *owned_instance = bamf_legacy_window_get_class_instance_name (window);

      if (g_strcmp0 (window_class, owned_win_class) == 0 &&
          g_strcmp0 (instance_name, owned_instance) == 0)
        {
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
bamf_application_manages_xid (BamfApplication *application,
                              guint32 xid)
{
  return (bamf_application_get_window (application, xid) != NULL);
}

BamfWindow *
bamf_application_get_window (BamfApplication *application,
                             guint32 xid)
{
  GList *l;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), NULL);

  for (l = bamf_view_get_children (BAMF_VIEW (application)); l; l = l->next)
    {
      BamfView *view = l->data;

      if (!BAMF_IS_WINDOW (view))
        continue;

      BamfWindow *window = BAMF_WINDOW (view);

      if (bamf_window_get_xid (window) == xid)
        {
          return window;
        }
    }

  return NULL;
}

static const char *
bamf_application_get_view_type (BamfView *view)
{
  return "application";
}

static char *
bamf_application_get_stable_bus_name (BamfView *view)
{
  BamfApplication *self;

  g_return_val_if_fail (BAMF_IS_APPLICATION (view), NULL);
  self = BAMF_APPLICATION (view);

  if (self->priv->desktop_file)
    return g_strdup_printf ("application/%i", abs (g_str_hash (self->priv->desktop_file)));

  return g_strdup_printf ("application/%p", view);
}

static void
bamf_application_ensure_flags (BamfApplication *self)
{
  gboolean urgent = FALSE, visible = FALSE, running = FALSE, active = FALSE;
  GList *l;
  BamfView *view;

  for (l = bamf_view_get_children (BAMF_VIEW (self)); l; l = l->next)
    {
      view = l->data;

      if (!BAMF_IS_VIEW (view))
        continue;

      running = TRUE;

      if (!BAMF_IS_WINDOW (view) && !BAMF_IS_TAB (view))
        continue;

      if (bamf_view_is_urgent (view))
        urgent = TRUE;
      if (bamf_view_is_user_visible (view))
        visible = TRUE;
      if (bamf_view_is_active (view))
        active = TRUE;

      if (urgent && visible && active)
        break;
    }

  gboolean close_when_empty = bamf_application_get_close_when_empty (self);
  bamf_view_set_urgent (BAMF_VIEW (self), urgent);
  bamf_view_set_user_visible (BAMF_VIEW (self), (visible || !close_when_empty));
  bamf_view_set_running (BAMF_VIEW (self), (running || !close_when_empty));
  bamf_view_set_active (BAMF_VIEW (self), active);
}

static void
view_active_changed (BamfView *view, gboolean active, BamfApplication *self)
{
  bamf_application_ensure_flags (self);
}

static void
view_urgent_changed (BamfView *view, gboolean urgent, BamfApplication *self)
{
  bamf_application_ensure_flags (self);
}

static void
view_visible_changed (BamfView *view, gboolean visible, BamfApplication *self)
{
  bamf_application_ensure_flags (self);
}

static void
view_xid_changed (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  BamfApplication *self;

  self = (BamfApplication *)user_data;
  bamf_application_ensure_flags (self);
}

static void
on_main_child_name_changed (BamfView *child, const gchar *old_name,
                            const gchar *new_name, BamfApplication *self)
{
  bamf_view_set_name (BAMF_VIEW (self), new_name);
}

static void
bamf_application_set_main_child (BamfApplication *self, BamfView *child)
{
  if (self->priv->main_child == child)
    return;

  if (self->priv->main_child)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->priv->main_child),
                                    (gpointer*) &self->priv->main_child);
      g_signal_handlers_disconnect_by_func (self->priv->main_child,
                                            on_main_child_name_changed, self);
    }

  self->priv->main_child = child;

  if (self->priv->main_child)
    {
      g_object_add_weak_pointer (G_OBJECT (self->priv->main_child),
                                 (gpointer*) &self->priv->main_child);

      if (!self->priv->desktop_file)
        {
          g_signal_connect (child, "name-changed",
                            G_CALLBACK (on_main_child_name_changed), self);
        }
    }
}

BamfView *
bamf_application_get_main_child (BamfApplication *self)
{
  g_return_val_if_fail (BAMF_IS_APPLICATION (self), NULL);

  return self->priv->main_child;
}

static void
view_exported (BamfView *view, BamfApplication *self)
{
  g_signal_emit_by_name (self, "window-added", bamf_view_get_path (view));
  g_signal_handlers_disconnect_by_func (view, view_exported, self);
}

static void
bamf_application_child_added (BamfView *view, BamfView *child)
{
  BamfApplication *application;
  BamfWindow *window = NULL;
  gboolean reset_emblems = FALSE;

  application = BAMF_APPLICATION (view);

  if (BAMF_IS_WINDOW (child))
    {
      window = BAMF_WINDOW (child);

      if (bamf_view_is_on_bus (child))
        {
          g_signal_emit_by_name (BAMF_APPLICATION (view), "window-added",
                                 bamf_view_get_path (child));
        }
      else
        {
          g_signal_connect (G_OBJECT (child), "exported",
                            (GCallback) view_exported, view);
        }
    }

  g_signal_connect (G_OBJECT (child), "active-changed",
                    (GCallback) view_active_changed, view);
  g_signal_connect (G_OBJECT (child), "urgent-changed",
                    (GCallback) view_urgent_changed, view);
  g_signal_connect (G_OBJECT (child), "user-visible-changed",
                    (GCallback) view_visible_changed, view);

  if (BAMF_IS_TAB (child))
    {
      g_signal_connect (G_OBJECT (child), "notify::xid",
                        (GCallback) view_xid_changed, view);
    }

  if (application->priv->main_child)
    {
      if (window && BAMF_IS_WINDOW (application->priv->main_child))
        {
          BamfWindow *main_window = BAMF_WINDOW (application->priv->main_child);

          if (bamf_window_get_window_type (main_window) != BAMF_WINDOW_NORMAL &&
              bamf_window_get_window_type (window) == BAMF_WINDOW_NORMAL)
            {
              bamf_application_set_main_child (application, child);
            }
        }
    }
  else
    {
      bamf_application_set_main_child (application, child);
    }

  bamf_application_ensure_flags (BAMF_APPLICATION (view));

  if (!application->priv->desktop_file && application->priv->main_child == child)
    reset_emblems = TRUE;

  bamf_application_setup_icon_and_name (application, reset_emblems);
}

static char *
bamf_application_favorite_from_list (BamfApplication *self, GList *desktop_list)
{
  BamfMatcher *matcher;
  GList *favs, *l;
  char *result = NULL;
  const char *desktop_class;

  g_return_val_if_fail (BAMF_IS_APPLICATION (self), NULL);

  matcher = bamf_matcher_get_default ();
  favs = bamf_matcher_get_favorites (matcher);

  if (favs)
    {
      for (l = favs; l; l = l->next)
        {
          if (g_list_find_custom (desktop_list, l->data, (GCompareFunc) g_strcmp0))
            {
              desktop_class = bamf_matcher_get_desktop_file_class (matcher, l->data);

              if (!desktop_class || g_strcmp0 (self->priv->wmclass, desktop_class) == 0)
                {
                  result = l->data;
                  break;
                }
            }
        }
    }

  return result;
}

static void
bamf_application_set_desktop_file_from_list (BamfApplication *self, GList *list)
{
  BamfApplicationPrivate *priv;
  GList *l;
  char *desktop_file;

  g_return_if_fail (BAMF_IS_APPLICATION (self));
  g_return_if_fail (list);

  priv = self->priv;

  if (priv->desktop_file_list)
    {
      g_list_free_full (priv->desktop_file_list, g_free);
      priv->desktop_file_list = NULL;
    }

  for (l = list; l; l = l->next)
    priv->desktop_file_list = g_list_prepend (priv->desktop_file_list, g_strdup (l->data));

  priv->desktop_file_list = g_list_reverse (priv->desktop_file_list);

  desktop_file = bamf_application_favorite_from_list (self, priv->desktop_file_list);

  /* items, after reversing them, are in priority order */
  if (!desktop_file)
    desktop_file = list->data;

  bamf_application_set_desktop_file (self, desktop_file);
}

static void
bamf_application_child_removed (BamfView *view, BamfView *child)
{
  BamfApplication *self = BAMF_APPLICATION (view);
  GList *children, *l;

  if (BAMF_IS_WINDOW (child))
    {
      if (bamf_view_is_on_bus (child))
        g_signal_emit_by_name (BAMF_APPLICATION (view), "window-removed",
                               bamf_view_get_path (child));
    }

  g_signal_handlers_disconnect_by_data (G_OBJECT (child), view);

  bamf_application_ensure_flags (self);

  children = bamf_view_get_children (view);

  if (self->priv->main_child == child)
    {
      /* Giving priority to older windows, and BamfView has a reversed list */
      children = g_list_last (children);
      bamf_application_set_main_child (self, (children ? children->data : NULL));

      if (self->priv->app_type == BAMF_APPLICATION_SYSTEM)
        {
          /* We check if we have a better target in next windows */
          for (l = children; l; l = l->prev)
            {
              if (bamf_window_get_window_type (BAMF_WINDOW (l->data)) == BAMF_WINDOW_NORMAL)
                {
                  bamf_application_set_main_child (self, l->data);
                  break;
                }
            }
        }

        if (self->priv->main_child)
          {
            gboolean reset_emblems = (!self->priv->desktop_file);
            bamf_application_setup_icon_and_name (self, reset_emblems);
          }
    }

  if (!children && bamf_application_get_close_when_empty (self))
    {
      bamf_view_close (view);
    }
}

static void
matcher_favorites_changed (BamfMatcher *matcher, BamfApplication *self)
{
  char *new_desktop_file = NULL;

  g_return_if_fail (BAMF_IS_APPLICATION (self));
  g_return_if_fail (BAMF_IS_MATCHER (matcher));

  new_desktop_file = bamf_application_favorite_from_list (self, self->priv->desktop_file_list);

  if (new_desktop_file)
    {
      bamf_application_set_desktop_file (self, new_desktop_file);
    }
}

static void
on_window_added (BamfApplication *self, const gchar *win_path, gpointer _not_used)
{
  g_return_if_fail (BAMF_IS_APPLICATION (self));
  g_signal_emit_by_name (self->priv->dbus_iface, "window-added", win_path);
}

static void
on_window_removed (BamfApplication *self, const gchar *win_path, gpointer _not_used)
{
  g_return_if_fail (BAMF_IS_APPLICATION (self));
  g_signal_emit_by_name (self->priv->dbus_iface, "window-removed", win_path);
}

static void
on_desktop_file_updated (BamfApplication *self, const gchar *file, gpointer _not_used)
{
  g_return_if_fail (BAMF_IS_APPLICATION (self));
  g_signal_emit_by_name (self->priv->dbus_iface, "desktop-file-updated", file);
}

static gboolean
on_dbus_handle_show_stubs (BamfDBusItemApplication *interface,
                           GDBusMethodInvocation *invocation,
                           BamfApplication *self)
{
  gboolean show_stubs = bamf_application_get_show_stubs (self);
  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(b)", show_stubs));

  return TRUE;
}

static gboolean
on_dbus_handle_xids (BamfDBusItemApplication *interface,
                     GDBusMethodInvocation *invocation,
                     BamfApplication *self)
{
  GVariant *xids = bamf_application_get_xids (self);
  g_dbus_method_invocation_return_value (invocation, xids);

  return TRUE;
}

static gboolean
on_dbus_handle_focusable_child (BamfDBusItemApplication *interface,
                                GDBusMethodInvocation *invocation,
                                BamfApplication *self)
{
  GVariant *out_variant;
  BamfView *focusable_child;

  out_variant = NULL;

  focusable_child = bamf_application_get_focusable_child (self);

  if (focusable_child == NULL)
    {
      out_variant = g_variant_new("(s)", "");
    }
  else
    {
      const gchar *path;

      path = bamf_view_get_path (BAMF_VIEW (focusable_child));

      out_variant = g_variant_new("(s)", path);
    }

  g_dbus_method_invocation_return_value (invocation, out_variant);

  return TRUE;
}

static gboolean
on_dbus_handle_desktop_file (BamfDBusItemApplication *interface,
                             GDBusMethodInvocation *invocation,
                             BamfApplication *self)
{
  const char *desktop_file = self->priv->desktop_file ? self->priv->desktop_file : "";
  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s)", desktop_file));

  return TRUE;
}

static gboolean
on_dbus_handle_supported_mime_types (BamfDBusItemApplication *interface,
                                     GDBusMethodInvocation *invocation,
                                     BamfApplication *self)
{
  GVariant *list;
  GVariant *value;

  gchar **mimes = bamf_application_get_supported_mime_types (self);

  if (mimes)
    {
      list = g_variant_new_strv ((const gchar**) mimes, -1);
      g_strfreev (mimes);
    }
  else
    {
      list = g_variant_new_strv (NULL, 0);
    }

  value = g_variant_new ("(@as)", list);
  g_dbus_method_invocation_return_value (invocation, value);

  return TRUE;
}

static gboolean
on_dbus_handle_application_menu (BamfDBusItemApplication *interface,
                                 GDBusMethodInvocation *invocation,
                                 BamfApplication *self)
{
  gchar *name, *path;

  bamf_application_get_application_menu (self, &name, &path);

  name = name ? name : "";
  path = path ? path : "";

  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(ss)", name, path));

  return TRUE;
}

static gboolean
on_dbus_handle_application_type (BamfDBusItemApplication *interface,
                                 GDBusMethodInvocation *invocation,
                                 BamfApplication *self)
{
  const char *type = "";

  switch (self->priv->app_type)
    {
      case BAMF_APPLICATION_SYSTEM:
        type = "system";
        break;
      case BAMF_APPLICATION_WEB:
        type = "webapp";
        break;
      default:
        type = "unknown";
    }

  g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", type));

  return TRUE;
}

static void
bamf_application_dispose (GObject *object)
{
  BamfApplication *app;
  BamfApplicationPrivate *priv;

  app = BAMF_APPLICATION (object);
  priv = app->priv;

  if (priv->desktop_file)
    {
      g_free (priv->desktop_file);
      priv->desktop_file = NULL;
    }

  if (priv->desktop_file_list)
    {
      g_list_free_full (priv->desktop_file_list, g_free);
      priv->desktop_file_list = NULL;
    }

  if (priv->wmclass)
    {
      g_free (priv->wmclass);
      priv->wmclass = NULL;
    }

  if (priv->main_child)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->main_child),
                                    (gpointer*) &priv->main_child);
      g_signal_handlers_disconnect_by_data (priv->main_child, app);
      priv->main_child = NULL;
    }

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }

  g_strfreev (priv->mimes);
  priv->mimes = NULL;

  g_signal_handlers_disconnect_by_func (G_OBJECT (bamf_matcher_get_default ()),
                                        matcher_favorites_changed, object);

  G_OBJECT_CLASS (bamf_application_parent_class)->dispose (object);
}

static void
bamf_application_finalize (GObject *object)
{
  BamfApplication *self;
  self = BAMF_APPLICATION (object);

  g_object_unref (self->priv->dbus_iface);

  G_OBJECT_CLASS (bamf_application_parent_class)->finalize (object);
}

static void
bamf_application_init (BamfApplication * self)
{
  BamfApplicationPrivate *priv;
  priv = self->priv = BAMF_APPLICATION_GET_PRIVATE (self);

  priv->app_type = BAMF_APPLICATION_SYSTEM;
  priv->show_stubs = TRUE;

  priv->cancellable = g_cancellable_new ();

  /* Initializing the dbus interface */
  priv->dbus_iface = _bamf_dbus_item_application_skeleton_new ();

  /* We need to connect to the object own signals to redirect them to the dbus
   * interface                                                                */
  g_signal_connect (self, "window-added", G_CALLBACK (on_window_added), NULL);
  g_signal_connect (self, "window-removed", G_CALLBACK (on_window_removed), NULL);
  g_signal_connect (self, "desktop-file-updated", G_CALLBACK (on_desktop_file_updated), NULL);

  /* Registering signal callbacks to reply to dbus method calls */
  g_signal_connect (priv->dbus_iface, "handle-show-stubs",
                    G_CALLBACK (on_dbus_handle_show_stubs), self);

  g_signal_connect (priv->dbus_iface, "handle-xids",
                    G_CALLBACK (on_dbus_handle_xids), self);

  g_signal_connect (priv->dbus_iface, "handle-focusable-child",
                    G_CALLBACK (on_dbus_handle_focusable_child), self);

  g_signal_connect (priv->dbus_iface, "handle-desktop-file",
                    G_CALLBACK (on_dbus_handle_desktop_file), self);

  g_signal_connect (priv->dbus_iface, "handle-supported-mime-types",
                    G_CALLBACK (on_dbus_handle_supported_mime_types), self);

  g_signal_connect (priv->dbus_iface, "handle-application-menu",
                    G_CALLBACK (on_dbus_handle_application_menu), self);

  g_signal_connect (priv->dbus_iface, "handle-application-type",
                    G_CALLBACK (on_dbus_handle_application_type), self);

  /* Setting the interface for the dbus object */
  _bamf_dbus_item_object_skeleton_set_application (BAMF_DBUS_ITEM_OBJECT_SKELETON (self),
                                                   priv->dbus_iface);

  g_signal_connect (G_OBJECT (bamf_matcher_get_default ()), "favorites-changed",
                    (GCallback) matcher_favorites_changed, self);
}

static void
bamf_application_dbus_application_iface_init (BamfDBusItemApplicationIface *iface)
{
}

static void
bamf_application_class_init (BamfApplicationClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BamfViewClass *view_class = BAMF_VIEW_CLASS (klass);

  object_class->dispose = bamf_application_dispose;
  object_class->finalize = bamf_application_finalize;

  view_class->view_type = bamf_application_get_view_type;
  view_class->child_added = bamf_application_child_added;
  view_class->child_removed = bamf_application_child_removed;
  view_class->stable_bus_name = bamf_application_get_stable_bus_name;

  klass->get_supported_mime_types = bamf_application_default_get_supported_mime_types;
  klass->get_close_when_empty = bamf_application_default_get_close_when_empty;
  klass->supported_mimes_changed = bamf_application_supported_mime_types_changed;

  g_type_class_add_private (klass, sizeof (BamfApplicationPrivate));

  application_signals[SUPPORTED_MIMES_CHANGED] =
    g_signal_new ("supported-mimes-changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfApplicationClass, supported_mimes_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRV);
}

BamfApplication *
bamf_application_new (void)
{
  BamfApplication *application;
  application = (BamfApplication *) g_object_new (BAMF_TYPE_APPLICATION, NULL);

  return application;
}

BamfApplication *
bamf_application_new_from_desktop_file (const char * desktop_file)
{
  BamfApplication *application;
  application = (BamfApplication *) g_object_new (BAMF_TYPE_APPLICATION, NULL);

  bamf_application_set_desktop_file (application, desktop_file);

  return application;
}

BamfApplication *
bamf_application_new_from_desktop_files (GList *desktop_files)
{
  BamfApplication *application;
  application = (BamfApplication *) g_object_new (BAMF_TYPE_APPLICATION, NULL);

  bamf_application_set_desktop_file_from_list (application, desktop_files);

  return application;
}

BamfApplication *
bamf_application_new_with_wmclass (const char *wmclass)
{
  BamfApplication *application;
  application = (BamfApplication *) g_object_new (BAMF_TYPE_APPLICATION, NULL);

  bamf_application_set_wmclass (application, wmclass);

  return application;
}

gboolean
bamf_application_get_show_stubs (BamfApplication *application)
{
  g_return_val_if_fail (BAMF_IS_APPLICATION(application), TRUE);
  return application->priv->show_stubs;
}

gboolean
bamf_application_get_close_when_empty (BamfApplication *application)
{
  g_return_val_if_fail (BAMF_IS_APPLICATION(application), FALSE);

  if (BAMF_APPLICATION_GET_CLASS (application)->get_close_when_empty)
    {
      return BAMF_APPLICATION_GET_CLASS (application)->get_close_when_empty(application);
    }
  return TRUE;
}

void
bamf_application_get_application_menu (BamfApplication *application, gchar **name, gchar **object_path)
{
  g_return_if_fail (BAMF_IS_APPLICATION (application));

  if (BAMF_APPLICATION_GET_CLASS (application)->get_application_menu)
    {
      BAMF_APPLICATION_GET_CLASS (application)->get_application_menu (application, name, object_path);
    }
  else
    {
      *name = NULL;
      *object_path = NULL;
    }
}

BamfView *
bamf_application_get_focusable_child (BamfApplication *application)
{
  g_return_val_if_fail (BAMF_IS_APPLICATION (application), NULL);

  if (BAMF_APPLICATION_GET_CLASS (application)->get_focusable_child)
    {
      return BAMF_APPLICATION_GET_CLASS (application)->get_focusable_child (application);
    }

  return NULL;
}
