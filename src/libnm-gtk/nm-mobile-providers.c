/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2009 Novell, Inc.
 * Author: Tambet Ingo (tambet@gmail.com).
 *
 * Copyright (C) 2009 - 2012 Red Hat, Inc.
 * Copyright (C) 2012 Lanedo GmbH
 */

#include "nm-default.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "nm-mobile-providers.h"

#ifndef MOBILE_BROADBAND_PROVIDER_INFO
#define MOBILE_BROADBAND_PROVIDER_INFO DATADIR"/mobile-broadband-provider-info/serviceproviders.xml"
#endif

#define ISO_3166_COUNTRY_CODES ISO_CODES_PREFIX"/share/xml/iso-codes/iso_3166.xml"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX"/share/locale"

/******************************************************************************/
/* Access method type */

G_DEFINE_BOXED_TYPE (NMAMobileAccessMethod,
                     nma_mobile_access_method,
                     nma_mobile_access_method_ref,
                     nma_mobile_access_method_unref)

struct _NMAMobileAccessMethod {
	volatile gint refs;

	char *name;
	/* maps lang (char *) -> name (char *) */
	GHashTable *lcl_names;

	char *username;
	char *password;
	char *gateway;
	GPtrArray *dns; /* GPtrArray of 'char *' */

	/* Only used with 3GPP family type providers */
	char *apn;

	NMAMobileFamily family;
};

static NMAMobileAccessMethod *
access_method_new (void)
{
	NMAMobileAccessMethod *method;

	method = g_slice_new0 (NMAMobileAccessMethod);
	method->refs = 1;
	method->lcl_names = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                           (GDestroyNotify) g_free,
	                                           (GDestroyNotify) g_free);

	return method;
}

NMAMobileAccessMethod *
nma_mobile_access_method_ref (NMAMobileAccessMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);
	g_return_val_if_fail (method->refs > 0, NULL);

	g_atomic_int_inc (&method->refs);

	return method;
}

void
nma_mobile_access_method_unref (NMAMobileAccessMethod *method)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (method->refs > 0);

	if (g_atomic_int_dec_and_test (&method->refs)) {
		g_free (method->name);
		g_hash_table_destroy (method->lcl_names);
		g_free (method->username);
		g_free (method->password);
		g_free (method->gateway);
		g_free (method->apn);

		if (method->dns)
			g_ptr_array_unref (method->dns);

		g_slice_free (NMAMobileAccessMethod, method);
	}
}

/**
 * nma_mobile_access_method_get_name:
 *
 * Returns: (transfer none): the name of the method.
 */
const gchar *
nma_mobile_access_method_get_name (NMAMobileAccessMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);

	return method->name;
}

/**
 * nma_mobile_access_method_get_username:
 *
 * Returns: (transfer none): the username.
 */
const gchar *
nma_mobile_access_method_get_username (NMAMobileAccessMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);

	return method->username;
}

/**
 * nma_mobile_access_method_get_password:
 *
 * Returns: (transfer none): the password.
 */
const gchar *
nma_mobile_access_method_get_password (NMAMobileAccessMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);

	return method->password;
}

/**
 * nma_mobile_access_method_get_gateway:
 *
 * Returns: (transfer none): the gateway.
 */
const gchar *
nma_mobile_access_method_get_gateway (NMAMobileAccessMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);

	return method->gateway;
}

/**
 * nma_mobile_access_method_get_dns:
 *
 * Returns: (transfer none) (array zero-terminated=1) (element-type utf8): the list of DNS.
 */
const gchar **
nma_mobile_access_method_get_dns (NMAMobileAccessMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);

	return method->dns ? (const gchar **)method->dns->pdata : NULL;
}

/**
 * nma_mobile_access_method_get_3gpp_apn:
 *
 * Returns: (transfer none): the 3GPP APN.
 */
const gchar *
nma_mobile_access_method_get_3gpp_apn (NMAMobileAccessMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);

	return method->apn;
}

/**
 * nma_mobile_access_method_get_family:
 *
 * Returns: a #NMAMobileFamily.
 */
NMAMobileFamily
nma_mobile_access_method_get_family (NMAMobileAccessMethod *method)
{
	g_return_val_if_fail (method != NULL, NMA_MOBILE_FAMILY_UNKNOWN);

	return method->family;
}

/******************************************************************************/
/* Mobile provider type */

G_DEFINE_BOXED_TYPE (NMAMobileProvider,
                     nma_mobile_provider,
                     nma_mobile_provider_ref,
                     nma_mobile_provider_unref)

struct _NMAMobileProvider {
	volatile gint refs;

	char *name;
	/* maps lang (char *) -> name (char *) */
	GHashTable *lcl_names;

	GSList *methods; /* GSList of NmaMobileAccessMethod */

	GPtrArray *mcc_mnc;  /* GPtrArray of strings */
	GArray *cdma_sid; /* GArray of guint32 */
};

static NMAMobileProvider *
provider_new (void)
{
	NMAMobileProvider *provider;

	provider = g_slice_new0 (NMAMobileProvider);
	provider->refs = 1;
	provider->lcl_names = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                             (GDestroyNotify) g_free,
	                                             (GDestroyNotify) g_free);

	return provider;
}

NMAMobileProvider *
nma_mobile_provider_ref (NMAMobileProvider *provider)
{
	g_return_val_if_fail (provider != NULL, NULL);
	g_return_val_if_fail (provider->refs > 0, NULL);

	g_atomic_int_inc (&provider->refs);

	return provider;
}

