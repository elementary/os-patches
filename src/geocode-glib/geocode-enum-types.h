


#ifndef __GEOCODE_ENUM_TYPES_H__
#define __GEOCODE_ENUM_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS
/* enumerations from "geocode-location.h" */
GType geocode_location_uri_scheme_get_type (void) G_GNUC_CONST;
#define GEOCODE_TYPE_LOCATION_URI_SCHEME (geocode_location_uri_scheme_get_type())

/**
 * SECTION:geocode-enum-types
 * @short_description: Geocode enumerated types
 * @include: geocode-glib/geocode-glib.h
 *
 * The enumerated types defined and used by geocode-glib.
 **/
GType geocode_location_crs_get_type (void) G_GNUC_CONST;
#define GEOCODE_TYPE_LOCATION_CRS (geocode_location_crs_get_type())

/**
 * SECTION:geocode-enum-types
 * @short_description: Geocode enumerated types
 * @include: geocode-glib/geocode-glib.h
 *
 * The enumerated types defined and used by geocode-glib.
 **/
/* enumerations from "geocode-error.h" */
GType geocode_error_get_type (void) G_GNUC_CONST;
#define GEOCODE_TYPE_ERROR (geocode_error_get_type())

/**
 * SECTION:geocode-enum-types
 * @short_description: Geocode enumerated types
 * @include: geocode-glib/geocode-glib.h
 *
 * The enumerated types defined and used by geocode-glib.
 **/
/* enumerations from "geocode-place.h" */
GType geocode_place_type_get_type (void) G_GNUC_CONST;
#define GEOCODE_TYPE_PLACE_TYPE (geocode_place_type_get_type())

/**
 * SECTION:geocode-enum-types
 * @short_description: Geocode enumerated types
 * @include: geocode-glib/geocode-glib.h
 *
 * The enumerated types defined and used by geocode-glib.
 **/
GType geocode_place_osm_type_get_type (void) G_GNUC_CONST;
#define GEOCODE_TYPE_PLACE_OSM_TYPE (geocode_place_osm_type_get_type())

/**
 * SECTION:geocode-enum-types
 * @short_description: Geocode enumerated types
 * @include: geocode-glib/geocode-glib.h
 *
 * The enumerated types defined and used by geocode-glib.
 **/
G_END_DECLS

#endif /* __GEOCODE_ENUM_TYPES_H__ */



