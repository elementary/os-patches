/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2012 Aleksander Morgado <aleksander@gnu.org>
 */

#include <locale.h>
#include <string.h>

#include "nm-mobile-providers.h"

#if defined TEST_DATA_DIR
#  define COUNTRY_CODES_FILE     TEST_DATA_DIR "/iso3166-test.xml"
#  define SERVICE_PROVIDERS_FILE TEST_DATA_DIR "/serviceproviders-test.xml"
#else
#  error You broke it!
#endif

/******************************************************************************/
/* Common test utilities */

static NMAMobileProvidersDatabase *
common_create_mpd_sync (void)
{
	NMAMobileProvidersDatabase *mpd;
	GError *error = NULL;

	mpd = nma_mobile_providers_database_new_sync (COUNTRY_CODES_FILE,
	                                              SERVICE_PROVIDERS_FILE,
	                                              NULL, /* cancellable */
	                                              &error);
	g_assert_no_error (error);
	g_assert (NMA_IS_MOBILE_PROVIDERS_DATABASE (mpd));

	return mpd;
}

typedef struct {
	NMAMobileProvidersDatabase *mpd;
	GMainLoop *loop;
} NewAsyncContext;

static void
new_ready (GObject *source,
           GAsyncResult *res,
           NewAsyncContext *ctx)
{
	GError *error = NULL;

	ctx->mpd = nma_mobile_providers_database_new_finish (res, &error);
	g_assert_no_error (error);
	g_assert (NMA_IS_MOBILE_PROVIDERS_DATABASE (ctx->mpd));

	g_main_loop_quit (ctx->loop);
}

static NMAMobileProvidersDatabase *
common_create_mpd_async (void)
{
	NewAsyncContext ctx;

	ctx.loop = g_main_loop_new (NULL, FALSE);
	ctx.mpd = NULL;

	nma_mobile_providers_database_new (COUNTRY_CODES_FILE,
	                                   SERVICE_PROVIDERS_FILE,
	                                   NULL, /* cancellable */
	                                   (GAsyncReadyCallback)new_ready,
	                                   &ctx);
	g_main_loop_run (ctx.loop);
	g_main_loop_unref (ctx.loop);

	return ctx.mpd;
}

/******************************************************************************/

static void
new_sync (void)
{
	NMAMobileProvidersDatabase *mpd;

	mpd = common_create_mpd_sync ();
	g_object_unref (mpd);
}

static void
new_async (void)
{
	NMAMobileProvidersDatabase *mpd;

	mpd = common_create_mpd_async ();
	g_object_unref (mpd);
}

/******************************************************************************/

static void
dump (void)
{
	NMAMobileProvidersDatabase *mpd;

	/* Sync */
	mpd = common_create_mpd_sync ();
	nma_mobile_providers_database_dump (mpd);
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	nma_mobile_providers_database_dump (mpd);
	g_object_unref (mpd);
}

/******************************************************************************/

static void
lookup_country (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMACountryInfo *country_info;

	/* Sync */
	mpd = common_create_mpd_sync ();
	country_info = nma_mobile_providers_database_lookup_country (mpd, "US");
	g_assert (country_info != NULL);
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	country_info = nma_mobile_providers_database_lookup_country (mpd, "US");
	g_assert (country_info != NULL);
	g_object_unref (mpd);
}

static void
lookup_unknown_country (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMACountryInfo *country_info;

	/* Sync */
	mpd = common_create_mpd_sync ();
	country_info = nma_mobile_providers_database_lookup_country (mpd, "KK");
	g_assert (country_info == NULL);
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	country_info = nma_mobile_providers_database_lookup_country (mpd, "KK");
	g_assert (country_info == NULL);
	g_object_unref (mpd);
}

/******************************************************************************/

static void
lookup_3gpp_mccmnc_1 (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMAMobileProvider *provider;

	/* Look for a 3 digit MNC using a 3 digit MNC */

	/* Sync */
	mpd = common_create_mpd_sync ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "310150");
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "AT&T");
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "310150");
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "AT&T");
	g_object_unref (mpd);
}

static void
lookup_3gpp_mccmnc_2 (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMAMobileProvider *provider;

	/* Look for a 3 digit MNC using a 2 digit MNC */

	/* Sync */
	mpd = common_create_mpd_sync ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "31038");
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "AT&T");
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "31038");
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "AT&T");
	g_object_unref (mpd);
}

static void
lookup_3gpp_mccmnc_3 (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMAMobileProvider *provider;

	/* Look for a 2 digit MNC using a 2 digit MNC */

	/* Sync */
	mpd = common_create_mpd_sync ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "21405");
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "Movistar (Telefónica)");
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "21405");
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "Movistar (Telefónica)");
	g_object_unref (mpd);
}

