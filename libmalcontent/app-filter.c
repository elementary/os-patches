/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2018-2019 Endless Mobile, Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 *  - Andre Moreira Magalhaes <andre@endlessm.com>
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <libmalcontent/app-filter.h>

#include "libmalcontent/app-filter-private.h"


G_DEFINE_QUARK (MctAppFilterError, mct_app_filter_error)

/* struct _MctAppFilter is defined in app-filter-private.h */

G_DEFINE_BOXED_TYPE (MctAppFilter, mct_app_filter,
                     mct_app_filter_ref, mct_app_filter_unref)

/**
 * mct_app_filter_ref:
 * @filter: (transfer none): an #MctAppFilter
 *
 * Increment the reference count of @filter, and return the same pointer to it.
 *
 * Returns: (transfer full): the same pointer as @filter
 * Since: 0.2.0
 */
MctAppFilter *
mct_app_filter_ref (MctAppFilter *filter)
{
  g_return_val_if_fail (filter != NULL, NULL);
  g_return_val_if_fail (filter->ref_count >= 1, NULL);
  g_return_val_if_fail (filter->ref_count <= G_MAXINT - 1, NULL);

  filter->ref_count++;
  return filter;
}

/**
 * mct_app_filter_unref:
 * @filter: (transfer full): an #MctAppFilter
 *
 * Decrement the reference count of @filter. If the reference count reaches
 * zero, free the @filter and all its resources.
 *
 * Since: 0.2.0
 */
void
mct_app_filter_unref (MctAppFilter *filter)
{
  g_return_if_fail (filter != NULL);
  g_return_if_fail (filter->ref_count >= 1);

  filter->ref_count--;

  if (filter->ref_count <= 0)
    {
      g_strfreev (filter->app_list);
      g_variant_unref (filter->oars_ratings);
      g_free (filter);
    }
}

/**
 * mct_app_filter_get_user_id:
 * @filter: an #MctAppFilter
 *
 * Get the user ID of the user this #MctAppFilter is for.
 *
 * Returns: user ID of the relevant user
 * Since: 0.2.0
 */
uid_t
mct_app_filter_get_user_id (MctAppFilter *filter)
{
  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (filter->ref_count >= 1, FALSE);

  return filter->user_id;
}

/**
 * mct_app_filter_is_path_allowed:
 * @filter: an #MctAppFilter
 * @path: (type filename): absolute path of a program to check
 *
 * Check whether the program at @path is allowed to be run according to this
 * app filter. @path will be canonicalised without doing any I/O.
 *
 * Returns: %TRUE if the user this @filter corresponds to is allowed to run the
 *    program at @path according to the @filter policy; %FALSE otherwise
 * Since: 0.2.0
 */
gboolean
mct_app_filter_is_path_allowed (MctAppFilter *filter,
                                const gchar  *path)
{
  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (filter->ref_count >= 1, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (path), FALSE);

  g_autofree gchar *canonical_path = g_canonicalize_filename (path, "/");
  g_autofree gchar *canonical_path_utf8 = g_filename_to_utf8 (canonical_path, -1,
                                                              NULL, NULL, NULL);
  g_return_val_if_fail (canonical_path_utf8 != NULL, FALSE);

  gboolean path_in_list = g_strv_contains ((const gchar * const *) filter->app_list,
                                           canonical_path_utf8);

  switch (filter->app_list_type)
    {
    case MCT_APP_FILTER_LIST_BLACKLIST:
      return !path_in_list;
    case MCT_APP_FILTER_LIST_WHITELIST:
      return path_in_list;
    default:
      g_assert_not_reached ();
    }
}

/* Check whether a given @ref is a valid flatpak ref.
 *
 * For simplicity and to avoid duplicating the whole logic behind
 * flatpak_ref_parse() this method will only check whether:
 * - the @ref contains exactly 3 slash chars
 * - the @ref starts with either app/ or runtime/
 * - the name, arch and branch components of the @ref are not empty
 *
 * We avoid using flatpak_ref_parse() to allow for libflatpak
 * to depend on malcontent without causing a cyclic dependency.
 */
