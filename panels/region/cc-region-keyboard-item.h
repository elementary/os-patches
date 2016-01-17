/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __CC_REGION_KEYBOARD_ITEM_H
#define __CC_REGION_KEYBOARD_ITEM_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CC_TYPE_REGION_KEYBOARD_ITEM         (cc_region_keyboard_item_get_type ())
#define CC_REGION_KEYBOARD_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_REGION_KEYBOARD_ITEM, CcRegionKeyboardItem))
#define CC_REGION_KEYBOARD_ITEM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_REGION_KEYBOARD_ITEM, CcRegionKeyboardItemClass))
#define CC_IS_REGION_KEYBOARD_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_REGION_KEYBOARD_ITEM))
#define CC_IS_REGION_KEYBOARD_ITEM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_REGION_KEYBOARD_ITEM))
#define CC_REGION_KEYBOARD_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_REGION_KEYBOARD_ITEM, CcRegionKeyboardItemClass))

typedef enum
{
  BINDING_GROUP_SYSTEM,
  BINDING_GROUP_APPS,
  BINDING_GROUP_SEPARATOR,
  BINDING_GROUP_USER,
} BindingGroupType;

typedef enum {
	CC_REGION_KEYBOARD_ITEM_TYPE_NONE = 0,
	CC_REGION_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH,
	CC_REGION_KEYBOARD_ITEM_TYPE_GSETTINGS
} CcRegionKeyboardItemType;

typedef struct CcRegionKeyboardItemPrivate CcRegionKeyboardItemPrivate;

typedef struct
{
  GObject                parent;
  CcRegionKeyboardItemPrivate *priv;

  /* Move to priv */
  CcRegionKeyboardItemType type;

  /* common */
  /* FIXME move to priv? */
  guint keyval;
  guint keycode;
  GdkModifierType mask;
  BindingGroupType group;
  GtkTreeModel *model;
  char *description;
  char *gettext_package;
  char *binding;
  gboolean editable;

  /* GSettings path */
  char *gsettings_path;
  gboolean desc_editable;
  char *command;
  gboolean cmd_editable;

  /* GSettings */
  char *schema;
  char *key;
  GSettings *settings;
} CcRegionKeyboardItem;

typedef struct
{
  GObjectClass   parent_class;
} CcRegionKeyboardItemClass;

GType                  cc_region_keyboard_item_get_type                 (void);

CcRegionKeyboardItem * cc_region_keyboard_item_new                      (CcRegionKeyboardItemType  type);
gboolean               cc_region_keyboard_item_load_from_gsettings_path (CcRegionKeyboardItem     *item,
                                                                         const char               *path,
                                                                         gboolean                  reset);
gboolean               cc_region_keyboard_item_load_from_gsettings      (CcRegionKeyboardItem     *item,
                                                                         const char               *description,
                                                                         const char               *schema,
                                                                         const char               *key);

const char *           cc_region_keyboard_item_get_description          (CcRegionKeyboardItem     *item);
const char *           cc_region_keyboard_item_get_binding              (CcRegionKeyboardItem     *item);
const char *           cc_region_keyboard_item_get_command              (CcRegionKeyboardItem     *item);

gboolean               cc_region_keyboard_item_equal                    (CcRegionKeyboardItem     *a,
                                                                         CcRegionKeyboardItem     *b);

G_END_DECLS

#endif /* __CC_REGION_KEYBOARD_ITEM_H */
