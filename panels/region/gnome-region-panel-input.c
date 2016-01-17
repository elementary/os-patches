/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#endif

#ifdef HAVE_FCITX
#include <fcitx-config/fcitx-config.h>
#include <fcitx-gclient/fcitxinputmethod.h>
#include <fcitx-gclient/fcitxkbd.h>
#endif

#include "gdm-languages.h"
#include "gnome-region-panel-input.h"
#include "keyboard-shortcuts.h"
#include "gtkentryaccel.h"

#define WID(s) GTK_WIDGET(gtk_builder_get_object (builder, s))

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"
#define KEY_CURRENT_INPUT_SOURCE        "current"
#define KEY_INPUT_SOURCES               "sources"
#define INPUT_SOURCE_TYPE_XKB           "xkb"
#define INPUT_SOURCE_TYPE_IBUS          "ibus"
#define INPUT_SOURCE_TYPE_FCITX         "fcitx"
#define FCITX_XKB_PREFIX                "fcitx-keyboard-"

#define ENV_GTK_IM_MODULE   "GTK_IM_MODULE"
#define GTK_IM_MODULE_IBUS  "ibus"
#define GTK_IM_MODULE_FCITX "fcitx"

#define MEDIA_KEYS_SCHEMA_ID  "org.gnome.desktop.wm.keybindings"
#define KEY_PREV_INPUT_SOURCE "switch-input-source-backward"
#define KEY_NEXT_INPUT_SOURCE "switch-input-source"

#define INDICATOR_KEYBOARD_SCHEMA_ID "com.canonical.indicator.keyboard"
#define KEY_VISIBLE                  "visible"

#define LIBGNOMEKBD_DESKTOP_SCHEMA_ID "org.gnome.libgnomekbd.desktop"
#define KEY_GROUP_PER_WINDOW          "group-per-window"
#define KEY_DEFAULT_GROUP             "default-group"

#define IBUS_PANEL_SCHEMA_ID     "org.freedesktop.ibus.panel"
#define IBUS_ORIENTATION_KEY     "lookup-table-orientation"
#define IBUS_USE_CUSTOM_FONT_KEY "use-custom-font"
#define IBUS_CUSTOM_FONT_KEY     "custom-font"

#define LEGACY_IBUS_XML_DIR   "/usr/share/ibus/component"
#define LEGACY_IBUS_SETUP_DIR "/usr/lib/ibus"
#define LEGACY_IBUS_SETUP_FMT "ibus-setup-%s"

enum {
  NAME_COLUMN,
  TYPE_COLUMN,
  ID_COLUMN,
  COLOUR_COLUMN,
  SETUP_COLUMN,
  LEGACY_SETUP_COLUMN,
  N_COLUMNS
};

static GSettings *input_sources_settings = NULL;
static GSettings *libgnomekbd_settings = NULL;
static GSettings *ibus_panel_settings = NULL;
static GSettings *media_key_settings = NULL;
static GSettings *indicator_settings = NULL;
static GnomeXkbInfo *xkb_info = NULL;
static GtkBuilder *builder = NULL; /* weak pointer */
static GtkWidget *input_chooser = NULL; /* weak pointer */
static CcRegionKeyboardItem *prev_source_item = NULL;
static CcRegionKeyboardItem *next_source_item = NULL;
static GdkRGBA active_colour;
static GdkRGBA inactive_colour;

#ifdef HAVE_IBUS
static IBusBus *ibus = NULL;
static GHashTable *ibus_engines = NULL;
static GCancellable *ibus_cancellable = NULL;
static gboolean is_ibus_active = FALSE;
#endif  /* HAVE_IBUS */

#ifdef HAVE_FCITX
static FcitxInputMethod *fcitx = NULL;
static FcitxKbd *fcitx_keyboard = NULL;
static GHashTable *fcitx_engines = NULL;
static GCancellable *fcitx_cancellable = NULL;
static gboolean is_fcitx_active = FALSE;

struct _FcitxShareStateConfig
{
  FcitxGenericConfig config;
  gboolean config_valid;
  gint share_state;
};

typedef struct _FcitxShareStateConfig FcitxShareStateConfig;

static CONFIG_BINDING_BEGIN (FcitxShareStateConfig)
CONFIG_BINDING_REGISTER ("Program", "ShareStateAmongWindow", share_state)
CONFIG_BINDING_END ()

static CONFIG_DESC_DEFINE (get_fcitx_config_desc, "config.desc")

static FcitxShareStateConfig fcitx_config;
#endif /* HAVE_FCITX */

static void       populate_model             (GtkListStore  *store,
                                              GtkListStore  *active_sources_store);
static GtkWidget *input_chooser_new          (GtkWindow     *main_window,
                                              GtkListStore  *active_sources);
static gboolean   input_chooser_get_selected (GtkWidget     *chooser,
                                              GtkTreeModel **model,
                                              GtkTreeIter   *iter);
static GtkTreeModel *tree_view_get_actual_model (GtkTreeView *tv);

static gboolean
is_unity (void)
{
  return g_strcmp0 (g_getenv ("XDG_CURRENT_DESKTOP"), "Unity") == 0;
}

static gboolean
has_indicator_keyboard (void)
{
	GSettingsSchema *schema;

	if (!is_unity ())
		return FALSE;

	schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (), INDICATOR_KEYBOARD_SCHEMA_ID, TRUE);
	if (schema) {
		g_settings_schema_unref (schema);
		return TRUE;
	}

	return FALSE;
}

static gboolean
strv_contains (const gchar * const *strv,
               const gchar         *str)
{
  const gchar * const *p = strv;
  for (p = strv; *p; p++)
    if (g_strcmp0 (*p, str) == 0)
      return TRUE;

  return FALSE;
}

#ifdef HAVE_IBUS
static void
clear_ibus (void)
{
  g_cancellable_cancel (ibus_cancellable);
  g_clear_object (&ibus_cancellable);
  g_clear_pointer (&ibus_engines, g_hash_table_destroy);
  g_clear_object (&ibus);
}

static gchar *
engine_get_display_name (IBusEngineDesc *engine_desc)
{
  const gchar *name;
  const gchar *language_code;
  const gchar *language;
  gchar *display_name;

  name = ibus_engine_desc_get_longname (engine_desc);
  language_code = ibus_engine_desc_get_language (engine_desc);
  language = ibus_get_language_name (language_code);

  display_name = g_strdup_printf ("%s (%s)", language, name);

  return display_name;
}

static GDesktopAppInfo *
setup_app_info_for_id (const gchar *id)
{
  GDesktopAppInfo *app_info;
  gchar *desktop_file_name;
  gchar **strv;

  strv = g_strsplit (id, ":", 2);
  desktop_file_name = g_strdup_printf ("ibus-setup-%s.desktop", strv[0]);
  g_strfreev (strv);

  app_info = g_desktop_app_info_new (desktop_file_name);
  g_free (desktop_file_name);

  return app_info;
}

typedef struct _IBusXMLState IBusXMLState;

struct _IBusXMLState
{
  GHashTable *table;
  GString    *buffer;
  gchar      *name;
  gchar      *setup;
};

static IBusXMLState *
ibus_xml_state_new (GHashTable *table)
{
  IBusXMLState *state = g_new0 (IBusXMLState, 1);

  state->table = g_hash_table_ref (table);

  return state;
}

static void
ibus_xml_state_free (gpointer data)
{
  if (data)
    {
      IBusXMLState *state = data;

      g_free (state->setup);
      g_free (state->name);

      if (state->buffer)
        g_string_free (state->buffer, TRUE);

      g_hash_table_unref (state->table);

      g_free (state);
    }
}