static gboolean
is_valid_flatpak_ref (const gchar *ref)
{
  g_auto(GStrv) parts = NULL;

  if (ref == NULL)
    return FALSE;

  parts = g_strsplit (ref, "/", 0);
  return (g_strv_length (parts) == 4 &&
          (strcmp (parts[0], "app") == 0 ||
           strcmp (parts[0], "runtime") == 0) &&
          *parts[1] != '\0' &&
          *parts[2] != '\0' &&
          *parts[3] != '\0');
}

/**
 * mct_app_filter_is_flatpak_ref_allowed:
 * @filter: an #MctAppFilter
 * @app_ref: flatpak ref for the app, for example `app/org.gnome.Builder/x86_64/master`
 *
 * Check whether the flatpak app with the given @app_ref is allowed to be run
 * according to this app filter.
 *
 * Returns: %TRUE if the user this @filter corresponds to is allowed to run the
 *    flatpak called @app_ref according to the @filter policy; %FALSE otherwise
 * Since: 0.2.0
 */
gboolean
mct_app_filter_is_flatpak_ref_allowed (MctAppFilter *filter,
                                       const gchar  *app_ref)
{
  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (filter->ref_count >= 1, FALSE);
  g_return_val_if_fail (app_ref != NULL, FALSE);
  g_return_val_if_fail (is_valid_flatpak_ref (app_ref), FALSE);

  gboolean ref_in_list = g_strv_contains ((const gchar * const *) filter->app_list,
                                          app_ref);

  switch (filter->app_list_type)
    {
    case MCT_APP_FILTER_LIST_BLACKLIST:
      return !ref_in_list;
    case MCT_APP_FILTER_LIST_WHITELIST:
      return ref_in_list;
    default:
      g_assert_not_reached ();
    }
}

/**
 * mct_app_filter_is_flatpak_app_allowed:
 * @filter: an #MctAppFilter
 * @app_id: flatpak ID for the app, for example `org.gnome.Builder`
 *
 * Check whether the flatpak app with the given @app_id is allowed to be run
 * according to this app filter. This is a globbing match, matching @app_id
 * against potentially multiple entries in the blacklist, as the blacklist
 * contains flatpak refs (for example, `app/org.gnome.Builder/x86_64/master`)
 * which contain architecture and branch information. App IDs (for example,
 * `org.gnome.Builder`) do not contain architecture or branch information.
 *
 * Returns: %TRUE if the user this @filter corresponds to is allowed to run the
 *    flatpak called @app_id according to the @filter policy; %FALSE otherwise
 * Since: 0.2.0
 */
gboolean
mct_app_filter_is_flatpak_app_allowed (MctAppFilter *filter,
                                       const gchar  *app_id)
{
  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (filter->ref_count >= 1, FALSE);
  g_return_val_if_fail (app_id != NULL, FALSE);

  gsize app_id_len = strlen (app_id);

  gboolean id_in_list = FALSE;
  for (gsize i = 0; filter->app_list[i] != NULL; i++)
    {
      if (is_valid_flatpak_ref (filter->app_list[i]) &&
          g_str_has_prefix (filter->app_list[i], "app/") &&
          strncmp (filter->app_list[i] + strlen ("app/"), app_id, app_id_len) == 0 &&
          filter->app_list[i][strlen ("app/") + app_id_len] == '/')
        {
          id_in_list = TRUE;
          break;
        }
    }

  switch (filter->app_list_type)
    {
    case MCT_APP_FILTER_LIST_BLACKLIST:
      return !id_in_list;
    case MCT_APP_FILTER_LIST_WHITELIST:
      return id_in_list;
    default:
      g_assert_not_reached ();
    }
}

