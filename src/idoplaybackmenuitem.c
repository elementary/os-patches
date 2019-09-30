/*
 * Copyright 2013 Canonical Ltd.
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
 *     Conor Curran <conor.curran@canonical.com>
 *     Mirco Müller <mirco.mueller@canonical.com>
 *     Andrea Cimitan <andrea.cimitan@canonical.com>
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#include "config.h"

#include "idoplaybackmenuitem.h"

#include <gdk/gdkkeysyms.h>
#include <math.h>

#define RECT_WIDTH 130.0f
#define Y 7.0f
#define INNER_RADIUS 12.5
#define MIDDLE_RADIUS 13.0f
#define OUTER_RADIUS  14.5f
#define CIRCLE_RADIUS 21.0f
#define PREV_WIDTH  25.0f
#define PREV_HEIGHT 17.0f
#define NEXT_WIDTH  25.0f //PREV_WIDTH
#define NEXT_HEIGHT 17.0f //PREV_HEIGHT
#define TRI_WIDTH  11.0f
#define TRI_HEIGHT 13.0f
#define TRI_OFFSET  6.0f
#define PREV_X -2.0f
#define PREV_Y 13.0f
#define NEXT_X 76.0f //prev_y
#define NEXT_Y 13.0f //prev_y
#define PAUSE_WIDTH 21.0f
#define PAUSE_HEIGHT 27.0f
#define BAR_WIDTH 4.5f
#define BAR_HEIGHT 24.0f
#define BAR_OFFSET 10.0f
#define PAUSE_X 41.0f
#define PAUSE_Y 7.0f
#define PLAY_WIDTH 28.0f
#define PLAY_HEIGHT 29.0f
#define PLAY_PADDING 5.0f
#define INNER_START_SHADE 0.98
#define INNER_END_SHADE 0.98
#define MIDDLE_START_SHADE 1.0
#define MIDDLE_END_SHADE 1.0
#define OUTER_START_SHADE 0.75
#define OUTER_END_SHADE 1.3
#define SHADOW_BUTTON_SHADE 0.8
#define OUTER_PLAY_START_SHADE 0.7
#define OUTER_PLAY_END_SHADE 1.38
#define BUTTON_START_SHADE 1.1
#define BUTTON_END_SHADE 0.9
#define BUTTON_SHADOW_SHADE 0.8
#define INNER_COMPRESSED_START_SHADE 1.0
#define INNER_COMPRESSED_END_SHADE 1.0

typedef enum
{
  STATE_PAUSED,
  STATE_PLAYING,
  STATE_LAUNCHING
} State;

typedef enum
{
  BUTTON_NONE,
  BUTTON_PREVIOUS,
  BUTTON_PLAYPAUSE,
  BUTTON_NEXT,
  N_BUTTONS
} Button;

typedef GtkMenuItemClass IdoPlaybackMenuItemClass;

struct _IdoPlaybackMenuItem
{
  GtkMenuItem parent;

  State current_state;
  Button cur_pushed_button;
  Button cur_hover_button;
  gboolean has_focus;
  gboolean keyboard_activated; /* TRUE if the current button was activated with a key */

  GActionGroup *action_group;
  gchar *button_actions[N_BUTTONS];
};

G_DEFINE_TYPE (IdoPlaybackMenuItem, ido_playback_menu_item, GTK_TYPE_MENU_ITEM);

static gboolean ido_playback_menu_item_draw (GtkWidget* button, cairo_t *cr);

static void
ido_playback_menu_item_dispose (GObject *object)
{
  IdoPlaybackMenuItem *item = IDO_PLAYBACK_MENU_ITEM (object);

  if (item->action_group)
    {
      g_signal_handlers_disconnect_by_data (item->action_group, item);
      g_clear_object (&item->action_group);
    }

  G_OBJECT_CLASS (ido_playback_menu_item_parent_class)->dispose (object);
}

static void
ido_playback_menu_item_finalize (GObject *object)
{
  IdoPlaybackMenuItem *item = IDO_PLAYBACK_MENU_ITEM (object);
  gint i;

  for (i = 0; i < N_BUTTONS; i++)
    g_free (item->button_actions[i]);

  G_OBJECT_CLASS (ido_playback_menu_item_parent_class)->finalize (object);
}

static Button
ido_playback_menu_item_get_button_at_pos (GtkWidget *item,
                                          gint       x,
                                          gint       y)
{
  GtkAllocation alloc;
  gint left;

  /*     0    44      86    130
   *  5        +------+
   * 12  +-----+      +-----+
   *     |prev   play   next|
   * 40  +-----+      +-----+
   * 47        +------+
   */

  gtk_widget_get_allocation (item, &alloc);
  left = alloc.x + (alloc.width - RECT_WIDTH) / 2;

  if (x > left && x < left + 44 && y > 12 && y < 40)
    return BUTTON_PREVIOUS;

  if (x > left + 44 && x < left + 86 && y > 5 && y < 47)
    return BUTTON_PLAYPAUSE;

  if (x > left + 86 && x < left + 130 && y > 12 && y < 40)
    return BUTTON_NEXT;

  return BUTTON_NONE;
}

static gboolean
ido_playback_menu_item_parent_key_press_event (GtkWidget   *widget,
                                               GdkEventKey *event,
                                               gpointer     user_data)
{
  IdoPlaybackMenuItem *self = user_data;

  /* only listen to events when the playback menu item is selected */
  if (!self->has_focus)
    return FALSE;

  switch (event->keyval)
    {
    case GDK_KEY_Left:
      self->cur_pushed_button = BUTTON_PREVIOUS;
      break;

    case GDK_KEY_Right:
      self->cur_pushed_button = BUTTON_NEXT;
      break;

    case GDK_KEY_space:
      if (self->cur_hover_button != BUTTON_NONE)
        self->cur_pushed_button = self->cur_hover_button;
      else
        self->cur_pushed_button = BUTTON_PLAYPAUSE;
      break;

    default:
      self->cur_pushed_button = BUTTON_NONE;
    }

  if (self->cur_pushed_button != BUTTON_NONE)
    {
      const gchar *action = self->button_actions[self->cur_pushed_button];

      if (self->action_group && action)
        g_action_group_activate_action (self->action_group, action, NULL);

      self->keyboard_activated = TRUE;
      gtk_widget_queue_draw (widget);
      return TRUE;
    }

  return FALSE;
}

static gboolean
ido_playback_menu_item_parent_key_release_event (GtkWidget   *widget,
                                                 GdkEventKey *event,
                                                 gpointer     user_data)
{
  IdoPlaybackMenuItem *self = user_data;

  switch (event->keyval)
    {
    case GDK_KEY_Left:
    case GDK_KEY_Right:
    case GDK_KEY_space:
      self->cur_pushed_button = BUTTON_NONE;
      self->keyboard_activated = FALSE;
      gtk_widget_queue_draw (widget);
      break;
    }

  return FALSE;
}

