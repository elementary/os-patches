/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Red Hat, Inc.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __CC_APPEARANCE_ITEM_H
#define __CC_APPEARANCE_ITEM_H

#include <glib-object.h>

#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gdesktop-enums.h>
#include <libgnome-desktop/gnome-bg.h>

G_BEGIN_DECLS

#define CC_TYPE_APPEARANCE_ITEM         (cc_appearance_item_get_type ())
#define CC_APPEARANCE_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_APPEARANCE_ITEM, CcAppearanceItem))
#define CC_APPEARANCE_ITEM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_APPEARANCE_ITEM, CcAppearanceItemClass))
#define CC_IS_APPEARANCE_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_APPEARANCE_ITEM))
#define CC_IS_APPEARANCE_ITEM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_APPEARANCE_ITEM))
#define CC_APPEARANCE_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_APPEARANCE_ITEM, CcAppearanceItemClass))

typedef enum {
	CC_APPEARANCE_ITEM_HAS_SHADING   = 1 << 0,
	CC_APPEARANCE_ITEM_HAS_PLACEMENT = 1 << 1,
	CC_APPEARANCE_ITEM_HAS_PCOLOR    = 1 << 2,
	CC_APPEARANCE_ITEM_HAS_SCOLOR    = 1 << 3,
	CC_APPEARANCE_ITEM_HAS_URI       = 1 << 4
} CcAppearanceItemFlags;

#define CC_APPEARANCE_ITEM_HAS_ALL (CC_APPEARANCE_ITEM_HAS_SHADING &	\
				    CC_APPEARANCE_ITEM_HAS_PLACEMENT &	\
				    CC_APPEARANCE_ITEM_HAS_PCOLOR &	\
				    CC_APPEARANCE_ITEM_HAS_SCOLOR &	\
				    CC_APPEARANCE_ITEM_HAS_FNAME)

typedef struct CcAppearanceItemPrivate CcAppearanceItemPrivate;

typedef struct
{
        GObject                  parent;
        CcAppearanceItemPrivate *priv;
} CcAppearanceItem;

typedef struct
{
        GObjectClass   parent_class;
} CcAppearanceItemClass;

GType              cc_appearance_item_get_type (void);

CcAppearanceItem * cc_appearance_item_new                 (const char                   *uri);
CcAppearanceItem * cc_appearance_item_copy                (CcAppearanceItem             *item);
gboolean           cc_appearance_item_load                (CcAppearanceItem             *item,
							   GFileInfo                    *info);
gboolean           cc_appearance_item_changes_with_time   (CcAppearanceItem             *item);

GIcon     *        cc_appearance_item_get_thumbnail       (CcAppearanceItem             *item,
                                                           GnomeDesktopThumbnailFactory *thumbs,
                                                           int                           width,
                                                           int                           height);
GIcon     *        cc_appearance_item_get_frame_thumbnail (CcAppearanceItem             *item,
                                                           GnomeDesktopThumbnailFactory *thumbs,
                                                           int                           width,
                                                           int                           height,
                                                           int                           frame,
                                                           gboolean                      force_size);

GDesktopBackgroundStyle   cc_appearance_item_get_placement  (CcAppearanceItem *item);
GDesktopBackgroundShading cc_appearance_item_get_shading    (CcAppearanceItem *item);
const char *              cc_appearance_item_get_uri        (CcAppearanceItem *item);
const char *              cc_appearance_item_get_source_url (CcAppearanceItem *item);
const char *              cc_appearance_item_get_source_xml (CcAppearanceItem *item);
CcAppearanceItemFlags     cc_appearance_item_get_flags      (CcAppearanceItem *item);
const char *              cc_appearance_item_get_pcolor     (CcAppearanceItem *item);
const char *              cc_appearance_item_get_scolor     (CcAppearanceItem *item);
const char *              cc_appearance_item_get_name       (CcAppearanceItem *item);
const char *              cc_appearance_item_get_size       (CcAppearanceItem *item);
gboolean                  cc_appearance_item_get_needs_download (CcAppearanceItem *item);

gboolean                  cc_appearance_item_compare        (CcAppearanceItem *saved,
							     CcAppearanceItem *configured);

void                      cc_appearance_item_dump           (CcAppearanceItem *item);

G_END_DECLS

#endif /* __CC_APPEARANCE_ITEM_H */
