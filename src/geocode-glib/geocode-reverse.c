/*
   Copyright (C) 2011-2012 Bastien Nocera

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Authors: Bastien Nocera <hadess@hadess.net>

 */

#include <string.h>
#include <errno.h>
#include <locale.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <config.h>
#include <glib/gi18n-lib.h>
#include <geocode-glib/geocode-glib.h>
#include <geocode-glib/geocode-error.h>
#include <geocode-glib/geocode-reverse.h>
#include <geocode-glib/geocode-glib-private.h>

/**
 * SECTION:geocode-reverse
 * @short_description: Geocode reverse geocoding object
 * @include: geocode-glib/geocode-glib.h
 *
 * Contains functions for reverse geocoding using the
 * <ulink url="http://wiki.openstreetmap.org/wiki/Nominatim">OSM Nominatim APIs</ulink>
 **/

struct _GeocodeReversePrivate {
	GHashTable *ht;
        SoupSession *soup_session;
};

G_DEFINE_TYPE (GeocodeReverse, geocode_reverse, G_TYPE_OBJECT)

static void
geocode_reverse_finalize (GObject *gobject)
{
	GeocodeReverse *object = (GeocodeReverse *) gobject;

	g_clear_pointer (&object->priv->ht, g_hash_table_destroy);
        g_clear_object (&object->priv->soup_session);

	G_OBJECT_CLASS (geocode_reverse_parent_class)->finalize (gobject);
}

