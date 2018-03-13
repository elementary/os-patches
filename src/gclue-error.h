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

#ifndef GCLUE_ERROR_H
#define GCLUE_ERROR_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * GCLUE_ERROR:
 *
 * Error domain for gclue-glib. Errors from this domain will be from
 * the #GClueError enumeration.
 * See #GError for more information on error domains.
 **/
#define GCLUE_ERROR (gclue_error_quark ())

/**
 * GClueError:
 * @GCLUE_ERROR_PARSE: An error occured parsing the response from the web service.
 * @GCLUE_ERROR_NOT_SUPPORTED: The request made was not supported.
 * @GCLUE_ERROR_NO_MATCHES: The requests made didn't have any matches.
 * @GCLUE_ERROR_INVALID_ARGUMENTS: The request made contained invalid arguments.
 * @GCLUE_ERROR_INTERNAL_SERVER: The server encountered an (possibly unrecoverable) internal error.
 *
 * Error codes returned by gclue-glib functions.
 **/
typedef enum {
	GCLUE_ERROR_PARSE,
	GCLUE_ERROR_NOT_SUPPORTED,
	GCLUE_ERROR_NO_MATCHES,
	GCLUE_ERROR_INVALID_ARGUMENTS,
	GCLUE_ERROR_INTERNAL_SERVER
} GClueError;

GQuark gclue_error_quark (void);

G_END_DECLS

#endif /* GCLUE_ERROR_H */
