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

#include "config.h"


#include <gtk/gtk.h>
#include "idorange.h"
#include "idoscalemenuitem.h"
#include "idotypebuiltins.h"
#include "idoactionhelper.h"

static void     ido_scale_menu_item_set_property           (GObject               *object,
                                                            guint                  prop_id,
                                                            const GValue          *value,
                                                            GParamSpec            *pspec);
static void     ido_scale_menu_item_get_property           (GObject               *object,
                                                            guint                  prop_id,
                                                            GValue                *value,
                                                            GParamSpec            *pspec);
static gboolean ido_scale_menu_item_parent_key_press_event (GtkWidget             *widget,
                                                            GdkEventKey           *event,
                                                            gpointer               user_data);
static void     ido_scale_menu_item_select                  (GtkMenuItem          *item);
static void     ido_scale_menu_item_deselect                (GtkMenuItem          *item);
static gboolean ido_scale_menu_item_button_press_event     (GtkWidget             *menuitem,
                                                            GdkEventButton        *event);
static gboolean ido_scale_menu_item_button_release_event   (GtkWidget             *menuitem,
                                                            GdkEventButton        *event);
static gboolean ido_scale_menu_item_motion_notify_event    (GtkWidget             *menuitem,
                                                            GdkEventMotion        *event);
static void     ido_scale_menu_item_primary_image_notify   (GtkImage              *image,
                                                            GParamSpec            *pspec,
                                                            IdoScaleMenuItem      *item);
static void     ido_scale_menu_item_secondary_image_notify (GtkImage              *image,
                                                            GParamSpec            *pspec,
                                                            IdoScaleMenuItem      *item);
static void     ido_scale_menu_item_parent_set             (GtkWidget             *item,
                                                            GtkWidget             *previous_parent);
static void     update_packing                             (IdoScaleMenuItem      *self,
                                                            IdoScaleMenuItemStyle  style);
static void     default_primary_clicked_handler            (IdoScaleMenuItem      *self);
static void     default_secondary_clicked_handler          (IdoScaleMenuItem      *self);

struct _IdoScaleMenuItemPrivate {
  GtkWidget            *scale;
  GtkAdjustment        *adjustment;
  GtkWidget            *primary_image;
  GtkWidget            *secondary_image;
  GtkWidget            *primary_label;
  GtkWidget            *secondary_label;
  GtkWidget            *hbox;
  gboolean              reverse_scroll;
  gboolean              grabbed;
  IdoScaleMenuItemStyle style;
  IdoRangeStyle         range_style;
  gboolean              ignore_value_changed;
  gboolean              has_focus;
};

enum {
  SLIDER_GRABBED,
  SLIDER_RELEASED,
  PRIMARY_CLICKED,
  SECONDARY_CLICKED,
  VALUE_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ADJUSTMENT,
  PROP_REVERSE_SCROLL_EVENTS,
  PROP_STYLE,
  PROP_RANGE_STYLE
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (IdoScaleMenuItem, ido_scale_menu_item, GTK_TYPE_MENU_ITEM)

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), IDO_TYPE_SCALE_MENU_ITEM, IdoScaleMenuItemPrivate))

static gboolean
ido_scale_menu_item_scroll_event (GtkWidget      *menuitem,
                                  GdkEventScroll *event)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (menuitem);
  GtkWidget *scale = priv->scale;

  if (priv->reverse_scroll)
    {
      switch (event->direction)
        {
        case GDK_SCROLL_UP:
          event->direction = GDK_SCROLL_DOWN;
          break;

        case GDK_SCROLL_DOWN:
          event->direction = GDK_SCROLL_UP;
          break;

        default:
          break;
        }
    }

  gtk_widget_event (scale,
                    ((GdkEvent *)(void*)(event)));

  return TRUE;
}

static void
ido_scale_menu_item_scale_value_changed (GtkRange *range,
                                         gpointer  user_data)
{
  IdoScaleMenuItem *self = user_data;
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (self);

  /* The signal is not sent when it was set through
   * ido_scale_menu_item_set_value().  */

  if (!priv->ignore_value_changed)
    g_signal_emit (self, signals[VALUE_CHANGED], 0, gtk_range_get_value (range));
}