static void
ido_playback_menu_item_parent_set (GtkWidget *widget,
                                   GtkWidget *old_parent)
{
  GtkWidget *parent;

  /* Menus don't pass key events to their children. This works around
   * that by listening to key events on the parent widget. */

  if (old_parent)
    {
      g_signal_handlers_disconnect_by_func (old_parent, ido_playback_menu_item_parent_key_press_event, widget);
      g_signal_handlers_disconnect_by_func (old_parent, ido_playback_menu_item_parent_key_release_event, widget);
    }

  parent = gtk_widget_get_parent (widget);
  if (parent)
    {
      g_signal_connect (parent, "key-press-event",
                        G_CALLBACK (ido_playback_menu_item_parent_key_press_event), widget);
      g_signal_connect (parent, "key-release-event",
                        G_CALLBACK (ido_playback_menu_item_parent_key_release_event), widget);
    }
}

static void
ido_playback_menu_item_select (GtkMenuItem *item)
{
  IdoPlaybackMenuItem *self = IDO_PLAYBACK_MENU_ITEM (item);

  self->has_focus = TRUE;

  GTK_MENU_ITEM_CLASS (ido_playback_menu_item_parent_class)->select (item);
}

static void
ido_playback_menu_item_deselect (GtkMenuItem *item)
{
  IdoPlaybackMenuItem *self = IDO_PLAYBACK_MENU_ITEM (item);

  self->has_focus = FALSE;

  GTK_MENU_ITEM_CLASS (ido_playback_menu_item_parent_class)->deselect (item);
}

static gboolean
ido_playback_menu_item_button_press_event (GtkWidget      *menuitem,
                                           GdkEventButton *event)
{
  IdoPlaybackMenuItem *item = IDO_PLAYBACK_MENU_ITEM (menuitem);

  item->cur_pushed_button = ido_playback_menu_item_get_button_at_pos (menuitem, event->x, event->y);
  gtk_widget_queue_draw (menuitem);

  return TRUE;
}

static gboolean
ido_playback_menu_item_button_release_event (GtkWidget      *menuitem,
                                             GdkEventButton *event)
{
  IdoPlaybackMenuItem *item = IDO_PLAYBACK_MENU_ITEM (menuitem);
  Button button;
  const gchar *action = action;

  button = ido_playback_menu_item_get_button_at_pos (menuitem, event->x, event->y);
  if (button != item->cur_pushed_button)
    button = BUTTON_NONE;

  action = item->button_actions[item->cur_pushed_button];
  if (item->action_group && action)
    g_action_group_activate_action (item->action_group, action, NULL);

  item->cur_pushed_button = BUTTON_NONE;
  gtk_widget_queue_draw (menuitem);

  return TRUE;
}

static gboolean
ido_playback_menu_item_motion_notify_event (GtkWidget      *menuitem,
                                            GdkEventMotion *event)
{
  IdoPlaybackMenuItem *item = IDO_PLAYBACK_MENU_ITEM (menuitem);

  item->cur_hover_button = ido_playback_menu_item_get_button_at_pos (menuitem, event->x, event->y);
  gtk_widget_queue_draw (menuitem);

  return TRUE;
}

static gboolean
ido_playback_menu_item_leave_notify_event (GtkWidget        *menuitem,
                                           GdkEventCrossing *event)
{
  IdoPlaybackMenuItem *item = IDO_PLAYBACK_MENU_ITEM (menuitem);

  item->cur_pushed_button = BUTTON_NONE;
  item->cur_hover_button = BUTTON_NONE;
  gtk_widget_queue_draw (GTK_WIDGET(menuitem));

  return TRUE;
}

static void
ido_playback_menu_item_class_init (IdoPlaybackMenuItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkMenuItemClass *menuitem_class = GTK_MENU_ITEM_CLASS (klass);

  gobject_class->dispose = ido_playback_menu_item_dispose;
  gobject_class->finalize = ido_playback_menu_item_finalize;

  widget_class->button_press_event = ido_playback_menu_item_button_press_event;
  widget_class->button_release_event = ido_playback_menu_item_button_release_event;
  widget_class->motion_notify_event = ido_playback_menu_item_motion_notify_event;
  widget_class->leave_notify_event = ido_playback_menu_item_leave_notify_event;
  widget_class->parent_set = ido_playback_menu_item_parent_set;
  widget_class->draw = ido_playback_menu_item_draw;

  menuitem_class->select = ido_playback_menu_item_select;
  menuitem_class->deselect = ido_playback_menu_item_deselect;
}

static void
ido_playback_menu_item_init (IdoPlaybackMenuItem *self)
{
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), GTK_STYLE_CLASS_SPINNER);

  gtk_widget_set_size_request (GTK_WIDGET (self), 200, 43);
}

static void
ido_playback_menu_item_set_state (IdoPlaybackMenuItem *self,
                                  State                state)
{
  self->current_state = state;

  if (self->current_state == STATE_LAUNCHING)
    gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
ido_playback_menu_item_set_state_from_string (IdoPlaybackMenuItem *self,
                                              const gchar         *state)
{
  g_return_if_fail (state != NULL);

  if (g_str_equal (state, "Playing"))
    ido_playback_menu_item_set_state (self, STATE_PLAYING);
  else if (g_str_equal (state, "Launching"))
    ido_playback_menu_item_set_state (self, STATE_LAUNCHING);
  else /* "Paused" and fallback */
    ido_playback_menu_item_set_state (self, STATE_PAUSED);
}

static void
ido_playback_menu_item_action_added (GActionGroup *action_group,
                                     const gchar  *action_name,
                                     gpointer      user_data)
{
  IdoPlaybackMenuItem *self = user_data;
  const gchar *action;

  action = self->button_actions[BUTTON_PLAYPAUSE];
  if (action && g_str_equal (action_name, action))
    {
      GVariant *state;

      state = g_action_group_get_action_state (action_group, action);
      if (g_variant_is_of_type (state, G_VARIANT_TYPE_STRING))
        ido_playback_menu_item_set_state_from_string (self, g_variant_get_string (state, NULL));

      g_variant_unref (state);
    }
}

static void
ido_playback_menu_item_action_removed (GActionGroup *action_group,
                                       const gchar  *action_name,
                                       gpointer      user_data)
{
  IdoPlaybackMenuItem *self = user_data;
  const gchar *action;

  action = self->button_actions[BUTTON_PLAYPAUSE];
  if (action && g_str_equal (action_name, action))
    ido_playback_menu_item_set_state (self, STATE_PAUSED);
}

static void
ido_playback_menu_item_action_state_changed (GActionGroup *action_group,
                                             const gchar  *action_name,
                                             GVariant     *value,
                                             gpointer      user_data)
{
  IdoPlaybackMenuItem *self = user_data;
  const gchar *action;

  g_return_if_fail (action_name != NULL);

  action = self->button_actions[BUTTON_PLAYPAUSE];

  if (action && g_str_equal (action_name, action))
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        ido_playback_menu_item_set_state_from_string (self, g_variant_get_string (value, NULL));
    }
}