static void
parse_start (GMarkupParseContext  *context,
             const gchar          *element_name,
             const gchar         **attribute_names,
             const gchar         **attribute_values,
             gpointer              user_data,
             GError              **error)
{
  IBusXMLState *state = user_data;
  const GSList *stack = g_markup_parse_context_get_element_stack (context);
  const gchar *parent = stack->next ? stack->next->data : NULL;

  if (state->buffer)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Didn't expect a tag within a <%s>", element_name);
    }
  else if (parent && g_str_equal (parent, "engine"))
    {
      if (g_str_equal (element_name, "name") && !state->name)
        state->buffer = g_string_new (NULL);
      else if (g_str_equal (element_name, "setup") && !state->setup)
        state->buffer = g_string_new (NULL);
    }
}

static void
parse_end (GMarkupParseContext  *context,
           const gchar          *element_name,
           gpointer              user_data,
           GError              **error)
{
  IBusXMLState *state = user_data;

  /* only set for <name> and <setup> */
  if (state->buffer)
    {
      if (g_str_equal (element_name, "name"))
        state->name = g_string_free (state->buffer, FALSE);
      else /* g_str_equal (element_name, "setup") */
        state->setup = g_string_free (state->buffer, FALSE);

      state->buffer = NULL;
    }
  else if (g_str_equal (element_name, "engine"))
    {
      if (state->name && state->setup)
        g_hash_table_insert (state->table, g_strdup (state->name), g_strdup (state->setup));

      g_free (state->name);
      g_free (state->setup);

      state->name = NULL;
      state->setup = NULL;
    }
}

static void
parse_text (GMarkupParseContext  *context,
            const gchar          *text,
            gsize                 text_len,
            gpointer              user_data,
            GError              **error)
{
  IBusXMLState *state = user_data;

  if (state->buffer)
    g_string_append_len (state->buffer, text, text_len);
}

static void
parse_ibus_component (const gchar *path,
                      const gchar *text,
                      gssize       length,
                      GHashTable  *table)
{
  static const GMarkupParser parser = { parse_start, parse_end, parse_text, NULL, NULL };

  GMarkupParseContext *context;
  GError *error = NULL;

  context = g_markup_parse_context_new (&parser, 0, ibus_xml_state_new (table), ibus_xml_state_free);

  if (!(g_markup_parse_context_parse (context, text, length, &error) && g_markup_parse_context_end_parse (context, &error)))
    {
      g_warning ("Couldn't parse file '%s': %s", path, error->message);
      g_error_free (error);
    }

  g_markup_parse_context_free (context);
}

static void
fetch_setup_entries (GHashTable *table)
{
  GDir *dir;
  const gchar *name;
  GError *error = NULL;

  dir = g_dir_open (LEGACY_IBUS_XML_DIR, 0, &error);

  if (!dir)
    {
      g_warning ("Couldn't open directory '%s': %s", LEGACY_IBUS_XML_DIR, error->message);
      g_error_free (error);
      return;
    }

  for (name = g_dir_read_name (dir); name; name = g_dir_read_name (dir))
    {
      gchar *path;
      gchar *text;
      gssize length;

      path = g_build_filename (LEGACY_IBUS_XML_DIR, name, NULL);

      if (g_file_get_contents (path, &text, &length, &error))
        {
          parse_ibus_component (path, text, length, table);
          g_free (text);
        }
      else
        {
          g_warning ("Couldn't read file '%s': %s", path, error->message);
          g_clear_error (&error);
        }

      g_free (path);
    }

  g_dir_close (dir);
}

static gchar *
legacy_setup_for_id (const gchar *id)
{
  static GHashTable *table;

  const gchar *lookup;
  gchar *name;
  gchar *path;

  if (!table)
    {
      table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      fetch_setup_entries (table);
    }

  lookup = g_hash_table_lookup (table, id);

  if (lookup)
    return g_strdup (lookup);

  name = g_strdup_printf (LEGACY_IBUS_SETUP_FMT, id);
  path = g_build_filename (LEGACY_IBUS_SETUP_DIR, name, NULL);

  g_free (name);

  if (g_access (path, R_OK) != 0)
    {
      g_free (path);
      path = NULL;
    }

  if (path)
    g_hash_table_insert (table, g_strdup (id), g_strdup (path));

  return path;
}

static void
input_chooser_repopulate (GtkListStore *active_sources_store)
{
  GtkBuilder *builder;
  GtkListStore *model;

  if (!input_chooser)
    return;

  builder = g_object_get_data (G_OBJECT (input_chooser), "builder");
  model = GTK_LIST_STORE (gtk_builder_get_object (builder, "input_source_model"));

  gtk_list_store_clear (model);
  populate_model (model, active_sources_store);
}

static void
update_ibus_active_sources (GtkBuilder *builder)
{
  GtkTreeView *tv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *type, *id;
  gboolean ret;

  tv = GTK_TREE_VIEW (WID ("active_input_sources"));
  model = tree_view_get_actual_model (tv);

  ret = gtk_tree_model_get_iter_first (model, &iter);
  while (ret)
    {
      gtk_tree_model_get (model, &iter,
                          TYPE_COLUMN, &type,
                          ID_COLUMN, &id,
                          -1);

      if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
        {
          IBusEngineDesc *engine_desc = NULL;
          GDesktopAppInfo *app_info = NULL;
          gchar *legacy_setup = NULL;
          gchar *display_name = NULL;
          gchar *name = NULL;

          engine_desc = g_hash_table_lookup (ibus_engines, id);
          if (engine_desc)
            {
              display_name = engine_get_display_name (engine_desc);
              name = g_strdup_printf ("%s (IBus)", display_name);
              app_info = setup_app_info_for_id (id);
              legacy_setup = legacy_setup_for_id (id);

              gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                  NAME_COLUMN, name,
                                  COLOUR_COLUMN, is_ibus_active ? &active_colour : &inactive_colour,
                                  SETUP_COLUMN, app_info,
                                  LEGACY_SETUP_COLUMN, legacy_setup,
                                  -1);
              g_free (name);
              g_free (display_name);
              g_free (legacy_setup);
              if (app_info)
                g_object_unref (app_info);
            }
        }

      g_free (type);
      g_free (id);

      ret = gtk_tree_model_iter_next (model, &iter);
    }

  input_chooser_repopulate (GTK_LIST_STORE (model));
}

static void
fetch_ibus_engines_result (GObject      *object,
                           GAsyncResult *result,
                           GtkBuilder   *builder)
{
  gboolean show_all_sources;
  GList *list, *l;
  GError *error;

  error = NULL;
  list = ibus_bus_list_engines_async_finish (ibus, result, &error);

  g_clear_object (&ibus_cancellable);

  if (!list && error)
    {
      g_warning ("Couldn't finish IBus request: %s", error->message);
      g_error_free (error);
      return;
    }

  show_all_sources = g_settings_get_boolean (input_sources_settings, "show-all-sources");

  /* Maps engine ids to engine description objects */
  ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  for (l = list; l; l = l->next)
    {
      IBusEngineDesc *engine = l->data;
      const gchar *engine_id = ibus_engine_desc_get_name (engine);

      if (g_str_has_prefix (engine_id, "xkb:"))
        g_object_unref (engine);
      else
        g_hash_table_replace (ibus_engines, (gpointer)engine_id, engine);
    }
  g_list_free (list);

  update_ibus_active_sources (builder);
}

static void
fetch_ibus_engines (GtkBuilder *builder)
{
  ibus_cancellable = g_cancellable_new ();

  ibus_bus_list_engines_async (ibus,
                               -1,
                               ibus_cancellable,
                               (GAsyncReadyCallback)fetch_ibus_engines_result,
                               builder);
}

static void
maybe_start_ibus (void)
{
  /* IBus doesn't export API in the session bus. The only thing
   * we have there is a well known name which we can use as a
   * sure-fire way to activate it. */
  g_bus_unwatch_name (g_bus_watch_name (G_BUS_TYPE_SESSION,
                                        IBUS_SERVICE_IBUS,
                                        G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL));
}