/**
 * mct_app_filter_is_appinfo_allowed:
 * @filter: an #MctAppFilter
 * @app_info: (transfer none): application information
 *
 * Check whether the app with the given @app_info is allowed to be run
 * according to this app filter. This matches on multiple keys potentially
 * present in the #GAppInfo, including the path of the executable.
 *
 * Returns: %TRUE if the user this @filter corresponds to is allowed to run the
 *    app represented by @app_info according to the @filter policy; %FALSE
 *    otherwise
 * Since: 0.2.0
 */
gboolean
mct_app_filter_is_appinfo_allowed (MctAppFilter *filter,
                                   GAppInfo     *app_info)
{
  g_autofree gchar *abs_path = NULL;
  const gchar * const *types = NULL;

  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (filter->ref_count >= 1, FALSE);
  g_return_val_if_fail (G_IS_APP_INFO (app_info), FALSE);

  abs_path = g_find_program_in_path (g_app_info_get_executable (app_info));

  if (abs_path != NULL &&
      !mct_app_filter_is_path_allowed (filter, abs_path))
    return FALSE;

  types = g_app_info_get_supported_types (app_info);
  for (gsize i = 0; types != NULL && types[i] != NULL; i++)
    {
      if (!mct_app_filter_is_content_type_allowed (filter, types[i]))
        return FALSE;
    }

  if (G_IS_DESKTOP_APP_INFO (app_info))
    {
      g_autofree gchar *flatpak_app = NULL;
      g_autofree gchar *old_flatpak_apps_str = NULL;

      /* This gives `org.gnome.Builder`. */
      flatpak_app = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (app_info), "X-Flatpak");
      if (flatpak_app != NULL)
        flatpak_app = g_strstrip (flatpak_app);

      if (flatpak_app != NULL &&
          !mct_app_filter_is_flatpak_app_allowed (filter, flatpak_app))
        return FALSE;

      /* FIXME: This could do with the g_desktop_app_info_get_string_list() API
       * from GLib 2.60. Gives `gimp.desktop;org.gimp.Gimp.desktop;`. */
      old_flatpak_apps_str = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (app_info), "X-Flatpak-RenamedFrom");
      if (old_flatpak_apps_str != NULL)
        {
          g_auto(GStrv) old_flatpak_apps = g_strsplit (old_flatpak_apps_str, ";", -1);

          for (gsize i = 0; old_flatpak_apps[i] != NULL; i++)
            {
              gchar *old_flatpak_app = g_strstrip (old_flatpak_apps[i]);

              if (g_str_has_suffix (old_flatpak_app, ".desktop"))
                old_flatpak_app[strlen (old_flatpak_app) - strlen (".desktop")] = '\0';
              old_flatpak_app = g_strstrip (old_flatpak_app);

              if (*old_flatpak_app != '\0' &&
                  !mct_app_filter_is_flatpak_app_allowed (filter, old_flatpak_app))
                return FALSE;
            }
        }
    }

  return TRUE;
}

/* Check whether a given @content_type is valid.
 *
 * For simplicity this method will only check whether:
 * - the @content_type contains exactly 1 slash char
 * - the @content_type does not start with a slash char
 * - the type and subtype components of the @content_type are not empty
 */
static gboolean
is_valid_content_type (const gchar *content_type)
{
  g_auto(GStrv) parts = NULL;

  if (content_type == NULL)
    return FALSE;

  parts = g_strsplit (content_type, "/", 0);
  return (g_strv_length (parts) == 2 &&
          *parts[0] != '\0' &&
          *parts[1] != '\0');
}

/**
 * mct_app_filter_is_content_type_allowed:
 * @filter: an #MctAppFilter
 * @content_type: content type to check
 *
 * Check whether apps handling the given @content_type are allowed to be run
 * according to this app filter.
 *
 * Note that this method doesn’t match content subtypes. For example, if
 * `application/xml` is added to the blacklist but `application/xspf+xml` is not,
 * a check for whether `application/xspf+xml` is blacklisted would return false.
 *
 * Returns: %TRUE if the user this @filter corresponds to is allowed to run
 *    programs handling @content_type according to the @filter policy;
 *    %FALSE otherwise
 * Since: 0.4.0
 */