GtkMenuItem *
ido_playback_menu_item_new_from_model (GMenuItem    *item,
                                       GActionGroup *actions)
{
  IdoPlaybackMenuItem *widget;
  gchar *play_action;

  widget = g_object_new (IDO_TYPE_PLAYBACK_MENU_ITEM, NULL);

  widget->action_group = g_object_ref (actions);
  g_signal_connect (actions, "action-state-changed", G_CALLBACK (ido_playback_menu_item_action_state_changed), widget);
  g_signal_connect (actions, "action-added", G_CALLBACK (ido_playback_menu_item_action_added), widget);
  g_signal_connect (actions, "action-removed", G_CALLBACK (ido_playback_menu_item_action_removed), widget);

  g_menu_item_get_attribute (item, "x-canonical-play-action", "s", &widget->button_actions[BUTTON_PLAYPAUSE]);
  g_menu_item_get_attribute (item, "x-canonical-next-action", "s", &widget->button_actions[BUTTON_NEXT]);
  g_menu_item_get_attribute (item, "x-canonical-previous-action", "s", &widget->button_actions[BUTTON_PREVIOUS]);

  play_action = widget->button_actions[BUTTON_PLAYPAUSE];
  if (play_action && g_action_group_has_action (actions, play_action))
    ido_playback_menu_item_action_added (actions, play_action, widget);

  return GTK_MENU_ITEM (widget);
}


/*
 * Drawing
 */

typedef struct
{
  double r;
  double g;
  double b;
} CairoColorRGB;

static void
draw_gradient (cairo_t* cr,
               double   x,
               double   y,
               double   w,
               double   r,
               double*  rgba_start,
               double*  rgba_end)
{
  cairo_pattern_t* pattern = NULL;

  cairo_move_to (cr, x, y);
  cairo_line_to (cr, x + w - 2.0f * r, y);
  cairo_arc (cr,
       x + w - 2.0f * r,
       y + r,
       r,
       -90.0f * G_PI / 180.0f,
       90.0f * G_PI / 180.0f);
  cairo_line_to (cr, x, y + 2.0f * r);
  cairo_arc (cr,
       x,
       y + r,
       r,
       90.0f * G_PI / 180.0f,
       270.0f * G_PI / 180.0f);
  cairo_close_path (cr);

  pattern = cairo_pattern_create_linear (x, y, x, y + 2.0f * r);
  cairo_pattern_add_color_stop_rgba (pattern,
                                     0.0f,
                                     rgba_start[0],
                                     rgba_start[1],
                                     rgba_start[2],
                                     rgba_start[3]);
  cairo_pattern_add_color_stop_rgba (pattern,
                                     1.0f,
                                     rgba_end[0],
                                     rgba_end[1],
                                     rgba_end[2],
                                     rgba_end[3]);
  cairo_set_source (cr, pattern);
  cairo_fill (cr);
  cairo_pattern_destroy (pattern);
}

static void
draw_circle (cairo_t* cr,
       double   x,
       double   y,
       double   r,
       double*  rgba_start,
       double*  rgba_end)
{
  cairo_pattern_t* pattern = NULL;

  cairo_move_to (cr, x, y);
  cairo_arc (cr,
       x + r,
       y + r,
       r,
       0.0f * G_PI / 180.0f,
       360.0f * G_PI / 180.0f);

  pattern = cairo_pattern_create_linear (x, y, x, y + 2.0f * r);
  cairo_pattern_add_color_stop_rgba (pattern,
                                     0.0f,
                                     rgba_start[0],
                                     rgba_start[1],
                                     rgba_start[2],
                                     rgba_start[3]);
  cairo_pattern_add_color_stop_rgba (pattern,
                                     1.0f,
                                     rgba_end[0],
                                     rgba_end[1],
                                     rgba_end[2],
                                     rgba_end[3]);
  cairo_set_source (cr, pattern);
  cairo_fill (cr);
  cairo_pattern_destroy (pattern);
}