void
nma_mobile_provider_unref (NMAMobileProvider *provider)
{
	if (g_atomic_int_dec_and_test (&provider->refs)) {
		g_free (provider->name);
		g_hash_table_destroy (provider->lcl_names);

		g_slist_foreach (provider->methods, (GFunc) nma_mobile_access_method_unref, NULL);
		g_slist_free (provider->methods);

		if (provider->mcc_mnc)
			g_ptr_array_unref (provider->mcc_mnc);

		if (provider->cdma_sid)
			g_array_unref (provider->cdma_sid);

		g_slice_free (NMAMobileProvider, provider);
	}
}

/**
 * nma_mobile_provider_get_name:
 *
 * Returns: (transfer none): the name of the provider.
 */
const gchar *
nma_mobile_provider_get_name (NMAMobileProvider *provider)
{
	g_return_val_if_fail (provider != NULL, NULL);

	return provider->name;
}

/**
 * nma_mobile_provider_get_methods:
 * @provider: a #NMAMobileProvider
 *
 * Returns: (element-type NMGtk.MobileAccessMethod) (transfer none): the
 *	 list of #NMAMobileAccessMethod this provider exposes.
 */
GSList *
nma_mobile_provider_get_methods (NMAMobileProvider *provider)
{
	g_return_val_if_fail (provider != NULL, NULL);

	return provider->methods;
}

/**
 * nma_mobile_provider_get_3gpp_mcc_mnc:
 * @provider: a #NMAMobileProvider
 *
 * Returns: (transfer none) (array zero-terminated=1) (element-type utf8): a
 *	 list of strings with the MCC and MNC codes this provider exposes.
 */
const gchar **
nma_mobile_provider_get_3gpp_mcc_mnc (NMAMobileProvider *provider)
{
	g_return_val_if_fail (provider != NULL, NULL);

	return provider->mcc_mnc ? (const gchar **)provider->mcc_mnc->pdata : NULL;
}

/**
 * nma_mobile_provider_get_cdma_sid:
 * @provider: a #NMAMobileProvider
 *
 * Returns: (transfer none) (array zero-terminated=1) (element-type guint32): the
 *	 list of CDMA SIDs this provider exposes
 */
const guint32 *
nma_mobile_provider_get_cdma_sid (NMAMobileProvider *provider)
{
	g_return_val_if_fail (provider != NULL, NULL);

	return provider->cdma_sid ? (const guint32 *)provider->cdma_sid->data : NULL;
}

/******************************************************************************/
/* Country Info type */

G_DEFINE_BOXED_TYPE (NMACountryInfo,
                     nma_country_info,
                     nma_country_info_ref,
                     nma_country_info_unref)

struct _NMACountryInfo {
	volatile gint refs;

	char *country_code;
	char *country_name;
	GSList *providers;
};

static NMACountryInfo *
country_info_new (const char *country_code,
                  const gchar *country_name)
{
	NMACountryInfo *country_info;

	country_info = g_slice_new0 (NMACountryInfo);
	country_info->refs = 1;
	country_info->country_code = g_strdup (country_code);
	country_info->country_name = g_strdup (country_name);
	return country_info;
}

NMACountryInfo *
nma_country_info_ref (NMACountryInfo *country_info)
{
	g_return_val_if_fail (country_info != NULL, NULL);
	g_return_val_if_fail (country_info->refs > 0, NULL);

	g_atomic_int_inc (&country_info->refs);

	return country_info;
}

void
nma_country_info_unref (NMACountryInfo *country_info)
{
	if (g_atomic_int_dec_and_test (&country_info->refs)) {
		g_free (country_info->country_code);
		g_free (country_info->country_name);
		g_slist_free_full (country_info->providers,
		                   (GDestroyNotify) nma_mobile_provider_unref);
		g_slice_free (NMACountryInfo, country_info);
	}
}

/**
 * nma_country_info_get_country_code:
 *
 * Returns: (transfer none): the code of the country.
 */
const gchar *
nma_country_info_get_country_code (NMACountryInfo *country_info)
{
	g_return_val_if_fail (country_info != NULL, NULL);

	return country_info->country_code;
}

/**
 * nma_country_info_get_country_name:
 *
 * Returns: (transfer none): the name of the country.
 */
const gchar *
nma_country_info_get_country_name (NMACountryInfo *country_info)
{
	g_return_val_if_fail (country_info != NULL, NULL);

	return country_info->country_name;
}

/**
 * nma_country_info_get_providers:
 *
 * Returns: (element-type NMGtk.MobileProvider) (transfer none): the
 *	 list of #NMAMobileProvider this country exposes.
 */
GSList *
nma_country_info_get_providers (NMACountryInfo *country_info)
{
	g_return_val_if_fail (country_info != NULL, NULL);

	return country_info->providers;
}

/******************************************************************************/
/* XML Parser for iso_3166.xml */

static void
iso_3166_parser_start_element (GMarkupParseContext *context,
                               const gchar *element_name,
                               const gchar **attribute_names,
                               const gchar **attribute_values,
                               gpointer data,
                               GError **error)
{
	int i;
	const char *country_code = NULL;
	const char *common_name = NULL;
	const char *name = NULL;
	GHashTable *table = (GHashTable *) data;

	if (!strcmp (element_name, "iso_3166_entry")) {
		NMACountryInfo *country_info;

		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "alpha_2_code"))
				country_code = attribute_values[i];
			else if (!strcmp (attribute_names[i], "common_name"))
				common_name = attribute_values[i];
			else if (!strcmp (attribute_names[i], "name"))
				name = attribute_values[i];
		}
		if (!country_code) {
			g_warning ("%s: missing mandatory 'alpha_2_code' atribute in '%s'"
			           " element.", __func__, element_name);
			return;
		}
		if (!name) {
			g_warning ("%s: missing mandatory 'name' atribute in '%s'"
			           " element.", __func__, element_name);
			return;
		}

		country_info = country_info_new (country_code,
		                                 dgettext ("iso_3166", common_name ? common_name : name));

		g_hash_table_insert (table, g_strdup (country_code), country_info);
	}
}

