/*
   Copyright (C) 2011 Bastien Nocera

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
#include <stdlib.h>
#include <locale.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <geocode-glib/geocode-forward.h>
#include <geocode-glib/geocode-bounding-box.h>
#include <geocode-glib/geocode-error.h>
#include <geocode-glib/geocode-glib-private.h>

/**
 * SECTION:geocode-forward
 * @short_description: Geocode forward geocoding object
 * @include: geocode-glib/geocode-glib.h
 *
 * Contains functions for geocoding using the
 * <ulink url="http://wiki.openstreetmap.org/wiki/Nominatim">OSM Nominatim APIs</ulink>
 **/

struct _GeocodeForwardPrivate {
	GHashTable *ht;
        SoupSession *soup_session;
	guint       answer_count;
	GeocodeBoundingBox *search_area;
	gboolean bounded;
};

enum {
        PROP_0,

        PROP_ANSWER_COUNT,
        PROP_SEARCH_AREA,
        PROP_BOUNDED
};

G_DEFINE_TYPE (GeocodeForward, geocode_forward, G_TYPE_OBJECT)

static void
geocode_forward_get_property (GObject	 *object,
			      guint	  property_id,
			      GValue	 *value,
			      GParamSpec *pspec)
{
	GeocodeForward *forward = GEOCODE_FORWARD (object);

	switch (property_id) {
		case PROP_ANSWER_COUNT:
			g_value_set_uint (value,
					  geocode_forward_get_answer_count (forward));
			break;

		case PROP_SEARCH_AREA:
			g_value_set_object (value,
					    geocode_forward_get_search_area (forward));
			break;

		case PROP_BOUNDED:
			g_value_set_boolean (value,
					     geocode_forward_get_bounded (forward));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
geocode_forward_set_property(GObject	   *object,
			     guint	    property_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	GeocodeForward *forward = GEOCODE_FORWARD (object);

	switch (property_id) {
		case PROP_ANSWER_COUNT:
			geocode_forward_set_answer_count (forward,
							  g_value_get_uint (value));
			break;

		case PROP_SEARCH_AREA:
			geocode_forward_set_search_area (forward,
							 g_value_get_object (value));
			break;

		case PROP_BOUNDED:
			geocode_forward_set_bounded (forward,
						     g_value_get_boolean (value));
			break;

		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void geocode_forward_add (GeocodeForward *forward,
				 const char     *key,
				 const char     *value);

static void
geocode_forward_finalize (GObject *gforward)
{
	GeocodeForward *forward = (GeocodeForward *) gforward;

	g_clear_pointer (&forward->priv->ht, g_hash_table_destroy);
        g_clear_object (&forward->priv->soup_session);

	G_OBJECT_CLASS (geocode_forward_parent_class)->finalize (gforward);
}

static void
geocode_forward_class_init (GeocodeForwardClass *klass)
{
	GObjectClass *gforward_class = G_OBJECT_CLASS (klass);
	GParamSpec *pspec;

	gforward_class->finalize = geocode_forward_finalize;
	gforward_class->get_property = geocode_forward_get_property;
	gforward_class->set_property = geocode_forward_set_property;


	g_type_class_add_private (klass, sizeof (GeocodeForwardPrivate));

	/**
	* GeocodeForward:answer-count:
	*
	* The number of requested results to a search query.
	*/
	pspec = g_param_spec_uint ("answer-count",
				   "Answer count",
				   "The number of requested results",
				   0,
				   G_MAXINT,
				   DEFAULT_ANSWER_COUNT,
				   G_PARAM_READWRITE |
				   G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gforward_class, PROP_ANSWER_COUNT, pspec);

	/**
	* GeocodeForward:search-area:
	*
	* The bounding box that limits the search area.
	* If #GeocodeForward:bounded property is set to #TRUE only results from
	* this area is returned.
	*/
	pspec = g_param_spec_object ("search-area",
				     "Search area",
				     "The area to limit search within",
				     GEOCODE_TYPE_BOUNDING_BOX,
				     G_PARAM_READWRITE |
				     G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gforward_class, PROP_SEARCH_AREA, pspec);

	/**
	* GeocodeForward:bounded:
	*
	* If set to #TRUE then only results in the #GeocodeForward:search-area
	* bounding box are returned.
	* If set to #FALSE the #GeocodeForward:search-area is treated like a
	* preferred area for results.
	*/
	pspec = g_param_spec_boolean ("bounded",
				      "Bounded",
				      "Bind search results to search-area",
				      FALSE,
				      G_PARAM_READWRITE |
				      G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gforward_class, PROP_BOUNDED, pspec);
}

static void
geocode_forward_init (GeocodeForward *forward)
{
	forward->priv = G_TYPE_INSTANCE_GET_PRIVATE ((forward), GEOCODE_TYPE_FORWARD, GeocodeForwardPrivate);
	forward->priv->ht = g_hash_table_new_full (g_str_hash, g_str_equal,
						   g_free, g_free);
	forward->priv->soup_session = _geocode_glib_build_soup_session ();
	forward->priv->answer_count = DEFAULT_ANSWER_COUNT;
	forward->priv->search_area = NULL;
	forward->priv->bounded = FALSE;
}

static struct {
	const char *tp_attr;
	const char *gc_attr; /* NULL to ignore */
} attrs_map[] = {
	{ "countrycode", NULL },
	{ "country", "country" },
	{ "region", "state" },
	{ "county", "county" },
	{ "locality", "city" },
	{ "area", NULL },
	{ "postalcode", "postalcode" },
	{ "street", "street" },
	{ "building", NULL },
	{ "floor", NULL },
	{ "room",  NULL },
	{ "text", NULL },
	{ "description", NULL },
	{ "uri", NULL },
	{ "language", "accept-language" },
};

static const char *
tp_attr_to_gc_attr (const char *attr,
		    gboolean   *found)
{
	guint i;

	*found = FALSE;

	for (i = 0; i < G_N_ELEMENTS (attrs_map); i++) {
		if (g_str_equal (attr, attrs_map[i].tp_attr)){
			*found = TRUE;
			return attrs_map[i].gc_attr;
		}
	}

	return NULL;
}

static void
geocode_forward_fill_params (GeocodeForward *forward,
			     GHashTable    *params)
{
	GHashTableIter iter;
	GValue *value;
	const char *key;

	g_hash_table_iter_init (&iter, params);
	while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &value)) {
		gboolean found;
		const char *gc_attr;
		const char *str;

		gc_attr = tp_attr_to_gc_attr (key, &found);
		if (found == FALSE) {
			g_warning ("XEP attribute '%s' unhandled", key);
			continue;
		}
		if (gc_attr == NULL)
			continue;

		str = g_value_get_string (value);
		if (str == NULL)
			continue;

		geocode_forward_add (forward, gc_attr, str);
	}
}

/**
 * geocode_forward_new_for_params:
 * @params: (transfer none) (element-type utf8 GValue): a #GHashTable with string keys, and #GValue values.
 *
 * Creates a new #GeocodeForward to perform geocoding with. The
 * #GHashTable is in the format used by Telepathy, and documented
 * on <ulink url="http://telepathy.freedesktop.org/spec/Connection_Interface_Location.html#Mapping:Location">Telepathy's specification site</ulink>.
 *
 * See also: <ulink url="http://xmpp.org/extensions/xep-0080.html">XEP-0080 specification</ulink>.
 *
 * Returns: a new #GeocodeForward. Use g_object_unref() when done.
 **/
GeocodeForward *
geocode_forward_new_for_params (GHashTable *params)
{
	GeocodeForward *forward;

	g_return_val_if_fail (params != NULL, NULL);

	if (g_hash_table_lookup (params, "lat") != NULL &&
	    g_hash_table_lookup (params, "long") != NULL) {
		g_warning ("You already have longitude and latitude in those parameters");
	}

	forward = g_object_new (GEOCODE_TYPE_FORWARD, NULL);
	geocode_forward_fill_params (forward, params);

	return forward;
}

/**
 * geocode_forward_new_for_string:
 * @str: a string containing a free-form description of the location
 *
 * Creates a new #GeocodeForward to perform forward geocoding with. The
 * string is in free-form format.
 *
 * Returns: a new #GeocodeForward. Use g_object_unref() when done.
 **/
GeocodeForward *
geocode_forward_new_for_string (const char *location)
{
	GeocodeForward *forward;

	g_return_val_if_fail (location != NULL, NULL);

	forward = g_object_new (GEOCODE_TYPE_FORWARD, NULL);
	geocode_forward_add (forward, "location", location);

	return forward;
}

static void
geocode_forward_add (GeocodeForward *forward,
		     const char    *key,
		     const char    *value)
{
	g_return_if_fail (GEOCODE_IS_FORWARD (forward));
	g_return_if_fail (key != NULL);
	g_return_if_fail (value == NULL || g_utf8_validate (value, -1, NULL));

	g_hash_table_insert (forward->priv->ht,
			     g_strdup (key),
			     g_strdup (value));
}

static void
on_query_data_loaded (SoupSession *session,
                      SoupMessage *query,
                      gpointer     user_data)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GError *error = NULL;
	char *contents;
	gpointer ret;

        if (query->status_code != SOUP_STATUS_OK) {
		g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                     query->reason_phrase ? query->reason_phrase : "Query failed");
                g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		return;
	}

        contents = g_strndup (query->response_body->data, query->response_body->length);
        ret = _geocode_parse_search_json (contents, &error);

	if (ret == NULL) {
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		g_free (contents);
		return;
	}

	/* Now that we can parse the result, save it to cache */
	_geocode_glib_cache_save (query, contents);
	g_free (contents);

	g_simple_async_result_set_op_res_gpointer (simple, ret, NULL);
	g_simple_async_result_complete_in_idle (simple);
	g_object_unref (simple);
}