static void
update_source_radios (GtkBuilder *builder)
{
  GtkWidget *same_source_radio = WID ("same-source-radio");
  GtkWidget *different_source_radio = WID ("different-source-radio");
  GtkWidget *default_source_radio = WID ("default-source-radio");
  GtkWidget *current_source_radio = WID ("current-source-radio");
  gboolean group_per_window = g_settings_get_boolean (libgnomekbd_settings, KEY_GROUP_PER_WINDOW);
  gboolean default_group = g_settings_get_int (libgnomekbd_settings, KEY_DEFAULT_GROUP) >= 0;

  gtk_widget_set_sensitive (default_source_radio, group_per_window);
  gtk_widget_set_sensitive (current_source_radio, group_per_window);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (different_source_radio)) != group_per_window)
    {
      if (group_per_window)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (different_source_radio), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (same_source_radio), TRUE);
    }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (default_source_radio)) != default_group)
    {
      if (default_group)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (default_source_radio), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (current_source_radio), TRUE);
    }
}

static void
source_radio_toggled (GtkToggleButton *widget,
                      gpointer         user_data)
{
  GtkWidget *same_source_radio = WID ("same-source-radio");
  GtkWidget *different_source_radio = WID ("different-source-radio");
  GtkWidget *default_source_radio = WID ("default-source-radio");
  GtkWidget *current_source_radio = WID ("current-source-radio");
  gboolean different_source_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (different_source_radio));
  gboolean default_source_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (default_source_radio));
  gboolean group_per_window = g_settings_get_boolean (libgnomekbd_settings, KEY_GROUP_PER_WINDOW);
  gboolean default_group = g_settings_get_int (libgnomekbd_settings, KEY_DEFAULT_GROUP) >= 0;

  if (different_source_active != group_per_window)
    g_settings_set_boolean (libgnomekbd_settings, KEY_GROUP_PER_WINDOW, different_source_active);

  if (default_source_active != default_group)
    g_settings_set_int (libgnomekbd_settings, KEY_DEFAULT_GROUP, default_source_active ? 0 : -1);

  gtk_widget_set_sensitive (default_source_radio, different_source_active);
  gtk_widget_set_sensitive (current_source_radio, different_source_active);
}

static void
ibus_connected (IBusBus  *bus,
                gpointer  user_data)
{
  GtkBuilder *builder = user_data;

  fetch_ibus_engines (builder);

#ifdef HAVE_FCITX
  if (has_indicator_keyboard () && !is_fcitx_active)
#else
  if (has_indicator_keyboard ())
#endif
    update_source_radios (builder);

  /* We've got everything we needed, don't want to be called again. */
  g_signal_handlers_disconnect_by_func (ibus, ibus_connected, builder);
}
#endif  /* HAVE_IBUS */

static gboolean
add_source_to_table (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      data)
{
  GHashTable *hash = data;
  gchar *type;
  gchar *id;

  gtk_tree_model_get (model, iter,
                      TYPE_COLUMN, &type,
                      ID_COLUMN, &id,
                      -1);

  g_hash_table_add (hash, g_strconcat (type, id, NULL));

  g_free (type);
  g_free (id);

  return FALSE;
}

static void
populate_model (GtkListStore *store,
                GtkListStore *active_sources_store)
{
  GHashTable *active_sources_table;
  GtkTreeIter iter;
  const gchar *name;
  GList *sources, *tmp;
  gchar *source_id = NULL;

  active_sources_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  gtk_tree_model_foreach (GTK_TREE_MODEL (active_sources_store),
                          add_source_to_table,
                          active_sources_table);

  sources = gnome_xkb_info_get_all_layouts (xkb_info);

  for (tmp = sources; tmp; tmp = tmp->next)
    {
      g_free (source_id);
      source_id = g_strconcat (INPUT_SOURCE_TYPE_XKB, tmp->data, NULL);

      if (g_hash_table_contains (active_sources_table, source_id))
        continue;

      gnome_xkb_info_get_layout_info (xkb_info, (const gchar *)tmp->data,
                                      &name, NULL, NULL, NULL);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          NAME_COLUMN, name,
                          TYPE_COLUMN, INPUT_SOURCE_TYPE_XKB,
                          ID_COLUMN, tmp->data,
                          COLOUR_COLUMN, &active_colour,
                          -1);
    }
  g_free (source_id);

  g_list_free (sources);

#ifdef HAVE_IBUS
  if (ibus_engines)
    {
      gchar *display_name;
      gchar *name;

      sources = g_hash_table_get_keys (ibus_engines);

      source_id = NULL;
      for (tmp = sources; tmp; tmp = tmp->next)
        {
          g_free (source_id);
          source_id = g_strconcat (INPUT_SOURCE_TYPE_IBUS, tmp->data, NULL);

          if (g_hash_table_contains (active_sources_table, source_id))
            continue;

          display_name = engine_get_display_name (g_hash_table_lookup (ibus_engines, tmp->data));
          name = g_strdup_printf ("%s (IBus)", display_name);

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              NAME_COLUMN, name,
                              TYPE_COLUMN, INPUT_SOURCE_TYPE_IBUS,
                              ID_COLUMN, tmp->data,
                              COLOUR_COLUMN, is_ibus_active ? &active_colour : &inactive_colour,
                              -1);
          g_free (name);
          g_free (display_name);
        }
      g_free (source_id);

      g_list_free (sources);
    }
#endif

#ifdef HAVE_FCITX
  if (fcitx_engines)
    {
      GHashTableIter engines_iter;
      gpointer key;
      gpointer value;

      g_hash_table_iter_init (&engines_iter, fcitx_engines);
      while (g_hash_table_iter_next (&engines_iter, &key, &value))
        {
          const gchar *id = key;
          const FcitxIMItem *engine = value;

          if (g_str_has_prefix (id, FCITX_XKB_PREFIX))
            continue;

          source_id = g_strconcat (INPUT_SOURCE_TYPE_FCITX, id, NULL);

          if (!g_hash_table_contains (active_sources_table, source_id))
            {
              gchar *name = g_strdup_printf ("%s (Fcitx)", engine->name);

              gtk_list_store_append (store, &iter);
              gtk_list_store_set (store, &iter,
                                  TYPE_COLUMN, INPUT_SOURCE_TYPE_FCITX,
                                  ID_COLUMN, id,
                                  NAME_COLUMN, name,
                                  COLOUR_COLUMN, is_fcitx_active ? &active_colour : &inactive_colour,
                                  -1);

              g_free (name);
            }

          g_free (source_id);
        }
    }
#endif

  g_hash_table_destroy (active_sources_table);
}

