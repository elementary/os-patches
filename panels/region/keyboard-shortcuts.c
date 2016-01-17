/*
 * Copyright (C) 2010 Intel, Inc
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 */

#include <config.h>

#include <glib/gi18n.h>

#include "keyboard-shortcuts.h"
#include "wm-common.h"

#define BINDINGS_SCHEMA "org.gnome.settings-daemon.plugins.media-keys"
#define CUSTOM_KEYS_BASENAME "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings"
#define CUSTOM_SHORTCUTS_ID "custom"

typedef struct {
  /* The untranslated name, combine with ->package to translate */
  char *name;
  /* The group of keybindings (system or application) */
  char *group;
  /* The gettext package to use to translate the section title */
  char *package;
  /* Name of the window manager the keys would apply to */
  char *wm_name;
  /* The GSettings schema for the whole file, if any */
  char *schema;
  /* an array of KeyListEntry */
  GArray *entries;
} KeyList;

typedef struct
{
  CcRegionKeyboardItemType type;
  char *schema; /* GSettings schema name, if any */
  char *description; /* description for GSettings types */
  char *gettext_package;
  char *name; /* GSettings schema path, or GSettings key name depending on type */
} KeyListEntry;

static GSettings *binding_settings = NULL;
static GHashTable *kb_system_sections = NULL;
static GHashTable *kb_apps_sections = NULL;
static GHashTable *kb_user_sections = NULL;

static void
free_key_array (GPtrArray *keys)
{
  if (keys != NULL)
    {
      gint i;

      for (i = 0; i < keys->len; i++)
        {
          CcRegionKeyboardItem *item;

          item = g_ptr_array_index (keys, i);

          g_object_unref (item);
        }

      g_ptr_array_free (keys, TRUE);
    }
}

static GHashTable *
get_hash_for_group (BindingGroupType group)
{
  GHashTable *hash;

  switch (group)
    {
    case BINDING_GROUP_SYSTEM:
      hash = kb_system_sections;
      break;
    case BINDING_GROUP_APPS:
      hash = kb_apps_sections;
      break;
    case BINDING_GROUP_USER:
      hash = kb_user_sections;
      break;
    default:
      hash = NULL;
    }
  return hash;
}

static gboolean
have_key_for_group (int group, const gchar *name)
{
  GHashTableIter iter;
  GPtrArray *keys;
  gint i;

  g_hash_table_iter_init (&iter, get_hash_for_group (group));
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&keys))
    {
      for (i = 0; i < keys->len; i++)
        {
          CcRegionKeyboardItem *item = g_ptr_array_index (keys, i);

	  if (item->type == CC_REGION_KEYBOARD_ITEM_TYPE_GSETTINGS &&
	      g_strcmp0 (name, item->key) == 0)
	    {
	          return TRUE;
	    }

	  return FALSE;
        }
    }

  return FALSE;
}

static void
append_section (const gchar        *id,
                BindingGroupType    group,
                const KeyListEntry *keys_list)
{
  GPtrArray *keys_array;
  gint i;
  GHashTable *hash;
  gboolean is_new;

  hash = get_hash_for_group (group);
  if (!hash)
    return;

  /* Add all CcRegionKeyboardItems for this section */
  is_new = FALSE;
  keys_array = g_hash_table_lookup (hash, id);
  if (keys_array == NULL)
    {
      keys_array = g_ptr_array_new ();
      is_new = TRUE;
    }

  for (i = 0; keys_list != NULL && keys_list[i].name != NULL; i++)
    {
      CcRegionKeyboardItem *item;
      gboolean ret;

      if (have_key_for_group (group, keys_list[i].name))
        continue;

      item = cc_region_keyboard_item_new (keys_list[i].type);
      switch (keys_list[i].type)
        {
	case CC_REGION_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH:
          ret = cc_region_keyboard_item_load_from_gsettings_path (item, keys_list[i].name, FALSE);
          break;
	case CC_REGION_KEYBOARD_ITEM_TYPE_GSETTINGS:
	  ret = cc_region_keyboard_item_load_from_gsettings (item,
	                                                     keys_list[i].description,
	                                                     keys_list[i].schema,
	                                                     keys_list[i].name);
	  break;
	default:
	  g_assert_not_reached ();
	}

      if (ret == FALSE)
        {
          /* We don't actually want to popup a dialog - just skip this one */
          g_object_unref (item);
          continue;
        }

      item->group = group;

      g_ptr_array_add (keys_array, item);
    }

  /* Add the keys to the hash table */
  if (is_new)
    g_hash_table_insert (hash, g_strdup (id), keys_array);
}