static void
ido_scale_menu_item_constructed (GObject *object)
{
  IdoScaleMenuItem *self = IDO_SCALE_MENU_ITEM (object);
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (self);
  GObject *adj = G_OBJECT (gtk_adjustment_new (0.0, 0.0, 100.0, 1.0, 10.0, 0.0));
  IdoRangeStyle range_style;
  GtkWidget *hbox;

  priv->adjustment  = NULL;

  g_object_get (self,
                "range-style", &range_style,
                NULL);

  priv->scale = ido_range_new (adj, range_style);
  g_signal_connect (priv->scale, "value-changed", G_CALLBACK (ido_scale_menu_item_scale_value_changed), self);
  g_object_ref (priv->scale);
  gtk_scale_set_draw_value (GTK_SCALE (priv->scale), FALSE);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  priv->primary_image = gtk_image_new ();
  g_signal_connect (priv->primary_image, "notify",
                    G_CALLBACK (ido_scale_menu_item_primary_image_notify),
                    self);

  priv->secondary_image = gtk_image_new ();
  g_signal_connect (priv->secondary_image, "notify",
                    G_CALLBACK (ido_scale_menu_item_secondary_image_notify),
                    self);

  priv->primary_label = gtk_label_new ("");
  priv->secondary_label = gtk_label_new ("");

  priv->hbox = hbox;

  update_packing (self, priv->style);

  gtk_container_add (GTK_CONTAINER (self), hbox);

  gtk_widget_add_events (GTK_WIDGET(self), GDK_SCROLL_MASK);
}

static void
ido_scale_menu_item_class_init (IdoScaleMenuItemClass *item_class)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (item_class);
  GtkWidgetClass    *widget_class = GTK_WIDGET_CLASS (item_class);
  GtkMenuItemClass  *menuitem_class = GTK_MENU_ITEM_CLASS (item_class);

  item_class->primary_clicked        = default_primary_clicked_handler;
  item_class->secondary_clicked      = default_secondary_clicked_handler;

  menuitem_class->select = ido_scale_menu_item_select;
  menuitem_class->deselect = ido_scale_menu_item_deselect;

  widget_class->button_press_event   = ido_scale_menu_item_button_press_event;
  widget_class->button_release_event = ido_scale_menu_item_button_release_event;
  widget_class->motion_notify_event  = ido_scale_menu_item_motion_notify_event;
  widget_class->scroll_event         = ido_scale_menu_item_scroll_event;
  widget_class->parent_set           = ido_scale_menu_item_parent_set;

  gobject_class->constructed  = ido_scale_menu_item_constructed;
  gobject_class->set_property = ido_scale_menu_item_set_property;
  gobject_class->get_property = ido_scale_menu_item_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_STYLE,
                                   g_param_spec_enum ("accessory-style",
                                                      "Style of primary/secondary widgets",
                                                      "The style of the primary/secondary widgets",
                                                      IDO_TYPE_SCALE_MENU_ITEM_STYLE,
                                                      IDO_SCALE_MENU_ITEM_STYLE_NONE,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_RANGE_STYLE,
                                   g_param_spec_enum ("range-style",
                                                      "Range style",
                                                      "Style of the range",
                                                      IDO_TYPE_RANGE_STYLE,
                                                      IDO_RANGE_STYLE_DEFAULT,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
                                                        "Adjustment",
                                                        "The adjustment containing the scale value",
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_REVERSE_SCROLL_EVENTS,
                                   g_param_spec_boolean ("reverse-scroll-events",
                                                         "Reverse scroll events",
                                                         "Reverses how up/down scroll events are interpreted",
                                                         TRUE,
                                                         G_PARAM_READWRITE));

  /**
   * IdoScaleMenuItem::slider-grabbed:
   * @menuitem: The #IdoScaleMenuItem emitting the signal.
   *
   * The ::slider-grabbed signal is emitted when the pointer selects the slider. 
   */
  signals[SLIDER_GRABBED] = g_signal_new ("slider-grabbed",
                                          G_OBJECT_CLASS_TYPE (gobject_class),
                                          G_SIGNAL_RUN_FIRST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE, 0);

  /**
   * IdoScaleMenuItem::slider-released:
   * @menuitem: The #IdoScaleMenuItem emitting the signal.
   *
   * The ::slider-released signal is emitted when the pointer releases the slider.
   */
  signals[SLIDER_RELEASED] = g_signal_new ("slider-released",
                                           G_OBJECT_CLASS_TYPE (gobject_class),
                                           G_SIGNAL_RUN_FIRST,
                                           0,
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, 0);

  /**
   * IdoScaleMenuItem::primary-clicked:
   * @menuitem: The #IdoScaleMenuItem emitting the signal.
   *
   * The ::primary-clicked signal is emitted when the pointer clicks the primary label.
   */
  signals[PRIMARY_CLICKED] = g_signal_new ("primary-clicked",
                                           G_TYPE_FROM_CLASS (item_class),
                                           G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                           G_STRUCT_OFFSET (IdoScaleMenuItemClass, primary_clicked),
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, /* return type */
                                           0 /* n_params */);

  /**
   * IdoScaleMenuItem::secondary-clicked:
   * @menuitem: The #IdoScaleMenuItem emitting the signal.
   *
   * The ::secondary-clicked signal is emitted when the pointer clicks the secondary label.
   */
  signals[SECONDARY_CLICKED] = g_signal_new ("secondary-clicked",
                                             G_TYPE_FROM_CLASS (item_class),
                                             G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                             G_STRUCT_OFFSET (IdoScaleMenuItemClass, secondary_clicked),
                                             NULL, NULL,
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE, /* return type */
                                             0 /* n_params */);

  /**
   * IdoScaleMenuItem::value-changed:
   * @menuitem: the #IdoScaleMenuItem for which the value changed
   * @value: the new value
   *
   * Emitted whenever the value of the contained scale changes because
   * of user input.
   */
  signals[VALUE_CHANGED] = g_signal_new ("value-changed",
                                         IDO_TYPE_SCALE_MENU_ITEM,
                                         G_SIGNAL_RUN_LAST,
                                         0, NULL, NULL,
                                         g_cclosure_marshal_VOID__DOUBLE,
                                         G_TYPE_NONE,
                                         1, G_TYPE_DOUBLE);


  g_type_class_add_private (item_class, sizeof (IdoScaleMenuItemPrivate));
}