static const GMarkupParser iso_3166_parser = {
	iso_3166_parser_start_element,
	NULL, /* end element */
	NULL, /* text */
	NULL, /* passthrough */
	NULL  /* error */
};

static GHashTable *
read_country_codes (const gchar *country_codes_file,
                    GCancellable *cancellable,
                    GError **error)
{
	GHashTable *table = NULL;
	GMarkupParseContext *ctx;
	char *buf;
	gsize buf_len;

	/* Set domain to iso_3166 for country name translation */
	bindtextdomain ("iso_3166", ISO_CODES_LOCALESDIR);
	bind_textdomain_codeset ("iso_3166", "UTF-8");

	if (!g_file_get_contents (country_codes_file, &buf, &buf_len, error)) {
		g_prefix_error (error,
		                "Failed to load '%s' from 'iso-codes': ",
		                country_codes_file);
		return NULL;
	}

	table = g_hash_table_new_full (g_str_hash,
	                               g_str_equal,
	                               g_free,
	                               (GDestroyNotify)nma_country_info_unref);

	ctx = g_markup_parse_context_new (&iso_3166_parser, 0, table, NULL);
	if (!g_markup_parse_context_parse (ctx, buf, buf_len, error)) {
		g_prefix_error (error,
		                "Failed to parse '%s' from 'iso-codes': ",
		                country_codes_file);
		g_hash_table_destroy (table);
		return NULL;
	}

	g_markup_parse_context_free (ctx);
	g_free (buf);
	return table;
}

/******************************************************************************/
/* XML Parser for serviceproviders.xml */

typedef enum {
	PARSER_TOPLEVEL = 0,
	PARSER_COUNTRY,
	PARSER_PROVIDER,
	PARSER_METHOD_GSM,
	PARSER_METHOD_GSM_APN,
	PARSER_METHOD_CDMA,
	PARSER_ERROR
} MobileContextState;

typedef struct {
	GHashTable *table;

	char *current_country;
	GSList *current_providers;
	NMAMobileProvider *current_provider;
	NMAMobileAccessMethod *current_method;

	char *text_buffer;
	MobileContextState state;
} MobileParser;

static void
provider_list_free (gpointer data)
{
	GSList *list = (GSList *) data;

	while (list) {
		nma_mobile_provider_unref ((NMAMobileProvider *) list->data);
		list = g_slist_delete_link (list, list);
	}
}

static void
parser_toplevel_start (MobileParser *parser,
                       const char *name,
                       const char **attribute_names,
                       const char **attribute_values)
{
	int i;

	if (!strcmp (name, "serviceproviders")) {
		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "format")) {
				if (strcmp (attribute_values[i], "2.0")) {
					g_warning ("%s: mobile broadband provider database format '%s'"
					           " not supported.", __func__, attribute_values[i]);
					parser->state = PARSER_ERROR;
					break;
				}
			}
		}
	} else if (!strcmp (name, "country")) {
		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "code")) {
				char *country_code;
				NMACountryInfo *country_info;

				country_code = g_ascii_strup (attribute_values[i], -1);
				country_info = g_hash_table_lookup (parser->table, country_code);
				/* Ensure we have a country provider for this country code */
				if (!country_info) {
					g_warning ("%s: adding providers for unknown country '%s'", __func__, country_code);
					country_info = country_info_new (country_code, NULL);
					g_hash_table_insert (parser->table, country_code, country_info);
				}
				parser->current_country = country_code;

				parser->state = PARSER_COUNTRY;
				break;
			}
		}
	}
}

static void
parser_country_start (MobileParser *parser,
                      const char *name,
                      const char **attribute_names,
                      const char **attribute_values)
{
	if (!strcmp (name, "provider")) {
		parser->state = PARSER_PROVIDER;
		parser->current_provider = provider_new ();
	}
}

static void
parser_provider_start (MobileParser *parser,
                       const char *name,
                       const char **attribute_names,
                       const char **attribute_values)
{
	if (!strcmp (name, "gsm"))
		parser->state = PARSER_METHOD_GSM;
	else if (!strcmp (name, "cdma")) {
		parser->state = PARSER_METHOD_CDMA;
		parser->current_method = access_method_new ();
	}
}

static void
parser_gsm_start (MobileParser *parser,
                  const char *name,
                  const char **attribute_names,
                  const char **attribute_values)
{
	if (!strcmp (name, "network-id")) {
		const char *mcc = NULL, *mnc = NULL;
		int i;

		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "mcc"))
				mcc = attribute_values[i];
			else if (!strcmp (attribute_names[i], "mnc"))
				mnc = attribute_values[i];

			if (mcc && strlen (mcc) && mnc && strlen (mnc)) {
				gchar *mccmnc;

				if (!parser->current_provider->mcc_mnc)
					parser->current_provider->mcc_mnc = g_ptr_array_new_full (2, g_free);

				mccmnc = g_strdup_printf ("%s%s", mcc, mnc);
				g_ptr_array_add (parser->current_provider->mcc_mnc, mccmnc);
				break;
			}
		}
	} else if (!strcmp (name, "apn")) {
		int i;

		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "value")) {

				parser->state = PARSER_METHOD_GSM_APN;
				parser->current_method = access_method_new ();
				parser->current_method->apn = g_strstrip (g_strdup (attribute_values[i]));
				break;
			}
		}
	}
}