static void
lookup_3gpp_mccmnc_4 (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMAMobileProvider *provider;

	/* Look for a 2 digit MNC using a 3 digit MNC */

	/* Sync */
	mpd = common_create_mpd_sync ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "214005");
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "Movistar (Telefónica)");
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "214005");
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "Movistar (Telefónica)");
	g_object_unref (mpd);
}

static void
lookup_unknown_3gpp_mccmnc (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMAMobileProvider *provider;

	/* Sync */
	mpd = common_create_mpd_sync ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "12345");
	g_assert (provider == NULL);
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "12345");
	g_assert (provider == NULL);
	g_object_unref (mpd);
}

static void
ensure_movistar (NMAMobileProvider *provider)
{
	NMAMobileAccessMethod *method;

	/* Check Name */
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "Movistar (Telefónica)");

	/* Check MCCMNC values */
	{
		const gchar **mcc_mnc;
		guint i;
		gboolean found_21405 = FALSE;
		gboolean found_21407 = FALSE;

		mcc_mnc = nma_mobile_provider_get_3gpp_mcc_mnc (provider);
		g_assert (mcc_mnc != NULL);
		for (i = 0; mcc_mnc[i]; i++) {
			if (!strcmp (mcc_mnc[i], "21405")) {
				g_assert (!found_21405);
				found_21405 = TRUE;
			} else if (!strcmp (mcc_mnc[i], "21407")) {
				g_assert (!found_21407);
				found_21407 = TRUE;
			} else
				g_assert_not_reached ();
		}
		g_assert (found_21405);
		g_assert (found_21407);
	}

	/* Check SID */
	g_assert (nma_mobile_provider_get_cdma_sid (provider) == NULL);

	/* Check access methods */
	{
		GSList *methods;

		methods = nma_mobile_provider_get_methods (provider);
		g_assert_cmpuint (g_slist_length (methods), ==, 1);
		method = methods->data;
	}

	/* Check access method name, APN and type */
	g_assert_cmpstr (nma_mobile_access_method_get_name (method), ==, "Default");
	g_assert_cmpint (nma_mobile_access_method_get_family (method), ==, NMA_MOBILE_FAMILY_3GPP);
	g_assert_cmpstr (nma_mobile_access_method_get_3gpp_apn (method), ==, "movistar.es");

	/* Check DNS */
	{
		const gchar **dns;
		guint i;
		gboolean found_100 = FALSE;
		gboolean found_101 = FALSE;

		dns = nma_mobile_access_method_get_dns (method);
		g_assert (dns != NULL);
		for (i = 0; dns[i]; i++) {
			if (!strcmp (dns[i], "194.179.1.100")) {
				g_assert (!found_100);
				found_100 = TRUE;
			} else if (!strcmp (dns[i], "194.179.1.101")) {
				g_assert (!found_101);
				found_101 = TRUE;
			} else
				g_assert_not_reached ();
		}
		g_assert (found_100);
		g_assert (found_101);
	}

	/* Check Username and Password */
	g_assert_cmpstr (nma_mobile_access_method_get_username (method), ==, "movistar");
	g_assert_cmpstr (nma_mobile_access_method_get_password (method), ==, "movistar");
}

static void
ensure_provider_contents_3gpp (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMAMobileProvider *provider;

	/* Sync */
	mpd = common_create_mpd_sync ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "21405");
	g_assert (provider != NULL);
	ensure_movistar (provider);
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (mpd, "21405");
	g_assert (provider != NULL);
	ensure_movistar (provider);
	g_object_unref (mpd);
}

/******************************************************************************/

static void
lookup_cdma_sid (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMAMobileProvider *provider;

	/* Sync */
	mpd = common_create_mpd_sync ();
	provider = nma_mobile_providers_database_lookup_cdma_sid (mpd, 2);
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "Verizon");
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	provider = nma_mobile_providers_database_lookup_cdma_sid (mpd, 2);
	g_assert (provider != NULL);
	g_assert_cmpstr (nma_mobile_provider_get_name (provider), ==, "Verizon");
	g_object_unref (mpd);
}

static void
lookup_unknown_cdma_sid (void)
{
	NMAMobileProvidersDatabase *mpd;
	NMAMobileProvider *provider;

	/* Sync */
	mpd = common_create_mpd_sync ();
	provider = nma_mobile_providers_database_lookup_cdma_sid (mpd, 42);
	g_assert (provider == NULL);
	g_object_unref (mpd);

	/* Async */
	mpd = common_create_mpd_async ();
	provider = nma_mobile_providers_database_lookup_cdma_sid (mpd, 42);
	g_assert (provider == NULL);
	g_object_unref (mpd);
}

