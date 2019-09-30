/*
 * Copyright 2010 Canonical, Ltd.
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

#include "idorange.h"
#include "idotypebuiltins.h"
#include "config.h"

struct _IdoRangePrivate
{
  IdoRangeStyle style;
};

static void ido_range_constructed    (GObject          *object);
static void ido_range_set_property   (GObject          *object,
                                      guint             prop_id,
                                      const GValue     *value,
                                      GParamSpec       *pspec);
static void ido_range_get_property   (GObject          *object,
                                      guint             prop_id,
                                      GValue           *value,
                                      GParamSpec       *pspec);

#define IDO_RANGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), IDO_TYPE_RANGE, IdoRangePrivate))

G_DEFINE_TYPE (IdoRange, ido_range, GTK_TYPE_SCALE)

enum {
  PROP_0,
  PROP_STYLE
};

static void
ido_range_class_init (IdoRangeClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->constructed  = ido_range_constructed;
  gobject_class->set_property = ido_range_set_property;
  gobject_class->get_property = ido_range_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_STYLE,
                                   g_param_spec_enum ("range-style",
                                                      "Range style",
                                                      "The style of the range",
                                                      IDO_TYPE_RANGE_STYLE,
                                                      IDO_RANGE_STYLE_SMALL,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("knob-width",
                                                             "The knob width",
                                                             "The knob width",
                                                             G_MININT,
                                                             G_MAXINT,
                                                             8,
                                                             G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("knob-height",
                                                             "The knob height",
                                                             "The knob height",
                                                             G_MININT,
                                                             G_MAXINT,
                                                             8,
                                                             G_PARAM_READABLE));

  g_type_class_add_private (class, sizeof (IdoRangePrivate));
}

static void
ido_range_get_property (GObject      *object,
                        guint         prop_id,
                        GValue       *value,
                        GParamSpec   *pspec)
{
  IdoRangePrivate *priv = IDO_RANGE (object)->priv;

  switch (prop_id)
    {
    case PROP_STYLE:
      g_value_set_enum (value, priv->style);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ido_range_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  IdoRangePrivate *priv = IDO_RANGE (object)->priv;

  switch (prop_id)
    {
    case PROP_STYLE:
      priv->style = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ido_range_constructed (GObject *object)
{
  IdoRange *range = IDO_RANGE (object);
  IdoRangeStyle style;
  char buf[1024];

  g_object_get (range,
                "range-style", &style,
                NULL);

  g_snprintf (buf, sizeof (buf), "idorange-%p", range);
  gtk_widget_set_name (GTK_WIDGET (range), buf);

  if (style == IDO_RANGE_STYLE_SMALL)
    {
      gint width, height;

      gtk_widget_style_get (GTK_WIDGET (range),
                            "knob-width", &width,
                            "knob-height", &height,
                            NULL);
    }

  gtk_range_set_slider_size_fixed (GTK_RANGE (range), TRUE);

  G_OBJECT_CLASS (ido_range_parent_class)->constructed (object);
}

static void
ido_range_init (IdoRange *range)
{
  range->priv = IDO_RANGE_GET_PRIVATE (range);
}

/**
 * ido_range_new:
 * @adj: A #GtkAdjustment providing the range values
 * @style: The range style
 *
 * Creates a new #IdoRange widget.
 *
 * Return Value: A new #IdoRange
 **/
GtkWidget *
ido_range_new (GObject *adj,
               IdoRangeStyle  style)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adj), NULL);

  return g_object_new (IDO_TYPE_RANGE,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "adjustment",  adj,
                       "range-style", style,
                       NULL);
}