static void
parser_cdma_start (MobileParser *parser,
                   const char *name,
                   const char **attribute_names,
                   const char **attribute_values)
{
	if (!strcmp (name, "sid")) {
		int i;

		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "value")) {
				guint32 tmp;

				errno = 0;
				tmp = (guint32) strtoul (attribute_values[i], NULL, 10);
				if (errno == 0 && tmp > 0) {
					if (!parser->current_provider->cdma_sid)
						parser->current_provider->cdma_sid = g_array_sized_new (TRUE, FALSE, sizeof (guint32), 2);
					g_array_append_val (parser->current_provider->cdma_sid, tmp);
				}
				break;
			}
		}
	}
}

static void
mobile_parser_start_element (GMarkupParseContext *context,
                             const gchar *element_name,
                             const gchar **attribute_names,
                             const gchar **attribute_values,
                             gpointer data,
                             GError **error)
{
	MobileParser *parser = (MobileParser *) data;

	if (parser->text_buffer) {
		g_free (parser->text_buffer);
		parser->text_buffer = NULL;
	}

	switch (parser->state) {
	case PARSER_TOPLEVEL:
		parser_toplevel_start (parser, element_name, attribute_names, attribute_values);
		break;
	case PARSER_COUNTRY:
		parser_country_start (parser, element_name, attribute_names, attribute_values);
		break;
	case PARSER_PROVIDER:
		parser_provider_start (parser, element_name, attribute_names, attribute_values);
		break;
	case PARSER_METHOD_GSM:
		parser_gsm_start (parser, element_name, attribute_names, attribute_values);
		break;
	case PARSER_METHOD_CDMA:
		parser_cdma_start (parser, element_name, attribute_names, attribute_values);
		break;
	default:
		break;
	}
}

static void
parser_country_end (MobileParser *parser,
                    const char *name)
{
	if (!strcmp (name, "country")) {
		NMACountryInfo *country_info;

		country_info = g_hash_table_lookup (parser->table, parser->current_country);
		g_assert (country_info);

		/* Store providers for this country */
		country_info->providers = parser->current_providers;

		g_free (parser->current_country);
		parser->current_country = NULL;
		parser->current_providers = NULL;
		g_free (parser->text_buffer);
		parser->text_buffer = NULL;
		parser->state = PARSER_TOPLEVEL;
	}
}

static void
parser_provider_end (MobileParser *parser,
                     const char *name)
{
	if (!strcmp (name, "name")) {
		if (!parser->current_provider->name) {
			/* Use the first one. */
			parser->current_provider->name = parser->text_buffer;
			parser->text_buffer = NULL;
		}
	} else if (!strcmp (name, "provider")) {
		if (parser->current_provider->mcc_mnc)
			g_ptr_array_add (parser->current_provider->mcc_mnc, NULL);

		parser->current_provider->methods = g_slist_reverse (parser->current_provider->methods);

		parser->current_providers = g_slist_prepend (parser->current_providers, parser->current_provider);
		parser->current_provider = NULL;
		g_free (parser->text_buffer);
		parser->text_buffer = NULL;
		parser->state = PARSER_COUNTRY;
	}
}

static void
parser_gsm_end (MobileParser *parser,
                const char *name)
{
	if (!strcmp (name, "gsm")) {
		g_free (parser->text_buffer);
		parser->text_buffer = NULL;
		parser->state = PARSER_PROVIDER;
	}
}

static void
parser_gsm_apn_end (MobileParser *parser,
                    const char *name)
{
	if (!strcmp (name, "name")) {
		if (!parser->current_method->name) {
			/* Use the first one. */
			parser->current_method->name = parser->text_buffer;
			parser->text_buffer = NULL;
		}
	} else if (!strcmp (name, "username")) {
		parser->current_method->username = parser->text_buffer;
		parser->text_buffer = NULL;
	} else if (!strcmp (name, "password")) {
		parser->current_method->password = parser->text_buffer;
		parser->text_buffer = NULL;
	} else if (!strcmp (name, "dns")) {
		if (!parser->current_method->dns)
			parser->current_method->dns = g_ptr_array_new_full (2, g_free);
		g_ptr_array_add (parser->current_method->dns, parser->text_buffer);
		parser->text_buffer = NULL;
	} else if (!strcmp (name, "gateway")) {
		parser->current_method->gateway = parser->text_buffer;
		parser->text_buffer = NULL;
	} else if (!strcmp (name, "apn")) {
		parser->current_method->family = NMA_MOBILE_FAMILY_3GPP;

		if (!parser->current_method->name)
			parser->current_method->name = g_strdup (_("Default"));

		if (parser->current_method->dns)
			g_ptr_array_add (parser->current_method->dns, NULL);

		parser->current_provider->methods = g_slist_prepend (parser->current_provider->methods,
		                                                     parser->current_method);
		parser->current_method = NULL;
		g_free (parser->text_buffer);
		parser->text_buffer = NULL;
		parser->state = PARSER_METHOD_GSM;
	}
}

