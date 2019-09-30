/*
 * Copyright (C) 2010 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of either or both of the following licenses:
 *
 * 1) the GNU Lesser General Public License version 3, as published by the
 * Free Software Foundation; and/or
 * 2) the GNU Lesser General Public License version 2.1, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the applicable version of the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License version 3 and version 2.1 along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 *
 * Authors:
 *    Cody Russell <crussell@canonical.com>
 */

#ifndef __IDO_RANGE_H__
#define __IDO_RANGE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDO_TYPE_RANGE         (ido_range_get_type ())
#define IDO_RANGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), IDO_TYPE_RANGE, IdoRange))
#define IDO_RANGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), IDO_TYPE_RANGE, IdoRangeClass))
#define IDO_IS_RANGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), IDO_TYPE_RANGE))
#define IDO_IS_RANGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), IDO_TYPE_RANGE))
#define IDO_RANGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), IDO_TYPE_RANGE, IdoRangeClass))

typedef enum
{
  IDO_RANGE_STYLE_DEFAULT,
  IDO_RANGE_STYLE_SMALL
} IdoRangeStyle;

typedef struct _IdoRange        IdoRange;
typedef struct _IdoRangePrivate IdoRangePrivate;
typedef struct _IdoRangeClass   IdoRangeClass;

struct _IdoRange
{
  GtkScale parent_instance;
  IdoRangePrivate *priv;
};

struct _IdoRangeClass
{
  GtkScaleClass parent_class;

  /* Padding for future expansion */
  void (*_ido_reserved1) (void);
  void (*_ido_reserved2) (void);
  void (*_ido_reserved3) (void);
  void (*_ido_reserved4) (void);
};

GType      ido_range_get_type (void) G_GNUC_CONST;

GtkWidget* ido_range_new       (GObject *adj,
                                IdoRangeStyle  style);

G_END_DECLS

#endif /* __IDO_RANGE_H__ */