static void
on_cache_data_loaded (GObject      *source_forward,
		      GAsyncResult *res,
		      gpointer      user_data)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GFile *cache;
	GError *error = NULL;
	char *contents;
	gpointer ret;

	cache = G_FILE (source_forward);
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
		soup_session_queue_message (GEOCODE_FORWARD (object)->priv->soup_session,
                                            query,
					    on_query_data_loaded,
					    simple);
		return;
	}

        ret = _geocode_parse_search_json (contents, &error);
	g_free (contents);

	if (ret == NULL)
		g_simple_async_result_take_error (simple, error);
	else
		g_simple_async_result_set_op_res_gpointer (simple, ret, NULL);

	g_simple_async_result_complete_in_idle (simple);
	g_object_unref (simple);
}

static SoupMessage *
get_search_query_for_params (GeocodeForward *forward,
			     GError        **error)
{
	SoupMessage *ret;
	GHashTable *ht;
	char *lang;
	char *params;
	char *search_term;
	char *uri;
        guint8 i;
        gboolean query_possible = FALSE;
        char *location;
        const char *allowed_attributes[] = { "country",
                                             "region",
                                             "county",
                                             "locality",
                                             "postalcode",
                                             "street",
                                             "location",
                                             NULL };

        /* Make sure we have at least one parameter that Nominatim allows querying for */
	for (i = 0; allowed_attributes[i] != NULL; i++) {
	        if (g_hash_table_lookup (forward->priv->ht,
                                         allowed_attributes[i]) != NULL) {
			query_possible = TRUE;
			break;
		}
	}

        if (!query_possible) {
                char *str;

                str = g_strjoinv (", ", (char **) allowed_attributes);
                g_set_error (error, GEOCODE_ERROR, GEOCODE_ERROR_INVALID_ARGUMENTS,
                             "Only following parameters supported: %s", str);
                g_free (str);

		return NULL;
	}

	/* Prepare the query parameters */
	ht = _geocode_glib_dup_hash_table (forward->priv->ht);
	g_hash_table_insert (ht, (gpointer) "format", (gpointer) "jsonv2");
	g_hash_table_insert (ht, (gpointer) "email", (gpointer) "zeeshanak@gnome.org");
	g_hash_table_insert (ht, (gpointer) "addressdetails", (gpointer) "1");

	lang = NULL;
	if (g_hash_table_lookup (ht, "accept-language") == NULL) {
		lang = _geocode_object_get_lang ();
		if (lang)
			g_hash_table_insert (ht, (gpointer) "accept-language", lang);
	}

        location = g_strdup (g_hash_table_lookup (ht, "location"));
        g_hash_table_remove (ht, "location");
	params = soup_form_encode_hash (ht);
	g_hash_table_destroy (ht);
        if (lang)
                g_free (lang);

        if (location != NULL) {
	        /* Prepare the search term */
                search_term = soup_uri_encode (location, NULL);
                uri = g_strdup_printf ("https://nominatim.gnome.org/search?q=%s&limit=%u&bounded=%d&%s",
                                       search_term,
                                       forward->priv->answer_count,
                                       !!forward->priv->bounded,
                                       params);
                g_free (search_term);
                g_free (location);
        } else {
                uri = g_strdup_printf ("https://nominatim.gnome.org/search?limit=1&%s",
                                       params);
        }
	g_free (params);

	ret = soup_message_new ("GET", uri);
	g_free (uri);

	return ret;
}