static void
parse_start_tag (GMarkupParseContext *ctx,
                 const gchar         *element_name,
                 const gchar        **attr_names,
                 const gchar        **attr_values,
                 gpointer             user_data,
                 GError             **error)
{
  KeyList *keylist = (KeyList *) user_data;
  KeyListEntry key;
  const char *name, *schema, *description, *package;

  name = NULL;
  schema = NULL;
  package = NULL;

  /* The top-level element, names the section in the tree */
  if (g_str_equal (element_name, "KeyListEntries"))
    {
      const char *wm_name = NULL;
      const char *group = NULL;

      while (*attr_names && *attr_values)
        {
          if (g_str_equal (*attr_names, "name"))
            {
              if (**attr_values)
                name = *attr_values;
            } else if (g_str_equal (*attr_names, "group")) {
              if (**attr_values)
                group = *attr_values;
            } else if (g_str_equal (*attr_names, "wm_name")) {
              if (**attr_values)
                wm_name = *attr_values;
	    } else if (g_str_equal (*attr_names, "schema")) {
	      if (**attr_values)
	        schema = *attr_values;
            } else if (g_str_equal (*attr_names, "package")) {
              if (**attr_values)
                package = *attr_values;
            }
          ++attr_names;
          ++attr_values;
        }

      if (name)
        {
          if (keylist->name)
            g_warning ("Duplicate section name");
          g_free (keylist->name);
          keylist->name = g_strdup (name);
        }
      if (wm_name)
        {
          if (keylist->wm_name)
            g_warning ("Duplicate window manager name");
          g_free (keylist->wm_name);
          keylist->wm_name = g_strdup (wm_name);
        }
      if (package)
        {
          if (keylist->package)
            g_warning ("Duplicate gettext package name");
          g_free (keylist->package);
          keylist->package = g_strdup (package);
	  bind_textdomain_codeset (keylist->package, "UTF-8");
        }
      if (group)
        {
          if (keylist->group)
            g_warning ("Duplicate group");
          g_free (keylist->group);
          keylist->group = g_strdup (group);
        }
      if (schema)
        {
          if (keylist->schema)
            g_warning ("Duplicate schema");
          g_free (keylist->schema);
          keylist->schema = g_strdup (schema);
	}
      return;
    }

  if (!g_str_equal (element_name, "KeyListEntry")
      || attr_names == NULL
      || attr_values == NULL)
    return;

  schema = NULL;
  description = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "name"))
        {
          /* skip if empty */
          if (**attr_values)
            name = *attr_values;
	} else if (g_str_equal (*attr_names, "schema")) {
	  if (**attr_values) {
	   schema = *attr_values;
	  }
	} else if (g_str_equal (*attr_names, "description")) {
          if (**attr_values) {
            if (keylist->package)
	      {
	        description = dgettext (keylist->package, *attr_values);
	      }
	    else
	      {
	        description = _(*attr_values);
	      }
	  }
        }

      ++attr_names;
      ++attr_values;
    }

  if (name == NULL)
    return;

  if (schema == NULL &&
      keylist->schema == NULL) {
    g_debug ("Ignored GConf keyboard shortcut '%s'", name);
    return;
  }

  key.name = g_strdup (name);
  key.type = CC_REGION_KEYBOARD_ITEM_TYPE_GSETTINGS;
  key.description = g_strdup (description);
  key.gettext_package = g_strdup (keylist->package);
  key.schema = schema ? g_strdup (schema) : g_strdup (keylist->schema);
  g_array_append_val (keylist->entries, key);
}

static gboolean
strv_contains (char **strv,
               char  *str)
{
  char **p = strv;
  for (p = strv; *p; p++)
    if (strcmp (*p, str) == 0)
      return TRUE;

  return FALSE;
}

