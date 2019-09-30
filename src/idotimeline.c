/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* gtktimeline.c
 *
 * Copyright (C) 2007 Carlos Garnacho <carlos@imendio.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "idotimeline.h"
#include "idotypebuiltins.h"

#include <gtk/gtk.h>
#include <math.h>

#define IDO_TIMELINE_GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), IDO_TYPE_TIMELINE, IdoTimelinePriv))
#define MSECS_PER_SEC 1000
#define FRAME_INTERVAL(nframes) (MSECS_PER_SEC / nframes)
#define DEFAULT_FPS 30

typedef struct IdoTimelinePriv IdoTimelinePriv;

struct IdoTimelinePriv
{
  guint duration;
  guint fps;
  guint source_id;

  GTimer *timer;

  gdouble progress;
  gdouble last_progress;

  GdkScreen *screen;

  guint animations_enabled : 1;
  guint loop               : 1;
  guint direction          : 1;
};

enum {
  PROP_0,
  PROP_FPS,
  PROP_DURATION,
  PROP_LOOP,
  PROP_DIRECTION,
  PROP_SCREEN
};

enum {
  STARTED,
  PAUSED,
  FINISHED,
  FRAME,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };


static void  ido_timeline_set_property  (GObject         *object,
                                         guint            prop_id,
                                         const GValue    *value,
                                         GParamSpec      *pspec);
static void  ido_timeline_get_property  (GObject         *object,
                                         guint            prop_id,
                                         GValue          *value,
                                         GParamSpec      *pspec);
static void  ido_timeline_finalize      (GObject *object);


G_DEFINE_TYPE (IdoTimeline, ido_timeline, G_TYPE_OBJECT)