static void
update_packing (IdoScaleMenuItem *self, IdoScaleMenuItemStyle style)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (self);
  GtkBox * box = GTK_BOX (priv->hbox);
  GtkContainer *container = GTK_CONTAINER (priv->hbox);

  /* remove the old layout */
  GList * children = gtk_container_get_children (container);
  GList * l;
  for (l=children; l!=NULL; l=l->next)
    gtk_container_remove (container, l->data);
  g_list_free (children);

  /* add the new layout */
  switch (style)
    {
    case IDO_SCALE_MENU_ITEM_STYLE_IMAGE:
      gtk_box_pack_start (box, priv->primary_image, FALSE, FALSE, 0);
      gtk_box_pack_start (box, priv->scale, TRUE, TRUE, 0);
      gtk_box_pack_start (box, priv->secondary_image, FALSE, FALSE, 0);
      break;

    case IDO_SCALE_MENU_ITEM_STYLE_LABEL:
      gtk_box_pack_start (box, priv->primary_label, FALSE, FALSE, 0);
      gtk_box_pack_start (box, priv->scale, TRUE, TRUE, 0);
      gtk_box_pack_start (box, priv->secondary_label, FALSE, FALSE, 0);
      break;

    default:
      gtk_box_pack_start (box, priv->scale, TRUE, TRUE, 0);
      break;
    }

  gtk_widget_show_all (priv->hbox);
}

static void
ido_scale_menu_item_init (IdoScaleMenuItem *self)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (self);

  priv->reverse_scroll = TRUE;

  gtk_widget_set_size_request (GTK_WIDGET (self), 200, -1);
}