gboolean
mct_app_filter_is_content_type_allowed (MctAppFilter *filter,
                                        const gchar  *content_type)
{
  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (filter->ref_count >= 1, FALSE);
  g_return_val_if_fail (content_type != NULL, FALSE);
  g_return_val_if_fail (is_valid_content_type (content_type), FALSE);

  gboolean ref_in_list = g_strv_contains ((const gchar * const *) filter->app_list,
                                          content_type);

  switch (filter->app_list_type)
    {
    case MCT_APP_FILTER_LIST_BLACKLIST:
      return !ref_in_list;
    case MCT_APP_FILTER_LIST_WHITELIST:
      return ref_in_list;
    default:
      g_assert_not_reached ();
    }
}

static gint
strcmp_cb (gconstpointer a,
           gconstpointer b)
{
  const gchar *str_a = *((const gchar * const *) a);
  const gchar *str_b = *((const gchar * const *) b);

  return g_strcmp0 (str_a, str_b);
}

/**
 * mct_app_filter_get_oars_sections:
 * @filter: an #MctAppFilter
 *
 * List the OARS sections present in this app filter. The sections are returned
 * in lexicographic order. A section will be listed even if its stored value is
 * %MCT_APP_FILTER_OARS_VALUE_UNKNOWN. The returned list may be empty.
 *
 * Returns: (transfer container) (array zero-terminated=1): %NULL-terminated
 *    array of OARS sections
 * Since: 0.2.0
 */
const gchar **
mct_app_filter_get_oars_sections (MctAppFilter *filter)
{
  g_autoptr(GPtrArray) sections = g_ptr_array_new_with_free_func (NULL);
  GVariantIter iter;
  const gchar *oars_section;

  g_return_val_if_fail (filter != NULL, NULL);
  g_return_val_if_fail (filter->ref_count >= 1, NULL);

  g_variant_iter_init (&iter, filter->oars_ratings);

  while (g_variant_iter_loop (&iter, "{&s&s}", &oars_section, NULL))
    g_ptr_array_add (sections, (gpointer) oars_section);

  /* Sort alphabetically for easier comparisons later. */
  g_ptr_array_sort (sections, strcmp_cb);

  g_ptr_array_add (sections, NULL);  /* NULL terminator */

  return (const gchar **) g_ptr_array_free (g_steal_pointer (&sections), FALSE);
}

/**
 * mct_app_filter_get_oars_value:
 * @filter: an #MctAppFilter
 * @oars_section: name of the OARS section to get the value from
 *
 * Get the value assigned to the given @oars_section in the OARS filter stored
 * within @filter. If that section has no value explicitly defined,
 * %MCT_APP_FILTER_OARS_VALUE_UNKNOWN is returned.
 *
 * This value is the most intense value allowed for apps to have in this
 * section, inclusive. Any app with a more intense value for this section must
 * be hidden from the user whose @filter this is.
 *
 * This does not factor in mct_app_filter_is_system_installation_allowed().
 *
 * Returns: an #MctAppFilterOarsValue
 * Since: 0.2.0
 */
MctAppFilterOarsValue
mct_app_filter_get_oars_value (MctAppFilter *filter,
                               const gchar  *oars_section)
{
  const gchar *value_str;

  g_return_val_if_fail (filter != NULL, MCT_APP_FILTER_OARS_VALUE_UNKNOWN);
  g_return_val_if_fail (filter->ref_count >= 1,
                        MCT_APP_FILTER_OARS_VALUE_UNKNOWN);
  g_return_val_if_fail (oars_section != NULL && *oars_section != '\0',
                        MCT_APP_FILTER_OARS_VALUE_UNKNOWN);

  if (!g_variant_lookup (filter->oars_ratings, oars_section, "&s", &value_str))
    return MCT_APP_FILTER_OARS_VALUE_UNKNOWN;

  if (g_str_equal (value_str, "none"))
    return MCT_APP_FILTER_OARS_VALUE_NONE;
  else if (g_str_equal (value_str, "mild"))
    return MCT_APP_FILTER_OARS_VALUE_MILD;
  else if (g_str_equal (value_str, "moderate"))
    return MCT_APP_FILTER_OARS_VALUE_MODERATE;
  else if (g_str_equal (value_str, "intense"))
    return MCT_APP_FILTER_OARS_VALUE_INTENSE;
  else
    return MCT_APP_FILTER_OARS_VALUE_UNKNOWN;
}

