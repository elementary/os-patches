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

#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * MctAppFilterOarsValue:
 * @MCT_APP_FILTER_OARS_VALUE_UNKNOWN: Unknown value for the given
 *    section.
 * @MCT_APP_FILTER_OARS_VALUE_NONE: No rating for the given section.
 * @MCT_APP_FILTER_OARS_VALUE_MILD: Mild rating for the given section.
 * @MCT_APP_FILTER_OARS_VALUE_MODERATE: Moderate rating for the given
 *    section.
 * @MCT_APP_FILTER_OARS_VALUE_INTENSE: Intense rating for the given
 *    section.
 *
 * Rating values of the intensity of a given section in an app or game.
 * These are directly equivalent to the values in the #AsContentRatingValue
 * enumeration in libappstream.
 *
 * Since: 0.2.0
 */
typedef enum
{
  MCT_APP_FILTER_OARS_VALUE_UNKNOWN,
  MCT_APP_FILTER_OARS_VALUE_NONE,
  MCT_APP_FILTER_OARS_VALUE_MILD,
  MCT_APP_FILTER_OARS_VALUE_MODERATE,
  MCT_APP_FILTER_OARS_VALUE_INTENSE,
} MctAppFilterOarsValue;

/**
 * MctAppFilter:
 *
 * #MctAppFilter is an opaque, immutable structure which contains a snapshot of
 * the app filtering settings for a user at a given time. This includes a list
 * of apps which are explicitly banned or allowed to be run by that user.
 *
 * Typically, app filter settings can only be changed by the administrator, and
 * are read-only for non-administrative users. The precise policy is set using
 * polkit.
 *
 * Since: 0.2.0
 */
typedef struct _MctAppFilter MctAppFilter;
GType mct_app_filter_get_type (void);
#define MCT_TYPE_APP_FILTER mct_app_filter_get_type ()

MctAppFilter *mct_app_filter_ref   (MctAppFilter *filter);
void          mct_app_filter_unref (MctAppFilter *filter);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MctAppFilter, mct_app_filter_unref)

uid_t    mct_app_filter_get_user_id            (MctAppFilter *filter);

gboolean mct_app_filter_is_enabled             (MctAppFilter *filter);

gboolean mct_app_filter_is_path_allowed        (MctAppFilter *filter,
                                                const gchar  *path);
gboolean mct_app_filter_is_flatpak_ref_allowed (MctAppFilter *filter,
                                                const gchar  *app_ref);
gboolean mct_app_filter_is_flatpak_app_allowed (MctAppFilter *filter,
                                                const gchar  *app_id);
gboolean mct_app_filter_is_appinfo_allowed     (MctAppFilter *filter,
                                                GAppInfo     *app_info);
gboolean mct_app_filter_is_content_type_allowed (MctAppFilter *filter,
                                                 const gchar  *content_type);

const gchar           **mct_app_filter_get_oars_sections (MctAppFilter *filter);
MctAppFilterOarsValue   mct_app_filter_get_oars_value    (MctAppFilter *filter,
                                                          const gchar  *oars_section);

gboolean                mct_app_filter_is_user_installation_allowed   (MctAppFilter *filter);
gboolean                mct_app_filter_is_system_installation_allowed (MctAppFilter *filter);

GVariant     *mct_app_filter_serialize   (MctAppFilter  *filter);
MctAppFilter *mct_app_filter_deserialize (GVariant      *variant,
                                          uid_t          user_id,
                                          GError       **error);

gboolean mct_app_filter_equal (MctAppFilter *a,
                               MctAppFilter *b);

/**
 * MctAppFilterBuilder:
 *
 * #MctAppFilterBuilder is a stack-allocated mutable structure used to build an
 * #MctAppFilter instance. Use mct_app_filter_builder_init(), various method
 * calls to set properties of the app filter, and then
 * mct_app_filter_builder_end(), to construct an #MctAppFilter.
 *
 * Since: 0.2.0
 */
typedef struct
{
  /*< private >*/
  gpointer p0;
  gpointer p1;
  gboolean b0;
  gboolean b1;
  gpointer p2;
  gpointer p3;
} MctAppFilterBuilder;

GType mct_app_filter_builder_get_type (void);

/**
 * MCT_APP_FILTER_BUILDER_INIT:
 *
 * Initialise a stack-allocated #MctAppFilterBuilder instance at declaration
 * time.
 *
 * This is typically used with g_auto():
 * |[
 * g_auto(MctAppFilterBuilder) builder = MCT_APP_FILTER_BUILDER_INIT ();
 * ]|
 *
 * Since: 0.2.0
 */
#define MCT_APP_FILTER_BUILDER_INIT() \
  { \
    g_ptr_array_new_with_free_func (g_free), \
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL), \
    TRUE, \
    FALSE, \
    /* padding: */ \
    NULL, \
    NULL \
  }

void mct_app_filter_builder_init  (MctAppFilterBuilder *builder);
void mct_app_filter_builder_clear (MctAppFilterBuilder *builder);

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (MctAppFilterBuilder,
                                  mct_app_filter_builder_clear)

MctAppFilterBuilder *mct_app_filter_builder_new  (void);
MctAppFilterBuilder *mct_app_filter_builder_copy (MctAppFilterBuilder *builder);
void                 mct_app_filter_builder_free (MctAppFilterBuilder *builder);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MctAppFilterBuilder, mct_app_filter_builder_free)

MctAppFilter *mct_app_filter_builder_end (MctAppFilterBuilder *builder);

void mct_app_filter_builder_blocklist_path        (MctAppFilterBuilder   *builder,
                                                   const gchar           *path);
void mct_app_filter_builder_blocklist_flatpak_ref (MctAppFilterBuilder *builder,
                                                   const gchar         *app_ref);
void mct_app_filter_builder_blocklist_content_type (MctAppFilterBuilder *builder,
                                                    const gchar         *content_type);

void mct_app_filter_builder_set_oars_value        (MctAppFilterBuilder   *builder,
                                                   const gchar           *oars_section,
                                                   MctAppFilterOarsValue  value);

void mct_app_filter_builder_set_allow_user_installation   (MctAppFilterBuilder *builder,
                                                           gboolean             allow_user_installation);
void mct_app_filter_builder_set_allow_system_installation (MctAppFilterBuilder *builder,
                                                           gboolean             allow_system_installation);

#include <libmalcontent/manager.h>

/* FIXME: Eventually deprecate these compatibility fallbacks. */
typedef MctManagerError MctAppFilterError;
#define MCT_APP_FILTER_ERROR_INVALID_USER MCT_MANAGER_ERROR_INVALID_USER
#define MCT_APP_FILTER_ERROR_PERMISSION_DENIED MCT_MANAGER_ERROR_PERMISSION_DENIED
#define MCT_APP_FILTER_ERROR_INVALID_DATA MCT_MANAGER_ERROR_INVALID_DATA
#define MCT_APP_FILTER_ERROR_DISABLED MCT_MANAGER_ERROR_DISABLED

GQuark mct_app_filter_error_quark (void);
#define MCT_APP_FILTER_ERROR mct_app_filter_error_quark ()

G_END_DECLS