static void
parser_cdma_end (MobileParser *parser,
                 const char *name)
{
	if (!strcmp (name, "username")) {
		parser->current_method->username = parser->text_buffer;
		parser->text_buffer = NULL;
	} else if (!strcmp (name, "password")) {
		parser->current_method->password = parser->text_buffer;
		parser->text_buffer = NULL;
	} else if (!strcmp (name, "dns")) {
		if (!parser->current_method->dns)
			parser->current_method->dns = g_ptr_array_new_full (2, g_free);
		g_ptr_array_add (parser->current_method->dns, parser->text_buffer);
		parser->text_buffer = NULL;
	} else if (!strcmp (name, "gateway")) {
		parser->current_method->gateway = parser->text_buffer;
		parser->text_buffer = NULL;
	} else if (!strcmp (name, "cdma")) {
		parser->current_method->family = NMA_MOBILE_FAMILY_CDMA;

		if (!parser->current_method->name)
			parser->current_method->name = g_strdup (parser->current_provider->name);

		if (parser->current_method->dns)
			g_ptr_array_add (parser->current_method->dns, NULL);

		parser->current_provider->methods = g_slist_prepend (parser->current_provider->methods,
		                                                     parser->current_method);
		parser->current_method = NULL;
		g_free (parser->text_buffer);
		parser->text_buffer = NULL;
		parser->state = PARSER_PROVIDER;
	}
}

static void
mobile_parser_end_element (GMarkupParseContext *context,
                           const gchar *element_name,
                           gpointer data,
                           GError **error)
{
	MobileParser *parser = (MobileParser *) data;

	switch (parser->state) {
	case PARSER_COUNTRY:
		parser_country_end (parser, element_name);
		break;
	case PARSER_PROVIDER:
		parser_provider_end (parser, element_name);
		break;
	case PARSER_METHOD_GSM:
		parser_gsm_end (parser, element_name);
		break;
	case PARSER_METHOD_GSM_APN:
		parser_gsm_apn_end (parser, element_name);
		break;
	case PARSER_METHOD_CDMA:
		parser_cdma_end (parser, element_name);
		break;
	default:
		break;
	}
}

static void
mobile_parser_characters (GMarkupParseContext *context,
                          const gchar *text,
                          gsize text_len,
                          gpointer data,
                          GError **error)
{
	MobileParser *parser = (MobileParser *) data;

	g_free (parser->text_buffer);
	parser->text_buffer = g_strdup (text);
}

static const GMarkupParser mobile_parser = {
	mobile_parser_start_element,
	mobile_parser_end_element,
	mobile_parser_characters,
	NULL, /* passthrough */
	NULL /* error */
};

static gboolean
read_service_providers (GHashTable *countries,
                        const gchar *service_providers,
                        GCancellable *cancellable,
                        GError **error)
{
	GMarkupParseContext *ctx;
	GIOChannel *channel;
	MobileParser parser;
	char buffer[4096];
	GIOStatus status;
	gsize len = 0;

	memset (&parser, 0, sizeof (MobileParser));
    parser.table = countries;

	channel = g_io_channel_new_file (service_providers, "r", error);
	if (!channel) {
		g_prefix_error (error,
		                "Could not read '%s': ",
		                service_providers);
		return FALSE;
	}

	parser.state = PARSER_TOPLEVEL;

	ctx = g_markup_parse_context_new (&mobile_parser, 0, &parser, NULL);

	status = G_IO_STATUS_NORMAL;
	while (status == G_IO_STATUS_NORMAL) {
		status = g_io_channel_read_chars (channel, buffer, sizeof (buffer), &len, error);

		switch (status) {
		case G_IO_STATUS_NORMAL:
			if (!g_markup_parse_context_parse (ctx, buffer, len, error)) {
				status = G_IO_STATUS_ERROR;
				g_prefix_error (error,
				                "Error while parsing XML at '%s': ",
				                service_providers);
			}
			break;
		case G_IO_STATUS_EOF:
			break;
		case G_IO_STATUS_ERROR:
			g_prefix_error (error,
			                "Error while reading '%s': ",
			                service_providers);
			break;
		case G_IO_STATUS_AGAIN:
			/* FIXME: Try again a few times, but really, it never happens, right? */
			break;
		}

		if (g_cancellable_set_error_if_cancelled (cancellable, error))
			status = G_IO_STATUS_ERROR;
	}

	g_io_channel_unref (channel);
	g_markup_parse_context_free (ctx);

	if (parser.current_provider) {
		g_warning ("pending current provider");
		nma_mobile_provider_unref (parser.current_provider);
	}

	if (parser.current_providers) {
		g_warning ("pending current providers");
		provider_list_free (parser.current_providers);
	}

	g_free (parser.current_country);
	g_free (parser.text_buffer);

	return (status == G_IO_STATUS_EOF);
}

static GHashTable *
mobile_providers_parse_sync (const gchar *country_codes,
                             const gchar *service_providers,
                             GCancellable *cancellable,
                             GError **error)
{
	GHashTable *countries;

	/* Use default paths if none given */
	if (!country_codes)
		country_codes = ISO_3166_COUNTRY_CODES;
	if (!service_providers)
		service_providers = MOBILE_BROADBAND_PROVIDER_INFO;

	countries = read_country_codes (country_codes,
	                                cancellable,
	                                error);
	if (!countries)
		return NULL;

	if (!read_service_providers (countries,
	                             service_providers,
	                             cancellable,
	                             error)) {
		g_hash_table_unref (countries);
		return NULL;
	}

	return countries;
}

/******************************************************************************/
/* Dump to stdout contents */