static void
geocode_reverse_class_init (GeocodeReverseClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        bindtextdomain (GETTEXT_PACKAGE, GEOCODE_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	gobject_class->finalize = geocode_reverse_finalize;

	g_type_class_add_private (klass, sizeof (GeocodeReversePrivate));
}

static void
geocode_reverse_init (GeocodeReverse *object)
{
	object->priv = G_TYPE_INSTANCE_GET_PRIVATE ((object), GEOCODE_TYPE_REVERSE, GeocodeReversePrivate);
	object->priv->ht = g_hash_table_new_full (g_str_hash, g_str_equal,
						  g_free, g_free);
	object->priv->soup_session = _geocode_glib_build_soup_session ();
}

/**
 * geocode_reverse_new_for_location:
 * @location: a #GeocodeLocation object
 *
 * Creates a new #GeocodeReverse to perform reverse geocoding with.
 * Use geocode_reverse_resolve_async() to perform the resolution.
 *
 * Returns: a new #GeocodeReverse. Use g_object_unref() when done.
 **/
GeocodeReverse *
geocode_reverse_new_for_location (GeocodeLocation *location)
{
	GeocodeReverse *object;
	char coord[G_ASCII_DTOSTR_BUF_SIZE];
	char *lat;
	char *lon;

	lat = g_strdup (g_ascii_dtostr (coord,
	                                G_ASCII_DTOSTR_BUF_SIZE,
	                                geocode_location_get_latitude (location)));

	lon = g_strdup (g_ascii_dtostr (coord,
	                                G_ASCII_DTOSTR_BUF_SIZE,
	                                geocode_location_get_longitude (location)));
	object = g_object_new (GEOCODE_TYPE_REVERSE, NULL);

	g_hash_table_insert (object->priv->ht, g_strdup ("lat"), lat);
	g_hash_table_insert (object->priv->ht, g_strdup ("lon"), lon);

	return object;
}

static void
insert_bounding_box_element (GHashTable *ht,
			     GType       value_type,
			     const char *name,
			     JsonReader *reader)
{
	if (value_type == G_TYPE_STRING) {
		const char *bbox_val;

		bbox_val = json_reader_get_string_value (reader);
		g_hash_table_insert (ht, g_strdup (name), g_strdup (bbox_val));
	} else if (value_type == G_TYPE_DOUBLE) {
		gdouble bbox_val;

		bbox_val = json_reader_get_double_value (reader);
		g_hash_table_insert(ht, g_strdup (name), g_strdup_printf ("%lf", bbox_val));
	} else if (value_type == G_TYPE_INT64) {
		gint64 bbox_val;

		bbox_val = json_reader_get_double_value (reader);
		g_hash_table_insert(ht, g_strdup (name), g_strdup_printf ("%"G_GINT64_FORMAT, bbox_val));
	} else {
		g_debug ("Unhandled node type %s for %s", g_type_name (value_type), name);
	}
}

void
_geocode_read_nominatim_attributes (JsonReader *reader,
                                    GHashTable *ht)
{
	char **members;
	guint i;
        gboolean is_address;
        const char *house_number = NULL;

	is_address = (g_strcmp0 (json_reader_get_member_name (reader), "address") == 0);

	members = json_reader_list_members (reader);
        if (members == NULL) {
                json_reader_end_member (reader);
                return;
        }

	for (i = 0; members[i] != NULL; i++) {
                const char *value = NULL;

                json_reader_read_member (reader, members[i]);

                if (json_reader_is_value (reader)) {
                        JsonNode *node = json_reader_get_value (reader);
                        if (json_node_get_value_type (node) == G_TYPE_STRING) {
                                value = json_node_get_string (node);
                                if (value && *value == '\0')
                                        value = NULL;
                        }
                }

                if (value != NULL) {
                        g_hash_table_insert (ht, g_strdup (members[i]), g_strdup (value));

                        if (i == 0 && is_address) {
	                        if (g_strcmp0 (members[i], "house_number") != 0)
                                        /* Since Nominatim doesn't give us a short name,
                                         * we use the first component of address as name.
                                         */
                                        g_hash_table_insert (ht, g_strdup ("name"), g_strdup (value));
                                else
                                        house_number = value;
                        } else if (house_number != NULL && g_strcmp0 (members[i], "road") == 0) {
                                gboolean number_after;
                                char *name;

                                number_after = _geocode_object_is_number_after_street ();
                                name = g_strdup_printf ("%s %s",
                                                        number_after ? value : house_number,
                                                        number_after ? house_number : value);
                                g_hash_table_insert (ht, g_strdup ("name"), name);
                        }
                } else if (g_strcmp0 (members[i], "boundingbox") == 0) {
                        JsonNode *node;
                        GType value_type;

                        json_reader_read_element (reader, 0);
                        node = json_reader_get_value (reader);
                        value_type = json_node_get_value_type (node);

                        insert_bounding_box_element (ht, value_type, "boundingbox-bottom", reader);
                        json_reader_end_element (reader);

                        json_reader_read_element (reader, 1);
                        insert_bounding_box_element (ht, value_type, "boundingbox-top", reader);
                        json_reader_end_element (reader);

                        json_reader_read_element (reader, 2);
                        insert_bounding_box_element (ht, value_type, "boundingbox-left", reader);
                        json_reader_end_element (reader);

                        json_reader_read_element (reader, 3);
                        insert_bounding_box_element (ht, value_type, "boundingbox-right", reader);
                        json_reader_end_element (reader);
                }
                json_reader_end_member (reader);
        }

	g_strfreev (members);

        if (json_reader_read_member (reader, "address"))
                _geocode_read_nominatim_attributes (reader, ht);
        json_reader_end_member (reader);
}

static GHashTable *
resolve_json (const char *contents,
              GError    **error)
{
	GHashTable *ret;
	JsonParser *parser;
	JsonNode *root;
	JsonReader *reader;

	ret = NULL;

	parser = json_parser_new ();
	if (json_parser_load_from_data (parser, contents, -1, error) == FALSE) {
		g_object_unref (parser);
		return ret;
	}

	root = json_parser_get_root (parser);
	reader = json_reader_new (root);

	if (json_reader_read_member (reader, "error")) {
		const char *msg;

                msg = json_reader_get_string_value (reader);
                json_reader_end_member (reader);
		if (msg && *msg == '\0')
			msg = NULL;

		g_set_error_literal (error,
                                     GEOCODE_ERROR,
                                     GEOCODE_ERROR_NOT_SUPPORTED,
                                     msg ? msg : "Query not supported");
		g_object_unref (parser);
		g_object_unref (reader);
		return NULL;
	}

	ret = g_hash_table_new_full (g_str_hash, g_str_equal,
				     g_free, g_free);

        _geocode_read_nominatim_attributes (reader, ret);

	g_object_unref (parser);
	g_object_unref (reader);

	return ret;
}

static void
on_query_data_loaded (SoupSession *session,
                      SoupMessage *query,
                      gpointer     user_data)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GError *error = NULL;
	char *contents;
	GeocodePlace *ret;
	GHashTable *attributes;

        if (query->status_code != SOUP_STATUS_OK) {
		g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                     query->reason_phrase ? query->reason_phrase : "Query failed");
                g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		return;
	}

        contents = g_strndup (query->response_body->data, query->response_body->length);
	attributes = resolve_json (contents, &error);

	if (attributes == NULL) {
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		g_free (contents);
		return;
	}

	/* Now that we can parse the result, save it to cache */
	_geocode_glib_cache_save (query, contents);
	g_free (contents);

        ret = _geocode_create_place_from_attributes (attributes);
        g_hash_table_destroy (attributes);
	g_simple_async_result_set_op_res_gpointer (simple, ret, NULL);

	g_simple_async_result_complete_in_idle (simple);
	g_object_unref (simple);
}