/******************************************************************************/

static void
split_mccmnc_1 (void)
{
	gchar *mcc = NULL;
	gchar *mnc = NULL;

	g_assert (nma_mobile_providers_split_3gpp_mcc_mnc ("123456", &mcc, &mnc));
	g_assert_cmpstr (mcc, == , "123");
	g_assert_cmpstr (mnc, == , "456");
	g_free (mcc);
	g_free (mnc);
}

static void
split_mccmnc_2 (void)
{
	gchar *mcc = NULL;
	gchar *mnc = NULL;

	g_assert (nma_mobile_providers_split_3gpp_mcc_mnc ("12345", &mcc, &mnc));
	g_assert_cmpstr (mcc, == , "123");
	g_assert_cmpstr (mnc, == , "45");
	g_free (mcc);
	g_free (mnc);
}

static void
split_mccmnc_error_1 (void)
{
	gchar *mcc = NULL;
	gchar *mnc = NULL;

	g_assert (nma_mobile_providers_split_3gpp_mcc_mnc ("1", &mcc, &mnc) == FALSE);
	g_assert (mcc == NULL);
	g_assert (mnc == NULL);
}

static void
split_mccmnc_error_2 (void)
{
	gchar *mcc = NULL;
	gchar *mnc = NULL;

	g_assert (nma_mobile_providers_split_3gpp_mcc_mnc ("123", &mcc, &mnc) == FALSE);
	g_assert (mcc == NULL);
	g_assert (mnc == NULL);
}

static void
split_mccmnc_error_3 (void)
{
	gchar *mcc = NULL;
	gchar *mnc = NULL;

	g_assert (nma_mobile_providers_split_3gpp_mcc_mnc ("1234567", &mcc, &mnc) == FALSE);
	g_assert (mcc == NULL);
	g_assert (mnc == NULL);
}

static void
split_mccmnc_error_4 (void)
{
	gchar *mcc = NULL;
	gchar *mnc = NULL;

	g_assert (nma_mobile_providers_split_3gpp_mcc_mnc ("123ab", &mcc, &mnc) == FALSE);
	g_assert (mcc == NULL);
	g_assert (mnc == NULL);
}

/******************************************************************************/

int main (int argc, char **argv)
{
	setlocale (LC_ALL, "");

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/MobileProvidersDatabase/new-sync",  new_sync);
	g_test_add_func ("/MobileProvidersDatabase/new-async", new_async);

	g_test_add_func ("/MobileProvidersDatabase/dump",  dump);

	g_test_add_func ("/MobileProvidersDatabase/lookup-country",         lookup_country);
	g_test_add_func ("/MobileProvidersDatabase/lookup-unknown-country", lookup_unknown_country);

	g_test_add_func ("/MobileProvidersDatabase/lookup-3gpp-mccmnc-1",          lookup_3gpp_mccmnc_1);
	g_test_add_func ("/MobileProvidersDatabase/lookup-3gpp-mccmnc-2",          lookup_3gpp_mccmnc_2);
	g_test_add_func ("/MobileProvidersDatabase/lookup-3gpp-mccmnc-3",          lookup_3gpp_mccmnc_3);
	g_test_add_func ("/MobileProvidersDatabase/lookup-3gpp-mccmnc-4",          lookup_3gpp_mccmnc_4);
	g_test_add_func ("/MobileProvidersDatabase/lookup-unknown-3gpp-mccmnc",    lookup_unknown_3gpp_mccmnc);
	g_test_add_func ("/MobileProvidersDatabase/ensure-providers-content-3gpp", ensure_provider_contents_3gpp);

	g_test_add_func ("/MobileProvidersDatabase/lookup-cdma-sid",         lookup_cdma_sid);
	g_test_add_func ("/MobileProvidersDatabase/lookup-unknown-cdma-sid", lookup_unknown_cdma_sid);

	g_test_add_func ("/MobileProvidersDatabase/split-mccmnc-1",       split_mccmnc_1);
	g_test_add_func ("/MobileProvidersDatabase/split-mccmnc-2",       split_mccmnc_2);
	g_test_add_func ("/MobileProvidersDatabase/split-mccmnc-error-1", split_mccmnc_error_1);
	g_test_add_func ("/MobileProvidersDatabase/split-mccmnc-error-2", split_mccmnc_error_2);
	g_test_add_func ("/MobileProvidersDatabase/split-mccmnc-error-3", split_mccmnc_error_3);
	g_test_add_func ("/MobileProvidersDatabase/split-mccmnc-error-4", split_mccmnc_error_4);

	return g_test_run ();
}
