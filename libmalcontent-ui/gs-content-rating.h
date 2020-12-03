/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015-2016 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

G_BEGIN_DECLS

#include <appstream-glib.h>
#include <glib-object.h>
#include <libmalcontent/malcontent.h>

#if AS_CHECK_VERSION(0, 7, 18)
#define GS_CONTENT_RATING_SYSTEM_UNKNOWN AS_CONTENT_RATING_SYSTEM_UNKNOWN
#define GS_CONTENT_RATING_SYSTEM_INCAA AS_CONTENT_RATING_SYSTEM_INCAA
#define GS_CONTENT_RATING_SYSTEM_ACB AS_CONTENT_RATING_SYSTEM_ACB
#define GS_CONTENT_RATING_SYSTEM_DJCTQ AS_CONTENT_RATING_SYSTEM_DJCTQ
#define GS_CONTENT_RATING_SYSTEM_GSRR AS_CONTENT_RATING_SYSTEM_GSRR
#define GS_CONTENT_RATING_SYSTEM_PEGI AS_CONTENT_RATING_SYSTEM_PEGI
#define GS_CONTENT_RATING_SYSTEM_KAVI AS_CONTENT_RATING_SYSTEM_KAVI
#define GS_CONTENT_RATING_SYSTEM_USK AS_CONTENT_RATING_SYSTEM_USK
#define GS_CONTENT_RATING_SYSTEM_ESRA AS_CONTENT_RATING_SYSTEM_ESRA
#define GS_CONTENT_RATING_SYSTEM_CERO AS_CONTENT_RATING_SYSTEM_CERO
#define GS_CONTENT_RATING_SYSTEM_OFLCNZ AS_CONTENT_RATING_SYSTEM_OFLCNZ
#define GS_CONTENT_RATING_SYSTEM_RUSSIA AS_CONTENT_RATING_SYSTEM_RUSSIA
#define GS_CONTENT_RATING_SYSTEM_MDA AS_CONTENT_RATING_SYSTEM_MDA
#define GS_CONTENT_RATING_SYSTEM_GRAC AS_CONTENT_RATING_SYSTEM_GRAC
#define GS_CONTENT_RATING_SYSTEM_ESRB AS_CONTENT_RATING_SYSTEM_ESRB
#define GS_CONTENT_RATING_SYSTEM_IARC AS_CONTENT_RATING_SYSTEM_IARC
#define GS_CONTENT_RATING_SYSTEM_LAST AS_CONTENT_RATING_SYSTEM_LAST
#define GsContentRatingSystem AsContentRatingSystem

#define gs_utils_content_rating_age_to_str as_content_rating_system_format_age
#define gs_utils_content_rating_system_from_locale as_content_rating_system_from_locale
#define gs_content_rating_system_to_str as_content_rating_system_to_string
#define gs_utils_content_rating_get_values as_content_rating_system_get_formatted_ages
#define gs_utils_content_rating_get_ages as_content_rating_system_get_csm_ages
#define as_content_rating_id_csm_age_to_value as_content_rating_attribute_from_csm_age
#else
typedef enum {
	GS_CONTENT_RATING_SYSTEM_UNKNOWN,
	GS_CONTENT_RATING_SYSTEM_INCAA,
	GS_CONTENT_RATING_SYSTEM_ACB,
	GS_CONTENT_RATING_SYSTEM_DJCTQ,
	GS_CONTENT_RATING_SYSTEM_GSRR,
	GS_CONTENT_RATING_SYSTEM_PEGI,
	GS_CONTENT_RATING_SYSTEM_KAVI,
	GS_CONTENT_RATING_SYSTEM_USK,
	GS_CONTENT_RATING_SYSTEM_ESRA,
	GS_CONTENT_RATING_SYSTEM_CERO,
	GS_CONTENT_RATING_SYSTEM_OFLCNZ,
	GS_CONTENT_RATING_SYSTEM_RUSSIA,
	GS_CONTENT_RATING_SYSTEM_MDA,
	GS_CONTENT_RATING_SYSTEM_GRAC,
	GS_CONTENT_RATING_SYSTEM_ESRB,
	GS_CONTENT_RATING_SYSTEM_IARC,
	/*< private >*/
	GS_CONTENT_RATING_SYSTEM_LAST
} GsContentRatingSystem;

gchar *gs_utils_content_rating_age_to_str (GsContentRatingSystem system,
						 guint age);
GsContentRatingSystem gs_utils_content_rating_system_from_locale (const gchar *locale);
const gchar *gs_content_rating_system_to_str (GsContentRatingSystem system);
gchar **gs_utils_content_rating_get_values (GsContentRatingSystem system);
const guint *gs_utils_content_rating_get_ages (GsContentRatingSystem system, gsize *length_out);
MctAppFilterOarsValue as_content_rating_id_csm_age_to_value (const gchar *id, guint age);
#endif  /* appstream-glib < 0.7.18 */

#if AS_CHECK_VERSION(0, 7, 15)
#define as_content_rating_id_value_to_csm_age as_content_rating_attribute_to_csm_age
#else
guint as_content_rating_id_value_to_csm_age (const gchar *id, MctAppFilterOarsValue value);
#endif  /* appstream-glib < 0.7.15 */

G_END_DECLS
