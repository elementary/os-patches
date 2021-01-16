/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2020 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#include "config.h"

#include <flatpak.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libmalcontent/app-filter.h>

#include "restrict-applications-selector.h"


#define WEB_BROWSERS_CONTENT_TYPE "x-scheme-handler/http"

static void app_info_changed_cb (GAppInfoMonitor *monitor,
                                 gpointer         user_data);
static void reload_apps (MctRestrictApplicationsSelector *self);
static GtkWidget *create_row_for_app_cb (gpointer item,
                                         gpointer user_data);

/**
 * MctRestrictApplicationsSelector:
 *
 * The ‘Restrict Applications’ selector is a list box which shows the available
 * applications on the system alongside a column of toggle switches, which
 * allows the given user to be prevented from running each application.
 *
 * The selector takes an #MctRestrictApplicationsSelector:app-filter as input
 * to set up the UI, and returns its output as set of modifications to a given
 * #MctAppFilterBuilder using
 * mct_restrict_applications_selector_build_app_filter().
 *
 * Since: 0.5.0
 */
struct _MctRestrictApplicationsSelector
{
  GtkBox parent_instance;

  GtkListBox *listbox;

  GList *cached_apps;  /* (nullable) (owned) (element-type GAppInfo) */
  GListStore *apps;  /* (owned) */
  GAppInfoMonitor *app_info_monitor;  /* (owned) */
  gulong app_info_monitor_changed_id;
  GHashTable *blocklisted_apps; /* (owned) (element-type GAppInfo) */

  MctAppFilter *app_filter;  /* (owned) */

  FlatpakInstallation *system_installation; /* (owned) */
  FlatpakInstallation *user_installation; /* (owned) */

  GtkCssProvider *css_provider;  /* (owned) */
};

G_DEFINE_TYPE (MctRestrictApplicationsSelector, mct_restrict_applications_selector, GTK_TYPE_BOX)

typedef enum
{
  PROP_APP_FILTER = 1,
} MctRestrictApplicationsSelectorProperty;

static GParamSpec *properties[PROP_APP_FILTER + 1];

enum {
  SIGNAL_CHANGED,
};

static guint signals[SIGNAL_CHANGED + 1];

static void
mct_restrict_applications_selector_constructed (GObject *obj)
{
  MctRestrictApplicationsSelector *self = MCT_RESTRICT_APPLICATIONS_SELECTOR (obj);

  /* Default app filter, typically for when we’re instantiated by #GtkBuilder. */
  if (self->app_filter == NULL)
    {
      g_auto(MctAppFilterBuilder) builder = MCT_APP_FILTER_BUILDER_INIT ();
      self->app_filter = mct_app_filter_builder_end (&builder);
    }

  g_assert (self->app_filter != NULL);

  /* Load the apps. */
  reload_apps (self);

  G_OBJECT_CLASS (mct_restrict_applications_selector_parent_class)->constructed (obj);
}

