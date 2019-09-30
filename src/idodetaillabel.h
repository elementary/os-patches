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

#ifndef __IDO_DETAIL_LABEL_H__
#define __IDO_DETAIL_LABEL_H__

#include <gtk/gtk.h>

#define IDO_TYPE_DETAIL_LABEL            (ido_detail_label_get_type())
#define IDO_DETAIL_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDO_TYPE_DETAIL_LABEL, IdoDetailLabel))
#define IDO_DETAIL_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), IDO_TYPE_DETAIL_LABEL, IdoDetailLabelClass))
#define IDO_IS_DETAIL_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDO_TYPE_DETAIL_LABEL))
#define IDO_IS_DETAIL_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), IDO_TYPE_DETAIL_LABEL))
#define IDO_DETAIL_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), IDO_TYPE_DETAIL_LABEL, IdoDetailLabelClass))

typedef struct _IdoDetailLabel        IdoDetailLabel;
typedef struct _IdoDetailLabelClass   IdoDetailLabelClass;
typedef struct _IdoDetailLabelPrivate IdoDetailLabelPrivate;

struct _IdoDetailLabel
{
  GtkWidget parent;
  IdoDetailLabelPrivate *priv;
};

struct _IdoDetailLabelClass
{
  GtkWidgetClass parent_class;
};

GType         ido_detail_label_get_type  (void) G_GNUC_CONST;

GtkWidget *   ido_detail_label_new       (const gchar *str);

const gchar * ido_detail_label_get_text  (IdoDetailLabel *label);

void          ido_detail_label_set_text  (IdoDetailLabel *label,
                                          const gchar    *text);

void          ido_detail_label_set_count (IdoDetailLabel *label,
                                          gint            count);

#endif