/**
 * mct_app_filter_is_user_installation_allowed:
 * @filter: an #MctAppFilter
 *
 * Get whether the user is allowed to install to their flatpak user repository.
 * This should be queried in addition to the OARS values
 * (mct_app_filter_get_oars_value()) — if it returns %FALSE, the OARS values
 * should be ignored and app installation should be unconditionally disallowed.
 *
 * Returns: %TRUE if app installation is allowed to the user repository for
 *    this user; %FALSE if it is unconditionally disallowed for this user
 * Since: 0.2.0
 */
gboolean
mct_app_filter_is_user_installation_allowed (MctAppFilter *filter)
{
  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (filter->ref_count >= 1, FALSE);

  return filter->allow_user_installation;
}

/**
 * mct_app_filter_is_system_installation_allowed:
 * @filter: an #MctAppFilter
 *
 * Get whether the user is allowed to install to the flatpak system repository.
 * This should be queried in addition to the OARS values
 * (mct_app_filter_get_oars_value()) — if it returns %FALSE, the OARS values
 * should be ignored and app installation should be unconditionally disallowed.
 *
 * Returns: %TRUE if app installation is allowed to the system repository for
 *    this user; %FALSE if it is unconditionally disallowed for this user
 * Since: 0.2.0
 */
gboolean
mct_app_filter_is_system_installation_allowed (MctAppFilter *filter)
{
  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (filter->ref_count >= 1, FALSE);

  return filter->allow_system_installation;
}

/*
 * Actual implementation of #MctAppFilterBuilder.
 *
 * All members are %NULL if un-initialised, cleared, or ended.
 */
typedef struct
{
  GPtrArray *blacklist;  /* (nullable) (owned) (element-type utf8) */
  GHashTable *oars;  /* (nullable) (owned) (element-type utf8 MctAppFilterOarsValue) */
  gboolean allow_user_installation;
  gboolean allow_system_installation;

  /*< private >*/
  gpointer padding[2];
} MctAppFilterBuilderReal;

G_STATIC_ASSERT (sizeof (MctAppFilterBuilderReal) ==
                 sizeof (MctAppFilterBuilder));
G_STATIC_ASSERT (__alignof__ (MctAppFilterBuilderReal) ==
                 __alignof__ (MctAppFilterBuilder));

G_DEFINE_BOXED_TYPE (MctAppFilterBuilder, mct_app_filter_builder,
                     mct_app_filter_builder_copy, mct_app_filter_builder_free)

/**
 * mct_app_filter_builder_init:
 * @builder: an uninitialised #MctAppFilterBuilder
 *
 * Initialise the given @builder so it can be used to construct a new
 * #MctAppFilter. @builder must have been allocated on the stack, and must not
 * already be initialised.
 *
 * Construct the #MctAppFilter by calling methods on @builder, followed by
 * mct_app_filter_builder_end(). To abort construction, use
 * mct_app_filter_builder_clear().
 *
 * Since: 0.2.0
 */
void
mct_app_filter_builder_init (MctAppFilterBuilder *builder)
{
  MctAppFilterBuilder local_builder = MCT_APP_FILTER_BUILDER_INIT ();
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;

  g_return_if_fail (_builder != NULL);
  g_return_if_fail (_builder->blacklist == NULL);
  g_return_if_fail (_builder->oars == NULL);

  memcpy (builder, &local_builder, sizeof (local_builder));
}