static void
populate_with_active_sources (GtkListStore *store)
{
  GVariant *sources;
  GVariantIter iter;
  const gchar *name;
  const gchar *type;
  const gchar *id;
  gchar *display_name;
  GDesktopAppInfo *app_info;
  gchar *legacy_setup;
  GtkTreeIter tree_iter;
  gboolean active;

  sources = g_settings_get_value (input_sources_settings, KEY_INPUT_SOURCES);

  g_variant_iter_init (&iter, sources);
  while (g_variant_iter_next (&iter, "(&s&s)", &type, &id))
    {
      display_name = NULL;
      app_info = NULL;
      legacy_setup = NULL;
      active = FALSE;

      if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
        {
          gnome_xkb_info_get_layout_info (xkb_info, id, &name, NULL, NULL, NULL);
          if (!name)
            {
              g_warning ("Couldn't find XKB input source '%s'", id);
              continue;
            }
          display_name = g_strdup (name);
          active = TRUE;
        }
      else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
        {
#ifdef HAVE_IBUS
          IBusEngineDesc *engine_desc = NULL;

          if (ibus_engines)
            engine_desc = g_hash_table_lookup (ibus_engines, id);

          if (engine_desc)
            {
              gchar *engine_name = engine_get_display_name (engine_desc);
              display_name = g_strdup_printf ("%s (IBus)", engine_name);
              app_info = setup_app_info_for_id (id);
              legacy_setup = legacy_setup_for_id (id);
              active = is_ibus_active;
              g_free (engine_name);
            }
#else
          g_warning ("IBus input source type specified but IBus support was not compiled");
          continue;
#endif
        }
      else if (g_str_equal (type, INPUT_SOURCE_TYPE_FCITX))
        {
#ifdef HAVE_FCITX
          if (fcitx_engines)
            {
              const FcitxIMItem *engine = g_hash_table_lookup (fcitx_engines, id);

              if (engine)
                {
                  display_name = g_strdup_printf ("%s (Fcitx)", engine->name);
                  active = is_fcitx_active;
                }
            }
#else
          g_warning ("Fcitx input source type specified but Fcitx support was not compiled");
          continue;
#endif
        }
      else
        {
          g_warning ("Unknown input source type '%s'", type);
          continue;
        }

      gtk_list_store_append (store, &tree_iter);
      gtk_list_store_set (store, &tree_iter,
                          NAME_COLUMN, display_name,
                          TYPE_COLUMN, type,
                          ID_COLUMN, id,
                          COLOUR_COLUMN, active ? &active_colour : &inactive_colour,
                          SETUP_COLUMN, app_info,
                          LEGACY_SETUP_COLUMN, legacy_setup,
                          -1);
      g_free (display_name);
      g_free (legacy_setup);
      if (app_info)
        g_object_unref (app_info);
    }

  g_variant_unref (sources);
}

static void
update_configuration (GtkTreeModel *model)
{
  GtkTreeIter iter;
  gchar *type;
  gchar *id;
  GVariantBuilder builder;
  GVariant *old_sources;
  const gchar *old_current_type;
  const gchar *old_current_id;
  guint old_current_index;
  guint old_n_sources;
  guint index;

  old_sources = g_settings_get_value (input_sources_settings, KEY_INPUT_SOURCES);
  old_current_index = g_settings_get_uint (input_sources_settings, KEY_CURRENT_INPUT_SOURCE);
  old_n_sources = g_variant_n_children (old_sources);

  if (old_n_sources > 0 && old_current_index < old_n_sources)
    {
      g_variant_get_child (old_sources,
                           old_current_index,
                           "(&s&s)",
                           &old_current_type,
                           &old_current_id);
    }
  else
    {
      old_current_type = "";
      old_current_id = "";
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  index = 0;
  gtk_tree_model_get_iter_first (model, &iter);
  do
    {
      gtk_tree_model_get (model, &iter,
                          TYPE_COLUMN, &type,
                          ID_COLUMN, &id,
                          -1);
      if (index != old_current_index &&
          g_str_equal (type, old_current_type) &&
          g_str_equal (id, old_current_id))
        {
          g_settings_set_uint (input_sources_settings, KEY_CURRENT_INPUT_SOURCE, index);
        }
      g_variant_builder_add (&builder, "(ss)", type, id);
      g_free (type);
      g_free (id);
      index += 1;
    }
  while (gtk_tree_model_iter_next (model, &iter));

  g_settings_set_value (input_sources_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
  g_settings_apply (input_sources_settings);

  g_variant_unref (old_sources);
}

static gboolean
get_selected_iter (GtkBuilder    *builder,
                   GtkTreeModel **model,
                   GtkTreeIter   *iter)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("active_input_sources")));

  return gtk_tree_selection_get_selected (selection, model, iter);
}

static gint
idx_from_model_iter (GtkTreeModel *model,
                     GtkTreeIter  *iter)
{
  GtkTreePath *path;
  gint idx;

  path = gtk_tree_model_get_path (model, iter);
  if (path == NULL)
    return -1;

  idx = gtk_tree_path_get_indices (path)[0];
  gtk_tree_path_free (path);

  return idx;
}

static void
update_button_sensitivity (GtkBuilder *builder)
{
  GtkWidget *remove_button;
  GtkWidget *up_button;
  GtkWidget *down_button;
  GtkWidget *show_button;
  GtkWidget *settings_button;
  GtkTreeView *tv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint n_active;
  gint index;
  gboolean settings_sensitive;
  GDesktopAppInfo *app_info;
  gchar *legacy_setup;
  gchar *type;

  remove_button = WID("input_source_remove");
  show_button = WID("input_source_show");
  up_button = WID("input_source_move_up");
  down_button = WID("input_source_move_down");
  settings_button = WID("input_source_settings");

  tv = GTK_TREE_VIEW (WID ("active_input_sources"));
  n_active = gtk_tree_model_iter_n_children (gtk_tree_view_get_model (tv), NULL);

  if (get_selected_iter (builder, &model, &iter))
    {
      index = idx_from_model_iter (model, &iter);
      gtk_tree_model_get (model, &iter,
                          TYPE_COLUMN, &type,
                          SETUP_COLUMN, &app_info,
                          LEGACY_SETUP_COLUMN, &legacy_setup,
                          -1);
    }
  else
    {
      index = -1;
      type = NULL;
      app_info = NULL;
      legacy_setup = NULL;
    }

#ifdef HAVE_FCITX
  settings_sensitive = (index >= 0 && (app_info != NULL || legacy_setup != NULL || g_strcmp0 (type, INPUT_SOURCE_TYPE_FCITX) == 0));
#else
  settings_sensitive = (index >= 0 && (app_info != NULL || legacy_setup != NULL));
#endif

  if (app_info)
    g_object_unref (app_info);

  g_free (legacy_setup);

  gtk_widget_set_sensitive (remove_button, index >= 0 && n_active > 1);
  gtk_widget_set_sensitive (show_button, index >= 0);
  gtk_widget_set_sensitive (up_button, index > 0);
  gtk_widget_set_sensitive (down_button, index >= 0 && index < n_active - 1);
  gtk_widget_set_visible (settings_button, settings_sensitive);
}

static void
set_selected_path (GtkBuilder  *builder,
                   GtkTreePath *path)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("active_input_sources")));

  gtk_tree_selection_select_path (selection, path);
}

static GtkTreeModel *
tree_view_get_actual_model (GtkTreeView *tv)
{
  GtkTreeModel *filtered_store;

  filtered_store = gtk_tree_view_get_model (tv);

  return gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (filtered_store));
}

static void
chooser_response (GtkWidget *chooser, gint response_id, gpointer data)
{
  GtkBuilder *builder = data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GtkTreeModel *model;
      GtkTreeIter iter;

      if (input_chooser_get_selected (chooser, &model, &iter))
        {
          GtkTreeView *tv;
          GtkListStore *child_model;
          GtkTreeIter child_iter, filter_iter;
          gchar *name;
          gchar *type;
          gchar *id;
          GDesktopAppInfo *app_info = NULL;
          gchar *legacy_setup = NULL;

          gtk_tree_model_get (model, &iter,
                              NAME_COLUMN, &name,
                              TYPE_COLUMN, &type,
                              ID_COLUMN, &id,
                              -1);

#ifdef HAVE_IBUS
          if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
            {
              app_info = setup_app_info_for_id (id);
              legacy_setup = legacy_setup_for_id (id);
            }
#endif

          tv = GTK_TREE_VIEW (WID ("active_input_sources"));
          child_model = GTK_LIST_STORE (tree_view_get_actual_model (tv));

          gtk_list_store_append (child_model, &child_iter);

          gtk_list_store_set (child_model, &child_iter,
                              NAME_COLUMN, name,
                              TYPE_COLUMN, type,
                              ID_COLUMN, id,
                              SETUP_COLUMN, app_info,
                              LEGACY_SETUP_COLUMN, legacy_setup,
                              -1);
          g_free (legacy_setup);
          g_free (name);
          g_free (type);
          g_free (id);
          if (app_info)
            g_object_unref (app_info);

          gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (gtk_tree_view_get_model (tv)),
                                                            &filter_iter,
                                                            &child_iter);
          gtk_tree_selection_select_iter (gtk_tree_view_get_selection (tv), &filter_iter);

          update_button_sensitivity (builder);
          update_configuration (GTK_TREE_MODEL (child_model));
        }
      else
        {
          g_debug ("nothing selected, nothing added");
        }
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
add_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkWidget *chooser;
  GtkWidget *toplevel;
  GtkWidget *treeview;
  GtkListStore *active_sources;

  g_debug ("add an input source");

  toplevel = gtk_widget_get_toplevel (WID ("active_input_sources"));
  treeview = WID ("active_input_sources");
  active_sources = GTK_LIST_STORE (tree_view_get_actual_model (GTK_TREE_VIEW (treeview)));

  chooser = input_chooser_new (GTK_WINDOW (toplevel), active_sources);
  g_signal_connect (chooser, "response",
                    G_CALLBACK (chooser_response), builder);
}