static void
ido_scale_menu_item_set_property (GObject         *object,
                                  guint            prop_id,
                                  const GValue    *value,
                                  GParamSpec      *pspec)
{
  IdoScaleMenuItem *menu_item = IDO_SCALE_MENU_ITEM (object);
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (menu_item);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      gtk_range_set_adjustment (GTK_RANGE (priv->scale), g_value_get_object (value));
      break;

    case PROP_REVERSE_SCROLL_EVENTS:
      priv->reverse_scroll = g_value_get_boolean (value);
      break;

    case PROP_STYLE:
      ido_scale_menu_item_set_style (menu_item, g_value_get_enum (value));
      break;

    case PROP_RANGE_STYLE:
      priv->range_style = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ido_scale_menu_item_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
  IdoScaleMenuItem *menu_item = IDO_SCALE_MENU_ITEM (object);
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (menu_item);
  GtkAdjustment *adjustment;

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      adjustment = gtk_range_get_adjustment (GTK_RANGE (priv->scale));
      g_value_set_object (value, adjustment);
      break;

    case PROP_REVERSE_SCROLL_EVENTS:
      g_value_set_boolean (value, priv->reverse_scroll);
      break;

    case PROP_RANGE_STYLE:
      g_value_set_enum (value, priv->range_style);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ido_scale_menu_item_get_scale_allocation (IdoScaleMenuItem *menuitem,
                                          GtkAllocation    *allocation)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (menuitem);
  GtkAllocation parent_allocation;

  gtk_widget_get_allocation (GTK_WIDGET (menuitem), &parent_allocation);
  gtk_widget_get_allocation (priv->scale, allocation);

  allocation->x -= parent_allocation.x;
  allocation->y -= parent_allocation.y;
}

static gboolean
ido_scale_menu_item_parent_key_press_event (GtkWidget   *widget,
                                            GdkEventKey *event,
                                            gpointer     user_data)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (user_data);

  /* only listen to events when the playback menu item is selected */
  if (!priv->has_focus)
    return FALSE;

  switch (event->keyval)
    {
    case GDK_KEY_Left:
    case GDK_KEY_minus:
    case GDK_KEY_KP_Subtract:
      GTK_RANGE_GET_CLASS (priv->scale)->move_slider (GTK_RANGE (priv->scale), GTK_SCROLL_STEP_LEFT);
      return TRUE;

    case GDK_KEY_Right:
    case GDK_KEY_plus:
    case GDK_KEY_KP_Add:
      GTK_RANGE_GET_CLASS (priv->scale)->move_slider (GTK_RANGE (priv->scale), GTK_SCROLL_STEP_RIGHT);
      return TRUE;
    }

  return FALSE;
}

static void
ido_scale_menu_item_select (GtkMenuItem *item)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (item);

  priv->has_focus = TRUE;

  GTK_MENU_ITEM_CLASS (ido_scale_menu_item_parent_class)->select (item);
}

static void
ido_scale_menu_item_deselect (GtkMenuItem *item)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (item);

  priv->has_focus = FALSE;

  GTK_MENU_ITEM_CLASS (ido_scale_menu_item_parent_class)->deselect (item);
}

static gboolean
ido_scale_menu_item_button_press_event (GtkWidget      *menuitem,
                                        GdkEventButton *event)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (menuitem);
  GtkAllocation alloc;

  ido_scale_menu_item_get_scale_allocation (IDO_SCALE_MENU_ITEM (menuitem), &alloc);

  // can we block emissions of "grab-notify" on parent??

  event->x -= alloc.x;
  event->y -= alloc.y;

  event->x_root -= alloc.x;
  event->y_root -= alloc.y;

  gtk_widget_event (priv->scale,
                    ((GdkEvent *)(void*)(event)));

  if (!priv->grabbed)
    {
      priv->grabbed = TRUE;
      g_signal_emit (menuitem, signals[SLIDER_GRABBED], 0);
    }

  return FALSE;
}

static gboolean
ido_scale_menu_item_button_release_event (GtkWidget *menuitem,
                                          GdkEventButton *event)
{
  IdoScaleMenuItem *item = IDO_SCALE_MENU_ITEM (menuitem);
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (menuitem);
  GtkWidget *scale = priv->scale;
  GtkAllocation alloc;

  ido_scale_menu_item_get_scale_allocation (IDO_SCALE_MENU_ITEM (menuitem), &alloc);

  /* if user clicked to the left of the scale... */
  if (event->x < alloc.x)
    {
      if (gtk_widget_get_direction (menuitem) == GTK_TEXT_DIR_LTR)
        {
          ido_scale_menu_item_primary_clicked (item);
        }
      else
        {
          ido_scale_menu_item_secondary_clicked (item);
        }
    }

  /* if user clicked to the right of the scale... */
  else if (event->x > alloc.x + alloc.width)
    {
      if (gtk_widget_get_direction (menuitem) == GTK_TEXT_DIR_LTR)
        {
          ido_scale_menu_item_secondary_clicked (item);
        }
      else
        {
          ido_scale_menu_item_primary_clicked (item);
        }
    }

  /* user clicked on the scale... */
  else
    {
      event->x -= alloc.x;
      event->y -= alloc.y;

      event->x_root -= alloc.x;
      event->y_root -= alloc.y;

      gtk_widget_event (scale, (GdkEvent*)event);
    }

  if (priv->grabbed)
    {
      priv->grabbed = FALSE;
      g_signal_emit (menuitem, signals[SLIDER_RELEASED], 0);
    }

  return TRUE;
}