static void
dump_generic (NMAMobileAccessMethod *method)
{
	g_print ("		  username: %s\n", method->username ? method->username : "");
	g_print ("		  password: %s\n", method->password ? method->password : "");

	if (method->dns) {
		guint i;
		const gchar **dns;
		GString *dns_str;

		dns = nma_mobile_access_method_get_dns (method);
		dns_str = g_string_new (NULL);
		for (i = 0; dns[i]; i++)
			g_string_append_printf (dns_str, "%s%s", i == 0 ? "" : ", ", dns[i]);
		g_print ("		  dns	  : %s\n", dns_str->str);
		g_string_free (dns_str, TRUE);
	}

	g_print ("		  gateway : %s\n", method->gateway ? method->gateway : "");
}

static void
dump_cdma (NMAMobileAccessMethod *method)
{
	g_print ("	   CDMA: %s\n", method->name);

	dump_generic (method);
}

static void
dump_3gpp (NMAMobileAccessMethod *method)
{
	g_print ("	   APN: %s (%s)\n", method->name, method->apn);

	dump_generic (method);
}

static void
dump_country (gpointer key, gpointer value, gpointer user_data)
{
	GSList *miter, *citer;
	NMACountryInfo *country_info = value;

	g_print ("Country: %s (%s)\n",
	         country_info->country_code,
	         country_info->country_name);

	for (citer = country_info->providers; citer; citer = g_slist_next (citer)) {
		NMAMobileProvider *provider = citer->data;
		const gchar **mcc_mnc;
		const guint *sid;
		guint n;

		g_print ("	  Provider: %s (%s)\n", provider->name, (const char *) key);

		mcc_mnc = nma_mobile_provider_get_3gpp_mcc_mnc (provider);
		if (mcc_mnc) {
			for (n = 0; mcc_mnc[n]; n++)
				g_print ("		  MCC/MNC: %s\n", mcc_mnc[n]);
		}

		sid = nma_mobile_provider_get_cdma_sid (provider);
		if (sid) {
			for (n = 0; sid[n]; n++)
				g_print ("		  SID: %u\n", sid[n]);
		}

		for (miter = provider->methods; miter; miter = g_slist_next (miter)) {
			NMAMobileAccessMethod *method = miter->data;

			switch (method->family) {
			case NMA_MOBILE_FAMILY_CDMA:
				dump_cdma (method);
				break;
			case NMA_MOBILE_FAMILY_3GPP:
				dump_3gpp (method);
				break;
			default:
				break;
			}
			g_print ("\n");
		}
	}
}

/******************************************************************************/
/* Mobile providers database type */

static void initable_iface_init       (GInitableIface      *iface);
static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (NMAMobileProvidersDatabase, nma_mobile_providers_database, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
	PROP_0,
	PROP_COUNTRY_CODES_PATH,
	PROP_SERVICE_PROVIDERS_PATH,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _NMAMobileProvidersDatabasePrivate {
	/* Paths to input files */
	gchar *country_codes_path;
	gchar *service_providers_path;

	/* The HT with country code as key and NMACountryInfo as value. */
	GHashTable *countries;
};

/**********************************/

/**
 * nma_mobile_providers_database_get_countries:
 * @self: a #NMAMobileProvidersDatabase.
 *
 * Returns: (element-type utf8 NMGtk.CountryInfo) (transfer none): a
 *	 hash table where keys are country names #gchar and values are #NMACountryInfos.
 */
GHashTable *
nma_mobile_providers_database_get_countries (NMAMobileProvidersDatabase *self)
{
	g_return_val_if_fail (NMA_IS_MOBILE_PROVIDERS_DATABASE (self), NULL);

	/* Warn if the object hasn't been initialized */
	g_return_val_if_fail (self->priv->countries != NULL, NULL);

	return self->priv->countries;
}

/**
 * nma_mobile_providers_database_dump:
 * @self: a #NMAMobileProvidersDatabase.
 *
 */
void
nma_mobile_providers_database_dump (NMAMobileProvidersDatabase *self)
{
	g_return_if_fail (NMA_IS_MOBILE_PROVIDERS_DATABASE (self));

	/* Warn if the object hasn't been initialized */
	g_return_if_fail (self->priv->countries != NULL);

	g_hash_table_foreach (self->priv->countries, dump_country, NULL);
}

/**
 * nma_mobile_providers_database_lookup_country:
 * @self: a #NMAMobileProvidersDatabase.
 * @country_code: the country code string to look for.
 *
 * Returns: (transfer none): a #NMACountryInfo or %NULL if not found.
 */
NMACountryInfo *
nma_mobile_providers_database_lookup_country (NMAMobileProvidersDatabase *self,
                                              const gchar *country_code)
{
	g_return_val_if_fail (NMA_IS_MOBILE_PROVIDERS_DATABASE (self), NULL);

	/* Warn if the object hasn't been initialized */
	g_return_val_if_fail (self->priv->countries != NULL, NULL);

	return (NMACountryInfo *) g_hash_table_lookup (self->priv->countries, country_code);
}

/**
 * nma_mobile_providers_database_lookup_3gpp_mcc_mnc:
 * @self: a #NMAMobileProvidersDatabase.
 * @mccmnc: the MCC/MNC string to look for.
 *
 * Returns: (transfer none): a #NMAMobileProvider or %NULL if not found.
 */
NMAMobileProvider *
nma_mobile_providers_database_lookup_3gpp_mcc_mnc (NMAMobileProvidersDatabase *self,
                                                   const gchar *mccmnc)
{
	GHashTableIter iter;
	gpointer value;
	GSList *piter;
	NMAMobileProvider *provider_match_2mnc = NULL;
	guint mccmnc_len;

	g_return_val_if_fail (NMA_IS_MOBILE_PROVIDERS_DATABASE (self), NULL);
	g_return_val_if_fail (mccmnc != NULL, NULL);
	/* Warn if the object hasn't been initialized */
	g_return_val_if_fail (self->priv->countries != NULL, NULL);

	/* Expect only 5 or 6 digit MCCMNC strings */
	mccmnc_len = strlen (mccmnc);
	if (mccmnc_len != 5 && mccmnc_len != 6)
		return NULL;

	g_hash_table_iter_init (&iter, self->priv->countries);
	/* Search through each country */
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		NMACountryInfo *country_info = value;

		/* Search through each country's providers */
		for (piter = nma_country_info_get_providers (country_info);
		     piter;
		     piter = g_slist_next (piter)) {
			NMAMobileProvider *provider = piter->data;
			const gchar **mccmnc_list;
			guint i;

			/* Search through MCC/MNC list */
			mccmnc_list = nma_mobile_provider_get_3gpp_mcc_mnc (provider);
			if (!mccmnc_list)
				continue;

			for (i = 0; mccmnc_list[i]; i++) {
				const gchar *mccmnc_iter;
				guint mccmnc_iter_len;

				mccmnc_iter = mccmnc_list[i];
				mccmnc_iter_len = strlen (mccmnc_iter);

				/* Match both 2-digit and 3-digit MNC; prefer a
				 * 3-digit match if found, otherwise a 2-digit one.
				 */

				if (strncmp (mccmnc_iter, mccmnc, 3))
					/* MCC was wrong */
					continue;

				/* Now we have the following match cases, examples given:
				 *  a) input: 123/456 --> iter: 123/456 (3-digit match)
				 *  b) input: 123/45  --> iter: 123/045 (3-digit match)
				 *  c) input: 123/045 --> iter: 123/45  (2-digit match)
				 *  d) input: 123/45  --> iter: 123/45  (2-digit match)
				 */

				if (mccmnc_iter_len == 6) {
					/* Covers cases a) and b) */
					if (   (mccmnc_len == 6 && !strncmp (mccmnc + 3, mccmnc_iter + 3, 3))
					    || (mccmnc_len == 5 && mccmnc_iter[3] == '0' && !strncmp (mccmnc + 3, mccmnc_iter + 4, 2)))
						/* 3-digit MNC match! */
						return provider;

					/* MNC was wrong */
					continue;
				}

				if (   !provider_match_2mnc
				    && mccmnc_iter_len == 5) {
					if (   (mccmnc_len == 5 && !strncmp (mccmnc + 3, mccmnc_iter + 3, 2))
					    || (mccmnc_len == 6 && mccmnc[3] == '0' && !strncmp (mccmnc + 4, mccmnc_iter + 3, 2))) {
						/* Store the 2-digit MNC match, but keep looking,
						 * we may have a 3-digit MNC match */
						provider_match_2mnc = provider;
						continue;
					}

					/* MNC was wrong */
					continue;
				}
			}
		}
	}

	return provider_match_2mnc;
}

