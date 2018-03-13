/* vim: set et ts=8 sw=8: */
/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#ifndef GCLUE_MOZILLA_H
#define GCLUE_MOZILLA_H

#include <glib.h>
#include <libsoup/soup.h>
#include "wpa_supplicant-interface.h"
#include "gclue-location.h"
#include "gclue-3g-tower.h"

G_BEGIN_DECLS

SoupMessage *
gclue_mozilla_create_query (GList        *bss_list, /* As in Access Points */
                            GClue3GTower *tower,
                            GError      **error);
GClueLocation *
gclue_mozilla_parse_response (const char *json,
                              GError    **error);
SoupMessage *
gclue_mozilla_create_submit_query (GClueLocation   *location,
                                   GList           *bss_list, /* As in Access Points */
                                   GClue3GTower    *tower,
                                   GError         **error);
gboolean
gclue_mozilla_should_ignore_bss (WPABSS *bss);

G_END_DECLS

#endif /* GCLUE_MOZILLA_H */