static gboolean
ido_scale_menu_item_motion_notify_event (GtkWidget      *menuitem,
                                         GdkEventMotion *event)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (menuitem);
  GtkWidget *scale = priv->scale;
  GtkAllocation alloc;

  ido_scale_menu_item_get_scale_allocation (IDO_SCALE_MENU_ITEM (menuitem), &alloc);

  event->x -= alloc.x;
  event->y -= alloc.y;

  event->x_root -= alloc.x;
  event->y_root -= alloc.y;

  gtk_widget_event (scale,
                    ((GdkEvent *)(void*)(event)));

  return TRUE;
}

static void
menu_hidden (GtkWidget        *menu,
             IdoScaleMenuItem *scale)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (scale);

  if (priv->grabbed)
    {
      priv->grabbed = FALSE;
      g_signal_emit (scale, signals[SLIDER_RELEASED], 0);
    }
}

static void
ido_scale_menu_item_parent_set (GtkWidget *item,
                                GtkWidget *previous_parent)

{
  GtkWidget *parent;

  if (previous_parent)
    {
      g_signal_handlers_disconnect_by_func (previous_parent, menu_hidden, item);
      g_signal_handlers_disconnect_by_func (previous_parent, ido_scale_menu_item_parent_key_press_event, item);
    }

  parent = gtk_widget_get_parent (item);

  if (parent)
    {
      g_signal_connect (parent, "hide", G_CALLBACK (menu_hidden), item);

      /* Menus don't pass key events to their children. This works around
       * that by listening to key events on the parent widget. */
      g_signal_connect (parent, "key-press-event",
                        G_CALLBACK (ido_scale_menu_item_parent_key_press_event), item);
    }
}

static void
ido_scale_menu_item_primary_image_notify (GtkImage         *image,
                                          GParamSpec       *pspec,
                                          IdoScaleMenuItem *item)
{
  if (gtk_image_get_storage_type (image) == GTK_IMAGE_EMPTY)
    gtk_widget_hide (GTK_WIDGET (image));
  else
    gtk_widget_show (GTK_WIDGET (image));
}

static void
ido_scale_menu_item_secondary_image_notify (GtkImage         *image,
                                            GParamSpec       *pspec,
                                            IdoScaleMenuItem *item)
{
  if (gtk_image_get_storage_type (image) == GTK_IMAGE_EMPTY)
    gtk_widget_hide (GTK_WIDGET (image));
  else
    gtk_widget_show (GTK_WIDGET (image));
}

/**
 * ido_scale_menu_item_new:
 * @label: the text of the new menu item.
 * @size: The size style of the range.
 * @adjustment: A #GtkAdjustment describing the slider value.
 *
 * Creates a new #IdoScaleMenuItem with an empty label.
 *
 * Return Value: a new #IdoScaleMenuItem.
 **/
GtkWidget*
ido_scale_menu_item_new (const gchar   *label,
                         IdoRangeStyle  range_style,
                         GtkAdjustment *adjustment)
{
  return g_object_new (IDO_TYPE_SCALE_MENU_ITEM,
                       "adjustment",  adjustment,
                       "range-style", range_style,
                       NULL);
}

/**
 * ido_scale_menu_item_new_with_label:
 * @label: the text of the menu item.
 * @size: The size style of the range.
 * @min: The minimum value of the slider.
 * @max: The maximum value of the slider.
 * @step: The step increment of the slider.
 *
 * Creates a new #IdoScaleMenuItem containing a label.
 *
 * Return Value: a new #IdoScaleMenuItem.
 **/