static void
append_sections_from_file (const gchar *path, const char *datadir, gchar **wm_keybindings)
{
  GError *err = NULL;
  char *buf;
  gsize buf_len;
  KeyList *keylist;
  KeyListEntry key, *keys;
  int group;
  guint i;
  GMarkupParseContext *ctx;
  GMarkupParser parser = { parse_start_tag, NULL, NULL, NULL, NULL };

  /* Parse file */
  if (!g_file_get_contents (path, &buf, &buf_len, &err))
    return;

  keylist = g_new0 (KeyList, 1);
  keylist->entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));
  ctx = g_markup_parse_context_new (&parser, 0, keylist, NULL);

  if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err))
    {
      g_warning ("Failed to parse '%s': '%s'", path, err->message);
      g_error_free (err);
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      for (i = 0; i < keylist->entries->len; i++)
        g_free (((KeyListEntry *) &(keylist->entries->data[i]))->name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      keylist = NULL;
    }
  g_markup_parse_context_free (ctx);
  g_free (buf);

  if (keylist == NULL)
    return;

  /* If there's no keys to add, or the settings apply to a window manager
   * that's not the one we're running */
  if (keylist->entries->len == 0
      || (keylist->wm_name != NULL && !strv_contains (wm_keybindings, keylist->wm_name))
      || keylist->name == NULL)
    {
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      return;
    }

  /* Empty KeyListEntry to end the array */
  key.name = NULL;
  g_array_append_val (keylist->entries, key);

  keys = (KeyListEntry *) g_array_free (keylist->entries, FALSE);
  if (keylist->package)
    {
      char *localedir;

      localedir = g_build_filename (datadir, "locale", NULL);
      bindtextdomain (keylist->package, localedir);
      g_free (localedir);
    }
  if (keylist->group && strcmp (keylist->group, "system") == 0)
    group = BINDING_GROUP_SYSTEM;
  else
    group = BINDING_GROUP_APPS;

  append_section (keylist->name, group, keys);

  g_free (keylist->name);
  g_free (keylist->package);
  g_free (keylist->wm_name);
  g_free (keylist->schema);
  g_free (keylist->group);

  for (i = 0; keys[i].name != NULL; i++) {
    KeyListEntry *entry = &keys[i];
    g_free (entry->schema);
    g_free (entry->description);
    g_free (entry->gettext_package);
    g_free (entry->name);
  }

  g_free (keylist);
}

static void
append_sections_from_gsettings (void)
{
  char **custom_paths;
  GArray *entries;
  KeyListEntry key;
  int i;

  /* load custom shortcuts from GSettings */
  entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));

  custom_paths = g_settings_get_strv (binding_settings, "custom-keybindings");
  for (i = 0; custom_paths[i]; i++)
    {
      key.name = g_strdup (custom_paths[i]);
      if (!have_key_for_group (BINDING_GROUP_USER, key.name))
        {
          key.type = CC_REGION_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH;
          g_array_append_val (entries, key);
        }
      else
        g_free (key.name);
    }
  g_strfreev (custom_paths);

  if (entries->len > 0)
    {
      KeyListEntry *keys;
      int i;

      /* Empty KeyListEntry to end the array */
      key.name = NULL;
      g_array_append_val (entries, key);

      keys = (KeyListEntry *) entries->data;
      append_section (CUSTOM_SHORTCUTS_ID, BINDING_GROUP_USER, keys);
      for (i = 0; i < entries->len; ++i)
        {
          g_free (keys[i].name);
        }
    }
  else
    {
      append_section (CUSTOM_SHORTCUTS_ID, BINDING_GROUP_USER, NULL);
    }

  g_array_free (entries, TRUE);
}

static void
reload_sections (void)
{
  gchar **wm_keybindings;
  GDir *dir;
  const gchar * const * data_dirs;
  guint i;
  GHashTable *loaded_files;
  const char *section_to_set;

  /* Clear previous hash tables */
  if (kb_system_sections != NULL)
    g_hash_table_destroy (kb_system_sections);
  kb_system_sections = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              g_free,
                                              (GDestroyNotify) free_key_array);

  if (kb_apps_sections != NULL)
    g_hash_table_destroy (kb_apps_sections);
  kb_apps_sections = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            (GDestroyNotify) free_key_array);

  if (kb_user_sections != NULL)
    g_hash_table_destroy (kb_user_sections);
  kb_user_sections = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            (GDestroyNotify) free_key_array);

  /* Load WM keybindings */
  wm_keybindings = wm_common_get_current_keybindings ();

  loaded_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  data_dirs = g_get_system_data_dirs ();
  for (i = 0; data_dirs[i] != NULL; i++)
    {
      char *dir_path;
      const gchar *name;

      dir_path = g_build_filename (data_dirs[i], "unity-control-center", "keybindings", NULL);

      dir = g_dir_open (dir_path, 0, NULL);
      if (!dir)
        {
          g_free (dir_path);
          continue;
        }

      for (name = g_dir_read_name (dir) ; name ; name = g_dir_read_name (dir))
        {
          gchar *path;

	  if (g_str_has_suffix (name, ".xml") == FALSE)
	    continue;

          if (g_hash_table_lookup (loaded_files, name) != NULL)
	    {
	      g_debug ("Not loading %s, it was already loaded from another directory", name);
              continue;
	    }

	  g_hash_table_insert (loaded_files, g_strdup (name), GINT_TO_POINTER (1));
	  path = g_build_filename (dir_path, name, NULL);
	  append_sections_from_file (path, data_dirs[i], wm_keybindings);
	  g_free (path);
	}
      g_free (dir_path);
      g_dir_close (dir);
    }

  g_hash_table_destroy (loaded_files);
  g_strfreev (wm_keybindings);

  /* Load custom keybindings */
  append_sections_from_gsettings ();
}