static void
on_cache_data_loaded (GObject      *source_object,
		      GAsyncResult *res,
		      gpointer      user_data)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GFile *cache;
	GError *error = NULL;
	char *contents;
	GHashTable *result;

	cache = G_FILE (source_object);
	if (g_file_load_contents_finish (cache,
					 res,
					 &contents,
					 NULL,
					 NULL,
					 NULL) == FALSE) {
               GObject *object;
		SoupMessage *query;

                object = g_async_result_get_source_object (G_ASYNC_RESULT (simple));
		query = g_object_get_data (G_OBJECT (cache), "query");
                g_object_ref (query); /* soup_session_queue_message steals ref */
		soup_session_queue_message (GEOCODE_REVERSE (object)->priv->soup_session,
                                            query,
					    on_query_data_loaded,
					    simple);
		return;
	}

	result = resolve_json (contents, &error);
	g_free (contents);

	if (result == NULL) {
		g_simple_async_result_take_error (simple, error);
        } else {
                GeocodePlace *place;

                place = _geocode_create_place_from_attributes (result);
                g_hash_table_destroy (result);
                g_simple_async_result_set_op_res_gpointer (simple, place, NULL);
        }

	g_simple_async_result_complete_in_idle (simple);
	g_object_unref (simple);
}

static void
copy_item (char       *key,
	   char       *value,
	   GHashTable *ret)
{
	g_hash_table_insert (ret, key, value);
}

GHashTable *
_geocode_glib_dup_hash_table (GHashTable *ht)
{
	GHashTable *ret;

	ret = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_foreach (ht, (GHFunc) copy_item, ret);

	return ret;
}

static SoupMessage *
get_resolve_query_for_params (GHashTable  *orig_ht)
{
	SoupMessage *ret;
	GHashTable *ht;
	char *locale;
	char *params, *uri;

	ht = _geocode_glib_dup_hash_table (orig_ht);

	g_hash_table_insert (ht, (gpointer) "format", (gpointer) "json");
	g_hash_table_insert (ht, (gpointer) "email", (gpointer) "zeeshanak@gnome.org");
	g_hash_table_insert (ht, (gpointer) "addressdetails", (gpointer) "1");

	locale = NULL;
	if (g_hash_table_lookup (ht, "accept-language") == NULL) {
		locale = _geocode_object_get_lang ();
		if (locale)
			g_hash_table_insert (ht, (gpointer) "accept-language", locale);
	}

	params = soup_form_encode_hash (ht);
	g_hash_table_destroy (ht);
	g_free (locale);

	uri = g_strdup_printf ("https://nominatim.gnome.org/reverse?%s", params);
	g_free (params);

	ret = soup_message_new ("GET", uri);
	g_free (uri);

	return ret;
}