GtkWidget*
ido_scale_menu_item_new_with_range (const gchar  *label,
                                    IdoRangeStyle range_style,
                                    gdouble       value,
                                    gdouble       min,
                                    gdouble       max,
                                    gdouble       step)
{
  GObject *adjustment = G_OBJECT (gtk_adjustment_new (value, min, max, step, 10 * step, 0));

  return GTK_WIDGET (g_object_new (IDO_TYPE_SCALE_MENU_ITEM,
                                  "label",       label,
                                  "range-style", range_style,
                                  "adjustment",  adjustment,
                     NULL));
}

/**
 * ido_scale_menu_item_get_scale:
 * @menuitem: The #IdoScaleMenuItem
 *
 * Retrieves the scale widget.
 *
 * Return Value: (transfer none): The #IdoRange in this item
 **/
GtkWidget*
ido_scale_menu_item_get_scale (IdoScaleMenuItem *menuitem)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_val_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem), NULL);

  priv = GET_PRIVATE (menuitem);

  return priv->scale;
}

/**
 * ido_scale_menu_item_get_style:
 * @menuitem: The #IdoScaleMenuItem
 *
 * Retrieves the type of widgets being used for the primary and
 * secondary widget slots.  This could be images, labels, or nothing.
 *
 * Return Value: A #IdoScaleMenuItemStyle enum describing the style.
 **/
IdoScaleMenuItemStyle
ido_scale_menu_item_get_style (IdoScaleMenuItem *menuitem)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_val_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem), IDO_SCALE_MENU_ITEM_STYLE_NONE);

  priv = GET_PRIVATE (menuitem);

  return priv->style;
}

/**
 * ido_scale_menu_item_set_style:
 * @menuitem: The #IdoScaleMenuItem
 * @style: Set the style use for the primary and secondary widget slots.
 *
 * Sets the type of widgets being used for the primary and
 * secondary widget slots.  This could be images, labels, or nothing.
 **/
void
ido_scale_menu_item_set_style (IdoScaleMenuItem      *menuitem,
                               IdoScaleMenuItemStyle  style)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem));

  priv = GET_PRIVATE (menuitem);

  priv->style = style;

  update_packing (menuitem, style);
}

/**
 * ido_scale_menu_item_get_primary_image:
 * @menuitem: The #IdoScaleMenuItem
 *
 * Retrieves a pointer to the image widget used in the primary slot.
 * Whether this is visible depends upon the return value from
 * ido_scale_menu_item_get_style().
 *
 * Return Value: (transfer none): A #GtkWidget pointer for the primary image.
 **/
GtkWidget *
ido_scale_menu_item_get_primary_image (IdoScaleMenuItem *menuitem)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_val_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem), NULL);

  priv = GET_PRIVATE (menuitem);

  return priv->primary_image;
}

/**
 * ido_scale_menu_item_get_secondary_image:
 * @menuitem: The #IdoScaleMenuItem
 *
 * Retrieves a pointer to the image widget used in the secondary slot.
 * Whether this is visible depends upon the return value from
 * ido_scale_menu_item_get_style().
 *
 * Return Value: (transfer none): A #GtkWidget pointer for the secondary image.
 **/
GtkWidget *
ido_scale_menu_item_get_secondary_image (IdoScaleMenuItem *menuitem)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_val_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem), NULL);

  priv = GET_PRIVATE (menuitem);

  return priv->secondary_image;
}

/**
 * ido_scale_menu_item_set_icons:
 * @item: a #IdoScaleMenuItem
 * @primary-icon: (allow-none): the primary icon, or %NULL
 * @secondary-icon: (allow-none): the secondary icon, %NULL
 *
 * Sets the icons of @item to  @primary_icon and @secondary_icon.
 * Pass %NULL for either of them to unset the icon.
 */
static void
ido_scale_menu_item_set_icons (IdoScaleMenuItem *item,
                               GIcon            *primary_icon,
                               GIcon            *secondary_icon)
{
  GtkWidget *primary;
  GtkWidget *secondary;

  primary = ido_scale_menu_item_get_primary_image (item);
  secondary = ido_scale_menu_item_get_secondary_image (item);

  if (primary_icon)
    gtk_image_set_from_gicon (GTK_IMAGE (primary), primary_icon, GTK_ICON_SIZE_MENU);
  else
    gtk_image_clear (GTK_IMAGE (primary));

  if (secondary_icon)
    gtk_image_set_from_gicon (GTK_IMAGE (secondary), secondary_icon, GTK_ICON_SIZE_MENU);
  else
    gtk_image_clear (GTK_IMAGE (secondary));
}

