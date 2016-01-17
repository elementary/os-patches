/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2006 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _CC_APPEARANCE_XML_H_
#define _CC_APPEARANCE_XML_H_

#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CC_TYPE_APPEARANCE_XML         (cc_appearance_xml_get_type ())
#define CC_APPEARANCE_XML(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_APPEARANCE_XML, CcAppearanceXml))
#define CC_APPEARANCE_XML_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_APPEARANCE_XML, CcAppearanceXmlClass))
#define CC_IS_APPEARANCE_XML(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_APPEARANCE_XML))
#define CC_IS_APPEARANCE_XML_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_APPEARANCE_XML))
#define CC_APPEARANCE_XML_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_APPEARANCE_XML, CcAppearanceXmlClass))

typedef struct CcAppearanceXmlPrivate CcAppearanceXmlPrivate;

typedef struct
{
  GObject parent;
  CcAppearanceXmlPrivate *priv;
} CcAppearanceXml;

typedef struct
{
  GObjectClass parent_class;
  void (*added) (CcAppearanceXml *xml, GObject *item);
} CcAppearanceXmlClass;

GType              cc_appearance_xml_get_type (void);

CcAppearanceXml *cc_appearance_xml_new (void);

void cc_appearance_xml_save                          (CcAppearanceItem *item,
						      const char       *filename);

CcAppearanceItem *cc_appearance_xml_get_item         (const char      *filename);
gboolean cc_appearance_xml_load_xml                  (CcAppearanceXml *data,
						      const char      *filename);
void cc_appearance_xml_load_list_async               (CcAppearanceXml *data,
						      GCancellable *cancellable,
						      GAsyncReadyCallback callback,
						      gpointer user_data);
const GHashTable *cc_appearance_xml_load_list_finish (GAsyncResult  *async_result);

G_END_DECLS

#endif