/**
 * geocode_reverse_resolve_async:
 * @object: a #GeocodeReverse representing a query
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously gets the result of a reverse geocoding
 * query using a web service. Use geocode_reverse_resolve() to do the same
 * thing synchronously.
 *
 * When the operation is finished, @callback will be called. You can then call
 * geocode_reverse_resolve_finish() to get the result of the operation.
 **/
void
geocode_reverse_resolve_async (GeocodeReverse     *object,
			       GCancellable       *cancellable,
			       GAsyncReadyCallback callback,
			       gpointer            user_data)
{
	GSimpleAsyncResult *simple;
	SoupMessage *query;
	char *cache_path;

	g_return_if_fail (GEOCODE_IS_REVERSE (object));

	simple = g_simple_async_result_new (G_OBJECT (object),
					    callback,
					    user_data,
					    geocode_reverse_resolve_async);
	g_simple_async_result_set_check_cancellable (simple, cancellable);

	query = get_resolve_query_for_params (object->priv->ht);

	cache_path = _geocode_glib_cache_path_for_query (query);
	if (cache_path == NULL) {
		soup_session_queue_message (object->priv->soup_session,
                                            query,
					    on_query_data_loaded,
					    simple);
	} else {
		GFile *cache;

		cache = g_file_new_for_path (cache_path);
		g_object_set_data_full (G_OBJECT (cache), "query", query, (GDestroyNotify) g_object_unref);
		g_file_load_contents_async (cache,
					    cancellable,
					    on_cache_data_loaded,
					    simple);
		g_object_unref (cache);
		g_free (cache_path);
	}
}

/**
 * geocode_reverse_resolve_finish:
 * @object: a #GeocodeReverse representing a query
 * @res: a #GAsyncResult.
 * @error: a #GError.
 *
 * Finishes a reverse geocoding operation. See geocode_reverse_resolve_async().
 *
 * Returns: (transfer full): A #GeocodePlace instance, or %NULL in case of
 * errors. Free the returned instance with #g_object_unref() when done.
 **/
GeocodePlace *
geocode_reverse_resolve_finish (GeocodeReverse *object,
				GAsyncResult   *res,
				GError        **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

	g_return_val_if_fail (GEOCODE_IS_REVERSE (object), NULL);

	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == geocode_reverse_resolve_async);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_simple_async_result_get_op_res_gpointer (simple);
}

/**
 * geocode_reverse_resolve:
 * @object: a #GeocodeReverse representing a query
 * @error: a #GError
 *
 * Gets the result of a reverse geocoding
 * query using a web service.
 *
 * Returns: (transfer full): A #GeocodePlace instance, or %NULL in case of
 * errors. Free the returned instance with #g_object_unref() when done.
 **/
GeocodePlace *
geocode_reverse_resolve (GeocodeReverse *object,
			 GError        **error)
{
	SoupMessage *query;
	char *contents;
	GHashTable *result;
	GeocodePlace *place;
	gboolean to_cache = FALSE;

	g_return_val_if_fail (GEOCODE_IS_REVERSE (object), NULL);

	query = get_resolve_query_for_params (object->priv->ht);

	if (_geocode_glib_cache_load (query, &contents) == FALSE) {
                if (soup_session_send_message (object->priv->soup_session,
                                               query) != SOUP_STATUS_OK) {
                        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                             query->reason_phrase ? query->reason_phrase : "Query failed");
                        g_object_unref (query);
                        return NULL;
                }
                contents = g_strndup (query->response_body->data, query->response_body->length);

		to_cache = TRUE;
	}

	result = resolve_json (contents, error);
	if (to_cache && result != NULL)
		_geocode_glib_cache_save (query, contents);

	g_free (contents);
	g_object_unref (query);

	if (result == NULL)
		return NULL;

	place = _geocode_create_place_from_attributes (result);
	g_hash_table_destroy (result);

	return place;
}
