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

#include "idodetaillabel.h"

#include <math.h>

G_DEFINE_TYPE (IdoDetailLabel, ido_detail_label, GTK_TYPE_WIDGET)

struct _IdoDetailLabelPrivate
{
  gchar *text;
  PangoLayout *layout;
  gboolean draw_lozenge;
};

enum
{
  PROP_0,
  PROP_TEXT,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

static void
ido_detail_label_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdoDetailLabel *self = IDO_DETAIL_LABEL (object);

  switch (property_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, self->priv->text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ido_detail_label_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdoDetailLabel *self = IDO_DETAIL_LABEL (object);

  switch (property_id)
    {
    case PROP_TEXT:
      ido_detail_label_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
ido_detail_label_finalize (GObject *object)
{
  IdoDetailLabelPrivate *priv = IDO_DETAIL_LABEL (object)->priv;

  g_free (priv->text);

  G_OBJECT_CLASS (ido_detail_label_parent_class)->finalize (object);
}

static void
ido_detail_label_dispose (GObject *object)
{
  IdoDetailLabelPrivate *priv = IDO_DETAIL_LABEL (object)->priv;

  g_clear_object (&priv->layout);

  G_OBJECT_CLASS (ido_detail_label_parent_class)->dispose (object);
}

static void
ido_detail_label_ensure_layout (IdoDetailLabel *label)
{
  IdoDetailLabelPrivate *priv = label->priv;

  if (priv->layout == NULL)
    {
      priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (label), priv->text);
      pango_layout_set_alignment (priv->layout, PANGO_ALIGN_CENTER);
      pango_layout_set_ellipsize (priv->layout, PANGO_ELLIPSIZE_END);
      pango_layout_set_height (priv->layout, -1);

      // TODO update layout on "style-updated" and "direction-changed"
    }
}

static void
cairo_lozenge (cairo_t *cr,
               double   x,
               double   y,
               double   w,
               double   h,
               double   radius)
{
  double x1 = x + w - radius;
  double x2 = x + radius;
  double y1 = y + radius;
  double y2 = y + h - radius;

  cairo_move_to (cr, x + radius, y);
  cairo_arc (cr, x1, y1, radius, G_PI * 1.5, G_PI * 2);
  cairo_arc (cr, x1, y2, radius, 0,          G_PI * 0.5);
  cairo_arc (cr, x2, y2, radius, G_PI * 0.5, G_PI);
  cairo_arc (cr, x2, y1, radius, G_PI,       G_PI * 1.5);
}

static PangoFontMetrics *
gtk_widget_get_font_metrics (GtkWidget    *widget,
                             PangoContext *context)
{
  PangoFontDescription *font;
  PangoFontMetrics *metrics;

  gtk_style_context_get (gtk_widget_get_style_context (widget),
                         gtk_widget_get_state_flags (widget), 
                         "font", &font, NULL);

  metrics = pango_context_get_metrics (context,
                                       font,
                                       pango_context_get_language (context));

  pango_font_description_free (font);
  return metrics;
}

static gint
ido_detail_label_get_minimum_text_width (IdoDetailLabel *label)
{
  IdoDetailLabelPrivate *priv = label->priv;
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint char_width;
  gint w;

  context = pango_layout_get_context (priv->layout);
  metrics = gtk_widget_get_font_metrics (GTK_WIDGET (label), context);
  char_width = pango_font_metrics_get_approximate_digit_width (metrics);

  w = 2 * char_width / PANGO_SCALE;
  pango_font_metrics_unref (metrics);
  return w;
}

static gboolean
ido_detail_label_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  IdoDetailLabel *label = IDO_DETAIL_LABEL (widget);
  IdoDetailLabelPrivate *priv = IDO_DETAIL_LABEL (widget)->priv;
  PangoRectangle extents;
  GtkAllocation allocation;
  double x, w, h, radius;
  GdkRGBA color;

  if (!priv->text || !*priv->text)
    return TRUE;

  gtk_widget_get_allocation (widget, &allocation);

  ido_detail_label_ensure_layout (IDO_DETAIL_LABEL (widget));

  pango_layout_get_extents (priv->layout, NULL, &extents);
  pango_extents_to_pixels (&extents, NULL);

  h = MIN (allocation.height, extents.height);
  radius = floor (h / 2.0);
  w = MAX (ido_detail_label_get_minimum_text_width (label), extents.width) + 2.0 * radius;
  x = allocation.width - w;

  pango_layout_set_width (priv->layout, (allocation.width - 2 * radius) * PANGO_SCALE);
  pango_layout_get_extents (priv->layout, NULL, &extents);
  pango_extents_to_pixels (&extents, NULL);

  gtk_style_context_get_color (gtk_widget_get_style_context (widget),
                               gtk_widget_get_state_flags (widget),
                               &color);
  gdk_cairo_set_source_rgba (cr, &color);

  cairo_set_line_width (cr, 1.0);
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);