/**
 * ido_scale_menu_item_get_primary_label:
 * @menuitem: The #IdoScaleMenuItem
 *
 * Retrieves a string of the text for the primary label widget.
 * Whether this is visible depends upon the return value from
 * ido_scale_menu_item_get_style().
 *
 * Return Value: The label text.
 **/
const gchar*
ido_scale_menu_item_get_primary_label (IdoScaleMenuItem *menuitem)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_val_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem), NULL);

  priv = GET_PRIVATE (menuitem);

  return gtk_label_get_text (GTK_LABEL (priv->primary_label));
}

/**
 * ido_scale_menu_item_get_secondary_label:
 * @menuitem: The #IdoScaleMenuItem
 *
 * Retrieves a string of the text for the secondary label widget.
 * Whether this is visible depends upon the return value from
 * ido_scale_menu_item_get_style().
 *
 * Return Value: The label text.
 **/
const gchar*
ido_scale_menu_item_get_secondary_label (IdoScaleMenuItem *menuitem)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_val_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem), NULL);

  priv = GET_PRIVATE (menuitem);

  return gtk_label_get_text (GTK_LABEL (priv->secondary_label));
}

/**
 * ido_scale_menu_item_set_primary_label:
 * @menuitem: The #IdoScaleMenuItem
 * @label: The label text
 *
 * Sets the text for the label widget in the primary slot.  This
 * widget will only be visibile if the return value of
 * ido_scale_menu_item_get_style() is set to %IDO_SCALE_MENU_ITEM_STYLE_LABEL.
 **/
void
ido_scale_menu_item_set_primary_label (IdoScaleMenuItem *menuitem,
                                       const gchar      *label)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem));

  priv = GET_PRIVATE (menuitem);

  if (priv->primary_label)
    {
      gtk_label_set_text (GTK_LABEL (priv->primary_label), label);
    }
}

/**
 * ido_scale_menu_item_set_secondary_label:
 * @menuitem: The #IdoScaleMenuItem
 * @label: The label text
 *
 * Sets the text for the label widget in the secondary slot.  This
 * widget will only be visibile if the return value of
 * ido_scale_menu_item_get_style() is set to %IDO_SCALE_MENU_ITEM_STYLE_LABEL.
 **/
void
ido_scale_menu_item_set_secondary_label (IdoScaleMenuItem *menuitem,
                                         const gchar      *label)
{
  IdoScaleMenuItemPrivate *priv;

  g_return_if_fail (IDO_IS_SCALE_MENU_ITEM (menuitem));

  priv = GET_PRIVATE (menuitem);

  if (priv->secondary_label)
    {
      gtk_label_set_text (GTK_LABEL (priv->secondary_label), label);
    }
}

/**
 * ido_scale_menu_item_primary_clicked:
 * @menuitem: the #IdoScaleMenuItem
 *
 * Emits the "primary-clicked" signal.
 *
 * The default handler for this signal lowers the scale's
 * adjustment to its lower bound.
 */
void
ido_scale_menu_item_primary_clicked (IdoScaleMenuItem * menuitem)
{
  g_signal_emit (menuitem, signals[PRIMARY_CLICKED], 0);
}
static void
default_primary_clicked_handler (IdoScaleMenuItem * item)
{
  g_debug ("%s: setting scale to lower bound", G_STRFUNC);
  IdoScaleMenuItemPrivate * priv = GET_PRIVATE (item);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (priv->scale));
  gtk_adjustment_set_value (adj, gtk_adjustment_get_lower (adj));
}

/**
 * ido_scale_menu_item_secondary_clicked:
 * @menuitem: the #IdoScaleMenuItem
 *
 * Emits the "secondary-clicked" signal.
 *
 * The default handler for this signal raises the scale's
 * adjustment to its upper bound.
 */