static void
mct_restrict_applications_selector_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  MctRestrictApplicationsSelector *self = MCT_RESTRICT_APPLICATIONS_SELECTOR (object);

  switch ((MctRestrictApplicationsSelectorProperty) prop_id)
    {
    case PROP_APP_FILTER:
      g_value_set_boxed (value, self->app_filter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mct_restrict_applications_selector_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  MctRestrictApplicationsSelector *self = MCT_RESTRICT_APPLICATIONS_SELECTOR (object);

  switch ((MctRestrictApplicationsSelectorProperty) prop_id)
    {
    case PROP_APP_FILTER:
      mct_restrict_applications_selector_set_app_filter (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mct_restrict_applications_selector_dispose (GObject *object)
{
  MctRestrictApplicationsSelector *self = (MctRestrictApplicationsSelector *)object;

  g_clear_pointer (&self->blocklisted_apps, g_hash_table_unref);
  g_clear_object (&self->apps);
  g_clear_list (&self->cached_apps, g_object_unref);

  if (self->app_info_monitor != NULL && self->app_info_monitor_changed_id != 0)
    {
      g_signal_handler_disconnect (self->app_info_monitor, self->app_info_monitor_changed_id);
      self->app_info_monitor_changed_id = 0;
    }
  g_clear_object (&self->app_info_monitor);
  g_clear_pointer (&self->app_filter, mct_app_filter_unref);
  g_clear_object (&self->system_installation);
  g_clear_object (&self->user_installation);
  g_clear_object (&self->css_provider);

  G_OBJECT_CLASS (mct_restrict_applications_selector_parent_class)->dispose (object);
}

static void
mct_restrict_applications_selector_class_init (MctRestrictApplicationsSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = mct_restrict_applications_selector_constructed;
  object_class->get_property = mct_restrict_applications_selector_get_property;
  object_class->set_property = mct_restrict_applications_selector_set_property;
  object_class->dispose = mct_restrict_applications_selector_dispose;

  /**
   * MctRestrictApplicationsSelector:app-filter: (not nullable)
   *
   * The user’s current app filter, used to set up the selector. As app filters
   * are immutable, it is not updated as the selector is changed. Use
   * mct_restrict_applications_selector_build_app_filter() to build the new app
   * filter.
   *
   * Since: 0.5.0
   */
  properties[PROP_APP_FILTER] =
      g_param_spec_boxed ("app-filter",
                          "App Filter",
                          "The user’s current app filter, used to set up the selector.",
                          MCT_TYPE_APP_FILTER,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  /**
   * MctRestrictApplicationsSelector::changed:
   *
   * Emitted whenever an application in the list is blocked or unblocked.
   *
   * Since: 0.5.0
   */
  signals[SIGNAL_CHANGED] =
      g_signal_new ("changed",
                    MCT_TYPE_RESTRICT_APPLICATIONS_SELECTOR,
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/freedesktop/MalcontentUi/ui/restrict-applications-selector.ui");

  gtk_widget_class_bind_template_child (widget_class, MctRestrictApplicationsSelector, listbox);
}

static void
mct_restrict_applications_selector_init (MctRestrictApplicationsSelector *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->apps = g_list_store_new (G_TYPE_APP_INFO);
  self->cached_apps = NULL;

  self->app_info_monitor = g_app_info_monitor_get ();
  self->app_info_monitor_changed_id =
      g_signal_connect (self->app_info_monitor, "changed",
                        (GCallback) app_info_changed_cb, self);

  gtk_list_box_bind_model (self->listbox,
                           G_LIST_MODEL (self->apps),
                           create_row_for_app_cb,
                           self,
                           NULL);

  self->blocklisted_apps = g_hash_table_new_full (g_direct_hash,
                                                  g_direct_equal,
                                                  g_object_unref,
                                                  NULL);

  self->system_installation = flatpak_installation_new_system (NULL, NULL);
  self->user_installation = flatpak_installation_new_user (NULL, NULL);

  self->css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (self->css_provider,
                                       "/org/freedesktop/MalcontentUi/ui/restricts-switch.css");
}

static void
on_switch_active_changed_cb (GtkSwitch  *s,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
  MctRestrictApplicationsSelector *self = MCT_RESTRICT_APPLICATIONS_SELECTOR (user_data);
  GAppInfo *app;
  gboolean allowed;

  app = g_object_get_data (G_OBJECT (s), "GAppInfo");
  allowed = !gtk_switch_get_active (s);

  if (allowed)
    {
      gboolean removed;

      g_debug ("Removing ‘%s’ from blocklisted apps", g_app_info_get_id (app));

      removed = g_hash_table_remove (self->blocklisted_apps, app);
      g_assert (removed);
    }
  else
    {
      gboolean added;

      g_debug ("Blocklisting ‘%s’", g_app_info_get_id (app));

      added = g_hash_table_add (self->blocklisted_apps, g_object_ref (app));
      g_assert (added);
    }

  g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
}

static void
update_listbox_row_switch (MctRestrictApplicationsSelector *self,
                           GtkSwitch                       *w)
{
  GAppInfo *app = g_object_get_data (G_OBJECT (w), "GAppInfo");
  gboolean allowed = mct_app_filter_is_appinfo_allowed (self->app_filter, app);

  gtk_switch_set_active (w, !allowed);

  if (allowed)
    g_hash_table_remove (self->blocklisted_apps, app);
  else
    g_hash_table_add (self->blocklisted_apps, g_object_ref (app));
}

static GtkWidget *
create_row_for_app_cb (gpointer item,
                       gpointer user_data)
{
  MctRestrictApplicationsSelector *self = MCT_RESTRICT_APPLICATIONS_SELECTOR (user_data);
  GAppInfo *app = G_APP_INFO (item);
  g_autoptr(GIcon) icon = NULL;
  GtkWidget *box, *w;
  const gchar *app_name;
  gint size;
  GtkStyleContext *context;

  app_name = g_app_info_get_name (app);

  g_assert (G_IS_DESKTOP_APP_INFO (app));

  icon = g_app_info_get_icon (app);
  if (icon == NULL)
    icon = g_themed_icon_new ("application-x-executable");
  else
    g_object_ref (icon);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  gtk_widget_set_margin_end (box, 12);

  /* Icon */
  w = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &size, NULL);
  gtk_image_set_pixel_size (GTK_IMAGE (w), size);
  gtk_container_add (GTK_CONTAINER (box), w);

  /* App name label */
  w = g_object_new (GTK_TYPE_LABEL,
                    "label", app_name,
                    "hexpand", TRUE,
                    "xalign", 0.0,
                    NULL);
  gtk_container_add (GTK_CONTAINER (box), w);

  /* Switch */
  w = g_object_new (GTK_TYPE_SWITCH,
                    "valign", GTK_ALIGN_CENTER,
                    NULL);
  context = gtk_widget_get_style_context (w);
  gtk_style_context_add_class (context, "restricts");
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (self->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION - 1);
  gtk_container_add (GTK_CONTAINER (box), w);

  gtk_widget_show_all (box);

  /* Fetch status from AccountService */
  g_object_set_data_full (G_OBJECT (w), "GAppInfo", g_object_ref (app), g_object_unref);
  update_listbox_row_switch (self, GTK_SWITCH (w));
  g_signal_connect (w, "notify::active", G_CALLBACK (on_switch_active_changed_cb), self);

  return box;
}

static gint
compare_app_info_cb (gconstpointer a,
                     gconstpointer b,
                     gpointer      user_data)
{
  GAppInfo *app_a = (GAppInfo*) a;
  GAppInfo *app_b = (GAppInfo*) b;

  return g_utf8_collate (g_app_info_get_display_name (app_a),
                         g_app_info_get_display_name (app_b));
}

static gint
app_compare_id_length_cb (gconstpointer a,
                          gconstpointer b)
{
  GAppInfo *info_a = (GAppInfo *) a, *info_b = (GAppInfo *) b;
  const gchar *id_a, *id_b;
  gsize id_a_len, id_b_len;

  id_a = g_app_info_get_id (info_a);
  id_b = g_app_info_get_id (info_b);

  if (id_a == NULL && id_b == NULL)
    return 0;
  else if (id_a == NULL)
    return -1;
  else if (id_b == NULL)
    return 1;

  id_a_len = strlen (id_a);
  id_b_len = strlen (id_b);
  if (id_a_len == id_b_len)
    return strcmp (id_a, id_b);
  else
    return id_a_len - id_b_len;
}

/* Elements in @added_out and @removed_out are valid as long as @old_apps and
 * @new_apps are valid.
 *
 * Both lists have to be sorted the same before calling this function. */
static void
diff_app_lists (GList      *old_apps,
                GList      *new_apps,
                GPtrArray **added_out,
                GPtrArray **removed_out)
{
  g_autoptr(GPtrArray) added = g_ptr_array_new_with_free_func (NULL);
  g_autoptr(GPtrArray) removed = g_ptr_array_new_with_free_func (NULL);
  GList *o, *n;

  g_return_if_fail (added_out != NULL);
  g_return_if_fail (removed_out != NULL);

  for (o = old_apps, n = new_apps; o != NULL || n != NULL;)
    {
      int comparison;

      if (o == NULL)
        comparison = 1;
      else if (n == NULL)
        comparison = -1;
      else
        comparison = app_compare_id_length_cb (o->data, n->data);

      if (comparison < 0)
        {
          g_ptr_array_add (removed, o->data);
          o = o->next;
        }
      else if (comparison > 0)
        {
          g_ptr_array_add (added, n->data);
          n = n->next;
        }
      else
        {
          o = o->next;
          n = n->next;
        }
    }

  *added_out = g_steal_pointer (&added);
  *removed_out = g_steal_pointer (&removed);
}

/* This is quite expensive to call, as there’s no way to avoid calling
 * g_app_info_get_all() to see if anything’s changed; and that’s quite expensive. */
static void
reload_apps (MctRestrictApplicationsSelector *self)
{
  g_autolist(GAppInfo) old_apps = NULL;
  g_autolist(GAppInfo) new_apps = NULL;
  g_autoptr(GPtrArray) added_apps = NULL, removed_apps = NULL;
  g_autoptr(GHashTable) seen_flatpak_ids = NULL;
  g_autoptr(GHashTable) seen_executables = NULL;

  old_apps = g_steal_pointer (&self->cached_apps);
  new_apps = g_app_info_get_all ();

  /* Sort the apps by increasing length of #GAppInfo ID. When coupled with the
   * deduplication of flatpak IDs and executable paths, below, this should ensure that we
   * pick the ‘base’ app out of any set with matching prefixes and identical app IDs (in
   * case of flatpak apps) or executables (for non-flatpak apps), and show only that.
   *
   * This is designed to avoid listing all the components of LibreOffice for example,
   * which all share an app ID and hence have the same entry in the parental controls
   * app filter.
   *
   * Then diff the old and new lists so that the code below doesn’t end up
   * removing more rows than are necessary, and hence potentially losing
   * in-progress user input. */
  new_apps = g_list_sort (new_apps, app_compare_id_length_cb);
  diff_app_lists (old_apps, new_apps, &added_apps, &removed_apps);

  g_debug ("%s: Diffed old and new app lists: %u apps added, %u apps removed",
           G_STRFUNC, added_apps->len, removed_apps->len);

  seen_flatpak_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  seen_executables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Remove items first. */
  for (guint i = 0; i < removed_apps->len; i++)
    {
      GAppInfo *app = removed_apps->pdata[i];
      guint pos;
      gboolean found;

      found = g_list_store_find_with_equal_func (self->apps, app,
                                                 (GEqualFunc) g_app_info_equal, &pos);

      /* The app being removed may have not passed the condition checks below
       * to have been added to self->apps. */
      if (!found)
        continue;

      g_debug ("Removing app ‘%s’", g_app_info_get_id (app));
      g_list_store_remove (self->apps, pos);
    }

  /* Now add the new items. */
  for (guint i = 0; i < added_apps->len; i++)
    {
      GAppInfo *app = added_apps->pdata[i];
      const gchar *app_name;
      const gchar * const *supported_types;

      app_name = g_app_info_get_name (app);

      supported_types = g_app_info_get_supported_types (app);

      if (!G_IS_DESKTOP_APP_INFO (app) ||
          !g_app_info_should_show (app) ||
          app_name[0] == '\0' ||
          /* Endless' link apps have the "eos-link" prefix, and should be ignored too */
          g_str_has_prefix (g_app_info_get_id (app), "eos-link") ||
          /* FIXME: Only list flatpak apps and apps with X-Parental-Controls
           * key set for now; we really need a system-wide MAC to be able to
           * reliably support blocklisting system programs. */
          (!g_desktop_app_info_has_key (G_DESKTOP_APP_INFO (app), "X-Flatpak") &&
           !g_desktop_app_info_has_key (G_DESKTOP_APP_INFO (app), "X-Parental-Controls")) ||
          /* Web browsers are special cased */
          (supported_types && g_strv_contains (supported_types, WEB_BROWSERS_CONTENT_TYPE)))
        {
          continue;
        }

      if (g_desktop_app_info_has_key (G_DESKTOP_APP_INFO (app), "X-Flatpak"))
        {
          g_autofree gchar *flatpak_id = NULL;

          flatpak_id = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (app), "X-Flatpak");
          g_debug ("Processing app ‘%s’ (Exec=%s, X-Flatpak=%s)",
                   g_app_info_get_id (app),
                   g_app_info_get_executable (app),
                   flatpak_id);

          /* Have we seen this flatpak ID before? */
          if (!g_hash_table_add (seen_flatpak_ids, g_steal_pointer (&flatpak_id)))
            {
              g_debug (" → Skipping ‘%s’ due to seeing its flatpak ID already",
                       g_app_info_get_id (app));
              continue;
            }
        }
      else if (g_desktop_app_info_has_key (G_DESKTOP_APP_INFO (app), "X-Parental-Controls"))
        {
          g_autofree gchar *parental_controls_type = NULL;
          g_autofree gchar *executable = NULL;

          parental_controls_type = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (app),
                                                                  "X-Parental-Controls");
          /* Ignore X-Parental-Controls=none */
          if (g_strcmp0 (parental_controls_type, "none") == 0)
            continue;

          executable = g_strdup (g_app_info_get_executable (app));
          g_debug ("Processing app ‘%s’ (Exec=%s, X-Parental-Controls=%s)",
                   g_app_info_get_id (app),
                   executable,
                   parental_controls_type);

          /* Have we seen this executable before? */
          if (!g_hash_table_add (seen_executables, g_steal_pointer (&executable)))
            {
              g_debug (" → Skipping ‘%s’ due to seeing its executable already",
                       g_app_info_get_id (app));
              continue;
            }
        }

      g_list_store_insert_sorted (self->apps,
                                  app,
                                  compare_app_info_cb,
                                  self);
    }

  /* Update the cache for next time. */
  self->cached_apps = g_steal_pointer (&new_apps);
}

static void
app_info_changed_cb (GAppInfoMonitor *monitor,
                     gpointer         user_data)
{
  MctRestrictApplicationsSelector *self = MCT_RESTRICT_APPLICATIONS_SELECTOR (user_data);

  reload_apps (self);
}

/* Will return %NULL if @flatpak_id is not installed. */
static gchar *
get_flatpak_ref_for_app_id (MctRestrictApplicationsSelector *self,
                            const gchar                     *flatpak_id,
                            GCancellable                    *cancellable)
{
  g_autoptr(FlatpakInstalledRef) ref = NULL;
  g_autoptr(GError) local_error = NULL;

  g_assert (self->system_installation != NULL);
  g_assert (self->user_installation != NULL);

  /* FIXME technically this does local file I/O and should be async */
  ref = flatpak_installation_get_current_installed_app (self->user_installation,
                                                        flatpak_id,
                                                        cancellable,
                                                        &local_error);

  if (local_error != NULL &&
      !g_error_matches (local_error, FLATPAK_ERROR, FLATPAK_ERROR_NOT_INSTALLED))
    {
      g_warning ("Error searching for Flatpak ref: %s", local_error->message);
      return NULL;
    }

  g_clear_error (&local_error);

  if (!ref || !flatpak_installed_ref_get_is_current (ref))
    {
      /* FIXME technically this does local file I/O and should be async */
      ref = flatpak_installation_get_current_installed_app (self->system_installation,
                                                            flatpak_id,
                                                            cancellable,
                                                            &local_error);
      if (local_error != NULL)
        {
          if (!g_error_matches (local_error, FLATPAK_ERROR, FLATPAK_ERROR_NOT_INSTALLED))
            g_warning ("Error searching for Flatpak ref: %s", local_error->message);
          return NULL;
        }
    }

  return flatpak_ref_format_ref (FLATPAK_REF (ref));
}

/**
 * mct_restrict_applications_selector_new:
 * @app_filter: (transfer none): app filter to configure the selector from initially
 *
 * Create a new #MctRestrictApplicationsSelector widget.
 *
 * Returns: (transfer full): a new restricted applications selector
 * Since: 0.5.0
 */
MctRestrictApplicationsSelector *
mct_restrict_applications_selector_new (MctAppFilter *app_filter)
{
  g_return_val_if_fail (app_filter != NULL, NULL);

  return g_object_new (MCT_TYPE_RESTRICT_APPLICATIONS_SELECTOR,
                       "app-filter", app_filter,
                       NULL);
}

/**
 * mct_restrict_applications_selector_build_app_filter:
 * @self: an #MctRestrictApplicationsSelector
 * @builder: an existing #MctAppFilterBuilder to modify
 *
 * Get the app filter settings currently configured in the selector, by modifying
 * the given @builder.
 *
 * Since: 0.5.0
 */
void
mct_restrict_applications_selector_build_app_filter (MctRestrictApplicationsSelector *self,
                                                     MctAppFilterBuilder             *builder)
{
  GDesktopAppInfo *app;
  GHashTableIter iter;

  g_return_if_fail (MCT_IS_RESTRICT_APPLICATIONS_SELECTOR (self));
  g_return_if_fail (builder != NULL);

  g_hash_table_iter_init (&iter, self->blocklisted_apps);
  while (g_hash_table_iter_next (&iter, (gpointer) &app, NULL))
    {
      g_autofree gchar *flatpak_id = NULL;

      flatpak_id = g_desktop_app_info_get_string (app, "X-Flatpak");
      if (flatpak_id)
        flatpak_id = g_strstrip (flatpak_id);

      if (flatpak_id)
        {
          g_autofree gchar *flatpak_ref = get_flatpak_ref_for_app_id (self, flatpak_id, NULL);

          if (!flatpak_ref)
            {
              g_warning ("Skipping blocklisting Flatpak ID ‘%s’ due to it not being installed", flatpak_id);
              continue;
            }

          g_debug ("\t\t → Blocklisting Flatpak ref: %s", flatpak_ref);
          mct_app_filter_builder_blocklist_flatpak_ref (builder, flatpak_ref);
        }
      else
        {
          const gchar *executable = g_app_info_get_executable (G_APP_INFO (app));
          g_autofree gchar *path = g_find_program_in_path (executable);

          if (!path)
            {
              g_warning ("Skipping blocklisting executable ‘%s’ due to it not being found", executable);
              continue;
            }

          g_debug ("\t\t → Blocklisting path: %s", path);
          mct_app_filter_builder_blocklist_path (builder, path);
        }
    }
}

/**
 * mct_restrict_applications_selector_get_app_filter:
 * @self: an #MctRestrictApplicationsSelector
 *
 * Get the value of #MctRestrictApplicationsSelector:app-filter. If the property
 * was originally set to %NULL, this will be the empty app filter.
 *
 * Returns: (transfer none) (not nullable): the initial app filter used to
 *    populate the selector
 * Since: 0.5.0
 */
MctAppFilter *
mct_restrict_applications_selector_get_app_filter (MctRestrictApplicationsSelector *self)
{
  g_return_val_if_fail (MCT_IS_RESTRICT_APPLICATIONS_SELECTOR (self), NULL);

  return self->app_filter;
}

/**
 * mct_restrict_applications_selector_set_app_filter:
 * @self: an #MctRestrictApplicationsSelector
 * @app_filter: (nullable) (transfer none): the app filter to configure the selector
 *    from, or %NULL to use an empty app filter
 *
 * Set the value of #MctRestrictApplicationsSelector:app-filter.
 *
 * This will overwrite any user changes to the selector, so they should be saved
 * first using mct_restrict_applications_selector_build_app_filter() if desired.
 *
 * Since: 0.5.0
 */
void
mct_restrict_applications_selector_set_app_filter (MctRestrictApplicationsSelector *self,
                                                   MctAppFilter                    *app_filter)
{
  g_autoptr(MctAppFilter) owned_app_filter = NULL;
  guint n_apps;

  g_return_if_fail (MCT_IS_RESTRICT_APPLICATIONS_SELECTOR (self));

  /* Default app filter, typically for when we’re instantiated by #GtkBuilder. */
  if (app_filter == NULL)
    {
      g_auto(MctAppFilterBuilder) builder = MCT_APP_FILTER_BUILDER_INIT ();
      owned_app_filter = mct_app_filter_builder_end (&builder);
      app_filter = owned_app_filter;
    }

  if (app_filter == self->app_filter)
    return;

  g_clear_pointer (&self->app_filter, mct_app_filter_unref);
  self->app_filter = mct_app_filter_ref (app_filter);

  /* Update the status of each app row. */
  n_apps = g_list_model_get_n_items (G_LIST_MODEL (self->apps));

  for (guint i = 0; i < n_apps; i++)
    {
      GtkListBoxRow *row;
      GtkWidget *box, *w;
      g_autoptr(GList) children = NULL;  /* (element-type GtkWidget) */

      /* Navigate the widget hierarchy set up in create_row_for_app_cb(). */
      row = gtk_list_box_get_row_at_index (self->listbox, i);
      g_assert (row != NULL && GTK_IS_LIST_BOX_ROW (row));

      box = gtk_bin_get_child (GTK_BIN (row));
      g_assert (box != NULL && GTK_IS_BOX (box));

      children = gtk_container_get_children (GTK_CONTAINER (box));
      g_assert (children != NULL);

      w = g_list_nth_data (children, 2);
      g_assert (w != NULL && GTK_IS_SWITCH (w));

      update_listbox_row_switch (self, GTK_SWITCH (w));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APP_FILTER]);
}