  if (priv->draw_lozenge)
    cairo_lozenge (cr, x, 0.0, w, h, radius);

  cairo_move_to (cr, x + radius, (allocation.height - extents.height) / 2.0);
  pango_cairo_layout_path (cr, priv->layout);
  cairo_fill (cr);

  return TRUE;
}

static void
ido_detail_label_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  IdoDetailLabelPrivate *priv = IDO_DETAIL_LABEL (widget)->priv;
  PangoRectangle extents;
  double radius;

  ido_detail_label_ensure_layout (IDO_DETAIL_LABEL (widget));

  pango_layout_get_extents (priv->layout, NULL, &extents);
  pango_extents_to_pixels (&extents, NULL);

  radius = floor (extents.height / 2.0);

  *minimum = ido_detail_label_get_minimum_text_width (IDO_DETAIL_LABEL (widget)) + 2.0 * radius;
  *natural = MAX (*minimum, extents.width + 2.0 * radius);
}

static void
ido_detail_label_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  IdoDetailLabelPrivate *priv = IDO_DETAIL_LABEL (widget)->priv;
  PangoContext *context;
  PangoFontMetrics *metrics;
  PangoRectangle extents;

  ido_detail_label_ensure_layout (IDO_DETAIL_LABEL (widget));

  pango_layout_get_extents (priv->layout, NULL, &extents);
  pango_extents_to_pixels (&extents, NULL);
  context = pango_layout_get_context (priv->layout);
  metrics = gtk_widget_get_font_metrics (widget, context);

  *minimum = *natural = (pango_font_metrics_get_ascent (metrics) +
                         pango_font_metrics_get_descent (metrics)) / PANGO_SCALE;

  pango_font_metrics_unref (metrics);
}

static void
ido_detail_label_class_init (IdoDetailLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ido_detail_label_get_property;
  object_class->set_property = ido_detail_label_set_property;
  object_class->finalize = ido_detail_label_finalize;
  object_class->dispose = ido_detail_label_dispose;

  widget_class->draw = ido_detail_label_draw;
  widget_class->get_preferred_width = ido_detail_label_get_preferred_width;
  widget_class->get_preferred_height = ido_detail_label_get_preferred_height;

  g_type_class_add_private (klass, sizeof (IdoDetailLabelPrivate));

  properties[PROP_TEXT] = g_param_spec_string ("text",
                                               "Text",
                                               "The text of the label",
                                               NULL,
                                               G_PARAM_READWRITE |
                                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
ido_detail_label_init (IdoDetailLabel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            IDO_TYPE_DETAIL_LABEL,
                                            IdoDetailLabelPrivate);

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

GtkWidget *
ido_detail_label_new (const gchar *label)
{
  return g_object_new (IDO_TYPE_DETAIL_LABEL,
                       "text", label,
                       NULL);
}

const gchar *
ido_detail_label_get_text (IdoDetailLabel *label)
{
  g_return_val_if_fail (IDO_IS_DETAIL_LABEL (label), NULL);
  return label->priv->text;
}

/* collapse_whitespace:
 * @str: the source string
 *
 * Collapses all occurences of consecutive whitespace charactes in @str
 * into a single space.
 *
 * Returns: (transfer full): a newly-allocated string
 */
static gchar *
collapse_whitespace (const gchar *str)
{
  GString *result;
  gboolean in_space = FALSE;

  if (str == NULL)
    return NULL;

  result = g_string_new ("");

  while (*str)
    {
      gunichar c = g_utf8_get_char_validated (str, -1);

      if (c == (gunichar) -1)
        break;

      if (!g_unichar_isspace (c))
        {
          g_string_append_unichar (result, c);
          in_space = FALSE;
        }
      else if (!in_space)
        {
          g_string_append_c (result, ' ');
          in_space = TRUE;
        }

      str = g_utf8_next_char (str);
    }

  return g_string_free (result, FALSE);
}

static void
ido_detail_label_set_text_impl (IdoDetailLabel *label,
                                const gchar    *text,
                                gboolean        draw_lozenge)
{
  IdoDetailLabelPrivate * priv = label->priv;

  g_clear_object (&priv->layout);
  g_free (priv->text);

  priv->text = g_strdup (text);
  priv->draw_lozenge = draw_lozenge;

  g_object_notify_by_pspec (G_OBJECT (label), properties[PROP_TEXT]);
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

void
ido_detail_label_set_text (IdoDetailLabel *label,
                           const gchar    *text)
{
  gchar *str;

  g_return_if_fail (IDO_IS_DETAIL_LABEL (label));

  str = collapse_whitespace (text);
  ido_detail_label_set_text_impl (label, str, FALSE);
  g_free (str);
}

void
ido_detail_label_set_count (IdoDetailLabel *label,
                            gint            count)
{
  gchar *text;

  g_return_if_fail (IDO_IS_DETAIL_LABEL (label));

  text = g_strdup_printf ("%d", count);
  ido_detail_label_set_text_impl (label, text, TRUE);
  g_free (text);
}
