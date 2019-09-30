/*
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

#ifndef __IDO_TIMELINE_H__
#define __IDO_TIMELINE_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define IDO_TYPE_TIMELINE         (ido_timeline_get_type ())
#define IDO_TIMELINE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), IDO_TYPE_TIMELINE, IdoTimeline))
#define IDO_TIMELINE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), IDO_TYPE_TIMELINE, IdoTimelineClass))
#define IDO_IS_TIMELINE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), IDO_TYPE_TIMELINE))
#define IDO_IS_TIMELINE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), IDO_TYPE_TIMELINE))
#define IDO_TIMELINE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), IDO_TYPE_TIMELINE, IdoTimelineClass))

typedef struct IdoTimeline      IdoTimeline;
typedef struct IdoTimelineClass IdoTimelineClass;

typedef enum {
  IDO_TIMELINE_DIRECTION_FORWARD,
  IDO_TIMELINE_DIRECTION_BACKWARD
} IdoTimelineDirection;

typedef enum {
  IDO_TIMELINE_PROGRESS_LINEAR,
  IDO_TIMELINE_PROGRESS_SINUSOIDAL,
  IDO_TIMELINE_PROGRESS_EXPONENTIAL,
  IDO_TIMELINE_PROGRESS_EASE_IN_EASE_OUT
} IdoTimelineProgressType;

struct IdoTimeline
{
  GObject parent_instance;
};

struct IdoTimelineClass
{
  GObjectClass parent_class;

  void (* started)           (IdoTimeline *timeline);
  void (* finished)          (IdoTimeline *timeline);
  void (* paused)            (IdoTimeline *timeline);

  void (* frame)             (IdoTimeline *timeline,
			      gdouble     progress);

  void (* __ido_reserved1) (void);
  void (* __ido_reserved2) (void);
  void (* __ido_reserved3) (void);
  void (* __ido_reserved4) (void);
};


GType                 ido_timeline_get_type            (void) G_GNUC_CONST;

IdoTimeline           *ido_timeline_new                (guint                     duration);
IdoTimeline           *ido_timeline_new_for_screen     (guint                     duration,
                                                        GdkScreen                *screen);

void                  ido_timeline_start               (IdoTimeline              *timeline);
void                  ido_timeline_pause               (IdoTimeline              *timeline);
void                  ido_timeline_rewind              (IdoTimeline              *timeline);

gboolean              ido_timeline_is_running          (IdoTimeline              *timeline);

guint                 ido_timeline_get_fps             (IdoTimeline              *timeline);
void                  ido_timeline_set_fps             (IdoTimeline              *timeline,
                                                        guint                     fps);

gboolean              ido_timeline_get_loop            (IdoTimeline              *timeline);
void                  ido_timeline_set_loop            (IdoTimeline              *timeline,
                                                        gboolean                  loop);

guint                 ido_timeline_get_duration        (IdoTimeline              *timeline);
void                  ido_timeline_set_duration        (IdoTimeline              *timeline,
                                                        guint                     duration);

GdkScreen            *ido_timeline_get_screen          (IdoTimeline              *timeline);
void                  ido_timeline_set_screen          (IdoTimeline              *timeline,
                                                        GdkScreen                *screen);

IdoTimelineDirection  ido_timeline_get_direction       (IdoTimeline              *timeline);
void                  ido_timeline_set_direction       (IdoTimeline              *timeline,
                                                        IdoTimelineDirection      direction);

gdouble               ido_timeline_get_progress        (IdoTimeline              *timeline);
void                  ido_timeline_set_progress        (IdoTimeline              *timeline,
                                                        gdouble                   progress);

gdouble               ido_timeline_calculate_progress  (gdouble                   linear_progress,
                                                        IdoTimelineProgressType   progress_type);

G_END_DECLS

#endif /* __IDO_TIMELINE_H__ */