static void
ido_timeline_class_init (IdoTimelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ido_timeline_set_property;
  object_class->get_property = ido_timeline_get_property;
  object_class->finalize = ido_timeline_finalize;

  g_object_class_install_property (object_class,
				   PROP_FPS,
				   g_param_spec_uint ("fps",
						      "FPS",
						      "Frames per second for the timeline",
						      1, G_MAXUINT,
						      DEFAULT_FPS,
						      G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_DURATION,
				   g_param_spec_uint ("duration",
						      "Animation Duration",
						      "Animation Duration",
						      0, G_MAXUINT,
						      0,
						      G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_LOOP,
				   g_param_spec_boolean ("loop",
							 "Loop",
							 "Whether the timeline loops or not",
							 FALSE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_DIRECTION,
				   g_param_spec_enum ("direction",
						      "Direction",
						      "Whether the timeline moves forward or backward in time",
						      IDO_TYPE_TIMELINE_DIRECTION,
						      IDO_TIMELINE_DIRECTION_FORWARD,
						      G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
							"Screen",
							"Screen to get the settings from",
							GDK_TYPE_SCREEN,
							G_PARAM_READWRITE));

  /**
   * IdoTimeline::started:
   * @timeline: The #IdoTimeline emitting the signal.
   * 
   * The ::started signal is emitted when the timeline starts.
   */
  signals[STARTED] =
    g_signal_new ("started",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (IdoTimelineClass, started),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * IdoTimeline::paused:
   * @timeline: The #IdoTimeline emitting the signal.
   * 
   * The ::paused signal is emitted when the timeline pauses.
   */
  signals[PAUSED] =
    g_signal_new ("paused",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (IdoTimelineClass, paused),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * IdoTimeline::finished:
   * @timeline: The #IdoTimeline emitting the signal.
   * 
   * The ::paused signal is emitted when the timeline finishes.
   */
  signals[FINISHED] =
    g_signal_new ("finished",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (IdoTimelineClass, finished),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * IdoTimeline::frame:
   * @timeline: The #IdoTimeline emitting the signal.
   * @progress: The progress position for this frame from 0.0 (start) to 1.0 (end).
   * 
   * The ::frame signal is emitted when a frame should be drawn.
   */
  signals[FRAME] =
    g_signal_new ("frame",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (IdoTimelineClass, frame),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__DOUBLE,
		  G_TYPE_NONE, 1,
		  G_TYPE_DOUBLE);

  g_type_class_add_private (klass, sizeof (IdoTimelinePriv));
}

static void
ido_timeline_init (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  priv->fps = DEFAULT_FPS;
  priv->duration = 0.0;
  priv->direction = IDO_TIMELINE_DIRECTION_FORWARD;
  priv->screen = gdk_screen_get_default ();

  priv->last_progress = 0;
}

static void
ido_timeline_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdoTimeline *timeline;

  timeline = IDO_TIMELINE (object);

  switch (prop_id)
    {
    case PROP_FPS:
      ido_timeline_set_fps (timeline, g_value_get_uint (value));
      break;
    case PROP_DURATION:
      ido_timeline_set_duration (timeline, g_value_get_uint (value));
      break;
    case PROP_LOOP:
      ido_timeline_set_loop (timeline, g_value_get_boolean (value));
      break;
    case PROP_DIRECTION:
      ido_timeline_set_direction (timeline, g_value_get_enum (value));
      break;
    case PROP_SCREEN:
      ido_timeline_set_screen (timeline,
                               (GdkScreen*)g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ido_timeline_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdoTimeline *timeline;
  IdoTimelinePriv *priv;

  timeline = IDO_TIMELINE (object);
  priv = IDO_TIMELINE_GET_PRIV (timeline);

  switch (prop_id)
    {
    case PROP_FPS:
      g_value_set_uint (value, priv->fps);
      break;
    case PROP_DURATION:
      g_value_set_uint (value, priv->duration);
      break;
    case PROP_LOOP:
      g_value_set_boolean (value, priv->loop);
      break;
    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
      break;
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ido_timeline_finalize (GObject *object)
{
  IdoTimelinePriv *priv;

  priv = IDO_TIMELINE_GET_PRIV (object);

  if (priv->source_id)
    {
      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }

  if (priv->timer)
    g_timer_destroy (priv->timer);

  G_OBJECT_CLASS (ido_timeline_parent_class)->finalize (object);
}

static gboolean
ido_timeline_run_frame (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;
  gdouble delta_progress, progress;
  guint elapsed_time;

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  elapsed_time = (guint) (g_timer_elapsed (priv->timer, NULL) * 1000);
  g_timer_start (priv->timer);

  if (priv->animations_enabled)
    {
      delta_progress = (gdouble) elapsed_time / priv->duration;
      progress = priv->last_progress;

      if (priv->direction == IDO_TIMELINE_DIRECTION_BACKWARD)
	progress -= delta_progress;
      else
	progress += delta_progress;

      priv->last_progress = progress;

      progress = CLAMP (progress, 0., 1.);
    }
  else
    progress = (priv->direction == IDO_TIMELINE_DIRECTION_FORWARD) ? 1.0 : 0.0;

  priv->progress = progress;
  g_signal_emit (timeline, signals [FRAME], 0, progress);

  if ((priv->direction == IDO_TIMELINE_DIRECTION_FORWARD && progress == 1.0) ||
      (priv->direction == IDO_TIMELINE_DIRECTION_BACKWARD && progress == 0.0))
    {
      if (!priv->loop)
	{
	  if (priv->source_id)
	    {
	      g_source_remove (priv->source_id);
	      priv->source_id = 0;
	    }
          g_timer_stop (priv->timer);
	  g_signal_emit (timeline, signals [FINISHED], 0);
	  return FALSE;
	}
      else
	ido_timeline_rewind (timeline);
    }

  return TRUE;
}

/**
 * ido_timeline_new:
 * @duration: duration in milliseconds for the timeline
 *
 * Creates a new #IdoTimeline with the specified number of frames.
 *
 * Return Value: the newly created #IdoTimeline
 **/
IdoTimeline *
ido_timeline_new (guint duration)
{
  return g_object_new (IDO_TYPE_TIMELINE,
		       "duration", duration,
		       NULL);
}

/**
 * ido_timeline_new_for_screen:
 * @duration: duration in milliseconds for the timeline
 * @screen: Screen to start on.
 *
 * Creates a new #IdoTimeline with the specified number of frames on the given screen.
 *
 * Return Value: the newly created #IdoTimeline
 **/
IdoTimeline *
ido_timeline_new_for_screen (guint      duration,
                             GdkScreen *screen)
{
  return g_object_new (IDO_TYPE_TIMELINE,
		       "duration", duration,
		       "screen", screen,
		       NULL);
}

/**
 * ido_timeline_start:
 * @timeline: A #IdoTimeline
 *
 * Runs the timeline from the current frame.
 **/
void
ido_timeline_start (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;
  gboolean enable_animations = FALSE;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  if (!priv->source_id)
    {
      if (priv->timer)
        g_timer_continue (priv->timer);
      else
        priv->timer = g_timer_new ();

      /* sanity check; CID: 12651 */
      priv->fps = priv->fps > 0 ? priv->fps : DEFAULT_FPS;

      if (priv->screen)
        {
#if 0
          GtkSettings *settings = gtk_settings_get_for_screen (priv->screen);
          g_object_get (settings, "ido-enable-animations", &enable_animations, NULL);
#else
          // XXX
          enable_animations = TRUE;
#endif
        }

      priv->animations_enabled = (enable_animations == TRUE);

      g_signal_emit (timeline, signals [STARTED], 0);

      if (enable_animations)
        priv->source_id = gdk_threads_add_timeout (FRAME_INTERVAL (priv->fps),
                                                   (GSourceFunc) ido_timeline_run_frame,
                                                   timeline);
      else
        priv->source_id = gdk_threads_add_idle ((GSourceFunc) ido_timeline_run_frame,
                                                timeline);
    }
}

/**
 * ido_timeline_pause:
 * @timeline: A #IdoTimeline
 *
 * Pauses the timeline.
 **/
void
ido_timeline_pause (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  if (priv->source_id)
    {
      g_timer_stop (priv->timer);
      g_source_remove (priv->source_id);
      priv->source_id = 0;
      g_signal_emit (timeline, signals [PAUSED], 0);
    }
}

/**
 * ido_timeline_rewind:
 * @timeline: A #IdoTimeline
 *
 * Rewinds the timeline.
 **/
void
ido_timeline_rewind (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  if (ido_timeline_get_direction(timeline) != IDO_TIMELINE_DIRECTION_FORWARD)
    priv->progress = priv->last_progress = 1.;
  else
    priv->progress = priv->last_progress = 0.;

  /* reset timer */
  if (priv->timer)
    {
      g_timer_start (priv->timer);

      if (!priv->source_id)
        g_timer_stop (priv->timer);
    }
}

/**
 * ido_timeline_is_running:
 * @timeline: A #IdoTimeline
 *
 * Returns whether the timeline is running or not.
 *
 * Return Value: %TRUE if the timeline is running
 **/
gboolean
ido_timeline_is_running (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_val_if_fail (IDO_IS_TIMELINE (timeline), FALSE);

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  return (priv->source_id != 0);
}

/**
 * ido_timeline_get_fps:
 * @timeline: A #IdoTimeline
 *
 * Returns the number of frames per second.
 *
 * Return Value: frames per second
 **/
guint
ido_timeline_get_fps (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_val_if_fail (IDO_IS_TIMELINE (timeline), 1);

  priv = IDO_TIMELINE_GET_PRIV (timeline);
  return priv->fps;
}

/**
 * ido_timeline_set_fps:
 * @timeline: A #IdoTimeline
 * @fps: frames per second
 *
 * Sets the number of frames per second that
 * the timeline will play.
 **/
void
ido_timeline_set_fps (IdoTimeline *timeline,
                      guint        fps)
{
  IdoTimelinePriv *priv;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));
  g_return_if_fail (fps > 0);

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  /* Coverity CID: 12650/12651: guard against division by 0. */
  priv->fps = fps > 0 ? fps : priv->fps;

  if (ido_timeline_is_running (timeline))
    {
      g_source_remove (priv->source_id);
      priv->source_id = gdk_threads_add_timeout (FRAME_INTERVAL (priv->fps),
						 (GSourceFunc) ido_timeline_run_frame,
						 timeline);
    }

  g_object_notify (G_OBJECT (timeline), "fps");
}

/**
 * ido_timeline_get_loop:
 * @timeline: A #IdoTimeline
 *
 * Returns whether the timeline loops to the
 * beginning when it has reached the end.
 *
 * Return Value: %TRUE if the timeline loops
 **/
gboolean
ido_timeline_get_loop (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_val_if_fail (IDO_IS_TIMELINE (timeline), FALSE);

  priv = IDO_TIMELINE_GET_PRIV (timeline);
  return priv->loop;
}

/**
 * ido_timeline_set_loop:
 * @timeline: A #IdoTimeline
 * @loop: %TRUE to make the timeline loop
 *
 * Sets whether the timeline loops to the beginning
 * when it has reached the end.
 **/
void
ido_timeline_set_loop (IdoTimeline *timeline,
                       gboolean     loop)
{
  IdoTimelinePriv *priv;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  if (loop != priv->loop)
    {
      priv->loop = loop;
      g_object_notify (G_OBJECT (timeline), "loop");
    }
}

/**
 * ido_timeline_set_duration:
 * @timeline: A #IdoTimeline
 * @duration: Duration in milliseconds.
 *
 * Set the animation duration.
 */
void
ido_timeline_set_duration (IdoTimeline *timeline,
                           guint        duration)
{
  IdoTimelinePriv *priv;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  if (duration != priv->duration)
    {
      priv->duration = duration;
      g_object_notify (G_OBJECT (timeline), "duration");
    }
}

/**
 * ido_timeline_get_duration:
 * @timeline: A #IdoTimeline
 *
 * Set the animation duration.
 *
 * Return Value: Duration in milliseconds.
 */
guint
ido_timeline_get_duration (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_val_if_fail (IDO_IS_TIMELINE (timeline), 0);

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  return priv->duration;
}

/**
 * ido_timeline_set_direction:
 * @timeline: A #IdoTimeline
 * @direction: direction
 *
 * Sets the direction of the timeline.
 **/
void
ido_timeline_set_direction (IdoTimeline          *timeline,
                            IdoTimelineDirection  direction)
{
  IdoTimelinePriv *priv;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  if (direction != priv->direction)
    {
      priv->direction = direction;
      g_object_notify (G_OBJECT (timeline), "direction");
    }
}

/**
 * ido_timeline_get_direction:
 * @timeline: A #IdoTimeline
 *
 * Returns the direction of the timeline.
 *
 * Return Value: direction
 **/
IdoTimelineDirection
ido_timeline_get_direction (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_val_if_fail (IDO_IS_TIMELINE (timeline), IDO_TIMELINE_DIRECTION_FORWARD);

  priv = IDO_TIMELINE_GET_PRIV (timeline);
  return priv->direction;
}

/**
 * ido_timeline_set_screen:
 * @timeline: A #IdoTimeline
 * @screen: A #GdkScreen to use
 *
 * Set the screen the timeline is running on.
 */
void
ido_timeline_set_screen (IdoTimeline *timeline,
                         GdkScreen   *screen)
{
  IdoTimelinePriv *priv;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  if (priv->screen)
    g_object_unref (priv->screen);

  priv->screen = g_object_ref (screen);

  g_object_notify (G_OBJECT (timeline), "screen");
}

/**
 * ido_timeline_get_screen:
 * @timeline: A #IdoTimeline
 * 
 * Get the screen this timeline is running on.
 *
 * Return Value: (transfer none): The #GdkScreen this timeline is running on.
 */
GdkScreen *
ido_timeline_get_screen (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_val_if_fail (IDO_IS_TIMELINE (timeline), NULL);

  priv = IDO_TIMELINE_GET_PRIV (timeline);
  return priv->screen;
}

/**
 * ido_timeline_get_progress:
 * @timeline: A #IdoTimeline
 *
 * Get the progress on the timeline.
 *
 * Return Value: The progress from 0.0 (start) to 1.0 (end)
 */
gdouble
ido_timeline_get_progress (IdoTimeline *timeline)
{
  IdoTimelinePriv *priv;

  g_return_val_if_fail (IDO_IS_TIMELINE (timeline), 0.);

  priv = IDO_TIMELINE_GET_PRIV (timeline);
  return priv->progress;
}

/**
 * ido_timeline_set_progress:
 * @timeline: A #IdoTimeline
 * @progress: The progress from 0.0 (start) to 1.0 (end)
 *
 * Set the progress on the timeline.
 */
void
ido_timeline_set_progress (IdoTimeline *timeline, gdouble progress)
{
  IdoTimelinePriv *priv;

  g_return_if_fail (IDO_IS_TIMELINE (timeline));

  priv = IDO_TIMELINE_GET_PRIV (timeline);

  if (priv->source_id)
    {
      g_timer_stop (priv->timer);
      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }

  priv->progress = priv->last_progress = progress;

  ido_timeline_start (timeline);
}

/**
 * ido_timeline_calculate_progress:
 * @linear_progress: The progress from 0.0 (start) to 1.0 (end)
 * @progress_type: The progress transform to apply
 *
 * Transform a linear progress position using the given transform.
 *
 * Return Value: the progress position using the provided transform.
 */
gdouble
ido_timeline_calculate_progress (gdouble                 linear_progress,
                                 IdoTimelineProgressType progress_type)
{
  gdouble progress;

  progress = linear_progress;

  switch (progress_type)
    {
    case IDO_TIMELINE_PROGRESS_LINEAR:
      break;
    case IDO_TIMELINE_PROGRESS_SINUSOIDAL:
      progress = sinf ((progress * G_PI) / 2);
      break;
    case IDO_TIMELINE_PROGRESS_EXPONENTIAL:
      progress *= progress;
      break;
    case IDO_TIMELINE_PROGRESS_EASE_IN_EASE_OUT:
      {
        progress *= 2;

        if (progress < 1)
          progress = pow (progress, 3) / 2;
        else
          progress = (pow (progress - 2, 3) + 2) / 2;
      }
    }

  return progress;
}