static const guint forbidden_keyvals[] = {
  /* Navigation keys */
  GDK_KEY_Home,
  GDK_KEY_Left,
  GDK_KEY_Up,
  GDK_KEY_Right,
  GDK_KEY_Down,
  GDK_KEY_Page_Up,
  GDK_KEY_Page_Down,
  GDK_KEY_End,

  /* Return */
  GDK_KEY_KP_Enter,
  GDK_KEY_Return,

  GDK_KEY_Mode_switch
};

static char*
binding_name (guint                   keyval,
              guint                   keycode,
              GdkModifierType         mask,
              gboolean                translate)
{
  if (keyval != 0 || keycode != 0)
    return translate ?
        gtk_accelerator_get_label_with_keycode (NULL, keyval, keycode, mask) :
        gtk_accelerator_name_with_keycode (NULL, keyval, keycode, mask);
  else
    return g_strdup (translate ? _("Disabled") : "");
}

static gboolean
keyval_is_forbidden (guint keyval)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS(forbidden_keyvals); i++) {
    if (keyval == forbidden_keyvals[i])
      return TRUE;
  }

  return FALSE;
}

typedef struct {
  CcRegionKeyboardItem *orig_item;
  CcRegionKeyboardItem *conflict_item;
  guint new_keyval;
  GdkModifierType new_mask;
  guint new_keycode;
} CcUniquenessData;

static gboolean
compare_keys_for_uniqueness (CcRegionKeyboardItem *element,
                             CcUniquenessData     *data)
{
  CcRegionKeyboardItem *orig_item;

  orig_item = data->orig_item;

  /* no conflict for : blanks, different modifiers, or ourselves */
  if (element == NULL || data->new_mask != element->mask ||
      cc_region_keyboard_item_equal (orig_item, element))
    return FALSE;

  if (data->new_keyval != 0) {
      if (data->new_keyval != element->keyval)
          return FALSE;
  } else if (element->keyval != 0 || data->new_keycode != element->keycode)
    return FALSE;

  data->conflict_item = element;

  return TRUE;
}

static gboolean
cb_check_for_uniqueness (gpointer          key,
                         GPtrArray        *keys_array,
                         CcUniquenessData *data)
{
  guint i;

  for (i = 0; i < keys_array->len; i++)
    {
      CcRegionKeyboardItem *item;

      item = keys_array->pdata[i];
      if (compare_keys_for_uniqueness (item, data))
        return TRUE;
    }
  return FALSE;
}