static void
_setup (cairo_t**         cr,
  cairo_surface_t** surf,
  gint              width,
  gint              height)
{
  if (!cr || !surf)
    return;

  *surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  *cr = cairo_create (*surf);
  cairo_scale (*cr, 1.0f, 1.0f);
  cairo_set_operator (*cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (*cr);
  cairo_set_operator (*cr, CAIRO_OPERATOR_OVER);
}

static void
_mask_prev (cairo_t* cr,
      double   x,
      double   y,
      double   tri_width,
      double   tri_height,
      double   tri_offset)
{
  if (!cr)
    return;

  cairo_move_to (cr, x,             y + tri_height / 2.0f);
  cairo_line_to (cr, x + tri_width, y);
  cairo_line_to (cr, x + tri_width, y + tri_height);
  x += tri_offset;
  cairo_move_to (cr, x,             y + tri_height / 2.0f);
  cairo_line_to (cr, x + tri_width, y);
  cairo_line_to (cr, x + tri_width, y + tri_height);
  x -= tri_offset;
  cairo_rectangle (cr, x, y, 2.5f, tri_height);
  cairo_close_path (cr);
}

static void
_mask_next (cairo_t* cr,
      double   x,
      double   y,
      double   tri_width,
      double   tri_height,
      double   tri_offset)
{
  if (!cr)
    return;

  cairo_move_to (cr, x,             y);
  cairo_line_to (cr, x + tri_width, y + tri_height / 2.0f);
  cairo_line_to (cr, x,             y + tri_height);
  x += tri_offset;
  cairo_move_to (cr, x,             y);
  cairo_line_to (cr, x + tri_width, y + tri_height / 2.0f);
  cairo_line_to (cr, x,             y + tri_height);
  x -= tri_offset;
  x += 2.0f * tri_width - tri_offset - 1.0f;
  cairo_rectangle (cr, x, y, 2.5f, tri_height);

  cairo_close_path (cr);
}

static void
_mask_pause (cairo_t* cr,
       double   x,
       double   y,
       double   bar_width,
       double   bar_height,
       double   bar_offset)
{
  if (!cr)
    return;

  cairo_set_line_width (cr, bar_width);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

  x += bar_width;
  y += bar_width;
  cairo_move_to (cr, x,              y);
  cairo_line_to (cr, x,              y + bar_height);
  cairo_move_to (cr, x + bar_offset, y);
  cairo_line_to (cr, x + bar_offset, y + bar_height);

}

static void
_mask_play (cairo_t* cr,
       double   x,
       double   y,
       double   tri_width,
       double   tri_height)
{
  if (!cr)
    return;

  cairo_move_to (cr, x,             y);
  cairo_line_to (cr, x + tri_width, y + tri_height / 2.0f);
  cairo_line_to (cr, x,             y + tri_height);
  cairo_close_path (cr);

}

static void
_fill (cairo_t* cr,
       double   x_start,
       double   y_start,
       double   x_end,
       double   y_end,
       double*  rgba_start,
       double*  rgba_end,
       gboolean stroke)
{
  cairo_pattern_t* pattern = NULL;

  if (!cr || !rgba_start || !rgba_end)
    return;

  pattern = cairo_pattern_create_linear (x_start, y_start, x_end, y_end);
  cairo_pattern_add_color_stop_rgba (pattern,
             0.0f,
             rgba_start[0],
             rgba_start[1],
             rgba_start[2],
             rgba_start[3]);
  cairo_pattern_add_color_stop_rgba (pattern,
             1.0f,
             rgba_end[0],
             rgba_end[1],
             rgba_end[2],
             rgba_end[3]);
  cairo_set_source (cr, pattern);
  if (stroke)
    cairo_stroke (cr);
  else
    cairo_fill (cr);
  cairo_pattern_destroy (pattern);
}

static void
_finalize (cairo_t*          cr,
     cairo_t**         cr_surf,
     cairo_surface_t** surf,
     double            x,
     double            y)
{
  if (!cr || !cr_surf || !surf)
    return;

  cairo_set_source_surface (cr, *surf, x, y);
  cairo_paint (cr);
  cairo_surface_destroy (*surf);
  cairo_destroy (*cr_surf);
}

static void
_finalize_repaint (cairo_t*          cr,
             cairo_t**         cr_surf,
             cairo_surface_t** surf,
             double            x,
             double            y,
             int               repaints)
{
  if (!cr || !cr_surf || !surf)
    return;

  while (repaints > 0)
  {
    cairo_set_source_surface (cr, *surf, x, y);
    cairo_paint (cr);
    repaints--;
  }

  cairo_surface_destroy (*surf);
  cairo_destroy (*cr_surf);
}

static void
_color_rgb_to_hls (gdouble *r,
                   gdouble *g,
                   gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h = 0;
  gdouble l;
  gdouble s;
  gdouble delta;

  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
  {
    if (red > blue)
      max = red;
    else
      max = blue;

    if (green < blue)
      min = green;
    else
    min = blue;
  }
  else
  {
    if (green > blue)
      max = green;
    else
    max = blue;

    if (red < blue)
      min = red;
    else
      min = blue;
  }
  l = (max+min)/2;
  if (fabs (max-min) < 0.0001)
  {
    h = 0;
    s = 0;
  }
  else
  {
    if (l <= 0.5)
    s = (max-min)/(max+min);
    else
    s = (max-min)/(2-max-min);

    delta = (max -min) != 0 ? (max -min) : 1;

    if(delta == 0)
      delta = 1;
    if (red == max)
      h = (green-blue)/delta;
    else if (green == max)
      h = 2+(blue-red)/delta;
    else if (blue == max)
      h = 4+(red-green)/delta;

    h *= 60;
    if (h < 0.0)
      h += 360;
  }

  *r = h;
  *g = l;
  *b = s;
}

static void
_color_hls_to_rgb (gdouble *h,
                   gdouble *l,
                   gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;

  lightness = *l;
  saturation = *s;

  if (lightness <= 0.5)
    m2 = lightness*(1+saturation);
  else
    m2 = lightness+saturation-lightness*saturation;

  m1 = 2*lightness-m2;

  if (saturation == 0)
  {
    *h = lightness;
    *l = lightness;
    *s = lightness;
  }
  else
  {
    hue = *h+120;
    while (hue > 360)
      hue -= 360;
    while (hue < 0)
      hue += 360;

    if (hue < 60)
      r = m1+(m2-m1)*hue/60;
    else if (hue < 180)
      r = m2;
    else if (hue < 240)
      r = m1+(m2-m1)*(240-hue)/60;
    else
      r = m1;

    hue = *h;
    while (hue > 360)
      hue -= 360;
    while (hue < 0)
      hue += 360;

    if (hue < 60)
      g = m1+(m2-m1)*hue/60;
    else if (hue < 180)
      g = m2;
    else if (hue < 240)
      g = m1+(m2-m1)*(240-hue)/60;
    else
      g = m1;

    hue = *h-120;
    while (hue > 360)
      hue -= 360;
    while (hue < 0)
      hue += 360;

    if (hue < 60)
      b = m1+(m2-m1)*hue/60;
    else if (hue < 180)
      b = m2;
    else if (hue < 240)
      b = m1+(m2-m1)*(240-hue)/60;
    else
      b = m1;

    *h = r;
    *l = g;
    *s = b;
  }
}

static void
_color_shade (const CairoColorRGB *a, float k, CairoColorRGB *b)
{
  double red;
  double green;
  double blue;

  red   = a->r;
  green = a->g;
  blue  = a->b;

  if (k == 1.0)
  {
    b->r = red;
    b->g = green;
    b->b = blue;
    return;
  }

  _color_rgb_to_hls (&red, &green, &blue);

  green *= k;
  if (green > 1.0)
    green = 1.0;
  else if (green < 0.0)
    green = 0.0;

  blue *= k;
  if (blue > 1.0)
    blue = 1.0;
  else if (blue < 0.0)
    blue = 0.0;

  _color_hls_to_rgb (&red, &green, &blue);

  b->r = red;
  b->g = green;
  b->b = blue;
}

static inline void
_blurinner (guchar* pixel,
      gint*   zR,
      gint*   zG,
      gint*   zB,
      gint*   zA,
      gint    alpha,
      gint    aprec,
      gint    zprec)
{
  gint R;
  gint G;
  gint B;
  guchar A;

  R = *pixel;
  G = *(pixel + 1);
  B = *(pixel + 2);
  A = *(pixel + 3);

  *zR += (alpha * ((R << zprec) - *zR)) >> aprec;
  *zG += (alpha * ((G << zprec) - *zG)) >> aprec;
  *zB += (alpha * ((B << zprec) - *zB)) >> aprec;
  *zA += (alpha * ((A << zprec) - *zA)) >> aprec;

  *pixel       = *zR >> zprec;
  *(pixel + 1) = *zG >> zprec;
  *(pixel + 2) = *zB >> zprec;
  *(pixel + 3) = *zA >> zprec;
}

static inline void
_blurrow (guchar* pixels,
    gint    width,
    gint    height,
    gint    channels,
    gint    line,
    gint    alpha,
    gint    aprec,
    gint    zprec)
{
  gint    zR;
  gint    zG;
  gint    zB;
  gint    zA;
  gint    index;
  guchar* scanline;

  scanline = &(pixels[line * width * channels]);

  zR = *scanline << zprec;
  zG = *(scanline + 1) << zprec;
  zB = *(scanline + 2) << zprec;
  zA = *(scanline + 3) << zprec;

  for (index = 0; index < width; index ++)
    _blurinner (&scanline[index * channels],
          &zR,
          &zG,
          &zB,
          &zA,
          alpha,
          aprec,
          zprec);

  for (index = width - 2; index >= 0; index--)
    _blurinner (&scanline[index * channels],
          &zR,
          &zG,
          &zB,
          &zA,
          alpha,
          aprec,
          zprec);
}

static inline void
_blurcol (guchar* pixels,
    gint    width,
    gint    height,
    gint    channels,
    gint    x,
    gint    alpha,
    gint    aprec,
    gint    zprec)
{
  gint zR;
  gint zG;
  gint zB;
  gint zA;
  gint index;
  guchar* ptr;

  ptr = pixels;

  ptr += x * channels;

  zR = *((guchar*) ptr    ) << zprec;
  zG = *((guchar*) ptr + 1) << zprec;
  zB = *((guchar*) ptr + 2) << zprec;
  zA = *((guchar*) ptr + 3) << zprec;

  for (index = width; index < (height - 1) * width; index += width)
    _blurinner ((guchar*) &ptr[index * channels],
          &zR,
          &zG,
          &zB,
          &zA,
          alpha,
          aprec,
          zprec);

  for (index = (height - 2) * width; index >= 0; index -= width)
    _blurinner ((guchar*) &ptr[index * channels],
          &zR,
          &zG,
          &zB,
          &zA,
          alpha,
          aprec,
          zprec);
}

static void
_expblur (guchar* pixels,
    gint    width,
    gint    height,
    gint    channels,
    gint    radius,
    gint    aprec,
    gint    zprec)
{
  gint alpha;
  gint row = 0;
  gint col = 0;

  if (radius < 1)
    return;

  // calculate the alpha such that 90% of
  // the kernel is within the radius.
  // (Kernel extends to infinity)
  alpha = (gint) ((1 << aprec) * (1.0f - expf (-2.3f / (radius + 1.f))));

  for (; row < height; row++)
    _blurrow (pixels,
        width,
        height,
        channels,
        row,
        alpha,
        aprec,
        zprec);

  for(; col < width; col++)
    _blurcol (pixels,
        width,
        height,
        channels,
        col,
        alpha,
        aprec,
        zprec);

  return;
}

static void
_surface_blur (cairo_surface_t* surface,
               guint            radius)
{
  guchar*        pixels;
  guint          width;
  guint          height;
  cairo_format_t format;

  // before we mess with the surface execute any pending drawing
  cairo_surface_flush (surface);

  pixels = cairo_image_surface_get_data (surface);
  width  = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);
  format = cairo_image_surface_get_format (surface);

  switch (format)
  {
    case CAIRO_FORMAT_ARGB32:
      _expblur (pixels, width, height, 4, radius, 16, 7);
    break;

    case CAIRO_FORMAT_RGB24:
      _expblur (pixels, width, height, 3, radius, 16, 7);
    break;

    case CAIRO_FORMAT_A8:
      _expblur (pixels, width, height, 1, radius, 16, 7);
    break;

    default :
      // do nothing
    break;
  }

  // inform cairo we altered the surfaces contents
  cairo_surface_mark_dirty (surface);
}

