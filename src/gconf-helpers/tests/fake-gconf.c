/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2011 Red Hat, Inc.
 */

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include "fake-gconf.h"

static GConfClient *singleton = NULL;

G_DEFINE_TYPE (GConfClient, gconf_client, G_TYPE_OBJECT)

#define GCONF_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                     GCONF_TYPE_CLIENT, \
                                     GConfClientPrivate))

typedef struct {
	GHashTable *keys;
} GConfClientPrivate;

GQuark
gconf_error_quark (void)
{
	static GQuark quark;

	if (G_UNLIKELY (!quark))
		quark = g_quark_from_static_string ("gconf-error-quark");
	return quark;
}

/****************************************************************/

typedef struct {
	GConfValueType type;
	GConfValueType list_type;
	GSList *v_list;
	char *v_str;
	gint v_int;
	double v_float;
	gboolean v_bool;
} Value;

GConfValue *
gconf_value_new (GConfValueType vtype)
{
	Value *val;

	val = g_malloc0 (sizeof (Value));
	val->type = vtype;
	return (GConfValue *) val;
}

void
gconf_value_free (GConfValue *gv)
{
	Value *value = (Value *) gv;

	g_slist_foreach (value->v_list, (GFunc) gconf_value_free, NULL);
	g_slist_free (value->v_list);
	g_free (value->v_str);
	g_free (value);
}

GConfValue *
gconf_value_copy (const GConfValue *gv)
{
	Value *src = (Value *) gv;
	Value *new;
	GSList *iter;

	new = (Value *) gconf_value_new (src->type);
	new->v_str = g_strdup (src->v_str);
	new->v_float = src->v_float;
	new->v_int = src->v_int;
	new->v_bool = src->v_bool;

	new->list_type = src->list_type;
	for (iter = src->v_list; iter; iter = g_slist_next (iter))
		new->v_list = g_slist_append (new->v_list, gconf_value_copy (iter->data));

	return (GConfValue *) new;
}

int
gconf_value_get_int (const GConfValue *gv)
{
	Value *value = (Value *) gv;

	g_return_val_if_fail (value->type == GCONF_VALUE_INT, 0);
	return value->v_int;
}

double
gconf_value_get_float (const GConfValue *gv)
{
	Value *value = (Value *) gv;

	g_return_val_if_fail (value->type == GCONF_VALUE_FLOAT, 0);
	return value->v_float;
}

const char *
gconf_value_get_string (const GConfValue *gv)
{
	Value *value = (Value *) gv;

	g_return_val_if_fail (value->type == GCONF_VALUE_STRING, NULL);
	return value->v_str;
}

gboolean
gconf_value_get_bool (const GConfValue *gv)
{
	Value *value = (Value *) gv;

	g_return_val_if_fail (value->type == GCONF_VALUE_BOOL, FALSE);
	return value->v_bool;
}

GConfValueType
gconf_value_get_list_type (const GConfValue *gv)
{
	Value *value = (Value *) gv;

	g_return_val_if_fail (value->type == GCONF_VALUE_LIST, GCONF_VALUE_INVALID);
	return value->list_type;
}

GSList *
gconf_value_get_list (const GConfValue *gv)
{
	Value *value = (Value *) gv;

	g_return_val_if_fail (value->type == GCONF_VALUE_LIST, NULL);
	return value->v_list;
}

/*********************************************************/

typedef struct {
	char *key;
	const GConfValue *value;
	guint32 refcount;
} Entry;

GConfEntry *
gconf_entry_new (const char *key, const GConfValue *val)
{
	Entry *e;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (val != NULL, NULL);

	e = g_malloc0 (sizeof (Entry));
	e->refcount = 1;
	e->key = g_strdup (key);
	e->value = val;
	return (GConfEntry *) e;
}

const char *
gconf_entry_get_key (const GConfEntry *ge)
{
	Entry *entry = (Entry *) ge;

	g_return_val_if_fail (entry != NULL, NULL);
	return entry->key;
}

GConfValue *
gconf_entry_get_value (const GConfEntry *ge)
{
	Entry *entry = (Entry *) ge;

	g_return_val_if_fail (entry != NULL, NULL);
	return (GConfValue *) entry->value;
}

void
gconf_entry_unref (GConfEntry *ge)
{
	Entry *entry = (Entry *) ge;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->refcount > 0);

	entry->refcount--;
	if (entry->refcount == 0) {
		g_free (entry->key);
		memset (entry, 0, sizeof (Entry));
		g_free (entry);
	}
}

/****************************************************************/

void
gconf_client_suggest_sync (GConfClient *client, GError **error)
{
}