static void
remove_selected_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter;
  GtkTreeIter child_iter;
  GtkTreePath *path;

  g_debug ("remove selected input source");

  if (get_selected_iter (builder, &model, &iter) == FALSE)
    return;

  path = gtk_tree_model_get_path (model, &iter);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_list_store_remove (GTK_LIST_STORE (child_model), &child_iter);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    gtk_tree_path_prev (path);

  set_selected_path (builder, path);

  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
move_selected_input_up (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter, prev;
  GtkTreeIter child_iter, child_prev;
  GtkTreePath *path;

  g_debug ("move selected input source up");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  prev = iter;
  if (!gtk_tree_model_iter_previous (model, &prev))
    return;

  path = gtk_tree_model_get_path (model, &prev);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_prev,
                                                    &prev);
  gtk_list_store_swap (GTK_LIST_STORE (child_model), &child_iter, &child_prev);

  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
move_selected_input_down (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter, next;
  GtkTreeIter child_iter, child_next;
  GtkTreePath *path;

  g_debug ("move selected input source down");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  next = iter;
  if (!gtk_tree_model_iter_next (model, &next))
    return;

  path = gtk_tree_model_get_path (model, &next);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_next,
                                                    &next);
  gtk_list_store_swap (GTK_LIST_STORE (child_model), &child_iter, &child_next);

  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
show_selected_layout (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *type;
  gchar *id;
  gchar *kbd_viewer_args;
  const gchar *xkb_layout;
  const gchar *xkb_variant;
  gchar *layout = NULL;
  gchar *variant = NULL;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      TYPE_COLUMN, &type,
                      ID_COLUMN, &id,
                      -1);

  if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
    {
      gnome_xkb_info_get_layout_info (xkb_info, id, NULL, NULL, &xkb_layout, &xkb_variant);

      if (!xkb_layout || !xkb_layout[0])
        {
          g_warning ("Couldn't find XKB input source '%s'", id);
          goto exit;
        }
    }
  else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
    {
#ifdef HAVE_IBUS
      IBusEngineDesc *engine_desc = NULL;

      if (ibus_engines)
        engine_desc = g_hash_table_lookup (ibus_engines, id);

      if (engine_desc)
        {
          xkb_layout = ibus_engine_desc_get_layout (engine_desc);
          xkb_variant = "";
        }
      else
        {
          g_warning ("Couldn't find IBus input source '%s'", id);
          goto exit;
        }
#else
      g_warning ("IBus input source type specified but IBus support was not compiled");
      goto exit;
#endif
    }
  else if (g_str_equal (type, INPUT_SOURCE_TYPE_FCITX))
    {
#ifdef HAVE_FCITX
      if (fcitx_keyboard)
        {
          fcitx_kbd_get_layout_for_im (fcitx_keyboard, id, &layout, &variant);
          xkb_layout = layout;
          xkb_variant = variant;
        }
#else
      g_warning ("Fcitx input source type specified but Fcitx support was not compiled");
      goto exit;
#endif
    }
  else
    {
      g_warning ("Unknown input source type '%s'", type);
      goto exit;
    }

  if (xkb_variant != NULL && xkb_variant[0])
    kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l \"%s\t%s\"",
                                       xkb_layout, xkb_variant);
  else if (xkb_layout != NULL && xkb_layout[0])
    kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l %s",
                                       xkb_layout);
  else
    kbd_viewer_args = g_strdup ("gkbd-keyboard-display -g 1");

  g_spawn_command_line_async (kbd_viewer_args, NULL);

  g_free (kbd_viewer_args);
  g_free (variant);
  g_free (layout);
 exit:
  g_free (type);
  g_free (id);
}

static void
show_selected_settings (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GdkAppLaunchContext *ctx;
  GDesktopAppInfo *app_info;
  gchar *legacy_setup;
  gchar *type;
  gchar *id;
  GError *error = NULL;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      ID_COLUMN, &id,
                      TYPE_COLUMN, &type,
                      SETUP_COLUMN, &app_info,
                      LEGACY_SETUP_COLUMN, &legacy_setup,
                      -1);

  if (app_info)
    {
      ctx = gdk_display_get_app_launch_context (gdk_display_get_default ());
      gdk_app_launch_context_set_timestamp (ctx, gtk_get_current_event_time ());

      g_app_launch_context_setenv (G_APP_LAUNCH_CONTEXT (ctx),
                                   "IBUS_ENGINE_NAME",
                                   id);

      if (!g_app_info_launch (G_APP_INFO (app_info), NULL, G_APP_LAUNCH_CONTEXT (ctx), &error))
        {
          g_warning ("Failed to launch input source setup: %s", error->message);
          g_error_free (error);
        }

      g_object_unref (ctx);
      g_object_unref (app_info);
    }
  else if (legacy_setup)
    {
      if (!g_spawn_command_line_async (legacy_setup, &error))
        {
          g_warning ("Failed to launch input source setup: %s", error->message);
          g_error_free (error);
        }
    }
#ifdef HAVE_FCITX
  else if (g_strcmp0 (type, INPUT_SOURCE_TYPE_FCITX) == 0 && fcitx)
    fcitx_input_method_configure_im (fcitx, id);
#endif

  g_free (legacy_setup);
}

static gboolean
go_to_shortcuts (GtkLinkButton *button,
                 CcRegionPanel *panel)
{
  CcShell *shell;
  const gchar *argv[] = { "shortcuts", "Typing", NULL };
  GError *error = NULL;

  g_clear_object (&input_sources_settings);

  shell = cc_panel_get_shell (CC_PANEL (panel));
  if (!cc_shell_set_active_panel_from_id (shell, "keyboard", argv, &error))
    {
      g_warning ("Failed to activate Keyboard panel: %s", error->message);
      g_error_free (error);
    }

  return TRUE;
}

static void
input_sources_changed (GSettings  *settings,
                       gchar      *key,
                       GtkBuilder *builder)
{
  GtkWidget *treeview;
  GtkTreeModel *store;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeModel *model;

  treeview = WID("active_input_sources");
  store = tree_view_get_actual_model (GTK_TREE_VIEW (treeview));

  if (get_selected_iter (builder, &model, &iter))
    path = gtk_tree_model_get_path (model, &iter);
  else
    path = NULL;

  gtk_list_store_clear (GTK_LIST_STORE (store));
  populate_with_active_sources (GTK_LIST_STORE (store));

  if (path)
    {
      set_selected_path (builder, path);
      gtk_tree_path_free (path);
    }
}