gboolean
keyboard_shortcuts_accel_edited (CcRegionKeyboardItem *item,
                                 guint                 keyval,
                                 guint                 keycode,
                                 GdkModifierType       mask,
                                 GtkWidget            *toplevel)
{
  CcUniquenessData data;

  /* sanity check */
  if (item == NULL)
    return FALSE;

  /* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
  mask &= ~GDK_LOCK_MASK;

  data.orig_item = item;
  data.new_keyval = keyval;
  data.new_mask = mask;
  data.new_keycode = keycode;
  data.conflict_item = NULL;

  if (keyval != 0 || keycode != 0) /* any number of shortcuts can be disabled */
    {
      BindingGroupType i;

      for (i = BINDING_GROUP_SYSTEM; i <= BINDING_GROUP_USER && data.conflict_item == NULL; i++)
        {
          GHashTable *table;

          table = get_hash_for_group (i);
          if (!table)
            continue;
          g_hash_table_find (table, (GHRFunc) cb_check_for_uniqueness, &data);
        }
    }

  /* Check for unmodified keys */
  if ((mask == 0 || mask == GDK_SHIFT_MASK) && keycode != 0)
    {
      if ((keyval >= GDK_KEY_a && keyval <= GDK_KEY_z)
           || (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z)
           || (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9)
           || (keyval >= GDK_KEY_kana_fullstop && keyval <= GDK_KEY_semivoicedsound)
           || (keyval >= GDK_KEY_Arabic_comma && keyval <= GDK_KEY_Arabic_sukun)
           || (keyval >= GDK_KEY_Serbian_dje && keyval <= GDK_KEY_Cyrillic_HARDSIGN)
           || (keyval >= GDK_KEY_Greek_ALPHAaccent && keyval <= GDK_KEY_Greek_omega)
           || (keyval >= GDK_KEY_hebrew_doublelowline && keyval <= GDK_KEY_hebrew_taf)
           || (keyval >= GDK_KEY_Thai_kokai && keyval <= GDK_KEY_Thai_lekkao)
           || (keyval >= GDK_KEY_Hangul && keyval <= GDK_KEY_Hangul_Special)
           || (keyval >= GDK_KEY_Hangul_Kiyeog && keyval <= GDK_KEY_Hangul_J_YeorinHieuh)
           || (keyval == GDK_KEY_Tab && mask == 0)
           || (keyval == GDK_KEY_space && mask == 0)
           || keyval_is_forbidden (keyval)) {
        GtkWidget *dialog;
        char *name;

        name = binding_name (keyval, keycode, mask, TRUE);

        dialog =
          gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_WARNING,
                                  GTK_BUTTONS_CANCEL,
                                  _("The shortcut \"%s\" cannot be used because it will become impossible to type using this key.\n"
                                  "Please try with a key such as Control, Alt or Shift at the same time."),
                                  name);

        g_free (name);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        return FALSE;
      }
    }

  /* flag to see if the new accelerator was in use by something */
  if (data.conflict_item != NULL)
    {
      GtkWidget *dialog;
      char *name;
      int response;

      name = binding_name (keyval, keycode, mask, TRUE);

      dialog =
        gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                GTK_MESSAGE_WARNING,
                                GTK_BUTTONS_CANCEL,
                                _("The shortcut \"%s\" is already used for\n\"%s\""),
                                name, data.conflict_item->description);
      g_free (name);

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
          _("If you reassign the shortcut to \"%s\", the \"%s\" shortcut "
            "will be disabled."),
          item->description,
          data.conflict_item->description);

      gtk_dialog_add_button (GTK_DIALOG (dialog),
                             _("_Reassign"),
                             GTK_RESPONSE_ACCEPT);

      gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_ACCEPT);

      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (response == GTK_RESPONSE_ACCEPT)
        g_object_set (G_OBJECT (data.conflict_item), "binding", "", NULL);
      else
        return FALSE;
    }

  return TRUE;
}

static void
on_window_manager_change (const char *wm_name,
                          gpointer    user_data)
{
  reload_sections ();
}

void
keyboard_shortcuts_init (void)
{
  wm_common_register_window_manager_change ((GFunc) on_window_manager_change, NULL);
  binding_settings = g_settings_new (BINDINGS_SCHEMA);
  reload_sections ();
}

void
keyboard_shortcuts_dispose (void)
{
  if (kb_system_sections != NULL)
    {
      g_hash_table_destroy (kb_system_sections);
      kb_system_sections = NULL;
    }
  if (kb_apps_sections != NULL)
    {
      g_hash_table_destroy (kb_apps_sections);
      kb_apps_sections = NULL;
    }
  if (kb_user_sections != NULL)
    {
      g_hash_table_destroy (kb_user_sections);
      kb_user_sections = NULL;
    }

  g_clear_object (&binding_settings);
}

static CcRegionKeyboardItem *
get_item_in_group (BindingGroupType  group,
                   const gchar      *schema,
                   const gchar      *key)
{
  GHashTable *hash_table = get_hash_for_group (group);
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, hash_table);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      GPtrArray *array = value;
      guint i;

      for (i = 0; i < array->len; i++)
        {
          CcRegionKeyboardItem *item = array->pdata[i];

          if (g_strcmp0 (item->schema, schema) == 0 &&
              g_strcmp0 (item->key, key) == 0)
            return item;
        }
    }

  return NULL;
}

CcRegionKeyboardItem *
keyboard_shortcuts_get_item (const gchar *schema,
                             const gchar *key)
{
  CcRegionKeyboardItem *item = get_item_in_group (BINDING_GROUP_SYSTEM, schema, key);

  if (item != NULL)
    return item;

  item = get_item_in_group (BINDING_GROUP_APPS, schema, key);

  if (item != NULL)
    return item;

  item = get_item_in_group (BINDING_GROUP_USER, schema, key);

  return item;
}