static gboolean
ido_playback_menu_item_draw (GtkWidget* button, cairo_t *cr)
{
  IdoPlaybackMenuItem *item = IDO_PLAYBACK_MENU_ITEM (button);
  GtkAllocation alloc;
  gint X;
  gint abs_pause_x;
  gint abs_prev_x;
  gint abs_next_x;

  g_return_val_if_fail(IDO_IS_PLAYBACK_MENU_ITEM (button), FALSE);
  g_return_val_if_fail(cr != NULL, FALSE);

  cairo_surface_t*  surf = NULL;
  cairo_t*       cr_surf = NULL;

  GtkStyle *style;

  CairoColorRGB bg_color, fg_color, bg_selected, bg_prelight;
  CairoColorRGB color_middle[2], color_middle_prelight[2], color_outer[2], color_outer_prelight[2],
                color_play_outer[2], color_play_outer_prelight[2],
                color_button[4], color_button_shadow, color_inner[2], color_inner_compressed[2];

  /* Use the menu's style instead of that of the menuitem ('button' is a
   * menuitem that is packed in a menu directly).  The menuitem's style
   * can't be used due to a change in light-themes (lp #1130183).
   * Menuitems now have a transparent background, which confuses
   * GtkStyle.
   *
   * This is a workaround until this code gets refactored to use
   * GtkStyleContext.
   */
  style = gtk_widget_get_style (gtk_widget_get_parent (button));

  bg_color.r = style->bg[0].red/65535.0;
  bg_color.g = style->bg[0].green/65535.0;
  bg_color.b = style->bg[0].blue/65535.0;

  bg_prelight.r = style->bg[GTK_STATE_PRELIGHT].red/65535.0;
  bg_prelight.g = style->bg[GTK_STATE_PRELIGHT].green/65535.0;
  bg_prelight.b = style->bg[GTK_STATE_PRELIGHT].blue/65535.0;

  bg_selected.r = style->bg[GTK_STATE_SELECTED].red/65535.0;
  bg_selected.g = style->bg[GTK_STATE_SELECTED].green/65535.0;
  bg_selected.b = style->bg[GTK_STATE_SELECTED].blue/65535.0;

  fg_color.r = style->fg[0].red/65535.0;
  fg_color.g = style->fg[0].green/65535.0;
  fg_color.b = style->fg[0].blue/65535.0;

  _color_shade (&bg_color,    MIDDLE_START_SHADE, &color_middle[0]);
  _color_shade (&bg_color,    MIDDLE_END_SHADE, &color_middle[1]);
  _color_shade (&bg_prelight, MIDDLE_START_SHADE, &color_middle_prelight[0]);
  _color_shade (&bg_prelight, MIDDLE_END_SHADE, &color_middle_prelight[1]);
  _color_shade (&bg_color,    OUTER_START_SHADE, &color_outer[0]);
  _color_shade (&bg_color,    OUTER_END_SHADE, &color_outer[1]);
  _color_shade (&bg_prelight, OUTER_START_SHADE, &color_outer_prelight[0]);
  _color_shade (&bg_prelight, OUTER_END_SHADE, &color_outer_prelight[1]);
  _color_shade (&bg_color,    OUTER_PLAY_START_SHADE, &color_play_outer[0]);
  _color_shade (&bg_color,    OUTER_PLAY_END_SHADE, &color_play_outer[1]);
  _color_shade (&bg_prelight, OUTER_PLAY_START_SHADE, &color_play_outer_prelight[0]);
  _color_shade (&bg_prelight, OUTER_PLAY_END_SHADE, &color_play_outer_prelight[1]);
  _color_shade (&bg_color, INNER_START_SHADE, &color_inner[0]);
  _color_shade (&bg_color, INNER_END_SHADE, &color_inner[1]);
  _color_shade (&fg_color, BUTTON_START_SHADE, &color_button[0]);
  _color_shade (&fg_color, BUTTON_END_SHADE, &color_button[1]);
  _color_shade (&bg_color, BUTTON_SHADOW_SHADE, &color_button[2]);
  _color_shade (&bg_color, SHADOW_BUTTON_SHADE, &color_button_shadow);
  _color_shade (&bg_selected, 1.0, &color_button[3]);
  _color_shade (&bg_color, INNER_COMPRESSED_START_SHADE, &color_inner_compressed[0]);
  _color_shade (&bg_color, INNER_COMPRESSED_END_SHADE, &color_inner_compressed[1]);

  double MIDDLE_END[]   = {color_middle[0].r, color_middle[0].g, color_middle[0].b, 1.0f};
  double MIDDLE_START[] = {color_middle[1].r, color_middle[1].g, color_middle[1].b, 1.0f};
  double MIDDLE_END_PRELIGHT[]   = {color_middle_prelight[0].r, color_middle_prelight[0].g, color_middle_prelight[0].b, 1.0f};
  double MIDDLE_START_PRELIGHT[] = {color_middle_prelight[1].r, color_middle_prelight[1].g, color_middle_prelight[1].b, 1.0f};
  double OUTER_END[]   = {color_outer[0].r, color_outer[0].g, color_outer[0].b, 1.0f};
  double OUTER_START[] = {color_outer[1].r, color_outer[1].g, color_outer[1].b, 1.0f};
  double OUTER_END_PRELIGHT[]   = {color_outer_prelight[0].r, color_outer_prelight[0].g, color_outer_prelight[0].b, 1.0f};
  double OUTER_START_PRELIGHT[] = {color_outer_prelight[1].r, color_outer_prelight[1].g, color_outer_prelight[1].b, 1.0f};
  double SHADOW_BUTTON[] = {color_button_shadow.r, color_button_shadow.g, color_button_shadow.b, 0.3f};
  double OUTER_PLAY_END[] = {color_play_outer[0].r, color_play_outer[0].g, color_play_outer[0].b, 1.0f};
  double OUTER_PLAY_START[] = {color_play_outer[1].r, color_play_outer[1].g, color_play_outer[1].b, 1.0f};
  double OUTER_PLAY_END_PRELIGHT[] = {color_play_outer_prelight[0].r, color_play_outer_prelight[0].g, color_play_outer_prelight[0].b, 1.0f};
  double OUTER_PLAY_START_PRELIGHT[] = {color_play_outer_prelight[1].r, color_play_outer_prelight[1].g, color_play_outer_prelight[1].b, 1.0f};
  double BUTTON_END[] = {color_button[0].r, color_button[0].g, color_button[0].b, 1.0f};
  double BUTTON_START[] = {color_button[1].r, color_button[1].g, color_button[1].b, 1.0f};
  double BUTTON_SHADOW[] = {color_button[2].r, color_button[2].g, color_button[2].b, 0.75f};
  double BUTTON_SHADOW_FOCUS[] = {color_button[3].r, color_button[3].g, color_button[3].b, 1.0f};
  double INNER_COMPRESSED_END[] = {color_inner_compressed[1].r, color_inner_compressed[1].g, color_inner_compressed[1].b, 1.0f};
  double INNER_COMPRESSED_START[] = {color_inner_compressed[0].r, color_inner_compressed[0].g, color_inner_compressed[0].b, 1.0f};

  gtk_widget_get_allocation (button, &alloc);
  X = alloc.x + (alloc.width - RECT_WIDTH) / 2 + OUTER_RADIUS;
  abs_pause_x = X + PAUSE_X;
  abs_prev_x = X + PREV_X;
  abs_next_x = X + NEXT_X;

  draw_gradient (cr,
                 X,
                 Y,
                 RECT_WIDTH,
                 OUTER_RADIUS,
                 OUTER_START,
                 OUTER_END);

  draw_gradient (cr,
                 X,
                 Y + 1,
                 RECT_WIDTH - 2,
                 MIDDLE_RADIUS,
                 MIDDLE_START,
                 MIDDLE_END);

  draw_gradient (cr,
                 X,
                 Y + 2,
                 RECT_WIDTH - 4,
                 MIDDLE_RADIUS,
                 MIDDLE_START,
                 MIDDLE_END);


  if(item->cur_pushed_button == BUTTON_PREVIOUS)
  {
    draw_gradient (cr,
                   X,
                   Y,
                   RECT_WIDTH/2,
                   OUTER_RADIUS,
                   OUTER_END,
                   OUTER_START);

    draw_gradient (cr,
                   X,
                   Y + 1,
                   RECT_WIDTH/2,
                   MIDDLE_RADIUS,
                   INNER_COMPRESSED_START,
                   INNER_COMPRESSED_END);

    draw_gradient (cr,
                   X,
                   Y + 2,
                   RECT_WIDTH/2,
                   MIDDLE_RADIUS,
                   INNER_COMPRESSED_START,
                   INNER_COMPRESSED_END);
  }
  else if(item->cur_pushed_button == BUTTON_NEXT)
  {
    draw_gradient (cr,
                   RECT_WIDTH / 2 + X,
                   Y,
                   RECT_WIDTH/2,
                   OUTER_RADIUS,
                   OUTER_END,
                   OUTER_START);

    draw_gradient (cr,
                   RECT_WIDTH / 2 + X,
                   Y + 1,
                   (RECT_WIDTH - 4.5)/2,
                   MIDDLE_RADIUS,
                   INNER_COMPRESSED_START,
                   INNER_COMPRESSED_END);

    draw_gradient (cr,
                   RECT_WIDTH / 2 + X,
                   Y + 2,
                   (RECT_WIDTH - 7)/2,
                   MIDDLE_RADIUS,
                   INNER_COMPRESSED_START,
                   INNER_COMPRESSED_END);
  }
  else if (item->cur_hover_button == BUTTON_PREVIOUS)
  {
    draw_gradient (cr,
                   X,
                   Y,
                   RECT_WIDTH/2,
                   OUTER_RADIUS,
                   OUTER_START_PRELIGHT,
                   OUTER_END_PRELIGHT);

    draw_gradient (cr,
                   X,
                   Y + 1,
                   RECT_WIDTH/2,
                   MIDDLE_RADIUS,
                   MIDDLE_START_PRELIGHT,
                   MIDDLE_END_PRELIGHT);

    draw_gradient (cr,
                   X,
                   Y + 2,
                   RECT_WIDTH/2,
                   MIDDLE_RADIUS,
                   MIDDLE_START_PRELIGHT,
                   MIDDLE_END_PRELIGHT);
  }
  else if (item->cur_hover_button == BUTTON_NEXT)
  {
    draw_gradient (cr,
                   RECT_WIDTH / 2 + X,
                   Y,
                   RECT_WIDTH/2,
                   OUTER_RADIUS,
                   OUTER_START_PRELIGHT,
                   OUTER_END_PRELIGHT);

    draw_gradient (cr,
                   RECT_WIDTH / 2 + X,
                   Y + 1,
                   (RECT_WIDTH - 4.5)/2,
                   MIDDLE_RADIUS,
                   MIDDLE_START_PRELIGHT,
                   MIDDLE_END_PRELIGHT);

    draw_gradient (cr,
                   RECT_WIDTH / 2 + X,
                   Y + 2,
                   (RECT_WIDTH - 7)/2,
                   MIDDLE_RADIUS,
                   MIDDLE_START_PRELIGHT,
                   MIDDLE_END_PRELIGHT);
  }

  // play/pause shadow
  if(item->cur_pushed_button != BUTTON_PLAYPAUSE)
  {
    cairo_save (cr);
    cairo_rectangle (cr, X, Y, RECT_WIDTH, MIDDLE_RADIUS*2);
    cairo_clip (cr);

    draw_circle (cr,
                 X + RECT_WIDTH / 2.0f - 2.0f * OUTER_RADIUS - 5.5f - 1.0f,
                 Y - ((CIRCLE_RADIUS - OUTER_RADIUS)) - 1.0f,
                 CIRCLE_RADIUS + 1.0f,
                 SHADOW_BUTTON,
                 SHADOW_BUTTON);

    cairo_restore (cr);
  }

  // play/pause button
  if(item->cur_pushed_button == BUTTON_PLAYPAUSE)
  {
    draw_circle (cr,
                 X + RECT_WIDTH / 2.0f - 2.0f * OUTER_RADIUS - 5.5f,
                 Y - ((CIRCLE_RADIUS - OUTER_RADIUS)) ,
                 CIRCLE_RADIUS,
                 OUTER_PLAY_END,
                 OUTER_PLAY_START);

    draw_circle (cr,
                 X + RECT_WIDTH / 2.0f - 2.0f * OUTER_RADIUS - 5.5f + 1.25f,
                 Y - ((CIRCLE_RADIUS - OUTER_RADIUS)) + 1.25f,
                 CIRCLE_RADIUS - 1.25,
                 INNER_COMPRESSED_START,
                 INNER_COMPRESSED_END);
  }
  else if (item->cur_hover_button == BUTTON_PLAYPAUSE)
  {
    /* this subtle offset is to fix alpha borders, should be removed once this draw routine will be refactored */
    draw_circle (cr,
                 X + RECT_WIDTH / 2.0f - 2.0f * OUTER_RADIUS - 5.5f + 0.1,
                 Y - ((CIRCLE_RADIUS - OUTER_RADIUS)) + 0.1,
                 CIRCLE_RADIUS - 0.1,
                 OUTER_PLAY_START_PRELIGHT,
                 OUTER_PLAY_END_PRELIGHT);

    draw_circle (cr,
                 X + RECT_WIDTH / 2.0f - 2.0f * OUTER_RADIUS - 5.5f + 1.25f,
                 Y - ((CIRCLE_RADIUS - OUTER_RADIUS)) + 1.25f,
                 CIRCLE_RADIUS - 1.25,
                 MIDDLE_START_PRELIGHT,
                 MIDDLE_END_PRELIGHT);
  }
  else
  {
    draw_circle (cr,
                 X + RECT_WIDTH / 2.0f - 2.0f * OUTER_RADIUS - 5.5f,
                 Y - ((CIRCLE_RADIUS - OUTER_RADIUS)),
                 CIRCLE_RADIUS,
                 OUTER_PLAY_START,
                 OUTER_PLAY_END);

    draw_circle (cr,
                 X + RECT_WIDTH / 2.0f - 2.0f * OUTER_RADIUS - 5.5f + 1.25f,
                 Y - ((CIRCLE_RADIUS - OUTER_RADIUS)) + 1.25f,
                 CIRCLE_RADIUS - 1.25,
                 MIDDLE_START,
                 MIDDLE_END);
  }

  // draw previous-button drop-shadow
  if ((item->cur_pushed_button == BUTTON_PREVIOUS && item->keyboard_activated) ||
      item->cur_hover_button == BUTTON_PREVIOUS)
  {
    _setup (&cr_surf, &surf, PREV_WIDTH+6, PREV_HEIGHT+6);
    _mask_prev (cr_surf,
                (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
                (PREV_HEIGHT - TRI_HEIGHT) / 2.0f,
                TRI_WIDTH,
                TRI_HEIGHT,
                TRI_OFFSET);
    _fill (cr_surf,
           (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
           (PREV_HEIGHT - TRI_HEIGHT) / 2.0f,
           (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
           (double) TRI_HEIGHT,
           BUTTON_SHADOW_FOCUS,
           BUTTON_SHADOW_FOCUS,
           FALSE);
    _surface_blur (surf, 3);
    _finalize_repaint (cr, &cr_surf, &surf, abs_prev_x, PREV_Y + 0.5f, 3);
  }
  else
  {
    _setup (&cr_surf, &surf, PREV_WIDTH, PREV_HEIGHT);
    _mask_prev (cr_surf,
                (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
                (PREV_HEIGHT - TRI_HEIGHT) / 2.0f,
                TRI_WIDTH,
                TRI_HEIGHT,
                TRI_OFFSET);
    _fill (cr_surf,
           (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
           (PREV_HEIGHT - TRI_HEIGHT) / 2.0f,
           (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
           (double) TRI_HEIGHT,
           BUTTON_SHADOW,
           BUTTON_SHADOW,
           FALSE);
    _surface_blur (surf, 1);
    _finalize (cr, &cr_surf, &surf, abs_prev_x, PREV_Y + 1.0f);
  }

  // draw previous-button
  _setup (&cr_surf, &surf, PREV_WIDTH, PREV_HEIGHT);
  _mask_prev (cr_surf,
              (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
              (PREV_HEIGHT - TRI_HEIGHT) / 2.0f,
              TRI_WIDTH,
              TRI_HEIGHT,
              TRI_OFFSET);
  _fill (cr_surf,
       (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
       (PREV_HEIGHT - TRI_HEIGHT) / 2.0f,
       (PREV_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
       (double) TRI_HEIGHT,
       BUTTON_START,
       BUTTON_END,
       FALSE);
  _finalize (cr, &cr_surf, &surf, abs_prev_x, PREV_Y);

  // draw next-button drop-shadow
  if ((item->cur_pushed_button == BUTTON_NEXT && item->keyboard_activated) ||
      item->cur_hover_button == BUTTON_NEXT)
  {
    _setup (&cr_surf, &surf, NEXT_WIDTH+6, NEXT_HEIGHT+6);
    _mask_next (cr_surf,
                (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
                (NEXT_HEIGHT - TRI_HEIGHT) / 2.0f,
                TRI_WIDTH,
                TRI_HEIGHT,
                TRI_OFFSET);
    _fill (cr_surf,
           (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
           (NEXT_HEIGHT - TRI_HEIGHT) / 2.0f,
           (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
           (double) TRI_HEIGHT,
           BUTTON_SHADOW_FOCUS,
           BUTTON_SHADOW_FOCUS,
           FALSE);
    _surface_blur (surf, 3);
    _finalize_repaint (cr, &cr_surf, &surf, abs_next_x, NEXT_Y + 0.5f, 3);
  }
  else
  {
    _setup (&cr_surf, &surf, NEXT_WIDTH, NEXT_HEIGHT);
    _mask_next (cr_surf,
                (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
                (NEXT_HEIGHT - TRI_HEIGHT) / 2.0f,
                TRI_WIDTH,
                TRI_HEIGHT,
                TRI_OFFSET);
    _fill (cr_surf,
           (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
           (NEXT_HEIGHT - TRI_HEIGHT) / 2.0f,
           (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
           (double) TRI_HEIGHT,
           BUTTON_SHADOW,
           BUTTON_SHADOW,
           FALSE);
    _surface_blur (surf, 1);
    _finalize (cr, &cr_surf, &surf, abs_next_x, NEXT_Y + 1.0f);
  }

  // draw next-button
  _setup (&cr_surf, &surf, NEXT_WIDTH, NEXT_HEIGHT);
  _mask_next (cr_surf,
              (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
              (NEXT_HEIGHT - TRI_HEIGHT) / 2.0f,
              TRI_WIDTH,
              TRI_HEIGHT,
              TRI_OFFSET);
  _fill (cr_surf,
         (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
         (NEXT_HEIGHT - TRI_HEIGHT) / 2.0f,
         (NEXT_WIDTH - (2.0f * TRI_WIDTH - TRI_OFFSET)) / 2.0f,
         (double) TRI_HEIGHT,
         BUTTON_START,
         BUTTON_END,
         FALSE);
  _finalize (cr, &cr_surf, &surf, abs_next_x, NEXT_Y);

  // draw pause-button drop-shadow
  if (item->current_state == STATE_PLAYING)
  {
    if (item->has_focus &&
        (item->cur_hover_button == BUTTON_NONE || item->cur_hover_button == BUTTON_PLAYPAUSE) &&
        (item->cur_pushed_button == BUTTON_NONE || item->cur_pushed_button == BUTTON_PLAYPAUSE))
    {
      _setup (&cr_surf, &surf, PAUSE_WIDTH+6, PAUSE_HEIGHT+6);
      _mask_pause (cr_surf,
                   (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
                   (PAUSE_HEIGHT - BAR_HEIGHT) / 2.0f,
                   BAR_WIDTH,
                   BAR_HEIGHT - 2.0f * BAR_WIDTH,
                   BAR_OFFSET);
      _fill (cr_surf,
             (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
             (PAUSE_HEIGHT - BAR_HEIGHT) / 2.0f,
             (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
             (double) BAR_HEIGHT,
             BUTTON_SHADOW_FOCUS,
             BUTTON_SHADOW_FOCUS,
             TRUE);
      _surface_blur (surf, 3);
      _finalize_repaint (cr, &cr_surf, &surf, abs_pause_x, PAUSE_Y + 0.5f, 3);
    }
    else
    {
      _setup (&cr_surf, &surf, PAUSE_WIDTH, PAUSE_HEIGHT);
      _mask_pause (cr_surf,
                   (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
                   (PAUSE_HEIGHT - BAR_HEIGHT) / 2.0f,
                   BAR_WIDTH,
                   BAR_HEIGHT - 2.0f * BAR_WIDTH,
                   BAR_OFFSET);
      _fill (cr_surf,
             (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
             (PAUSE_HEIGHT - BAR_HEIGHT) / 2.0f,
             (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
             (double) BAR_HEIGHT,
             BUTTON_SHADOW,
             BUTTON_SHADOW,
             TRUE);
      _surface_blur (surf, 1);
      _finalize (cr, &cr_surf, &surf, abs_pause_x, PAUSE_Y + 1.0f);
    }

    // draw pause-button
    _setup (&cr_surf, &surf, PAUSE_WIDTH, PAUSE_HEIGHT);
    _mask_pause (cr_surf,
                 (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
                 (PAUSE_HEIGHT - BAR_HEIGHT) / 2.0f,
                 BAR_WIDTH,
                 BAR_HEIGHT - 2.0f * BAR_WIDTH,
                 BAR_OFFSET);
    _fill (cr_surf,
           (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
           (PAUSE_HEIGHT - BAR_HEIGHT) / 2.0f,
           (PAUSE_WIDTH - (2.0f * BAR_WIDTH + BAR_OFFSET)) / 2.0f,
           (double) BAR_HEIGHT,
           BUTTON_START,
           BUTTON_END,
           TRUE);
    _finalize (cr, &cr_surf, &surf, abs_pause_x, PAUSE_Y);
  }
  else if (item->current_state == STATE_PAUSED)
  {
    if (item->has_focus &&
        (item->cur_hover_button == BUTTON_NONE || item->cur_hover_button == BUTTON_PLAYPAUSE) &&
        (item->cur_pushed_button == BUTTON_NONE || item->cur_pushed_button == BUTTON_PLAYPAUSE))
    {
      _setup (&cr_surf, &surf, PLAY_WIDTH+6, PLAY_HEIGHT+6);
      _mask_play (cr_surf,
                  PLAY_PADDING,
                  PLAY_PADDING,
                  PLAY_WIDTH - (2*PLAY_PADDING),
                  PLAY_HEIGHT - (2*PLAY_PADDING));
      _fill (cr_surf,
             PLAY_PADDING,
             PLAY_PADDING,
             PLAY_WIDTH - (2*PLAY_PADDING),
             PLAY_HEIGHT - (2*PLAY_PADDING),
             BUTTON_SHADOW_FOCUS,
             BUTTON_SHADOW_FOCUS,
             FALSE);
      _surface_blur (surf, 3);
      _finalize_repaint (cr, &cr_surf, &surf, abs_pause_x-0.5f, PAUSE_Y + 0.5f, 3);
    }
    else
    {
      _setup (&cr_surf, &surf, PLAY_WIDTH, PLAY_HEIGHT);
      _mask_play (cr_surf,
                  PLAY_PADDING,
                  PLAY_PADDING,
                  PLAY_WIDTH - (2*PLAY_PADDING),
                  PLAY_HEIGHT - (2*PLAY_PADDING));
      _fill (cr_surf,
             PLAY_PADDING,
             PLAY_PADDING,
             PLAY_WIDTH - (2*PLAY_PADDING),
             PLAY_HEIGHT - (2*PLAY_PADDING),
             BUTTON_SHADOW,
             BUTTON_SHADOW,
             FALSE);
      _surface_blur (surf, 1);
      _finalize (cr, &cr_surf, &surf, abs_pause_x-0.75f, PAUSE_Y + 1.0f);
    }

    // draw play-button
    _setup (&cr_surf, &surf, PLAY_WIDTH, PLAY_HEIGHT);
    cairo_set_line_width (cr, 10.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    _mask_play (cr_surf,
                PLAY_PADDING,
                PLAY_PADDING,
                PLAY_WIDTH - (2*PLAY_PADDING),
                PLAY_HEIGHT - (2*PLAY_PADDING));
    _fill (cr_surf,
           PLAY_PADDING,
           PLAY_PADDING,
           PLAY_WIDTH - (2*PLAY_PADDING),
           PLAY_HEIGHT - (2*PLAY_PADDING),
           BUTTON_START,
           BUTTON_END,
           FALSE);
    _finalize (cr, &cr_surf, &surf, abs_pause_x-0.5f, PAUSE_Y);
  }
  else if (item->current_state == STATE_LAUNCHING)
  {
    // the spinner is not aligned, why? because the play button has odd width/height numbers
    gtk_render_activity (gtk_widget_get_style_context (button), cr, 106, 6, 30, 30);
  }
  return FALSE;
}