static void
update_shortcut_label (GtkWidget  *widget,
		       const char *value)
{
  char *text;
  guint accel_key, *keycode;
  GdkModifierType mods;

  if (value == NULL || *value == '\0')
    {
      gtk_label_set_text (GTK_LABEL (widget), "\342\200\224");
      return;
    }
  gtk_accelerator_parse_with_keycode (value, &accel_key, &keycode, &mods);
  if (accel_key == 0 && keycode == NULL && mods == 0)
    {
      gtk_label_set_text (GTK_LABEL (widget), "\342\200\224");
      g_warning ("Failed to parse keyboard shortcut: '%s'", value);
      return;
    }

  text = gtk_accelerator_get_label_with_keycode (gtk_widget_get_display (widget), accel_key, *keycode, mods);
  g_free (keycode);
  gtk_label_set_text (GTK_LABEL (widget), text);
  g_free (text);
}

static void
update_shortcuts (GtkBuilder *builder)
{
  char **previous, **next;
  GSettings *settings;

  settings = g_settings_new ("org.gnome.desktop.wm.keybindings");

  previous = g_settings_get_strv (settings, "switch-input-source-backward");
  next = g_settings_get_strv (settings, "switch-input-source");

  update_shortcut_label (WID ("prev-source-shortcut-label"), previous[0]);
  update_shortcut_label (WID ("next-source-shortcut-label"), next[0]);

  g_strfreev (previous);
  g_strfreev (next);
}

static void
libgnomekbd_settings_changed (GSettings *settings,
                              gchar     *key,
                              gpointer   user_data)
{
#ifdef HAVE_FCITX
  if (!is_fcitx_active && (g_strcmp0 (key, KEY_GROUP_PER_WINDOW) == 0 || g_strcmp0 (key, KEY_DEFAULT_GROUP) == 0))
#else
  if (g_strcmp0 (key, KEY_GROUP_PER_WINDOW) == 0 || g_strcmp0 (key, KEY_DEFAULT_GROUP) == 0)
#endif
    update_source_radios (user_data);
}

static gboolean
active_sources_visible_func (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gpointer      data)
{
  gchar *display_name;

  gtk_tree_model_get (model, iter, NAME_COLUMN, &display_name, -1);

  if (!display_name)
    return FALSE;

  g_free (display_name);

  return TRUE;
}

static GtkEntryAccelPostAction
shortcut_key_pressed (GtkEntryAccel   *entry,
                      guint           *key,
                      guint           *code,
                      GdkModifierType *mask,
                      gpointer         user_data)
{
  GtkBuilder *builder = user_data;
  CcRegionKeyboardItem *item = NULL;
  gboolean edited;

  if (entry == GTK_ENTRY_ACCEL (WID ("prev-source-entry")))
    item = prev_source_item;
  else if (entry == GTK_ENTRY_ACCEL (WID ("next-source-entry")))
    item = next_source_item;

  if (*mask == 0 && *key == GDK_KEY_Escape)
    return GTK_ENTRY_ACCEL_CANCEL;

  if (*mask == 0 && *key == GDK_KEY_BackSpace)
    {
      *key = 0;
      *code = 0;
      *mask = 0;

      return GTK_ENTRY_ACCEL_UPDATE;
    }

  if ((*mask & ~GDK_LOCK_MASK) == 0 &&
      (*key == GDK_KEY_Tab ||
       *key == GDK_KEY_KP_Tab ||
       *key == GDK_KEY_ISO_Left_Tab ||
       *key == GDK_KEY_3270_BackTab))
    return GTK_ENTRY_ACCEL_IGNORE;

  edited = keyboard_shortcuts_accel_edited (item,
                                            *key,
                                            *code,
                                            *mask,
                                            gtk_widget_get_toplevel (GTK_WIDGET (entry)));

  return edited ? GTK_ENTRY_ACCEL_UPDATE : GTK_ENTRY_ACCEL_IGNORE;
}

#ifdef HAVE_FCITX
static void
clear_fcitx (void)
{
  if (fcitx_config.config_valid)
    FcitxConfigFree (&fcitx_config.config);

  if (fcitx_cancellable)
    g_cancellable_cancel (fcitx_cancellable);

  g_clear_pointer (&fcitx_engines, g_hash_table_unref);
  g_clear_object (&fcitx_cancellable);
  g_clear_object (&fcitx_keyboard);
  g_clear_object (&fcitx);
}
#endif

static void
builder_finalized (gpointer  data,
                   GObject  *where_the_object_was)
{
  keyboard_shortcuts_dispose ();

  g_clear_object (&input_sources_settings);
  g_clear_object (&libgnomekbd_settings);
  g_clear_object (&ibus_panel_settings);
  g_clear_object (&media_key_settings);
  g_clear_object (&indicator_settings);
  g_clear_object (&next_source_item);
  g_clear_object (&prev_source_item);

#ifdef HAVE_FCITX
  clear_fcitx ();
#endif

#ifdef HAVE_IBUS
  clear_ibus ();
#endif
}

static gboolean
get_key_setting (GValue   *value,
                 GVariant *variant,
                 gpointer  user_data)
{
    const gchar **switch_key;

    switch_key = g_variant_get_strv (variant, NULL);
    g_value_set_string (value, switch_key[0]);


    return TRUE;
}

static GVariant *
set_key_setting (const GValue   *value,
                 const GVariantType *expected_type,
                 gpointer  user_data)
{
    const gchar *switch_key;
    gchar **switch_strv;
    GVariant *ret = NULL;

    switch_strv = g_settings_get_strv(media_key_settings, user_data);
    switch_key = g_value_get_string (value);
    switch_strv[0] = g_strdup (switch_key);

    ret = g_variant_new_strv ((const gchar * const *) switch_strv, -1);

    return ret;
}

#ifdef HAVE_FCITX
static void
fcitx_init (void)
{
  GError *error = NULL;

  fcitx_cancellable = g_cancellable_new ();
  fcitx = fcitx_input_method_new (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  0,
                                  fcitx_cancellable,
                                  &error);
  g_clear_object (&fcitx_cancellable);

  if (fcitx)
    {
      GPtrArray *engines = fcitx_input_method_get_imlist_nofree (fcitx);

      if (engines)
        {
          guint i;

          fcitx_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) fcitx_im_item_free);

          for (i = 0; i < engines->len; i++)
            {
              FcitxIMItem *engine = g_ptr_array_index (engines, i);
              g_hash_table_insert (fcitx_engines, engine->unique_name, engine);
            }

          g_ptr_array_unref (engines);
        }
    }
  else
    {
      g_warning ("Fcitx input method framework unavailable: %s", error->message);
      g_clear_error (&error);
    }

  fcitx_cancellable = g_cancellable_new ();
  fcitx_keyboard = fcitx_kbd_new (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  0,
                                  fcitx_cancellable,
                                  &error);
  g_clear_object (&fcitx_cancellable);

  if (!fcitx_keyboard)
    {
      g_warning ("Fcitx keyboard module unavailable: %s", error->message);
      g_clear_error (&error);
    }
}

static void
save_fcitx_config (void)
{
  if (!fcitx_config.config_valid)
    return;

  FILE *file = FcitxXDGGetFileUserWithPrefix (NULL, "config", "w", NULL);
  FcitxConfigSaveConfigFileFp (file, &fcitx_config.config, get_fcitx_config_desc ());

  if (file)
    fclose (file);

  fcitx_input_method_reload_config (fcitx);
}

static void
load_fcitx_config (void)
{
  static gboolean attempted = FALSE;

  if (attempted)
    return;

  FcitxConfigFileDesc *config_file_desc = get_fcitx_config_desc ();

  if (config_file_desc)
    {
      FILE *file = FcitxXDGGetFileUserWithPrefix (NULL, "config", "r", NULL);

      FcitxConfigFile *config_file = FcitxConfigParseConfigFileFp (file, config_file_desc);
      FcitxShareStateConfigConfigBind (&fcitx_config, config_file, config_file_desc);
      FcitxConfigBindSync (&fcitx_config.config);
      fcitx_config.config_valid = TRUE;

      if (file)
        fclose (file);
    }

  attempted = TRUE;
}

