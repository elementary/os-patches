/*
 * Copyright 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#include "gsettingsstrv.h"

/**
 * g_settings_strv_append_unique:
 * @settings: a #GSettings object
 * @key: the key at which @settings contains a string array
 * @item: the string to append
 *
 * Appends @item to the string array at @key if that string array doesn't
 * contain @item yet.
 *
 * Returns: TRUE if @item was added to the list, FALSE if it already existed.
 */
gboolean
g_settings_strv_append_unique (GSettings   *settings,
                               const gchar *key,
                               const gchar *item)
{
  gchar **strv;
  gchar **it;
  gboolean add = TRUE;

  g_return_val_if_fail (G_IS_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (item != NULL, FALSE);

  strv = g_settings_get_strv (settings, key);

  for (it = strv; *it; it++)
    {
      if (g_str_equal (*it, item))
        {
          add = FALSE;
          break;
        }
    }

  if (add)
    {
      GVariantBuilder builder;

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

      for (it = strv; *it; it++)
        g_variant_builder_add (&builder, "s", *it);
      g_variant_builder_add (&builder, "s", item);

      g_settings_set_value (settings, key, g_variant_builder_end (&builder));
    }

  g_strfreev (strv);
  return add;
}

/**
 * g_settings_strv_remove:
 * @settings: a #GSettings object
 * @key: the key at which @settings contains a string array
 * @item: the string to remove
 *
 * Removes all occurences of @item in @key.
 */
void
g_settings_strv_remove (GSettings   *settings,
                        const gchar *key,
                        const gchar *item)
{
  gchar **strv;
  gchar **it;
  GVariantBuilder builder;

  g_return_if_fail (G_IS_SETTINGS (settings));
  g_return_if_fail (key != NULL);
  g_return_if_fail (item != NULL);

  strv = g_settings_get_strv (settings, key);

  g_variant_builder_init (&builder, (GVariantType *)"as");
  for (it = strv; *it; it++)
    {
      if (!g_str_equal (*it, item))
        g_variant_builder_add (&builder, "s", *it);
    }
  g_settings_set_value (settings, key, g_variant_builder_end (&builder));

  g_strfreev (strv);
}