/**
 * nma_mobile_providers_database_lookup_cdma_sid:
 * @self: a #NMAMobileProvidersDatabase.
 * @sid: the SID to look for.
 *
 * Returns: (transfer none): a #NMAMobileProvider, or %NULL if not found.
 */
NMAMobileProvider *
nma_mobile_providers_database_lookup_cdma_sid (NMAMobileProvidersDatabase *self,
                                               guint32 sid)
{
	GHashTableIter iter;
	gpointer value;
	GSList *piter;

	g_return_val_if_fail (NMA_IS_MOBILE_PROVIDERS_DATABASE (self), NULL);
	g_return_val_if_fail (sid > 0, NULL);
	/* Warn if the object hasn't been initialized */
	g_return_val_if_fail (self->priv->countries != NULL, NULL);

	g_hash_table_iter_init (&iter, self->priv->countries);
	/* Search through each country */
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		NMACountryInfo *country_info = value;

		/* Search through each country's providers */
		for (piter = nma_country_info_get_providers (country_info);
		     piter;
		     piter = g_slist_next (piter)) {
			NMAMobileProvider *provider = piter->data;
			const guint32 *sid_list;
			guint i;

			/* Search through CDMA SID list */
			sid_list = nma_mobile_provider_get_cdma_sid (provider);
			if (!sid_list)
				continue;

			for (i = 0; sid_list[i]; i++) {
				if (sid == sid_list[i])
					return provider;
			}
		}
	}

	return NULL;
}

/**********************************/

static gboolean
initable_init_sync (GInitable     *initable,
                    GCancellable  *cancellable,
                    GError       **error)
{
	NMAMobileProvidersDatabase *self = NMA_MOBILE_PROVIDERS_DATABASE (initable);

	/* Parse the files */
	self->priv->countries = mobile_providers_parse_sync (self->priv->country_codes_path,
	                                                     self->priv->service_providers_path,
	                                                     cancellable,
	                                                     error);
	if (!self->priv->countries)
		return FALSE;

	/* All good */
	return TRUE;
}

/**********************************/

/**
 * nma_mobile_providers_database_new:
 * @country_codes: (allow-none): Path to the country codes file.
 * @service_providers: (allow-none): Path to the service providers file.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: User data to pass to @callback.
 *
 */
void
nma_mobile_providers_database_new (const gchar *country_codes,
                                   const gchar *service_providers,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	g_async_initable_new_async (NMA_TYPE_MOBILE_PROVIDERS_DATABASE,
	                            G_PRIORITY_DEFAULT,
	                            cancellable,
	                            callback,
	                            user_data,
	                            "country-codes",     country_codes,
	                            "service-providers", service_providers,
	                            NULL);
}