static void
set_share_state (gint share_state)
{
  if (share_state != fcitx_config.share_state)
    {
      fcitx_config.share_state = share_state;
      save_fcitx_config ();
    }
}

static void
share_state_radio_toggled (GtkToggleButton *widget,
                           gpointer         user_data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("share-state-no-radio"))))
    set_share_state (0);
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("share-state-all-radio"))))
    set_share_state (1);
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("share-state-per-program-radio"))))
    set_share_state (2);
}
#endif

void
setup_input_tabs (GtkBuilder    *builder_,
                  CcRegionPanel *panel)
{
  GtkWidget *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkListStore *store;
  GtkTreeModel *filtered_store;
  GtkTreeSelection *selection;
  GtkStyleContext *context;
  const gchar *module;

  builder = builder_;

  g_object_weak_ref (G_OBJECT (builder), builder_finalized, NULL);

  keyboard_shortcuts_init ();

  prev_source_item = g_object_ref (keyboard_shortcuts_get_item (MEDIA_KEYS_SCHEMA_ID,
                                                                KEY_PREV_INPUT_SOURCE));
  next_source_item = g_object_ref (keyboard_shortcuts_get_item (MEDIA_KEYS_SCHEMA_ID,
                                                                KEY_NEXT_INPUT_SOURCE));

  /* set up the list of active inputs */
  treeview = WID("active_input_sources");
  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_add_attribute (column, cell, "text", NAME_COLUMN);
  gtk_tree_view_column_add_attribute (column, cell, "foreground-rgba", COLOUR_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  store = gtk_list_store_new (N_COLUMNS,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              GDK_TYPE_RGBA,
                              G_TYPE_DESKTOP_APP_INFO,
                              G_TYPE_STRING);

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));

  input_sources_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
  libgnomekbd_settings = g_settings_new (LIBGNOMEKBD_DESKTOP_SCHEMA_ID);

  g_settings_delay (input_sources_settings);

  if (!xkb_info)
    xkb_info = gnome_xkb_info_new ();

  context = gtk_widget_get_style_context (treeview);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &active_colour);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_INSENSITIVE, &inactive_colour);

  module = g_getenv (ENV_GTK_IM_MODULE);

#ifdef HAVE_IBUS
  is_ibus_active = g_strcmp0 (module, GTK_IM_MODULE_IBUS) == 0;

  if (is_ibus_active)
    {
      ibus_init ();
      if (!ibus)
        {
          ibus = ibus_bus_new_async ();
          if (ibus_bus_is_connected (ibus))
            ibus_connected (ibus, builder);
          else
            g_signal_connect (ibus, "connected", G_CALLBACK (ibus_connected), builder);
        }
      maybe_start_ibus ();
    }
#endif

#ifdef HAVE_FCITX
  is_fcitx_active = g_strcmp0 (module, GTK_IM_MODULE_FCITX) == 0;

  if (is_fcitx_active)
    fcitx_init ();
#endif

  populate_with_active_sources (store);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect_swapped (selection, "changed",
                            G_CALLBACK (update_button_sensitivity), builder);

  /* Some input source types might have their info loaded
   * asynchronously. In that case we don't want to show them
   * immediately so we use a filter model on top of the real model
   * which mirrors the GSettings key. */
  filtered_store = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filtered_store),
                                          active_sources_visible_func,
                                          NULL,
                                          NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), filtered_store);

  /* set up the buttons */
  g_signal_connect (WID("input_source_add"), "clicked",
                    G_CALLBACK (add_input), builder);
  g_signal_connect (WID("input_source_remove"), "clicked",
                    G_CALLBACK (remove_selected_input), builder);
  g_signal_connect (WID("input_source_move_up"), "clicked",
                    G_CALLBACK (move_selected_input_up), builder);
  g_signal_connect (WID("input_source_move_down"), "clicked",
                    G_CALLBACK (move_selected_input_down), builder);
  g_signal_connect (WID("input_source_show"), "clicked",
                    G_CALLBACK (show_selected_layout), builder);
  g_signal_connect (WID("input_source_settings"), "clicked",
                    G_CALLBACK (show_selected_settings), builder);
  g_signal_connect (WID("jump-to-shortcuts"), "activate-link",
                    G_CALLBACK (go_to_shortcuts), panel);
  g_signal_connect (G_OBJECT (input_sources_settings),
                    "changed::" KEY_INPUT_SOURCES,
                    G_CALLBACK (input_sources_changed),
                    builder);

  if (has_indicator_keyboard ())
    {
      ibus_panel_settings = g_settings_new (IBUS_PANEL_SCHEMA_ID);
      media_key_settings = g_settings_new (MEDIA_KEYS_SCHEMA_ID);
      indicator_settings = g_settings_new (INDICATOR_KEYBOARD_SCHEMA_ID);

      g_settings_bind (indicator_settings,
                       KEY_VISIBLE,
                       WID ("show-indicator-check"),
                       "active",
                       G_SETTINGS_BIND_DEFAULT);

#ifdef HAVE_FCITX
      if (is_fcitx_active)
        {
          load_fcitx_config ();

          switch (fcitx_config.share_state)
            {
              case 0:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("share-state-no-radio")), TRUE);
                break;
              case 1:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("share-state-all-radio")), TRUE);
                break;
              case 2:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("share-state-per-program-radio")), TRUE);
                break;
            }

          g_signal_connect (WID ("share-state-all-radio"), "toggled",
                            G_CALLBACK (share_state_radio_toggled), builder);
          g_signal_connect (WID ("share-state-no-radio"), "toggled",
                            G_CALLBACK (share_state_radio_toggled), builder);
          g_signal_connect (WID ("share-state-per-program-radio"), "toggled",
                            G_CALLBACK (share_state_radio_toggled), builder);
        }
      else
#endif /* HAVE_FCITX */
        {
          update_source_radios (builder);

          g_settings_bind (ibus_panel_settings,
                           IBUS_ORIENTATION_KEY,
                           WID ("orientation-combo"),
                           "active",
                           G_SETTINGS_BIND_DEFAULT);
          g_settings_bind (ibus_panel_settings,
                           IBUS_USE_CUSTOM_FONT_KEY,
                           WID ("custom-font-check"),
                           "active",
                           G_SETTINGS_BIND_DEFAULT);
          g_settings_bind (ibus_panel_settings,
                           IBUS_USE_CUSTOM_FONT_KEY,
                           WID ("custom-font-button"),
                           "sensitive",
                           G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);
          g_settings_bind (ibus_panel_settings,
                           IBUS_CUSTOM_FONT_KEY,
                           WID ("custom-font-button"),
                           "font-name",
                           G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

          g_signal_connect (WID ("same-source-radio"), "toggled",
                            G_CALLBACK (source_radio_toggled), builder);
          g_signal_connect (WID ("different-source-radio"), "toggled",
                            G_CALLBACK (source_radio_toggled), builder);
          g_signal_connect (WID ("default-source-radio"), "toggled",
                            G_CALLBACK (source_radio_toggled), builder);
          g_signal_connect (WID ("current-source-radio"), "toggled",
                            G_CALLBACK (source_radio_toggled), builder);
        }

      g_settings_bind_with_mapping (media_key_settings,
                                    KEY_PREV_INPUT_SOURCE,
                                    WID ("prev-source-entry"),
                                    "accel",
                                    G_SETTINGS_BIND_DEFAULT,
                                    get_key_setting,
                                    set_key_setting,
                                    KEY_PREV_INPUT_SOURCE, NULL);
      g_settings_bind_with_mapping (media_key_settings,
                                    KEY_NEXT_INPUT_SOURCE,
                                    WID ("next-source-entry"),
                                    "accel",
                                    G_SETTINGS_BIND_DEFAULT,
                                    get_key_setting,
                                    set_key_setting,
                                    KEY_NEXT_INPUT_SOURCE, NULL);

      g_signal_connect (WID ("prev-source-entry"), "key-pressed",
                        G_CALLBACK (shortcut_key_pressed), builder);
      g_signal_connect (WID ("next-source-entry"), "key-pressed",
                        G_CALLBACK (shortcut_key_pressed), builder);
      g_signal_connect (libgnomekbd_settings,
                        "changed",
                        G_CALLBACK (libgnomekbd_settings_changed),
                        builder);
    }
  else
    {
      g_settings_bind (input_sources_settings, "per-window",
                       WID("per-window-radio-true"), "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (input_sources_settings, "per-window",
                       WID("per-window-radio-false"), "active",
                       G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
      /* because we are in delay-apply mode */
      g_signal_connect_swapped (WID("per-window-radio-true"), "clicked",
                                G_CALLBACK (g_settings_apply), input_sources_settings);
      g_signal_connect_swapped (WID("per-window-radio-false"), "clicked",
                                G_CALLBACK (g_settings_apply), input_sources_settings);

      update_shortcuts (builder);
    }
}