/**
 * geocode_forward_search_async:
 * @forward: a #GeocodeForward representing a query
 * @cancellable: optional #GCancellable forward, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously performs a forward geocoding
 * query using a web service. Use geocode_forward_search() to do the same
 * thing synchronously.
 *
 * When the operation is finished, @callback will be called. You can then call
 * geocode_forward_search_finish() to get the result of the operation.
 **/
void
geocode_forward_search_async (GeocodeForward      *forward,
			      GCancellable        *cancellable,
			      GAsyncReadyCallback  callback,
			      gpointer             user_data)
{
	GSimpleAsyncResult *simple;
	SoupMessage *query;
	char *cache_path;
	GError *error = NULL;

	g_return_if_fail (GEOCODE_IS_FORWARD (forward));

	simple = g_simple_async_result_new (G_OBJECT (forward),
					    callback,
					    user_data,
					    geocode_forward_search_async);
	g_simple_async_result_set_check_cancellable (simple, cancellable);

        query = get_search_query_for_params (forward, &error);
	if (!query) {
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		return;
	}

	cache_path = _geocode_glib_cache_path_for_query (query);
	if (cache_path == NULL) {
		soup_session_queue_message (forward->priv->soup_session,
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
 * geocode_forward_search_finish:
 * @forward: a #GeocodeForward representing a query
 * @res: a #GAsyncResult.
 * @error: a #GError.
 *
 * Finishes a forward geocoding operation. See geocode_forward_search_async().
 *
 * Returns: (element-type GeocodePlace) (transfer container): A list of
 * places or %NULL in case of errors. Free the returned list with
 * g_list_free() when done.
 **/
GList *
geocode_forward_search_finish (GeocodeForward       *forward,
			       GAsyncResult        *res,
			       GError             **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

	g_return_val_if_fail (GEOCODE_IS_FORWARD (forward), NULL);

	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == geocode_forward_search_async);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_simple_async_result_get_op_res_gpointer (simple);
}

static struct {
	const char *nominatim_attr;
        const char *place_prop; /* NULL to ignore */
} nominatim_to_place_map[] = {
        { "license", NULL },
        { "osm_id", "osm-id" },
        { "lat", NULL },
        { "lon", NULL },
        { "display_name", NULL },
        { "house_number", "building" },
        { "road", "street" },
        { "suburb", "area" },
        { "city",  "town" },
        { "village",  "town" },
        { "county", "county" },
        { "state_district", "administrative-area" },
        { "state", "state" },
        { "postcode", "postal-code" },
        { "country", "country" },
        { "country_code", "country-code" },
        { "continent", "continent" },
        { "address", NULL },
};

static void
fill_place_from_entry (const char   *key,
                       const char   *value,
                       GeocodePlace *place)
{
        guint i;

        for (i = 0; i < G_N_ELEMENTS (nominatim_to_place_map); i++) {
                if (g_str_equal (key, nominatim_to_place_map[i].nominatim_attr)){
                        g_object_set (G_OBJECT (place),
                                      nominatim_to_place_map[i].place_prop,
                                      value,
                                      NULL);
                        break;
                }
        }

        if (g_str_equal (key, "osm_type")) {
                gpointer ref = g_type_class_ref (geocode_place_osm_type_get_type ());
                GEnumClass *class = G_ENUM_CLASS (ref);
                GEnumValue *evalue = g_enum_get_value_by_nick (class, value);

                if (evalue)
                        g_object_set (G_OBJECT (place), "osm-type", evalue->value, NULL);
                else
                        g_warning ("Unsupported osm-type %s", value);

                g_type_class_unref (ref);
        }
}

static gboolean
node_free_func (GNode    *node,
		gpointer  user_data)
{
	/* Leaf nodes are GeocodeLocation objects
	 * which we reuse for the results */
	if (G_NODE_IS_LEAF (node) == FALSE)
		g_free (node->data);

	return FALSE;
}

static const char const *attributes[] = {
	"country",
	"state",
	"county",
	"state_district",
	"postcode",
	"city",
	"suburb",
	"village",
};

static GeocodePlaceType
get_place_type_from_attributes (GHashTable *ht)
{
        char *category, *type;
        GeocodePlaceType place_type = GEOCODE_PLACE_TYPE_UNKNOWN;

        category = g_hash_table_lookup (ht, "category");
        type = g_hash_table_lookup (ht, "type");

        if (g_strcmp0 (category, "place") == 0) {
                if (g_strcmp0 (type, "house") == 0 ||
                    g_strcmp0 (type, "building") == 0 ||
                    g_strcmp0 (type, "residential") == 0 ||
                    g_strcmp0 (type, "plaza") == 0 ||
                    g_strcmp0 (type, "office") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_BUILDING;
                else if (g_strcmp0 (type, "estate") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_ESTATE;
                else if (g_strcmp0 (type, "town") == 0 ||
                         g_strcmp0 (type, "city") == 0 ||
                         g_strcmp0 (type, "hamlet") == 0 ||
                         g_strcmp0 (type, "isolated_dwelling") == 0 ||
                         g_strcmp0 (type, "village") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_TOWN;
                else if (g_strcmp0 (type, "suburb") == 0 ||
                         g_strcmp0 (type, "neighbourhood") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_SUBURB;
                else if (g_strcmp0 (type, "state") == 0 ||
                         g_strcmp0 (type, "region") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_STATE;
                else if (g_strcmp0 (type, "farm") == 0 ||
                         g_strcmp0 (type, "forest") == 0 ||
                         g_strcmp0 (type, "valey") == 0 ||
                         g_strcmp0 (type, "park") == 0 ||
                         g_strcmp0 (type, "hill") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_LAND_FEATURE;
                else if (g_strcmp0 (type, "island") == 0 ||
                         g_strcmp0 (type, "islet") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_ISLAND;
                else if (g_strcmp0 (type, "country") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_COUNTRY;
                else if (g_strcmp0 (type, "continent") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_CONTINENT;
                else if (g_strcmp0 (type, "lake") == 0 ||
                         g_strcmp0 (type, "bay") == 0 ||
                         g_strcmp0 (type, "river") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_DRAINAGE;
                else if (g_strcmp0 (type, "sea") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_SEA;
                else if (g_strcmp0 (type, "ocean") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_OCEAN;
        } else if (g_strcmp0 (category, "highway") == 0) {
                if (g_strcmp0 (type, "motorway") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_MOTORWAY;
                else if (g_strcmp0 (type, "bus_stop") == 0)
                        place_type =  GEOCODE_PLACE_TYPE_BUS_STOP;
                else
                        place_type =  GEOCODE_PLACE_TYPE_STREET;
        } else if (g_strcmp0 (category, "railway") == 0) {
                if (g_strcmp0 (type, "station") == 0 ||
                    g_strcmp0 (type, "halt") == 0)
                        place_type = GEOCODE_PLACE_TYPE_RAILWAY_STATION;
                else if (g_strcmp0 (type, "tram_stop") == 0)
                        place_type = GEOCODE_PLACE_TYPE_LIGHT_RAIL_STATION;
        } else if (g_strcmp0 (category, "waterway") == 0) {
                place_type =  GEOCODE_PLACE_TYPE_DRAINAGE;
        } else if (g_strcmp0 (category, "boundary") == 0) {
                if (g_strcmp0 (type, "administrative") == 0) {
                        int rank;

                        rank = atoi (g_hash_table_lookup (ht, "place_rank"));
                        if (rank < 2)
                                place_type =  GEOCODE_PLACE_TYPE_UNKNOWN;

                        if (rank == 28)
                                place_type =  GEOCODE_PLACE_TYPE_BUILDING;
                        else if (rank == 16)
                                place_type =  GEOCODE_PLACE_TYPE_TOWN;
                        else if (rank == 12)
                                place_type =  GEOCODE_PLACE_TYPE_COUNTY;
                        else if (rank == 10 || rank == 8)
                                place_type =  GEOCODE_PLACE_TYPE_STATE;
                        else if (rank == 4)
                                place_type =  GEOCODE_PLACE_TYPE_COUNTRY;
                }
        } else if (g_strcmp0 (category, "amenity") == 0) {
                if (g_strcmp0 (type, "school") == 0)
                        place_type = GEOCODE_PLACE_TYPE_SCHOOL;
                else if (g_strcmp0 (type, "place_of_worship") == 0)
                        place_type = GEOCODE_PLACE_TYPE_PLACE_OF_WORSHIP;
                else if (g_strcmp0 (type, "restaurant") == 0)
                        place_type = GEOCODE_PLACE_TYPE_RESTAURANT;
                else if (g_strcmp0 (type, "bar") == 0 ||
                         g_strcmp0 (type, "pub") == 0)
                        place_type = GEOCODE_PLACE_TYPE_BAR;
        } else if (g_strcmp0 (category, "aeroway") == 0) {
                if (g_strcmp0 (type, "aerodrome") == 0)
                        place_type = GEOCODE_PLACE_TYPE_AIRPORT;
        }

        return place_type;
}

GeocodePlace *
_geocode_create_place_from_attributes (GHashTable *ht)
{
        GeocodePlace *place;
        GeocodeLocation *loc = NULL;
        const char *name, *street, *building, *bbox_corner;
        GeocodePlaceType place_type;
        gdouble longitude, latitude;

        place_type = get_place_type_from_attributes (ht);

        name = g_hash_table_lookup (ht, "name");
        if (name == NULL)
                name = g_hash_table_lookup (ht, "display_name");

        place = geocode_place_new (name, place_type);

        /* If one corner exists, then all exists */
        bbox_corner = g_hash_table_lookup (ht, "boundingbox-top");
        if (bbox_corner != NULL) {
            GeocodeBoundingBox *bbox;
            gdouble top, bottom, left, right;

            top = g_ascii_strtod (bbox_corner, NULL);

            bbox_corner = g_hash_table_lookup (ht, "boundingbox-bottom");
            bottom = g_ascii_strtod (bbox_corner, NULL);

            bbox_corner = g_hash_table_lookup (ht, "boundingbox-left");
            left = g_ascii_strtod (bbox_corner, NULL);

            bbox_corner = g_hash_table_lookup (ht, "boundingbox-right");
            right = g_ascii_strtod (bbox_corner, NULL);

            bbox = geocode_bounding_box_new (top, bottom, left, right);
            geocode_place_set_bounding_box (place, bbox);
            g_object_unref (bbox);
        }

        /* Nominatim doesn't give us street addresses as such */
        street = g_hash_table_lookup (ht, "road");
        building = g_hash_table_lookup (ht, "house_number");
        if (street != NULL && building != NULL) {
            char *address;
            gboolean number_after;

            number_after = _geocode_object_is_number_after_street ();
            address = g_strdup_printf ("%s %s",
                                       number_after ? street : building,
                                       number_after ? building : street);
            geocode_place_set_street_address (place, address);
            g_free (address);
        }

        g_hash_table_foreach (ht, (GHFunc) fill_place_from_entry, place);

        /* Get latitude and longitude and create GeocodeLocation object. */
        longitude = g_ascii_strtod (g_hash_table_lookup (ht, "lon"), NULL);
        latitude = g_ascii_strtod (g_hash_table_lookup (ht, "lat"), NULL);
        name = geocode_place_get_name (place);

        loc = geocode_location_new_with_description (latitude,
                                                     longitude,
                                                     GEOCODE_LOCATION_ACCURACY_UNKNOWN,
                                                     name);
        geocode_place_set_location (place, loc);
        g_object_unref (loc);

        return place;
}

static void
insert_place_into_tree (GNode *place_tree, GHashTable *ht)
{
	GNode *start = place_tree;
        GeocodePlace *place = NULL;
	char *attr_val = NULL;
	guint i;

	for (i = 0; i < G_N_ELEMENTS(attributes); i++) {
		GNode *child = NULL;

		attr_val = g_hash_table_lookup (ht, attributes[i]);
		if (!attr_val) {
			/* Add a dummy node if the attribute value is not
			 * available for the place */
			child = g_node_insert_data (start, -1, NULL);
		} else {
			/* If the attr value (eg for country United States)
			 * already exists, then keep on adding other attributes under that node. */
			child = g_node_first_child (start);
			while (child &&
			       child->data &&
			       g_ascii_strcasecmp (child->data, attr_val) != 0) {
				child = g_node_next_sibling (child);
			}
			if (!child) {
				/* create a new node */
				child = g_node_insert_data (start, -1, g_strdup (attr_val));
			}
		}
		start = child;
	}

        place = _geocode_create_place_from_attributes (ht);

        /* The leaf node of the tree is the GeocodePlace object, containing
         * associated GeocodePlace object */
	g_node_insert_data (start, -1, place);
}

static void
make_place_list_from_tree (GNode  *node,
                           char  **s_array,
                           GList **place_list,
                           int     i)
{
	GNode *child;

	if (node == NULL)
		return;

	if (G_NODE_IS_LEAF (node)) {
		GPtrArray *rev_s_array;
		GeocodePlace *place;
		GeocodeLocation *loc;
		char *name;
		int counter = 0;

		rev_s_array = g_ptr_array_new ();

		/* If leaf node, then add all the attributes in the s_array
		 * and set it to the description of the loc object */
		place = (GeocodePlace *) node->data;
		name = (char *) geocode_place_get_name (place);
		loc = geocode_place_get_location (place);

		/* To print the attributes in a meaningful manner
		 * reverse the s_array */
		g_ptr_array_add (rev_s_array, (gpointer) name);
		for (counter = 1; counter <= i; counter++)
			g_ptr_array_add (rev_s_array, s_array[i - counter]);
		g_ptr_array_add (rev_s_array, NULL);
		name = g_strjoinv (", ", (char **) rev_s_array->pdata);
		g_ptr_array_unref (rev_s_array);

		geocode_place_set_name (place, name);
		geocode_location_set_description (loc, name);
		g_free (name);

		*place_list = g_list_prepend (*place_list, place);
	} else {
                GNode *prev, *next;

                prev = g_node_prev_sibling (node);
                next = g_node_next_sibling (node);

		/* If there are other attributes with a different value,
		 * add those attributes to the string to differentiate them */
		if (node->data && ((prev && prev->data) || (next && next->data))) {
                        s_array[i] = node->data;
                        i++;
		}
	}

	for (child = node->children; child != NULL; child = child->next)
		make_place_list_from_tree (child, s_array, place_list, i);
}

GList *
_geocode_parse_search_json (const char *contents,
			     GError    **error)
{
	GList *ret;
	JsonParser *parser;
	JsonNode *root;
	JsonReader *reader;
	const GError *err = NULL;
	int num_places, i;
	GNode *place_tree;
	char *s_array[G_N_ELEMENTS (attributes)];

	ret = NULL;

	parser = json_parser_new ();
	if (json_parser_load_from_data (parser, contents, -1, error) == FALSE) {
		g_object_unref (parser);
		return ret;
	}

	root = json_parser_get_root (parser);
	reader = json_reader_new (root);

	num_places = json_reader_count_elements (reader);
	if (num_places < 0)
		goto parse;
        if (num_places == 0) {
	        g_set_error_literal (error,
                                     GEOCODE_ERROR,
                                     GEOCODE_ERROR_NO_MATCHES,
                                     "No matches found for request");
		goto no_results;
        }

	place_tree = g_node_new (NULL);

	for (i = 0; i < num_places; i++) {
		GHashTable *ht;

		json_reader_read_element (reader, i);

                ht = g_hash_table_new_full (g_str_hash, g_str_equal,
				            g_free, g_free);
                _geocode_read_nominatim_attributes (reader, ht);

		/* Populate the tree with place details */
		insert_place_into_tree (place_tree, ht);

		g_hash_table_destroy (ht);

		json_reader_end_element (reader);
	}

	make_place_list_from_tree (place_tree, s_array, &ret, 0);

	g_node_traverse (place_tree,
			 G_IN_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 (GNodeTraverseFunc) node_free_func,
			 NULL);

	g_node_destroy (place_tree);

	g_object_unref (parser);
	g_object_unref (reader);
	ret = g_list_reverse (ret);

	return ret;
parse:
	err = json_reader_get_error (reader);
	g_set_error_literal (error, GEOCODE_ERROR, GEOCODE_ERROR_PARSE, err->message);
no_results:
	g_object_unref (parser);
	g_object_unref (reader);
	return NULL;
}

/**
 * geocode_forward_search:
 * @forward: a #GeocodeForward representing a query
 * @error: a #GError
 *
 * Gets the result of a forward geocoding
 * query using a web service.
 *
 * Returns: (element-type GeocodePlace) (transfer container): A list of
 * places or %NULL in case of errors. Free the returned list with
 * g_list_free() when done.
 **/
GList *
geocode_forward_search (GeocodeForward      *forward,
			GError             **error)
{
	SoupMessage *query;
	char *contents;
	GList *ret;
	gboolean to_cache = FALSE;

	g_return_val_if_fail (GEOCODE_IS_FORWARD (forward), NULL);

        query = get_search_query_for_params (forward, error);
	if (!query)
		return NULL;

	if (_geocode_glib_cache_load (query, &contents) == FALSE) {
                if (soup_session_send_message (forward->priv->soup_session,
                                               query) != SOUP_STATUS_OK) {
                        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                             query->reason_phrase ? query->reason_phrase : "Query failed");
                        g_object_unref (query);
                        return NULL;
                }
                contents = g_strndup (query->response_body->data, query->response_body->length);

		to_cache = TRUE;
	}

        ret = _geocode_parse_search_json (contents, error);
	if (to_cache && ret != NULL)
		_geocode_glib_cache_save (query, contents);

	g_free (contents);
	g_object_unref (query);

	return ret;
}

/**
 * geocode_forward_set_answer_count:
 * @forward: a #GeocodeForward representing a query
 * @count: the number of requested results
 *
 * Sets the number of requested results to @count.
 **/
void
geocode_forward_set_answer_count (GeocodeForward *forward,
				  guint           count)
{
	g_return_if_fail (GEOCODE_IS_FORWARD (forward));

	forward->priv->answer_count = count;
}

/**
 * geocode_forward_set_search_area:
 * @forward: a #GeocodeForward representing a query
 * @box: a bounding box to limit the search area.
 *
 * Sets the area to limit searches within.
 **/
void
geocode_forward_set_search_area (GeocodeForward     *forward,
				 GeocodeBoundingBox *bbox)
{
	char *area;
	char top[G_ASCII_DTOSTR_BUF_SIZE];
	char left[G_ASCII_DTOSTR_BUF_SIZE];
	char bottom[G_ASCII_DTOSTR_BUF_SIZE];
	char right[G_ASCII_DTOSTR_BUF_SIZE];

	g_return_if_fail (GEOCODE_IS_FORWARD (forward));

	forward->priv->search_area = bbox;

	/* need to convert with g_ascii_dtostr to be locale safe */
	g_ascii_dtostr (top, G_ASCII_DTOSTR_BUF_SIZE,
	                geocode_bounding_box_get_top (bbox));

	g_ascii_dtostr (bottom, G_ASCII_DTOSTR_BUF_SIZE,
	                geocode_bounding_box_get_bottom (bbox));

	g_ascii_dtostr (left, G_ASCII_DTOSTR_BUF_SIZE,
	                geocode_bounding_box_get_left (bbox));

	g_ascii_dtostr (right, G_ASCII_DTOSTR_BUF_SIZE,
	                geocode_bounding_box_get_right (bbox));

	area = g_strdup_printf ("%s,%s,%s,%s", left, top, right, bottom);
	geocode_forward_add (forward, "viewbox", area);
	g_free (area);
}

/**
 * geocode_forward_set_bounded:
 * @forward: a #GeocodeForward representing a query
 * @bounded: #TRUE to restrict results to only items contained within the
 * #GeocodeForward:search-area bounding box.
 *
 * Set the #GeocodeForward:bounded property that regulates whether the
 * #GeocodeForward:search-area property acts restricting or not.
 **/
void
geocode_forward_set_bounded (GeocodeForward *forward,
			     gboolean        bounded)
{
	g_return_if_fail (GEOCODE_IS_FORWARD (forward));

	forward->priv->bounded = bounded;
}

/**
 * geocode_forward_get_answer_count:
 * @forward: a #GeocodeForward representing a query
 *
 * Gets the number of requested results for searches.
 **/
guint
geocode_forward_get_answer_count (GeocodeForward *forward)
{
	g_return_val_if_fail (GEOCODE_IS_FORWARD (forward), 0);

	return forward->priv->answer_count;
}

/**
 * geocode_forward_get_search_area:
 * @forward: a #GeocodeForward representing a query
 *
 * Gets the area to limit searches within.
 **/
GeocodeBoundingBox *
geocode_forward_get_search_area (GeocodeForward *forward)
{
	g_return_val_if_fail (GEOCODE_IS_FORWARD (forward), NULL);

	return forward->priv->search_area;
}

/**
 * geocode_forward_get_bounded:
 * @forward: a #GeocodeForward representing a query
 *
 * Gets the #GeocodeForward:bounded property that regulates whether the
 * #GeocodeForward:search-area property acts restricting or not.
 **/
gboolean
geocode_forward_get_bounded (GeocodeForward *forward)
{
	g_return_val_if_fail (GEOCODE_IS_FORWARD (forward), FALSE);

	return forward->priv->bounded;
}
