/* ply-key-file.h - key file loader
 *
 * Copyright (C) 2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written By: Ray Strode <rstrode@redhat.com>
 */
#ifndef PLY_KEY_FILE_H
#define PLY_KEY_FILE_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

typedef struct _ply_key_file ply_key_file_t;
typedef void (ply_key_file_foreach_func_t) (const char *group_name,
                                            const char *key,
                                            const char *value,
                                            void       *user_data);

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_key_file_t *ply_key_file_new (const char *filename);
void ply_key_file_free (ply_key_file_t *key_file);
bool ply_key_file_load (ply_key_file_t *key_file);
/* For loading key=value pair files, which do not have ini style groups.
 * When a file is loaded this way, NULL must be passed as group_name
 * for subsequent ply_key_file_get_* calls.
 */
bool ply_key_file_load_groupless_file (ply_key_file_t *key_file);
bool ply_key_file_has_key (ply_key_file_t *key_file,
                           const char     *group_name,
                           const char     *key);
char *ply_key_file_get_value (ply_key_file_t *key_file,
                              const char     *group_name,
                              const char     *key);
/* Note this returns false for non existing keys */
bool ply_key_file_get_bool (ply_key_file_t *key_file,
                            const char     *group_name,
                            const char     *key);
double ply_key_file_get_double (ply_key_file_t *key_file,
                                const char     *group_name,
                                const char     *key,
                                double          default_value);
double ply_key_file_get_long (ply_key_file_t *key_file,
                              const char     *group,
                              const char     *key,
                              long            default_value);
void ply_key_file_foreach_entry (ply_key_file_t             *key_file,
                                 ply_key_file_foreach_func_t func,
                                 void                       *user_data);
#endif

#endif /* PLY_KEY_FILE_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