/**
 * mct_app_filter_builder_clear:
 * @builder: an #MctAppFilterBuilder
 *
 * Clear @builder, freeing any internal state in it. This will not free the
 * top-level storage for @builder itself, which is assumed to be allocated on
 * the stack.
 *
 * If called on an already-cleared #MctAppFilterBuilder, this function is
 * idempotent.
 *
 * Since: 0.2.0
 */
void
mct_app_filter_builder_clear (MctAppFilterBuilder *builder)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;

  g_return_if_fail (_builder != NULL);

  g_clear_pointer (&_builder->blacklist, g_ptr_array_unref);
  g_clear_pointer (&_builder->oars, g_hash_table_unref);
}

/**
 * mct_app_filter_builder_new:
 *
 * Construct a new #MctAppFilterBuilder on the heap. This is intended for
 * language bindings. The returned builder must eventually be freed with
 * mct_app_filter_builder_free(), but can be cleared zero or more times with
 * mct_app_filter_builder_clear() first.
 *
 * Returns: (transfer full): a new heap-allocated #MctAppFilterBuilder
 * Since: 0.2.0
 */
MctAppFilterBuilder *
mct_app_filter_builder_new (void)
{
  g_autoptr(MctAppFilterBuilder) builder = NULL;

  builder = g_new0 (MctAppFilterBuilder, 1);
  mct_app_filter_builder_init (builder);

  return g_steal_pointer (&builder);
}

/**
 * mct_app_filter_builder_copy:
 * @builder: an #MctAppFilterBuilder
 *
 * Copy the given @builder to a newly-allocated #MctAppFilterBuilder on the
 * heap. This is safe to use with cleared, stack-allocated
 * #MctAppFilterBuilders.
 *
 * Returns: (transfer full): a copy of @builder
 * Since: 0.2.0
 */
MctAppFilterBuilder *
mct_app_filter_builder_copy (MctAppFilterBuilder *builder)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;
  g_autoptr(MctAppFilterBuilder) copy = NULL;
  MctAppFilterBuilderReal *_copy;

  g_return_val_if_fail (builder != NULL, NULL);

  copy = mct_app_filter_builder_new ();
  _copy = (MctAppFilterBuilderReal *) copy;

  mct_app_filter_builder_clear (copy);
  if (_builder->blacklist != NULL)
    _copy->blacklist = g_ptr_array_ref (_builder->blacklist);
  if (_builder->oars != NULL)
    _copy->oars = g_hash_table_ref (_builder->oars);
  _copy->allow_user_installation = _builder->allow_user_installation;
  _copy->allow_system_installation = _builder->allow_system_installation;

  return g_steal_pointer (&copy);
}

/**
 * mct_app_filter_builder_free:
 * @builder: a heap-allocated #MctAppFilterBuilder
 *
 * Free an #MctAppFilterBuilder originally allocated using
 * mct_app_filter_builder_new(). This must not be called on stack-allocated
 * builders initialised using mct_app_filter_builder_init().
 *
 * Since: 0.2.0
 */
void
mct_app_filter_builder_free (MctAppFilterBuilder *builder)
{
  g_return_if_fail (builder != NULL);

  mct_app_filter_builder_clear (builder);
  g_free (builder);
}

/**
 * mct_app_filter_builder_end:
 * @builder: an initialised #MctAppFilterBuilder
 *
 * Finish constructing an #MctAppFilter with the given @builder, and return it.
 * The #MctAppFilterBuilder will be cleared as if mct_app_filter_builder_clear()
 * had been called.
 *
 * Returns: (transfer full): a newly constructed #MctAppFilter
 * Since: 0.2.0
 */
