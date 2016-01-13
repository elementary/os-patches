/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
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
 * (C) Copyright 2005 - 2010 Red Hat, Inc.
 */

#ifndef GCONF_HELPERS_H
#define GCONF_HELPERS_H

#include <gconf/gconf-client.h>
#include <glib.h>
#include <nm-connection.h>

#include "utils.h"

#define GCONF_PATH_CONNECTIONS "/system/networking/connections"

#define KEYRING_UUID_TAG "connection-uuid"
#define KEYRING_SN_TAG "setting-name"
#define KEYRING_SK_TAG "setting-key"

gboolean
nm_gconf_get_int_helper (GConfClient *client,
					const char *path,
					const char *key,
					const char *setting,
					int *value);

gboolean
nm_gconf_get_float_helper (GConfClient *client,
					const char *path,
					const char *key,
					const char *setting,
					gfloat *value);

gboolean
nm_gconf_get_string_helper (GConfClient *client,
					const char *path,
					const char *key,
					const char *setting,
					char **value);

gboolean
nm_gconf_get_bool_helper (GConfClient *client,
					const char *path,
					const char *key,
					const char *setting,
					gboolean *value);

gboolean
nm_gconf_get_stringlist_helper (GConfClient *client,
				const char *path,
				const char *key,
				const char *setting,
				GSList **value);

gboolean
nm_gconf_get_stringarray_helper (GConfClient *client,
                                 const char *path,
                                 const char *key,
                                 const char *setting,
                                 GPtrArray **value);

gboolean
nm_gconf_get_bytearray_helper (GConfClient *client,
			       const char *path,
			       const char *key,
			       const char *setting,
			       GByteArray **value);

gboolean
nm_gconf_get_uint_array_helper (GConfClient *client,
				const char *path,
				const char *key,
				const char *setting,
				GArray **value);

gboolean
nm_gconf_get_stringhash_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GHashTable **value);

gboolean
nm_gconf_get_ip4_helper (GConfClient *client,
						  const char *path,
						  const char *key,
						  const char *setting,
						  guint32 tuple_len,
						  GPtrArray **value);

gboolean
nm_gconf_get_ip6dns_array_helper (GConfClient *client,
								  const char *path,
								  const char *key,
								  const char *setting,
								  GPtrArray **value);

gboolean
nm_gconf_get_ip6addr_array_helper (GConfClient *client,
								   const char *path,
								   const char *key,
								   const char *setting,
								   GPtrArray **value);

gboolean
nm_gconf_get_ip6route_array_helper (GConfClient *client,
									const char *path,
									const char *key,
									const char *setting,
									GPtrArray **value);

/* Setters */

gboolean
nm_gconf_set_int_helper (GConfClient *client,
                         const char *path,
                         const char *key,
                         const char *setting,
                         int value);

gboolean
nm_gconf_set_float_helper (GConfClient *client,
                           const char *path,
                           const char *key,
                           const char *setting,
                           gfloat value);

gboolean
nm_gconf_set_string_helper (GConfClient *client,
                            const char *path,
                            const char *key,
                            const char *setting,
                            const char *value);

gboolean
nm_gconf_set_bool_helper (GConfClient *client,
                          const char *path,
                          const char *key,
                          const char *setting,
                          gboolean value);

gboolean
nm_gconf_set_stringlist_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GSList *value);

gboolean
nm_gconf_set_stringarray_helper (GConfClient *client,
                                 const char *path,
                                 const char *key,
                                 const char *setting,
                                 GPtrArray *value);

gboolean
nm_gconf_set_bytearray_helper (GConfClient *client,
                               const char *path,
                               const char *key,
                               const char *setting,
                               GByteArray *value);

gboolean
nm_gconf_set_uint_array_helper (GConfClient *client,
				const char *path,
				const char *key,
				const char *setting,
				GArray *value);

gboolean
nm_gconf_set_stringhash_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GHashTable *value);

gboolean
nm_gconf_set_ip4_helper (GConfClient *client,
					  const char *path,
					  const char *key,
					  const char *setting,
					  guint32 tuple_len,
					  GPtrArray *value);

gboolean
nm_gconf_set_ip6dns_array_helper (GConfClient *client,
								  const char *path,
								  const char *key,
								  const char *setting,
								  GPtrArray *value);

gboolean
nm_gconf_set_ip6addr_array_helper (GConfClient *client,
								   const char *path,
								   const char *key,
								   const char *setting,
								   GPtrArray *value);

gboolean
nm_gconf_set_ip6route_array_helper (GConfClient *client,
									const char *path,
									const char *key,
									const char *setting,
									GPtrArray *value);

gboolean
nm_gconf_key_is_set (GConfClient *client,
                     const char *path,
                     const char *key,
                     const char *setting);

NMConnection *
nm_gconf_read_connection (GConfClient *client,
                          const char *dir);

void
nm_gconf_write_connection (NMConnection *connection,
                           GConfClient *client,
                           const char *dir,
                           gboolean ignore_secrets);

void
nm_gconf_add_keyring_item (const char *connection_uuid,
                           const char *connection_name,
                           const char *setting_name,
                           const char *setting_key,
                           const char *secret);

typedef void (*AddToSettingsFunc) (NMConnection *connection, gpointer user_data);

void nm_gconf_move_connections_to_system (AddToSettingsFunc add_func,
                                          gpointer user_data);

#endif	/* GCONF_HELPERS_H */