static void
filter_clear (GtkEntry             *entry,
              GtkEntryIconPosition  icon_pos,
              GdkEvent             *event,
              gpointer              user_data)
{
  gtk_entry_set_text (entry, "");
}

static gchar **search_pattern_list;

static void
filter_changed (GtkBuilder *builder)
{
  GtkTreeModelFilter *filtered_model;
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;
  GtkTreeIter selected_iter;
  GtkWidget *filter_entry;
  const gchar *pattern;
  gchar *upattern;

  filter_entry = WID ("input_source_filter");
  pattern = gtk_entry_get_text (GTK_ENTRY (filter_entry));
  upattern = g_utf8_strup (pattern, -1);
  if (!g_strcmp0 (pattern, ""))
    g_object_set (G_OBJECT (filter_entry),
                  "secondary-icon-name", "edit-find-symbolic",
                  "secondary-icon-activatable", FALSE,
                  "secondary-icon-sensitive", FALSE,
                  NULL);
  else
    g_object_set (G_OBJECT (filter_entry),
                  "secondary-icon-name", "edit-clear-symbolic",
                  "secondary-icon-activatable", TRUE,
                  "secondary-icon-sensitive", TRUE,
                  NULL);

  if (search_pattern_list != NULL)
    g_strfreev (search_pattern_list);

  search_pattern_list = g_strsplit (upattern, " ", -1);
  g_free (upattern);

  filtered_model = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "filtered_input_source_model"));
  gtk_tree_model_filter_refilter (filtered_model);

  tree_view = GTK_TREE_VIEW (WID ("filtered_input_source_list"));
  selection = gtk_tree_view_get_selection (tree_view);
  if (gtk_tree_selection_get_selected (selection, NULL, &selected_iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (filtered_model),
                                                   &selected_iter);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.5);
      gtk_tree_path_free (path);
    }
  else
    {
      GtkTreeIter iter;
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filtered_model), &iter))
        gtk_tree_selection_select_iter (selection, &iter);
    }
}

static void
selection_changed (GtkTreeSelection *selection,
                   GtkBuilder       *builder)
{
  gtk_widget_set_sensitive (WID ("ok-button"),
                            gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
row_activated (GtkTreeView       *tree_view,
               GtkTreePath       *path,
               GtkTreeViewColumn *column,
               GtkBuilder        *builder)
{
  GtkWidget *add_button;
  GtkWidget *dialog;

  add_button = WID ("ok-button");
  dialog = WID ("input_source_chooser");
  if (gtk_widget_is_sensitive (add_button))
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
entry_activated (GtkBuilder *builder,
                 gpointer    data)
{
  row_activated (NULL, NULL, NULL, builder);
}

static gboolean
filter_func (GtkTreeModel *model,
             GtkTreeIter  *iter,
             gpointer      data)
{
  gchar *name = NULL;
  gchar **pattern;
  gboolean rv = TRUE;

  if (search_pattern_list == NULL || search_pattern_list[0] == NULL)
    return TRUE;

  gtk_tree_model_get (model, iter,
                      NAME_COLUMN, &name,
                      -1);

  pattern = search_pattern_list;
  do {
    gboolean is_pattern_found = FALSE;
    gchar *udesc = g_utf8_strup (name, -1);
    if (udesc != NULL && g_strstr_len (udesc, -1, *pattern))
      {
        is_pattern_found = TRUE;
      }
    g_free (udesc);

    if (!is_pattern_found)
      {
        rv = FALSE;
        break;
      }

  } while (*++pattern != NULL);

  g_free (name);

  return rv;
}

static GtkWidget *
input_chooser_new (GtkWindow    *main_window,
                   GtkListStore *active_sources)
{
  GtkBuilder *builder;
  GtkWidget *chooser;
  GtkWidget *filtered_list;
  GtkWidget *filter_entry;
  GtkTreeViewColumn *visible_column;
  GtkTreeSelection *selection;
  GtkListStore *model;
  GtkTreeModelFilter *filtered_model;
  GtkTreeIter iter;

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder,
                             GNOMECC_UI_DIR "/gnome-region-panel-input-chooser.ui",
                             NULL);
  chooser = WID ("input_source_chooser");
  input_chooser = chooser;
  g_object_add_weak_pointer (G_OBJECT (chooser), (gpointer *) &input_chooser);
  g_object_set_data_full (G_OBJECT (chooser), "builder", builder, g_object_unref);

  filtered_list = WID ("filtered_input_source_list");
  filter_entry = WID ("input_source_filter");

  g_object_set_data (G_OBJECT (chooser),
                     "filtered_input_source_list", filtered_list);
  visible_column =
    gtk_tree_view_column_new_with_attributes ("Input Sources",
                                              gtk_cell_renderer_text_new (),
                                              "text", NAME_COLUMN,
                                              "foreground-rgba", COLOUR_COLUMN,
                                              NULL);

  gtk_window_set_transient_for (GTK_WINDOW (chooser), main_window);

  gtk_tree_view_append_column (GTK_TREE_VIEW (filtered_list),
                               visible_column);
  /* We handle searching ourselves, thank you. */
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (filtered_list), FALSE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (filtered_list), -1);

  g_signal_connect_swapped (G_OBJECT (filter_entry), "activate",
                            G_CALLBACK (entry_activated), builder);
  g_signal_connect_swapped (G_OBJECT (filter_entry), "notify::text",
                            G_CALLBACK (filter_changed), builder);

  g_signal_connect (G_OBJECT (filter_entry), "icon-release",
                    G_CALLBACK (filter_clear), NULL);

  filtered_model = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "filtered_input_source_model"));
  model = GTK_LIST_STORE (gtk_builder_get_object (builder, "input_source_model"));

  populate_model (model, active_sources);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
                                        NAME_COLUMN, GTK_SORT_ASCENDING);

  gtk_tree_model_filter_set_visible_func (filtered_model,
                                          filter_func,
                                          NULL, NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (filtered_list));

  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (selection_changed), builder);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filtered_model), &iter))
    gtk_tree_selection_select_iter (selection, &iter);

  g_signal_connect (G_OBJECT (filtered_list), "row-activated",
                    G_CALLBACK (row_activated), builder);

  gtk_widget_grab_focus (filter_entry);

  gtk_widget_show (chooser);

  return chooser;
}

static gboolean
input_chooser_get_selected (GtkWidget     *dialog,
                            GtkTreeModel **model,
                            GtkTreeIter   *iter)
{
  GtkWidget *tv;
  GtkTreeSelection *selection;

  tv = g_object_get_data (G_OBJECT (dialog), "filtered_input_source_list");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));

  return gtk_tree_selection_get_selected (selection, model, iter);
}