MctAppFilter *
mct_app_filter_builder_end (MctAppFilterBuilder *builder)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;
  g_autoptr(MctAppFilter) app_filter = NULL;
  g_auto(GVariantBuilder) oars_builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a{ss}"));
  GHashTableIter iter;
  gpointer key, value;
  g_autoptr(GVariant) oars_variant = NULL;

  g_return_val_if_fail (_builder != NULL, NULL);
  g_return_val_if_fail (_builder->blacklist != NULL, NULL);
  g_return_val_if_fail (_builder->oars != NULL, NULL);

  /* Ensure the paths list is %NULL-terminated. */
  g_ptr_array_add (_builder->blacklist, NULL);

  /* Build the OARS variant. */
  g_hash_table_iter_init (&iter, _builder->oars);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *oars_section = key;
      MctAppFilterOarsValue oars_value = GPOINTER_TO_INT (value);
      const gchar *oars_value_strs[] =
        {
          NULL,  /* MCT_APP_FILTER_OARS_VALUE_UNKNOWN */
          "none",
          "mild",
          "moderate",
          "intense",
        };

      g_assert ((int) oars_value >= 0 &&
                (int) oars_value < (int) G_N_ELEMENTS (oars_value_strs));

      if (oars_value_strs[oars_value] != NULL)
        g_variant_builder_add (&oars_builder, "{ss}",
                               oars_section, oars_value_strs[oars_value]);
    }

  oars_variant = g_variant_ref_sink (g_variant_builder_end (&oars_builder));

  /* Build the #MctAppFilter. */
  app_filter = g_new0 (MctAppFilter, 1);
  app_filter->ref_count = 1;
  app_filter->user_id = -1;
  app_filter->app_list = (gchar **) g_ptr_array_free (g_steal_pointer (&_builder->blacklist), FALSE);
  app_filter->app_list_type = MCT_APP_FILTER_LIST_BLACKLIST;
  app_filter->oars_ratings = g_steal_pointer (&oars_variant);
  app_filter->allow_user_installation = _builder->allow_user_installation;
  app_filter->allow_system_installation = _builder->allow_system_installation;

  mct_app_filter_builder_clear (builder);

  return g_steal_pointer (&app_filter);
}

/**
 * mct_app_filter_builder_blacklist_path:
 * @builder: an initialised #MctAppFilterBuilder
 * @path: (type filename): an absolute path to blacklist
 *
 * Add @path to the blacklist of app paths in the filter under construction. It
 * will be canonicalised (without doing any I/O) before being added.
 * The canonicalised @path will not be added again if it’s already been added.
 *
 * Since: 0.2.0
 */
void
mct_app_filter_builder_blacklist_path (MctAppFilterBuilder *builder,
                                       const gchar         *path)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;

  g_return_if_fail (_builder != NULL);
  g_return_if_fail (_builder->blacklist != NULL);
  g_return_if_fail (path != NULL);
  g_return_if_fail (g_path_is_absolute (path));

  g_autofree gchar *canonical_path = g_canonicalize_filename (path, "/");
  g_autofree gchar *canonical_path_utf8 = g_filename_to_utf8 (canonical_path, -1,
                                                              NULL, NULL, NULL);
  g_return_if_fail (canonical_path_utf8 != NULL);

  if (!g_ptr_array_find_with_equal_func (_builder->blacklist,
                                         canonical_path_utf8, g_str_equal, NULL))
    g_ptr_array_add (_builder->blacklist, g_steal_pointer (&canonical_path_utf8));
}

/**
 * mct_app_filter_builder_blacklist_flatpak_ref:
 * @builder: an initialised #MctAppFilterBuilder
 * @app_ref: a flatpak app ref to blacklist
 *
 * Add @app_ref to the blacklist of flatpak refs in the filter under
 * construction. The @app_ref will not be added again if it’s already been
 * added.
 *
 * Since: 0.2.0
 */
void
mct_app_filter_builder_blacklist_flatpak_ref (MctAppFilterBuilder *builder,
                                              const gchar         *app_ref)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;

  g_return_if_fail (_builder != NULL);
  g_return_if_fail (_builder->blacklist != NULL);
  g_return_if_fail (app_ref != NULL);
  g_return_if_fail (is_valid_flatpak_ref (app_ref));

  if (!g_ptr_array_find_with_equal_func (_builder->blacklist,
                                         app_ref, g_str_equal, NULL))
    g_ptr_array_add (_builder->blacklist, g_strdup (app_ref));
}