GConfValue *
gconf_client_get (GConfClient *client, const gchar* key, GError **error)
{
	Value *v;

	g_return_val_if_fail (client != NULL, NULL);
	v = g_hash_table_lookup (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	return v ? gconf_value_copy ((GConfValue *) v) : NULL;
}

GConfValue *
gconf_client_get_without_default (GConfClient *client, const gchar* key, GError **error)
{
	g_return_val_if_fail (client != NULL, NULL);
	return gconf_client_get (client, key, error);
}

gboolean
gconf_client_get_bool (GConfClient *client, const gchar* key, GError **error)
{
	Value *v;

	g_return_val_if_fail (client != NULL, FALSE);

	v = g_hash_table_lookup (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	if (v) {
		g_return_val_if_fail (v->type == GCONF_VALUE_BOOL, FALSE);
		return v->v_bool;
	}
	return FALSE;
}

gboolean
gconf_client_set_bool (GConfClient *client, const char *key, gboolean val, GError **error)
{
	Value *v;

	g_return_val_if_fail (client != NULL, FALSE);

	v = g_hash_table_lookup (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	if (v) {
		if (v->type != GCONF_VALUE_BOOL) {
			g_set_error (error, GCONF_ERROR, GCONF_ERROR_TYPE_MISMATCH, "expected boolean type");
			return FALSE;
		}
		v->v_bool = val;
	} else {
		v = (Value *) gconf_value_new (GCONF_VALUE_BOOL);
		v->v_bool = val;
		g_hash_table_insert (GCONF_CLIENT_GET_PRIVATE (client)->keys, g_strdup (key), v);
	}
	return TRUE;
}

int
gconf_client_get_int (GConfClient *client, const gchar* key, GError **error)
{
	Value *v;

	g_return_val_if_fail (client != NULL, 0);

	v = g_hash_table_lookup (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	if (v) {
		g_return_val_if_fail (v->type == GCONF_VALUE_INT, 0);
		return v->v_int;
	}
	return 0;
}

gboolean
gconf_client_set_int (GConfClient *client, const char *key, int val, GError **error)
{
	Value *v;

	g_return_val_if_fail (client != NULL, FALSE);

	v = g_hash_table_lookup (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	if (v) {
		if (v->type != GCONF_VALUE_INT) {
			g_set_error (error, GCONF_ERROR, GCONF_ERROR_TYPE_MISMATCH, "expected int type");
			return FALSE;
		}
		v->v_int = val;
	} else {
		v = (Value *) gconf_value_new (GCONF_VALUE_INT);
		v->v_int = val;
		g_hash_table_insert (GCONF_CLIENT_GET_PRIVATE (client)->keys, g_strdup (key), v);
	}
	return TRUE;
}

gboolean
gconf_client_set_string (GConfClient *client, const char *key, const char *val, GError **error)
{
	Value *v;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (val != NULL, FALSE);

	v = g_hash_table_lookup (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	if (v) {
		if (v->type != GCONF_VALUE_STRING) {
			g_set_error (error, GCONF_ERROR, GCONF_ERROR_TYPE_MISMATCH, "expected string type");
			return FALSE;
		}
		g_free (v->v_str);
		v->v_str = g_strdup (val);
	} else {
		v = (Value *) gconf_value_new (GCONF_VALUE_STRING);
		v->v_str = g_strdup (val);
		g_hash_table_insert (GCONF_CLIENT_GET_PRIVATE (client)->keys, g_strdup (key), v);
	}
	return TRUE;
}

gboolean
gconf_client_set_float (GConfClient *client, const char *key, double val, GError **error)
{
	Value *v;

	g_return_val_if_fail (client != NULL, FALSE);

	v = g_hash_table_lookup (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	if (v) {
		if (v->type != GCONF_VALUE_FLOAT) {
			g_set_error (error, GCONF_ERROR, GCONF_ERROR_TYPE_MISMATCH, "expected float type");
			return FALSE;
		}
		v->v_float = val;
	} else {
		v = (Value *) gconf_value_new (GCONF_VALUE_FLOAT);
		v->v_float = val;
		g_hash_table_insert (GCONF_CLIENT_GET_PRIVATE (client)->keys, g_strdup (key), v);
	}
	return TRUE;
}

gboolean
gconf_client_set_list (GConfClient *client,
                       const char *key,
                       GConfValueType list_type,
                       GSList *list,
                       GError **error)
{
	Value *v;
	gboolean add = FALSE;
	GSList *iter;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (list != NULL, FALSE);

	v = g_hash_table_lookup (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	if (v) {
		if (v->type != GCONF_VALUE_LIST) {
			g_set_error (error, GCONF_ERROR, GCONF_ERROR_TYPE_MISMATCH, "expected string type");
			return FALSE;
		}
		g_slist_foreach (v->v_list, (GFunc) gconf_value_free, NULL);
		g_slist_free (v->v_list);

		v->list_type = list_type;
	} else {
		v = (Value *) gconf_value_new (GCONF_VALUE_LIST);
		v->list_type = list_type;
		add = TRUE;
	}

	for (iter = list; iter; iter = g_slist_next (iter)) {
		Value *nv = (Value *) gconf_value_new (list_type);

		switch (list_type) {
		case GCONF_VALUE_STRING:
			nv->v_str = g_strdup (iter->data);
			break;
		case GCONF_VALUE_BOOL:
			nv->v_bool = !!iter->data;
			break;
		case GCONF_VALUE_INT:
			nv->v_int = GPOINTER_TO_INT (iter->data);
			break;
		default:
			gconf_value_free ((GConfValue *) nv);
			nv = NULL;
			g_warn_if_reached ();
			break;
		}

		if (nv)
			v->v_list = g_slist_append (v->v_list, nv);
	}

	if (add)
		g_hash_table_insert (GCONF_CLIENT_GET_PRIVATE (client)->keys, g_strdup (key), v);

	return TRUE;
}

static gboolean
find_in_list (GSList *list, const char *item)
{
	GSList *iter;

	for (iter = list; iter; iter = g_slist_next (iter)) {
		if (g_strcmp0 (iter->data, item) == 0)
			return TRUE;
	}
	return FALSE;
}

GSList *
gconf_client_all_dirs (GConfClient *client, const char *dir, GError **error)
{
	GHashTableIter iter;
	const char *tmp = NULL;
	GSList *dirs = NULL;
	char *normalized;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (dir != NULL, NULL);

	if (dir[strlen (dir) - 1] == '/')
		normalized = g_strdup (dir);
	else
		normalized = g_strdup_printf ("%s/", dir);

	g_hash_table_iter_init (&iter, GCONF_CLIENT_GET_PRIVATE (client)->keys);
	while (g_hash_table_iter_next (&iter, (gpointer) &tmp, NULL)) {
		char *foo, *z;

		/* Is this key a child of the directory? */
		if (strncmp (tmp, normalized, strlen (normalized)) != 0)
			continue;

		/* Dupe key and add to the list if it's a directory */
		foo = g_strdup (tmp);
		z = strchr (foo + strlen (normalized), '/');
		if (z) {
			*z = '\0';  /* chop at the / */
			if (!find_in_list (dirs, foo))
				dirs = g_slist_prepend (dirs, g_strdup (foo));
		}
		g_free (foo);
	}

	g_free (normalized);
	return dirs;
}

static gint
sort_func (gconstpointer a, gconstpointer b)
{
	const GConfEntry *ea = a;
	const GConfEntry *eb = b;

	if (ea == eb)
		return 0;
	if (ea && !eb)
		return 1;
	if (!ea && eb)
		return -1;
	g_assert (ea && eb);
	return g_strcmp0 (ea->key, eb->key);
}

GSList *
gconf_client_all_entries (GConfClient *client, const char *dir, GError **error)
{
	GHashTableIter iter;
	const char *tmp = NULL;
	GSList *entries = NULL;
	GConfValue *val = NULL;
	char *normalized;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (dir != NULL, NULL);

	if (dir[strlen (dir) - 1] == '/')
		normalized = g_strdup (dir);
	else
		normalized = g_strdup_printf ("%s/", dir);

	g_hash_table_iter_init (&iter, GCONF_CLIENT_GET_PRIVATE (client)->keys);
	while (g_hash_table_iter_next (&iter, (gpointer) &tmp, (gpointer) &val)) {
		/* Is this key a child of the directory? */
		if (strncmp (tmp, normalized, strlen (normalized)) == 0) {
			/* only children won't have another / in the key name */
			if (strchr (tmp + strlen (normalized), '/') == NULL) {
				entries = g_slist_insert_sorted (entries,
				                                 gconf_entry_new (tmp, val),
				                                 sort_func);
			}
		}
	}

	return entries;
}

gboolean
gconf_client_recursive_unset (GConfClient *client,
                              const char *key,
                              GConfUnsetFlags flags,
                              GError **error)
{
	GConfClientPrivate *priv;
	GHashTableIter iter;
	char *others;
	const char *tmp = NULL;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	priv = GCONF_CLIENT_GET_PRIVATE (client);
	g_hash_table_remove (priv->keys, key);

	/* Remove children of key too*/
	if (key[strlen (key) - 1] == '/')
		others = g_strdup (key);
	else
		others = g_strdup_printf ("%s/", key);

	g_hash_table_iter_init (&iter, priv->keys);
	while (g_hash_table_iter_next (&iter, (gpointer) &tmp, NULL)) {
		if (strncmp (others, tmp, strlen (others)) == 0)
			g_hash_table_iter_remove (&iter);
	}
	g_free (others);

	return TRUE;
}

gboolean
gconf_client_unset (GConfClient *client, const gchar *key, GError **error)
{
	g_hash_table_remove (GCONF_CLIENT_GET_PRIVATE (client)->keys, key);
	return TRUE;
}

GConfClient *
gconf_client_get_default (void)
{
	if (singleton)
		g_object_ref (singleton);
	else
		singleton = g_object_new (GCONF_TYPE_CLIENT, NULL);\

	return singleton;
}

static const gchar invalid_chars[] = " \t\r\n\"$&<>,+=#!()'|{}[]?~`;%\\";

/* Copied from GConf */
char*
gconf_escape_key (const char *arbitrary_text,
                  int         len)
{
  const char *p;
  const char *end;
  GString *retval;

  g_return_val_if_fail (arbitrary_text != NULL, NULL);
  
  /* Nearly all characters we would normally use for escaping aren't allowed in key
   * names, so we use @ for that.
   *
   * Invalid chars and @ itself are escaped as @xxx@ where xxx is the
   * Latin-1 value in decimal
   */

  if (len < 0)
    len = strlen (arbitrary_text);

  retval = g_string_sized_new (len);

  p = arbitrary_text;
  end = arbitrary_text + len;
  while (p != end)
    {
      if (*p == '/' || *p == '.' || *p == '@' || ((guchar) *p) > 127 ||
          strchr (invalid_chars, *p))
        {
          g_string_append_printf (retval, "@%u@", (guchar) *p);
        }
      else
        g_string_append_c (retval, *p);
      
      ++p;
    }

  return g_string_free (retval, FALSE);
}

/* Copied from GConf */
char*
gconf_unescape_key (const char *escaped_key,
                    int         len)
{
  const char *p;
  const char *end;
  const char *start_seq;
  GString *retval;

  g_return_val_if_fail (escaped_key != NULL, NULL);
  
  if (len < 0)
    len = strlen (escaped_key);

  retval = g_string_new (NULL);

  p = escaped_key;
  end = escaped_key + len;
  start_seq = NULL;
  while (p != end)
    {
      if (start_seq)
        {
          if (*p == '@')
            {
              /* *p is the @ that ends a seq */
              char *end_seq;
              guchar val;
              
              val = strtoul (start_seq, &end_seq, 10);
              if (start_seq != end_seq)
                g_string_append_c (retval, val);
              
              start_seq = NULL;
            }
        }
      else
        {
          if (*p == '@')
            start_seq = p + 1;
          else
            g_string_append_c (retval, *p);
        }

      ++p;
    }

  return g_string_free (retval, FALSE);
}

/*********************************************************/

static gboolean
extract_item (char *line, const char *start, const char *end, char **val)
{
	char *p;

	if (!g_str_has_prefix (line, start))
		return FALSE;
	line += strlen (start);
	p = strstr (line, end);
	g_return_val_if_fail (p != NULL, FALSE);
	*p = '\0';
	*val = g_strdup (line);
	return TRUE;
}

#define LIST_TYPE_NONE 0
#define LIST_TYPE_STR  1
#define LIST_TYPE_INT  2

gboolean
fake_gconf_add_xml (GConfClient *client, const char *path_to_xml)
{
	GError *error = NULL;
	char *contents = NULL;
	char **lines, **iter;
	gboolean found_start = FALSE;
	gboolean found_base = FALSE;
	gboolean in_entry = FALSE;
	guint32 in_value = 0;
	char *key = NULL;
	guint list_type = LIST_TYPE_NONE;
	GSList *list_val = NULL;
	char *str_val = NULL;
	int int_val = 0;
	gboolean int_set = FALSE;
	gboolean bool_val = FALSE;
	gboolean bool_set = FALSE;
	gboolean success = FALSE;

	if (!g_file_get_contents (path_to_xml, &contents, NULL, &error)) {
		g_warning ("%s: failed to load '%s': %s", __func__, path_to_xml, error->message);
		g_clear_error (&error);
		return FALSE;
	}

	lines = g_strsplit_set (contents, "\n\r", -1);
	g_free (contents);
	if (!lines) {
		g_warning ("%s: failed to split '%s' contents", __func__, path_to_xml);
		return FALSE;
	}

	for (iter = lines; iter && *iter; iter++) {
		char *line = *iter;
		char *tmp = NULL;

		line = g_strstrip (line);
		if (!strlen (line))
			continue;

		if (!found_start) {
			if (g_str_has_prefix (line, "<gconfentryfile>")) {
				found_start = TRUE;
				continue;
			} else {
				g_warning ("%s: file does not start with <gconfentryfile>", __func__);
				break;
			}
		}

		if (!found_base) {
			if (g_str_has_prefix (line, "<entrylist base=\"/system/networking/connections\">")) {
				found_base = TRUE;
				continue;
			} else {
				g_warning ("%s: did not find <entrylist ...> early enough", __func__);
				break;
			}
		}

		if (strcmp (line, "<entry>") == 0) {
			in_entry = TRUE;
			g_warn_if_fail (key == NULL);
			continue;
		}

		if (strcmp (line, "</entry>") == 0) {
			/* Write the entry */
			g_warn_if_fail (in_entry == TRUE);
			g_warn_if_fail (in_value == 0);
			g_warn_if_fail (key != NULL);

			if (list_type == LIST_TYPE_STR && list_val) {
				gconf_client_set_list (client, key, GCONF_VALUE_STRING, list_val, NULL);
				g_slist_foreach (list_val, (GFunc) g_free, NULL);
			} else if (list_type == LIST_TYPE_INT && list_val)
				gconf_client_set_list (client, key, GCONF_VALUE_INT, list_val, NULL);
			else if (str_val) {
				gconf_client_set_string (client, key, str_val, NULL);
				g_free (str_val);
				str_val = NULL;
			} else if (int_set) {
				gconf_client_set_int (client, key, int_val, NULL);
				int_val = 0;
				int_set = FALSE;
			} else if (bool_set) {
				gconf_client_set_bool (client, key, bool_val, NULL);
				bool_set = FALSE;
			}

			g_slist_free (list_val);
			list_val = NULL;
			list_type = LIST_TYPE_NONE;
			g_free (key);
			key = NULL;
			in_entry = FALSE;
			continue;
		}

		if (strcmp (line, "<value>") == 0) {
			in_value++;
			continue;
		}

		if (strcmp (line, "</value>") == 0) {
			in_value--;
			continue;
		}

		if (strcmp (line, "<list type=\"string\">") == 0) {
			list_type = LIST_TYPE_STR;
			continue;
		}

		if (strcmp (line, "<list type=\"int\">") == 0) {
			list_type = LIST_TYPE_INT;
			continue;
		}

		if (strcmp (line, "</list>") == 0)
			continue;

		if (strcmp (line, "</gconfentryfile>") == 0) {
			success = TRUE;
			continue;
		}

		if (in_value == 0) {
			if (extract_item (line, "<key>", "</key>", &tmp)) {
				key = g_strdup_printf ("/system/networking/connections/%s", tmp);
				continue;
			}
		} else if (in_value == 1 || in_value == 2) {
			if (extract_item (line, "<string>", "</string>", &str_val)) {
				if (list_type == LIST_TYPE_STR) {
					list_val = g_slist_append (list_val, str_val);
					str_val = NULL;
				}
				continue;
			}
			if (extract_item (line, "<int>", "</int>", &tmp)) {
				int_val = (int) strtol (tmp, NULL, 10);
				if (list_type == LIST_TYPE_INT)
					list_val = g_slist_append (list_val, GINT_TO_POINTER (int_val));
				else
					int_set = TRUE;
				continue;
			}
			if (g_str_has_prefix (line, "<bool>")) {
				if (strcmp (line, "<bool>true</bool>") == 0)
					bool_val = TRUE;
				bool_set = TRUE;
				continue;
			}
		}
	}
	g_strfreev (lines);

	return success;
}

/*********************************************************/

static void
gconf_client_init (GConfClient *self)
{
	GConfClientPrivate *priv = GCONF_CLIENT_GET_PRIVATE (self);

	priv->keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gconf_value_free);
}

static void
finalize (GObject *object)
{
	GConfClientPrivate *priv = GCONF_CLIENT_GET_PRIVATE (object);

	g_hash_table_destroy (priv->keys);
	G_OBJECT_CLASS (gconf_client_parent_class)->finalize (object);
	singleton = NULL;
}

static void
gconf_client_class_init (GConfClientClass *config_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (config_class);

	g_type_class_add_private (config_class, sizeof (GConfClientPrivate));

	/* virtual methods */
	object_class->finalize = finalize;

}
