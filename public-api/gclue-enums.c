/* vim: set et ts=8 sw=8: */
/* gclue-enum.c
 *
 * Copyright 2018 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Zeeshan Ali <zeenix@collabora.com>
 */

#include "gclue-enum-types.h"

/**
 * gclue_accuracy_level_get_string:
 * @val: a GClueAccuracyLevel.
 *
 * Gets the nickname string for the #GClueAccuracyLevel specified at @val.
 *
 * Returns: (transfer none): a string with the nickname, or %NULL if not found. Do not free the returned value.
 */
const char *
gclue_accuracy_level_get_string (GClueAccuracyLevel val)
{
    static GEnumClass *enum_class = NULL;
    GEnumValue *value;

    if (enum_class == NULL) {
            enum_class = g_type_class_ref (GCLUE_TYPE_ACCURACY_LEVEL);
            g_return_val_if_fail (enum_class != NULL, NULL);
    }
    value = g_enum_get_value (enum_class, val);
    if (value == NULL)
            return NULL;

    /* Leaking enum_class ref on purpose here */

    return value->value_nick;
}