/**
 * mct_app_filter_builder_blacklist_content_type:
 * @builder: an initialised #MctAppFilterBuilder
 * @content_type: a content type to blacklist
 *
 * Add @content_type to the blacklist of content types in the filter under
 * construction. The @content_type will not be added again if it’s already been
 * added.
 *
 * Note that this method doesn’t handle content subtypes. For example, if
 * `application/xml` is added to the blacklist but `application/xspf+xml` is not,
 * a check for whether `application/xspf+xml` is blacklisted would return false.
 *
 * Since: 0.4.0
 */
void
mct_app_filter_builder_blacklist_content_type (MctAppFilterBuilder *builder,
                                               const gchar         *content_type)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;

  g_return_if_fail (_builder != NULL);
  g_return_if_fail (_builder->blacklist != NULL);
  g_return_if_fail (content_type != NULL);
  g_return_if_fail (is_valid_content_type (content_type));

  if (!g_ptr_array_find_with_equal_func (_builder->blacklist,
                                         content_type, g_str_equal, NULL))
    g_ptr_array_add (_builder->blacklist, g_strdup (content_type));
}

/**
 * mct_app_filter_builder_set_oars_value:
 * @builder: an initialised #MctAppFilterBuilder
 * @oars_section: name of the OARS section to set the value for
 * @value: value to set for the @oars_section
 *
 * Set the OARS value for the given @oars_section, indicating the intensity of
 * content covered by that section which the user is allowed to see (inclusive).
 * Any apps which have more intense content in this section should not be usable
 * by the user.
 *
 * Since: 0.2.0
 */
void
mct_app_filter_builder_set_oars_value (MctAppFilterBuilder   *builder,
                                       const gchar           *oars_section,
                                       MctAppFilterOarsValue  value)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;

  g_return_if_fail (_builder != NULL);
  g_return_if_fail (_builder->oars != NULL);
  g_return_if_fail (oars_section != NULL && *oars_section != '\0');

  g_hash_table_insert (_builder->oars, g_strdup (oars_section),
                       GUINT_TO_POINTER (value));
}

/**
 * mct_app_filter_builder_set_allow_user_installation:
 * @builder: an initialised #MctAppFilterBuilder
 * @allow_user_installation: %TRUE to allow app installation; %FALSE to
 *    unconditionally disallow it
 *
 * Set whether the user is allowed to install to their flatpak user repository.
 * If this is %TRUE, app installation is still subject to the OARS values
 * (mct_app_filter_builder_set_oars_value()). If it is %FALSE, app installation
 * is unconditionally disallowed for this user.
 *
 * Since: 0.2.0
 */
void
mct_app_filter_builder_set_allow_user_installation (MctAppFilterBuilder *builder,
                                                    gboolean             allow_user_installation)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;

  g_return_if_fail (_builder != NULL);

  _builder->allow_user_installation = allow_user_installation;
}

/**
 * mct_app_filter_builder_set_allow_system_installation:
 * @builder: an initialised #MctAppFilterBuilder
 * @allow_system_installation: %TRUE to allow app installation; %FALSE to
 *    unconditionally disallow it
 *
 * Set whether the user is allowed to install to the flatpak system repository.
 * If this is %TRUE, app installation is still subject to the OARS values
 * (mct_app_filter_builder_set_oars_value()). If it is %FALSE, app installation
 * is unconditionally disallowed for this user.
 *
 * Since: 0.2.0
 */
void
mct_app_filter_builder_set_allow_system_installation (MctAppFilterBuilder *builder,
                                                      gboolean             allow_system_installation)
{
  MctAppFilterBuilderReal *_builder = (MctAppFilterBuilderReal *) builder;

  g_return_if_fail (_builder != NULL);

  _builder->allow_system_installation = allow_system_installation;
}