void
ido_scale_menu_item_secondary_clicked (IdoScaleMenuItem * menuitem)
{
  g_signal_emit (menuitem, signals[SECONDARY_CLICKED], 0);
}
static void
default_secondary_clicked_handler (IdoScaleMenuItem * item)
{
  g_debug ("%s: setting scale to upper bound", G_STRFUNC);
  IdoScaleMenuItemPrivate * priv = GET_PRIVATE (item);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (priv->scale));
  gtk_adjustment_set_value (adj, gtk_adjustment_get_upper (adj));
}

/* ido_scale_menu_item_set_value:
 *
 * Sets the value of the scale inside @item to @value, without emitting
 * "value-changed".
 */
static void
ido_scale_menu_item_set_value (IdoScaleMenuItem *item,
                               gdouble           value)
{
  IdoScaleMenuItemPrivate *priv = GET_PRIVATE (item);

  /* set ignore_value_changed to signify to the scale menu item that it
   * should not emit its own value-changed signal, as that should only
   * be emitted when the value is changed by the user. */

  priv->ignore_value_changed = TRUE;
  gtk_range_set_value (GTK_RANGE (priv->scale), value);
  priv->ignore_value_changed = FALSE;
}

/**
 * ido_scale_menu_item_state_changed:
 *
 * Updates a IdoScaleMenuItem from @state. State must be a double which
 * contains the current value of the slider.
 */
static void
ido_scale_menu_item_state_changed (IdoActionHelper *helper,
                                   GVariant        *state,
                                   gpointer         user_data)
{
  GtkWidget *menuitem;

  menuitem = ido_action_helper_get_widget (helper);
  ido_scale_menu_item_set_value (IDO_SCALE_MENU_ITEM (menuitem), g_variant_get_double (state));
}

static void
ido_scale_menu_item_value_changed (GtkScale *scale,
                                   gdouble   value,
                                   gpointer  user_data)
{
  IdoActionHelper *helper = user_data;

  ido_action_helper_change_action_state (helper, g_variant_new_double (value));
}

static GIcon *
menu_item_get_icon (GMenuItem   *menuitem,
                    const gchar *attribute)
{
  GVariant *value;

  value = g_menu_item_get_attribute_value (menuitem, attribute, NULL);

  return value ? g_icon_deserialize (value) : NULL;
}

/**
 * ido_scale_menu_item_new_from_model:
 *
 * Creates a new #IdoScaleMenuItem. If @menuitem contains an action, it
 * will be bound to that action in @actions.
 *
 * Returns: (transfer full): a new #IdoScaleMenuItem
 */
GtkMenuItem *
ido_scale_menu_item_new_from_model (GMenuItem    *menuitem,
                                    GActionGroup *actions)
{
  GtkWidget *item;
  gchar *action;
  gdouble min = 0.0;
  gdouble max = 100.0;
  gdouble step = 1.0;
  GIcon *min_icon;
  GIcon *max_icon;

  g_menu_item_get_attribute (menuitem, "min-value", "d", &min);
  g_menu_item_get_attribute (menuitem, "max-value", "d", &max);
  g_menu_item_get_attribute (menuitem, "step", "d", &step);

  item = ido_scale_menu_item_new_with_range ("Volume", IDO_RANGE_STYLE_DEFAULT, 0.0, min, max, step);
  ido_scale_menu_item_set_style (IDO_SCALE_MENU_ITEM (item), IDO_SCALE_MENU_ITEM_STYLE_IMAGE);

  if (g_menu_item_get_attribute (menuitem, "action", "s", &action))
    {
      IdoActionHelper *helper;

      helper = ido_action_helper_new (item, actions, action, NULL);
      g_signal_connect (helper, "action-state-changed",
                        G_CALLBACK (ido_scale_menu_item_state_changed), NULL);

      g_signal_connect (item, "value-changed", G_CALLBACK (ido_scale_menu_item_value_changed), helper);
      g_signal_connect_swapped (item, "destroy", G_CALLBACK (g_object_unref), helper);

      g_free (action);
    }

  min_icon = menu_item_get_icon (menuitem, "min-icon");
  max_icon = menu_item_get_icon (menuitem, "max-icon");
  ido_scale_menu_item_set_icons (IDO_SCALE_MENU_ITEM (item), min_icon, max_icon);

  if (min_icon)
    g_object_unref (min_icon);
  if (max_icon)
  g_object_unref (max_icon);

  return GTK_MENU_ITEM (item);
}

#define __IDO_SCALE_MENU_ITEM_C__
