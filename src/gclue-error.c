/* vim: set et ts=8 sw=8: */
/* gclue-error.c
 *
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * Geoclue is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Geoclue is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Geoclue; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Bastien Nocera <hadess@hadess.net>
 */

#include "gclue-error.h"

/**
 * SECTION:gclue-error
 * @short_description: Error helper functions
 * @include: gclue-glib/gclue-glib.h
 *
 * Contains helper functions for reporting errors to the user.
 **/

/**
 * gclue_error_quark:
 *
 * Gets the gclue-glib error quark.
 *
 * Return value: a #GQuark.
 **/
GQuark
gclue_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gclue_error");

	return quark;
}