/**
 * nma_mobile_providers_database_new_finish:
 * @res: The #GAsyncResult obtained from the #GAsyncReadyCallback passed to nma_mobile_providers_database_new().
 * @error: Return location for error or %NULL.
 *
 * Returns: (transfer full) (type NMAMobileProvidersDatabase): The constructed object or %NULL if @error is set.
 */
NMAMobileProvidersDatabase *
nma_mobile_providers_database_new_finish (GAsyncResult *res,
                                          GError **error)
{
	GObject *initable;
	GObject *out;

	initable = g_async_result_get_source_object (res);
	out = g_async_initable_new_finish (G_ASYNC_INITABLE (initable), res, error);
	g_object_unref (initable);

	return out ? NMA_MOBILE_PROVIDERS_DATABASE (out) : NULL;
}

/**
 * nma_mobile_providers_database_new_sync:
 * @country_codes: (allow-none): Path to the country codes file.
 * @service_providers: (allow-none): Path to the service providers file.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Returns: (transfer full) (type NMAMobileProvidersDatabase): The constructed object or %NULL if @error is set.
 */
NMAMobileProvidersDatabase *
nma_mobile_providers_database_new_sync (const gchar *country_codes,
                                        const gchar *service_providers,
                                        GCancellable *cancellable,
                                        GError **error)
{
	GObject *out;

	out = g_initable_new (NMA_TYPE_MOBILE_PROVIDERS_DATABASE,
	                      cancellable,
	                      error,
	                      "country-codes",     country_codes,
	                      "service-providers", service_providers,
	                      NULL);

	return out ? NMA_MOBILE_PROVIDERS_DATABASE (out) : NULL;
}

/**********************************/

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
	NMAMobileProvidersDatabase *self = NMA_MOBILE_PROVIDERS_DATABASE (object);

	switch (prop_id) {
	case PROP_COUNTRY_CODES_PATH:
		self->priv->country_codes_path = g_value_dup_string (value);
		break;
	case PROP_SERVICE_PROVIDERS_PATH:
		self->priv->service_providers_path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
	NMAMobileProvidersDatabase *self = NMA_MOBILE_PROVIDERS_DATABASE (object);

	switch (prop_id) {
	case PROP_COUNTRY_CODES_PATH:
		g_value_set_string (value, self->priv->country_codes_path);
		break;
	case PROP_SERVICE_PROVIDERS_PATH:
		g_value_set_string (value, self->priv->service_providers_path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nma_mobile_providers_database_init (NMAMobileProvidersDatabase *self)
{
	/* Setup private data */
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
	                                          NMA_TYPE_MOBILE_PROVIDERS_DATABASE,
	                                          NMAMobileProvidersDatabasePrivate);
}

static void
finalize (GObject *object)
{
	NMAMobileProvidersDatabase *self = NMA_MOBILE_PROVIDERS_DATABASE (object);

	g_free (self->priv->country_codes_path);
	g_free (self->priv->service_providers_path);

	if (self->priv->countries)
		g_hash_table_unref (self->priv->countries);

	G_OBJECT_CLASS (nma_mobile_providers_database_parent_class)->finalize (object);
}

static void
initable_iface_init (GInitableIface *iface)
{
	iface->init = initable_init_sync;
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
	/* Just use defaults (run sync init() in a thread) */
}

static void
nma_mobile_providers_database_class_init (NMAMobileProvidersDatabaseClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (NMAMobileProvidersDatabasePrivate));

    /* Virtual methods */
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->finalize = finalize;

    properties[PROP_COUNTRY_CODES_PATH] =
	    g_param_spec_string ("country-codes",
                             "Country Codes",
                             "Path to the country codes file",
	                         NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_property (object_class, PROP_COUNTRY_CODES_PATH, properties[PROP_COUNTRY_CODES_PATH]);

    properties[PROP_SERVICE_PROVIDERS_PATH] =
	    g_param_spec_string ("service-providers",
	                         "Service Providers",
	                         "Path to the service providers file",
	                         NULL,
	                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_property (object_class, PROP_SERVICE_PROVIDERS_PATH, properties[PROP_SERVICE_PROVIDERS_PATH]);
}

/******************************************************************************/
/* Utils */

/**
 * nma_mobile_providers_split_3gpp_mcc_mnc:
 * @mccmnc: input MCCMNC string.
 * @mcc: (out) (transfer full): the MCC.
 * @mnc: (out) (transfer full): the MNC.
 *
 * Splits the input MCCMNC string into separate MCC and MNC strings.
 *
 * Returns: %TRUE if correctly split and @mcc and @mnc are set; %FALSE otherwise.
 */
gboolean
nma_mobile_providers_split_3gpp_mcc_mnc (const gchar *mccmnc,
                                         gchar **mcc,
                                         gchar **mnc)
{
	gint len;

	g_return_val_if_fail (mccmnc != NULL, FALSE);
	g_return_val_if_fail (mcc != NULL, FALSE);
	g_return_val_if_fail (mnc != NULL, FALSE);

	len = strlen (mccmnc);
	if (len != 5 && len != 6)
		return FALSE;

	/* MCCMNC is all digits */
	while (len > 0) {
		if (!g_ascii_isdigit (mccmnc[--len]))
			return FALSE;
	}

	*mcc = g_strndup (mccmnc, 3);
	*mnc = g_strdup (mccmnc + 3);
	return TRUE;
}
