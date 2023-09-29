/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:clutter-stage
 * @short_description: Top level visual element to which actors are placed.
 *
 * #ClutterStage is a top level 'window' on which child actors are placed
 * and manipulated.
 *
 * #ClutterStage is a proxy actor, wrapping the backend-specific implementation
 * (a #StageWindow) of the windowing system. It is possible to subclass
 * #ClutterStage, as long as every overridden virtual function chains up to the
 * parent class corresponding function.
 */

#include "clutter-build-config.h"

#include <math.h>
#include <cairo-gobject.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

#include "clutter-stage.h"
#include "deprecated/clutter-container.h"

#include "clutter-actor-private.h"
#include "clutter-backend-private.h"
#include "clutter-cairo.h"
#include "clutter-container.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-event-private.h"
#include "clutter-frame-clock.h"
#include "clutter-grab.h"
#include "clutter-id-pool.h"
#include "clutter-input-device-private.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-mutter.h"
#include "clutter-paint-context-private.h"
#include "clutter-paint-volume-private.h"
#include "clutter-pick-context-private.h"
#include "clutter-private.h"
#include "clutter-seat-private.h"
#include "clutter-stage-manager-private.h"
#include "clutter-stage-private.h"
#include "clutter-stage-view-private.h"
#include "clutter-private.h"

#include "cogl/cogl.h"

#define MAX_FRUSTA 64

typedef struct _QueueRedrawEntry
{
  gboolean has_clip;
  ClutterPaintVolume clip;
} QueueRedrawEntry;

typedef struct _PickRecord
{
  graphene_point_t vertex[4];
  ClutterActor *actor;
  int clip_stack_top;
} PickRecord;

typedef struct _PickClipRecord
{
  int prev;
  graphene_point_t vertex[4];
} PickClipRecord;

typedef struct _PointerDeviceEntry
{
  ClutterStage *stage;
  ClutterInputDevice *device;
  ClutterEventSequence *sequence;
  graphene_point_t coords;
  ClutterActor *current_actor;
  cairo_region_t *clear_area;
} PointerDeviceEntry;

struct _ClutterStagePrivate
{
  /* the stage implementation */
  ClutterStageWindow *impl;

  ClutterPerspective perspective;
  graphene_matrix_t projection;
  graphene_matrix_t inverse_projection;
  graphene_matrix_t view;
  float viewport[4];

  gchar *title;
  ClutterActor *key_focused_actor;

  ClutterGrab *topmost_grab;
  ClutterGrabState grab_state;

  GQueue *event_queue;

  GArray *paint_volume_stack;

  GSList *pending_relayouts;
  GHashTable *pending_queue_redraws;

  int update_freeze_count;

  gboolean pending_finish_queue_redraws;

  GHashTable *pointer_devices;
  GHashTable *touch_sequences;

  guint actor_needs_immediate_relayout : 1;
};

struct _ClutterGrab
{
  grefcount ref_count;
  ClutterStage *stage;
  ClutterActor *actor;
  ClutterGrab *prev;
  ClutterGrab *next;
};

enum
{
  PROP_0,

  PROP_PERSPECTIVE,
  PROP_TITLE,
  PROP_KEY_FOCUS,
  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

enum
{
  ACTIVATE,
  DEACTIVATE,
  DELETE_EVENT,
  BEFORE_UPDATE,
  PREPARE_FRAME,
  BEFORE_PAINT,
  AFTER_PAINT,
  AFTER_UPDATE,
  PAINT_VIEW,
  PRESENTED,
  GL_VIDEO_MEMORY_PURGED,

  LAST_SIGNAL
};

static guint stage_signals[LAST_SIGNAL] = { 0, };

static const ClutterColor default_stage_color = { 255, 255, 255, 255 };

static void free_queue_redraw_entry (QueueRedrawEntry *entry);
static void free_pointer_device_entry (PointerDeviceEntry *entry);
static void clutter_stage_update_view_perspective (ClutterStage *stage);
static void clutter_stage_set_viewport (ClutterStage *stage,
                                        float         width,
                                        float         height);

G_DEFINE_TYPE_WITH_PRIVATE (ClutterStage, clutter_stage, CLUTTER_TYPE_ACTOR)

static void
clutter_stage_get_preferred_width (ClutterActor *self,
                                   gfloat        for_height,
                                   gfloat       *min_width_p,
                                   gfloat       *natural_width_p)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  cairo_rectangle_int_t geom;

  if (priv->impl == NULL)
    return;

  _clutter_stage_window_get_geometry (priv->impl, &geom);

  if (min_width_p)
    *min_width_p = geom.width;

  if (natural_width_p)
    *natural_width_p = geom.width;
}

static void
clutter_stage_get_preferred_height (ClutterActor *self,
                                    gfloat        for_width,
                                    gfloat       *min_height_p,
                                    gfloat       *natural_height_p)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  cairo_rectangle_int_t geom;

  if (priv->impl == NULL)
    return;

  _clutter_stage_window_get_geometry (priv->impl, &geom);

  if (min_height_p)
    *min_height_p = geom.height;

  if (natural_height_p)
    *natural_height_p = geom.height;
}

static void
clutter_stage_add_redraw_clip (ClutterStage          *stage,
                               cairo_rectangle_int_t *clip)
{
  GList *l;

  for (l = clutter_stage_peek_stage_views (stage); l; l = l->next)
    {
      ClutterStageView *view = l->data;

      if (!clip)
        {
          clutter_stage_view_add_redraw_clip (view, NULL);
        }
      else
        {
          cairo_rectangle_int_t view_layout;
          cairo_rectangle_int_t intersection;

          clutter_stage_view_get_layout (view, &view_layout);
          if (_clutter_util_rectangle_intersection (&view_layout, clip,
                                                    &intersection))
            clutter_stage_view_add_redraw_clip (view, &intersection);
        }
    }
}

static inline void
queue_full_redraw (ClutterStage *stage)
{
  ClutterStageWindow *stage_window;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  /* Just calling clutter_actor_queue_redraw will typically only
   * redraw the bounding box of the children parented on the stage but
   * in this case we really need to ensure that the full stage is
   * redrawn so we add a NULL redraw clip to the stage window. */
  stage_window = _clutter_stage_get_window (stage);
  if (stage_window == NULL)
    return;

  clutter_stage_add_redraw_clip (stage, NULL);
}

static void
clutter_stage_allocate (ClutterActor           *self,
                        const ClutterActorBox  *box)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  ClutterActorBox alloc = CLUTTER_ACTOR_BOX_INIT_ZERO;
  float new_width, new_height;
  float width, height;
  cairo_rectangle_int_t window_size;
  ClutterActorBox children_box;
  ClutterLayoutManager *layout_manager = clutter_actor_get_layout_manager (self);

  if (priv->impl == NULL)
    return;

  /* the current allocation */
  clutter_actor_box_get_size (box, &width, &height);

  /* the current Stage implementation size */
  _clutter_stage_window_get_geometry (priv->impl, &window_size);

  children_box.x1 = children_box.y1 = 0.f;
  children_box.x2 = box->x2 - box->x1;
  children_box.y2 = box->y2 - box->y1;

  CLUTTER_NOTE (LAYOUT,
                "Following allocation to %.2fx%.2f",
                width, height);

  clutter_actor_set_allocation (self, box);

  clutter_layout_manager_allocate (layout_manager,
                                   CLUTTER_CONTAINER (self),
                                   &children_box);

  if (window_size.width != CLUTTER_NEARBYINT (width) ||
      window_size.height != CLUTTER_NEARBYINT (height))
    {
      _clutter_stage_window_resize (priv->impl,
                                    CLUTTER_NEARBYINT (width),
                                    CLUTTER_NEARBYINT (height));
    }

  /* set the viewport to the new allocation */
  clutter_actor_get_allocation_box (self, &alloc);
  clutter_actor_box_get_size (&alloc, &new_width, &new_height);

  clutter_stage_set_viewport (CLUTTER_STAGE (self), new_width, new_height);
}

static void
setup_clip_frustum (ClutterStage                *stage,
                    const cairo_rectangle_int_t *clip,
                    graphene_frustum_t          *frustum)
{
  ClutterStagePrivate *priv = stage->priv;
  cairo_rectangle_int_t geom;
  graphene_point3d_t camera_position;
  graphene_point3d_t p[4];
  graphene_plane_t planes[6];
  graphene_vec4_t v;
  int i;

  _clutter_stage_window_get_geometry (priv->impl, &geom);

  CLUTTER_NOTE (CLIPPING, "Creating stage clip frustum for "
                "x=%d, y=%d, width=%d, height=%d",
                clip->x, clip->y, clip->width, clip->height);

  camera_position = GRAPHENE_POINT3D_INIT_ZERO;

  p[0] = GRAPHENE_POINT3D_INIT (MAX (clip->x, 0), MAX (clip->y, 0), 0.f);
  p[2] = GRAPHENE_POINT3D_INIT (MIN (clip->x + clip->width, geom.width),
                                MIN (clip->y + clip->height, geom.height),
                                0.f);

  for (i = 0; i < 2; i++)
    {
      float w = 1.0;
      cogl_graphene_matrix_project_point (&priv->view,
                                          &p[2 * i].x,
                                          &p[2 * i].y,
                                          &p[2 * i].z,
                                          &w);
    }

  graphene_point3d_init (&p[1], p[2].x, p[0].y, p[0].z);
  graphene_point3d_init (&p[3], p[0].x, p[2].y, p[0].z);

  for (i = 0; i < 4; i++)
    {
      graphene_plane_init_from_points (&planes[i],
                                       &camera_position,
                                       &p[i],
                                       &p[(i + 1) % 4]);
    }

  graphene_vec4_init (&v, 0.f, 0.f, -1.f, priv->perspective.z_near);
  graphene_plane_init_from_vec4 (&planes[4], &v);

  graphene_vec4_init (&v, 0.f, 0.f, 1.f, priv->perspective.z_far);
  graphene_plane_init_from_vec4 (&planes[5], &v);

  graphene_frustum_init (frustum,
                         &planes[0], &planes[1],
                         &planes[2], &planes[3],
                         &planes[4], &planes[5]);
}

static void
clutter_stage_do_paint_view (ClutterStage         *stage,
                             ClutterStageView     *view,
                             const cairo_region_t *redraw_clip)
{
  ClutterPaintContext *paint_context;
  cairo_rectangle_int_t clip_rect;
  g_autoptr (GArray) clip_frusta = NULL;
  graphene_frustum_t clip_frustum;
  int n_rectangles;

  n_rectangles = redraw_clip ? cairo_region_num_rectangles (redraw_clip) : 0;
  if (redraw_clip && n_rectangles < MAX_FRUSTA)
    {
      int i;

      clip_frusta = g_array_sized_new (FALSE, FALSE,
                                       sizeof (graphene_frustum_t),
                                       n_rectangles);

      for (i = 0; i < n_rectangles; i++)
        {
          cairo_region_get_rectangle (redraw_clip, i, &clip_rect);
          setup_clip_frustum (stage, &clip_rect, &clip_frustum);
          g_array_append_val (clip_frusta, clip_frustum);
        }
    }
  else
    {
      clip_frusta = g_array_sized_new (FALSE, FALSE,
                                       sizeof (graphene_frustum_t),
                                       1);
      if (redraw_clip)
        cairo_region_get_extents (redraw_clip, &clip_rect);
      else
        clutter_stage_view_get_layout (view, &clip_rect);

      setup_clip_frustum (stage, &clip_rect, &clip_frustum);
      g_array_append_val (clip_frusta, clip_frustum);
    }

  _clutter_stage_paint_volume_stack_free_all (stage);

  paint_context = clutter_paint_context_new_for_view (view,
                                                      redraw_clip,
                                                      clip_frusta,
                                                      CLUTTER_PAINT_FLAG_NONE);

  clutter_actor_paint (CLUTTER_ACTOR (stage), paint_context);
  clutter_paint_context_destroy (paint_context);
}

/* This provides a common point of entry for painting the scenegraph
 * for picking or painting...
 */
void
clutter_stage_paint_view (ClutterStage         *stage,
                          ClutterStageView     *view,
                          const cairo_region_t *redraw_clip)
{
  ClutterStagePrivate *priv = stage->priv;

  if (!priv->impl)
    return;

  COGL_TRACE_BEGIN_SCOPED (ClutterStagePaintView, "Paint (view)");

  if (g_signal_has_handler_pending (stage, stage_signals[PAINT_VIEW],
                                    0, TRUE))
    g_signal_emit (stage, stage_signals[PAINT_VIEW], 0, view, redraw_clip);
  else
    CLUTTER_STAGE_GET_CLASS (stage)->paint_view (stage, view, redraw_clip);
}

void
clutter_stage_emit_before_update (ClutterStage     *stage,
                                  ClutterStageView *view)
{
  g_signal_emit (stage, stage_signals[BEFORE_UPDATE], 0, view);
}

void
clutter_stage_emit_prepare_frame (ClutterStage     *stage,
                                  ClutterStageView *view)
{
  g_signal_emit (stage, stage_signals[PREPARE_FRAME], 0, view);
}

void
clutter_stage_emit_before_paint (ClutterStage     *stage,
                                 ClutterStageView *view)
{
  g_signal_emit (stage, stage_signals[BEFORE_PAINT], 0, view);
}

void
clutter_stage_emit_after_paint (ClutterStage     *stage,
                                ClutterStageView *view)
{
  g_signal_emit (stage, stage_signals[AFTER_PAINT], 0, view);
}

void
clutter_stage_emit_after_update (ClutterStage     *stage,
                                 ClutterStageView *view)
{
  g_signal_emit (stage, stage_signals[AFTER_UPDATE], 0, view);
}

static gboolean
clutter_stage_get_paint_volume (ClutterActor *self,
                                ClutterPaintVolume *volume)
{
  /* Returning False effectively means Clutter has to assume it covers
   * everything... */
  return FALSE;
}

static void
clutter_stage_realize (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  gboolean is_realized;

  g_assert (priv->impl != NULL);
  is_realized = _clutter_stage_window_realize (priv->impl);

  if (!is_realized)
    CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
}

static void
clutter_stage_unrealize (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  /* and then unrealize the implementation */
  g_assert (priv->impl != NULL);
  _clutter_stage_window_unrealize (priv->impl);

  CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
}

static void
clutter_stage_show (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->show (self);

  /* Possibly do an allocation run so that the stage will have the
     right size before we map it */
  clutter_stage_maybe_relayout (self);

  g_assert (priv->impl != NULL);
  _clutter_stage_window_show (priv->impl, TRUE);
}

static void
clutter_stage_hide_all (ClutterActor *self)
{
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_hide (self);

  /* we don't do a recursive hide_all(), to maintain the old invariants
   * from ClutterGroup
   */
  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    clutter_actor_hide (child);
}

static void
clutter_stage_hide (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  g_assert (priv->impl != NULL);
  _clutter_stage_window_hide (priv->impl);

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->hide (self);
}

static void
clutter_stage_emit_key_focus_event (ClutterStage *stage,
                                    gboolean      focus_in)
{
  ClutterStagePrivate *priv = stage->priv;

  if (priv->key_focused_actor == NULL)
    return;

  _clutter_actor_set_has_key_focus (CLUTTER_ACTOR (stage), focus_in);

  g_object_notify_by_pspec (G_OBJECT (stage), obj_props[PROP_KEY_FOCUS]);
}

static void
clutter_stage_real_activate (ClutterStage *stage)
{
  clutter_stage_emit_key_focus_event (stage, TRUE);
}

static void
clutter_stage_real_deactivate (ClutterStage *stage)
{
  clutter_stage_emit_key_focus_event (stage, FALSE);
}

void
_clutter_stage_queue_event (ClutterStage *stage,
                            ClutterEvent *event,
                            gboolean      copy_event)
{
  ClutterStagePrivate *priv;
  gboolean first_event;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  first_event = priv->event_queue->length == 0;

  if (copy_event)
    event = clutter_event_copy (event);

  if (first_event)
    {
      gboolean compressible = event->type == CLUTTER_MOTION ||
                              event->type == CLUTTER_TOUCH_UPDATE;

      if (!compressible)
        {
          _clutter_process_event (event);
          clutter_event_free (event);
          return;
        }
    }

  g_queue_push_tail (priv->event_queue, event);

  if (first_event)
    clutter_stage_schedule_update (stage);
}

gboolean
_clutter_stage_has_queued_events (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  priv = stage->priv;

  return priv->event_queue->length > 0;
}

static void
clutter_stage_compress_motion (ClutterStage       *stage,
                               ClutterEvent       *event,
                               const ClutterEvent *to_discard)
{
  double dx, dy;
  double dx_unaccel, dy_unaccel;
  double dst_dx = 0.0, dst_dy = 0.0;
  double dst_dx_unaccel = 0.0, dst_dy_unaccel = 0.0;

  if (!clutter_event_get_relative_motion (to_discard,
                                          &dx, &dy,
                                          &dx_unaccel, &dy_unaccel))
    return;

  clutter_event_get_relative_motion (event,
                                     &dst_dx, &dst_dy,
                                     &dst_dx_unaccel, &dst_dy_unaccel);

  event->motion.flags |= CLUTTER_EVENT_FLAG_RELATIVE_MOTION;
  event->motion.dx = dx + dst_dx;
  event->motion.dy = dy + dst_dy;
  event->motion.dx_unaccel = dx_unaccel + dst_dx_unaccel;
  event->motion.dy_unaccel = dy_unaccel + dst_dy_unaccel;
}

void
_clutter_stage_process_queued_events (ClutterStage *stage)
{
  ClutterStagePrivate *priv;
  GList *events, *l;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->event_queue->length == 0)
    return;

  /* In case the stage gets destroyed during event processing */
  g_object_ref (stage);

  /* Steal events before starting processing to avoid reentrancy
   * issues */
  events = priv->event_queue->head;
  priv->event_queue->head =  NULL;
  priv->event_queue->tail = NULL;
  priv->event_queue->length = 0;

  for (l = events; l != NULL; l = l->next)
    {
      ClutterEvent *event;
      ClutterEvent *next_event;
      ClutterInputDevice *device;
      ClutterInputDevice *next_device;
      gboolean check_device = FALSE;

      event = l->data;
      next_event = l->next ? l->next->data : NULL;

      device = clutter_event_get_device (event);

      if (next_event != NULL)
        next_device = clutter_event_get_device (next_event);
      else
        next_device = NULL;

      if (device != NULL && next_device != NULL)
        check_device = TRUE;

      /* Skip consecutive motion events coming from the same device. */
      if (next_event != NULL)
        {
          if (event->type == CLUTTER_MOTION &&
              (next_event->type == CLUTTER_MOTION ||
               next_event->type == CLUTTER_LEAVE) &&
              (!check_device || (device == next_device)))
            {
              CLUTTER_NOTE (EVENT,
                            "Omitting motion event at %d, %d",
                            (int) event->motion.x,
                            (int) event->motion.y);

              if (next_event->type == CLUTTER_MOTION)
                clutter_stage_compress_motion (stage, next_event, event);

              goto next_event;
            }
          else if (event->type == CLUTTER_TOUCH_UPDATE &&
                   next_event->type == CLUTTER_TOUCH_UPDATE &&
                   event->touch.sequence == next_event->touch.sequence &&
                   (!check_device || (device == next_device)))
            {
              CLUTTER_NOTE (EVENT,
                            "Omitting touch update event at %d, %d",
                            (int) event->touch.x,
                            (int) event->touch.y);
              goto next_event;
            }
        }

      _clutter_process_event (event);

    next_event:
      clutter_event_free (event);
    }

  g_list_free (events);

  g_object_unref (stage);
}

void
clutter_stage_queue_actor_relayout (ClutterStage *stage,
                                    ClutterActor *actor)
{
  ClutterStagePrivate *priv = stage->priv;

  if (priv->pending_relayouts == NULL)
    clutter_stage_schedule_update (stage);

  priv->pending_relayouts = g_slist_prepend (priv->pending_relayouts,
                                             g_object_ref (actor));
}

void
clutter_stage_dequeue_actor_relayout (ClutterStage *stage,
                                      ClutterActor *actor)
{
  ClutterStagePrivate *priv = stage->priv;
  GSList *l;

  for (l = priv->pending_relayouts; l; l = l->next)
    {
      ClutterActor *relayout_actor = l->data;

      if (relayout_actor == actor)
        {
          g_object_unref (relayout_actor);
          priv->pending_relayouts =
            g_slist_delete_link (priv->pending_relayouts, l);

          return;
        }
    }
}

static void
clutter_stage_invalidate_views_devices (ClutterStage *stage)
{
  GList *l;

  for (l = clutter_stage_peek_stage_views (stage); l; l = l->next)
    {
      ClutterStageView *view = l->data;

      clutter_stage_view_invalidate_input_devices (view);
    }
}

void
clutter_stage_maybe_relayout (ClutterActor *actor)
{
  ClutterStage *stage = CLUTTER_STAGE (actor);
  ClutterStagePrivate *priv = stage->priv;
  g_autoptr (GSList) stolen_list = NULL;
  GSList *l;
  int count = 0;

  /* No work to do? Avoid the extraneous debug log messages too. */
  if (priv->pending_relayouts == NULL)
    return;

  COGL_TRACE_BEGIN_SCOPED (ClutterStageRelayout, "Layout");

  CLUTTER_NOTE (ACTOR, ">>> Recomputing layout");

  stolen_list = g_steal_pointer (&priv->pending_relayouts);
  for (l = stolen_list; l; l = l->next)
    {
      g_autoptr (ClutterActor) queued_actor = l->data;
      float x = 0.f;
      float y = 0.f;

      if (CLUTTER_ACTOR_IN_RELAYOUT (queued_actor))  /* avoid reentrancy */
        continue;

      if (queued_actor == actor)
        CLUTTER_NOTE (ACTOR, "    Deep relayout of stage %s",
                      _clutter_actor_get_debug_name (queued_actor));
      else
        CLUTTER_NOTE (ACTOR, "    Shallow relayout of actor %s",
                      _clutter_actor_get_debug_name (queued_actor));

      CLUTTER_SET_PRIVATE_FLAGS (queued_actor, CLUTTER_IN_RELAYOUT);

      clutter_actor_get_fixed_position (queued_actor, &x, &y);
      clutter_actor_allocate_preferred_size (queued_actor, x, y);

      CLUTTER_UNSET_PRIVATE_FLAGS (queued_actor, CLUTTER_IN_RELAYOUT);

      count++;
    }

  CLUTTER_NOTE (ACTOR, "<<< Completed recomputing layout of %d subtrees", count);

  if (count)
    clutter_stage_invalidate_views_devices (stage);
}

GSList *
clutter_stage_find_updated_devices (ClutterStage     *stage,
                                    ClutterStageView *view)
{
  ClutterStagePrivate *priv = stage->priv;
  GSList *updating = NULL;
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, priv->pointer_devices);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      PointerDeviceEntry *entry = value;
      ClutterStageView *pointer_view;

      pointer_view = clutter_stage_get_view_at (stage,
                                                entry->coords.x,
                                                entry->coords.y);
      if (!pointer_view)
        continue;
      if (pointer_view != view)
        continue;

      updating = g_slist_prepend (updating, entry->device);
    }

  return updating;
}

void
clutter_stage_finish_layout (ClutterStage *stage)
{
  ClutterActor *actor = CLUTTER_ACTOR (stage);
  ClutterStagePrivate *priv = stage->priv;
  int phase;

  COGL_TRACE_BEGIN_SCOPED (ClutterStageUpdateActorStageViews,
                           "Actor stage-views");

  /* If an actor needs an immediate relayout because its resource scale
   * changed, we give it another chance to allocate correctly before
   * the paint.
   *
   * We're doing the whole thing twice and pass the phase to
   * clutter_actor_finish_layout() to allow actors to detect loops:
   * If the resource scale changes again after the relayout, the new
   * allocation of an actor probably moved the actor onto another stage
   * view, so if an actor sees phase == 1, it can choose a "final" scale.
   */
  for (phase = 0; phase < 2; phase++)
    {
      clutter_actor_finish_layout (actor, phase);

      if (!priv->actor_needs_immediate_relayout)
        break;

      priv->actor_needs_immediate_relayout = FALSE;
      clutter_stage_maybe_relayout (actor);
      clutter_stage_maybe_finish_queue_redraws (stage);
    }

  g_warn_if_fail (!priv->actor_needs_immediate_relayout);
}

void
clutter_stage_update_devices (ClutterStage *stage,
                              GSList       *devices)
{
  ClutterStagePrivate *priv = stage->priv;
  GSList *l;

  COGL_TRACE_BEGIN (ClutterStageUpdateDevices, "UpdateDevices");

  for (l = devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;
      PointerDeviceEntry *entry = NULL;

      entry = g_hash_table_lookup (priv->pointer_devices, device);
      g_assert (entry != NULL);

      clutter_stage_pick_and_update_device (stage,
                                            device,
                                            NULL,
                                            CLUTTER_DEVICE_UPDATE_IGNORE_CACHE |
                                            CLUTTER_DEVICE_UPDATE_EMIT_CROSSING,
                                            entry->coords,
                                            CLUTTER_CURRENT_TIME);
    }
}

static void
clutter_stage_real_queue_relayout (ClutterActor *self)
{
  ClutterStage *stage = CLUTTER_STAGE (self);
  ClutterActorClass *parent_class;

  clutter_stage_queue_actor_relayout (stage, self);

  /* chain up */
  parent_class = CLUTTER_ACTOR_CLASS (clutter_stage_parent_class);
  parent_class->queue_relayout (self);
}

static gboolean
is_full_stage_redraw_queued (ClutterStage *stage)
{
  GList *l;

  for (l = clutter_stage_peek_stage_views (stage); l; l = l->next)
    {
      ClutterStageView *view = l->data;

      if (!clutter_stage_view_has_full_redraw_clip (view))
        return FALSE;
    }

  return TRUE;
}

gboolean
_clutter_stage_has_full_redraw_queued (ClutterStage *stage)
{
  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return FALSE;

  return is_full_stage_redraw_queued (stage);
}

static void
setup_ray_for_coordinates (ClutterStage       *stage,
                           float               x,
                           float               y,
                           graphene_point3d_t *point,
                           graphene_ray_t     *ray)
{
  ClutterStagePrivate *priv = stage->priv;
  graphene_point3d_t camera_position;
  graphene_point3d_t p;
  graphene_vec3_t direction;
  graphene_vec3_t cv;
  graphene_vec3_t v;

  camera_position = GRAPHENE_POINT3D_INIT_ZERO;
  graphene_vec3_init (&cv,
                      camera_position.x,
                      camera_position.y,
                      camera_position.z);

  p = GRAPHENE_POINT3D_INIT (x, y, 0.f);
  graphene_matrix_transform_point3d (&priv->view, &p, &p);

  graphene_vec3_init (&v, p.x, p.y, p.z);
  graphene_vec3_subtract (&v, &cv, &direction);
  graphene_vec3_normalize (&direction, &direction);

  graphene_ray_init (ray, &camera_position, &direction);
  graphene_point3d_init_from_point (point, &p);
}

static ClutterActor *
_clutter_stage_do_pick_on_view (ClutterStage      *stage,
                                float              x,
                                float              y,
                                ClutterPickMode    mode,
                                ClutterStageView  *view,
                                cairo_region_t   **clear_area)
{
  g_autoptr (ClutterPickStack) pick_stack = NULL;
  ClutterPickContext *pick_context;
  graphene_point3d_t p;
  graphene_ray_t ray;
  ClutterActor *actor;

  COGL_TRACE_BEGIN_SCOPED (ClutterStagePickView, "Pick (view)");

  setup_ray_for_coordinates (stage, x, y, &p, &ray);

  pick_context = clutter_pick_context_new_for_view (view, mode, &p, &ray);

  clutter_actor_pick (CLUTTER_ACTOR (stage), pick_context);
  pick_stack = clutter_pick_context_steal_stack (pick_context);
  clutter_pick_context_destroy (pick_context);

  actor = clutter_pick_stack_search_actor (pick_stack, &p, &ray, clear_area);
  return actor ? actor : CLUTTER_ACTOR (stage);
}

/**
 * clutter_stage_get_view_at: (skip)
 */
ClutterStageView *
clutter_stage_get_view_at (ClutterStage *stage,
                           float         x,
                           float         y)
{
  ClutterStagePrivate *priv = stage->priv;
  GList *l;

  for (l = _clutter_stage_window_get_views (priv->impl); l; l = l->next)
    {
      ClutterStageView *view = l->data;
      cairo_rectangle_int_t view_layout;

      clutter_stage_view_get_layout (view, &view_layout);
      if (x >= view_layout.x &&
          x < view_layout.x + view_layout.width &&
          y >= view_layout.y &&
          y < view_layout.y + view_layout.height)
        return view;
    }

  return NULL;
}

static ClutterActor *
_clutter_stage_do_pick (ClutterStage     *stage,
                        float             x,
                        float             y,
                        ClutterPickMode   mode,
                        cairo_region_t  **clear_area)
{
  ClutterActor *actor = CLUTTER_ACTOR (stage);
  ClutterStagePrivate *priv = stage->priv;
  float stage_width, stage_height;
  ClutterStageView *view = NULL;

  priv = stage->priv;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return actor;

  if (G_UNLIKELY (clutter_pick_debug_flags & CLUTTER_DEBUG_NOP_PICKING))
    return actor;

  if (G_UNLIKELY (priv->impl == NULL))
    return actor;

  clutter_actor_get_size (CLUTTER_ACTOR (stage), &stage_width, &stage_height);
  if (x < 0 || x >= stage_width || y < 0 || y >= stage_height)
    return actor;

  view = clutter_stage_get_view_at (stage, x, y);
  if (view)
    return _clutter_stage_do_pick_on_view (stage, x, y, mode, view, clear_area);

  return actor;
}

static void
clutter_stage_real_apply_transform (ClutterActor      *stage,
                                    graphene_matrix_t *matrix)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (stage)->priv;

  /* FIXME: we probably shouldn't be explicitly resetting the matrix
   * here... */
  graphene_matrix_init_from_matrix (matrix, &priv->view);
}

static void
clutter_stage_constructed (GObject *gobject)
{
  ClutterStage *self = CLUTTER_STAGE (gobject);
  ClutterStageManager *stage_manager;

  stage_manager = clutter_stage_manager_get_default ();

  /* this will take care to sinking the floating reference */
  _clutter_stage_manager_add_stage (stage_manager, self);

  G_OBJECT_CLASS (clutter_stage_parent_class)->constructed (gobject);
}

static void
clutter_stage_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterStage *stage = CLUTTER_STAGE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      clutter_stage_set_title (stage, g_value_get_string (value));
      break;

    case PROP_KEY_FOCUS:
      clutter_stage_set_key_focus (stage, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_stage_get_property (GObject    *gobject,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_PERSPECTIVE:
      g_value_set_boxed (value, &priv->perspective);
      break;

    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    case PROP_KEY_FOCUS:
      g_value_set_object (value, priv->key_focused_actor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_stage_dispose (GObject *object)
{
  ClutterStage        *stage = CLUTTER_STAGE (object);
  ClutterStagePrivate *priv = stage->priv;
  ClutterStageManager *stage_manager;

  clutter_actor_hide (CLUTTER_ACTOR (object));

  _clutter_clear_events_queue ();

  if (priv->impl != NULL)
    {
      CLUTTER_NOTE (BACKEND, "Disposing of the stage implementation");

      if (CLUTTER_ACTOR_IS_REALIZED (object))
        _clutter_stage_window_unrealize (priv->impl);

      g_object_unref (priv->impl);
      priv->impl = NULL;
    }

  clutter_actor_destroy_all_children (CLUTTER_ACTOR (object));

  g_hash_table_remove_all (priv->pending_queue_redraws);

  g_slist_free_full (priv->pending_relayouts,
                     (GDestroyNotify) g_object_unref);
  priv->pending_relayouts = NULL;

  /* this will release the reference on the stage */
  stage_manager = clutter_stage_manager_get_default ();
  _clutter_stage_manager_remove_stage (stage_manager, stage);

  g_hash_table_remove_all (priv->pointer_devices);
  g_hash_table_remove_all (priv->touch_sequences);

  G_OBJECT_CLASS (clutter_stage_parent_class)->dispose (object);
}

static void
clutter_stage_finalize (GObject *object)
{
  ClutterStage *stage = CLUTTER_STAGE (object);
  ClutterStagePrivate *priv = stage->priv;

  g_queue_foreach (priv->event_queue, (GFunc) clutter_event_free, NULL);
  g_queue_free (priv->event_queue);

  g_hash_table_destroy (priv->pointer_devices);
  g_hash_table_destroy (priv->touch_sequences);

  g_free (priv->title);

  g_array_free (priv->paint_volume_stack, TRUE);

  G_OBJECT_CLASS (clutter_stage_parent_class)->finalize (object);
}

static void
clutter_stage_real_paint_view (ClutterStage         *stage,
                               ClutterStageView     *view,
                               const cairo_region_t *redraw_clip)
{
  clutter_stage_do_paint_view (stage, view, redraw_clip);
}

static void
clutter_stage_paint (ClutterActor        *actor,
                     ClutterPaintContext *paint_context)
{
  ClutterStageView *view;

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->paint (actor, paint_context);

  view = clutter_paint_context_get_stage_view (paint_context);
  if (view &&
      G_UNLIKELY (clutter_paint_debug_flags & CLUTTER_DEBUG_PAINT_MAX_RENDER_TIME))
    {
      cairo_rectangle_int_t view_layout;
      ClutterFrameClock *frame_clock;
      g_autoptr (GString) string = NULL;
      PangoLayout *layout;
      PangoRectangle logical;
      ClutterColor color;
      g_autoptr (ClutterPaintNode) node = NULL;
      ClutterActorBox box;

      clutter_stage_view_get_layout (view, &view_layout);
      frame_clock = clutter_stage_view_get_frame_clock (view);

      string = clutter_frame_clock_get_max_render_time_debug_info (frame_clock);

      layout = clutter_actor_create_pango_layout (actor, string->str);
      pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
      pango_layout_get_pixel_extents (layout, NULL, &logical);

      clutter_color_init (&color, 255, 255, 255, 255);
      node = clutter_text_node_new (layout, &color);

      box.x1 = view_layout.x;
      box.y1 = view_layout.y + 30;
      box.x2 = box.x1 + logical.width;
      box.y2 = box.y1 + logical.height;
      clutter_paint_node_add_rectangle (node, &box);

      clutter_paint_node_paint (node, paint_context);

      g_object_unref (layout);
    }
}

static void
clutter_stage_class_init (ClutterStageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->constructed = clutter_stage_constructed;
  gobject_class->set_property = clutter_stage_set_property;
  gobject_class->get_property = clutter_stage_get_property;
  gobject_class->dispose = clutter_stage_dispose;
  gobject_class->finalize = clutter_stage_finalize;

  actor_class->allocate = clutter_stage_allocate;
  actor_class->get_preferred_width = clutter_stage_get_preferred_width;
  actor_class->get_preferred_height = clutter_stage_get_preferred_height;
  actor_class->get_paint_volume = clutter_stage_get_paint_volume;
  actor_class->realize = clutter_stage_realize;
  actor_class->unrealize = clutter_stage_unrealize;
  actor_class->show = clutter_stage_show;
  actor_class->hide = clutter_stage_hide;
  actor_class->hide_all = clutter_stage_hide_all;
  actor_class->queue_relayout = clutter_stage_real_queue_relayout;
  actor_class->apply_transform = clutter_stage_real_apply_transform;
  actor_class->paint = clutter_stage_paint;

  klass->paint_view = clutter_stage_real_paint_view;

  /**
   * ClutterStage:perspective:
   *
   * The parameters used for the perspective projection from 3D
   * coordinates to 2D
   *
   * Since: 0.8
   */
  obj_props[PROP_PERSPECTIVE] =
      g_param_spec_boxed ("perspective",
                          P_("Perspective"),
                          P_("Perspective projection parameters"),
                          CLUTTER_TYPE_PERSPECTIVE,
                          CLUTTER_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * ClutterStage:title:
   *
   * The stage's title - usually displayed in stage windows title decorations.
   *
   * Since: 0.4
   */
  obj_props[PROP_TITLE] =
      g_param_spec_string ("title",
                           P_("Title"),
                           P_("Stage Title"),
                           NULL,
                           CLUTTER_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY);

  /**
   * ClutterStage:key-focus:
   *
   * The #ClutterActor that will receive key events from the underlying
   * windowing system.
   *
   * If %NULL, the #ClutterStage will receive the events.
   *
   * Since: 1.2
   */
  obj_props[PROP_KEY_FOCUS] =
      g_param_spec_object ("key-focus",
                           P_("Key Focus"),
                           P_("The currently key focused actor"),
                           CLUTTER_TYPE_ACTOR,
                           CLUTTER_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);

  /**
   * ClutterStage::activate:
   * @stage: the stage which was activated
   *
   * The ::activate signal is emitted when the stage receives key focus
   * from the underlying window system.
   *
   * Since: 0.6
   */
  stage_signals[ACTIVATE] =
    g_signal_new (I_("activate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, activate),
		  NULL, NULL, NULL,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::deactivate:
   * @stage: the stage which was deactivated
   *
   * The ::deactivate signal is emitted when the stage loses key focus
   * from the underlying window system.
   *
   * Since: 0.6
   */
  stage_signals[DEACTIVATE] =
    g_signal_new (I_("deactivate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, deactivate),
		  NULL, NULL, NULL,
		  G_TYPE_NONE, 0);

  /**
   * ClutterStage::before-update:
   * @stage: the #ClutterStage
   * @view: a #ClutterStageView
   */
  stage_signals[BEFORE_UPDATE] =
    g_signal_new (I_("before-update"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_STAGE_VIEW);
  /**
   * ClutterStage::prepare-frame:
   * @stage: the stage that received the event
   * @view: a #ClutterStageView
   *
   * The ::prepare-frame signal is emitted after the stage is updated,
   * before the stage is painted, even if it will not be painted.
   */
  stage_signals[PREPARE_FRAME] =
    g_signal_new (I_("prepare-frame"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_STAGE_VIEW);

  /**
   * ClutterStage::before-paint:
   * @stage: the stage that received the event
   * @view: a #ClutterStageView
   *
   * The ::before-paint signal is emitted before the stage is painted.
   */
  stage_signals[BEFORE_PAINT] =
    g_signal_new (I_("before-paint"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterStageClass, before_paint),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_STAGE_VIEW);
  /**
   * ClutterStage::after-paint:
   * @stage: the stage that received the event
   * @view: a #ClutterStageView
   *
   * The ::after-paint signal is emitted after the stage is painted,
   * but before the results are displayed on the screen.
   *
   * Since: 1.20
   */
  stage_signals[AFTER_PAINT] =
    g_signal_new (I_("after-paint"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, /* no corresponding vfunc */
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_STAGE_VIEW);

  /**
   * ClutterStage::after-update:
   * @stage: the #ClutterStage
   * @view: a #ClutterStageView
   */
  stage_signals[AFTER_UPDATE] =
    g_signal_new (I_("after-update"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_STAGE_VIEW);

  /**
   * ClutterStage::paint-view:
   * @stage: the stage that received the event
   * @view: a #ClutterStageView
   * @redraw_clip: a #cairo_region_t with the redraw clip
   *
   * The ::paint-view signal is emitted before a #ClutterStageView is being
   * painted.
   *
   * The view is painted in the default handler. Hence, if you want to perform
   * some action after the view is painted, like reading the contents of the
   * framebuffer, use g_signal_connect_after() or pass %G_CONNECT_AFTER.
   */
  stage_signals[PAINT_VIEW] =
    g_signal_new (I_("paint-view"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterStageClass, paint_view),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  CLUTTER_TYPE_STAGE_VIEW,
                  CAIRO_GOBJECT_TYPE_REGION);

  /**
   * ClutterStage::presented: (skip)
   * @stage: the stage that received the event
   * @view: the #ClutterStageView presented
   * @frame_info: a #ClutterFrameInfo
   *
   * Signals that the #ClutterStage was presented on the screen to the user.
   */
  stage_signals[PRESENTED] =
    g_signal_new (I_("presented"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  CLUTTER_TYPE_STAGE_VIEW,
                  G_TYPE_POINTER);

 /**
   * ClutterStage::gl-video-memory-purged: (skip)
   * @stage: the stage that received the event
   *
   * Signals that the underlying GL driver has had its texture memory purged
   * so anything presently held in texture memory is now invalidated, and
   * likely corrupt. It needs redrawing.
   */
  stage_signals[GL_VIDEO_MEMORY_PURGED] =
    g_signal_new (I_("gl-video-memory-purged"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  klass->activate = clutter_stage_real_activate;
  klass->deactivate = clutter_stage_real_deactivate;
}

static void
clutter_stage_init (ClutterStage *self)
{
  cairo_rectangle_int_t geom = { 0, };
  ClutterStagePrivate *priv;
  ClutterStageWindow *impl;
  ClutterBackend *backend;
  GError *error;

  /* a stage is a top-level object */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_IS_TOPLEVEL);

  self->priv = priv = clutter_stage_get_instance_private (self);

  CLUTTER_NOTE (BACKEND, "Creating stage from the default backend");
  backend = clutter_get_default_backend ();

  error = NULL;
  impl = _clutter_backend_create_stage (backend, self, &error);

  if (G_LIKELY (impl != NULL))
    {
      _clutter_stage_set_window (self, impl);
      _clutter_stage_window_get_geometry (priv->impl, &geom);
    }
  else
    {
      if (error != NULL)
        {
          g_critical ("Unable to create a new stage implementation: %s",
                      error->message);
          g_error_free (error);
        }
      else
        g_critical ("Unable to create a new stage implementation.");
    }

  priv->event_queue = g_queue_new ();

  priv->pointer_devices =
    g_hash_table_new_full (NULL, NULL,
                           NULL, (GDestroyNotify) free_pointer_device_entry);
  priv->touch_sequences =
    g_hash_table_new_full (NULL, NULL,
                           NULL, (GDestroyNotify) free_pointer_device_entry);

  clutter_actor_set_background_color (CLUTTER_ACTOR (self),
                                      &default_stage_color);

  clutter_stage_queue_actor_relayout (self, CLUTTER_ACTOR (self));

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);
  clutter_stage_set_title (self, g_get_prgname ());
  clutter_stage_set_key_focus (self, NULL);
  clutter_stage_set_viewport (self, geom.width, geom.height);

  priv->pending_queue_redraws =
    g_hash_table_new_full (NULL, NULL,
                           g_object_unref,
                           (GDestroyNotify) free_queue_redraw_entry);

  priv->paint_volume_stack =
    g_array_new (FALSE, FALSE, sizeof (ClutterPaintVolume));
}

static void
clutter_stage_set_perspective (ClutterStage       *stage,
                               ClutterPerspective *perspective)
{
  ClutterStagePrivate *priv = stage->priv;

  if (priv->perspective.fovy == perspective->fovy &&
      priv->perspective.aspect == perspective->aspect &&
      priv->perspective.z_near == perspective->z_near &&
      priv->perspective.z_far == perspective->z_far)
    return;

  priv->perspective = *perspective;

  graphene_matrix_init_perspective (&priv->projection,
                                    priv->perspective.fovy,
                                    priv->perspective.aspect,
                                    priv->perspective.z_near,
                                    priv->perspective.z_far);
  graphene_matrix_inverse (&priv->projection,
                           &priv->inverse_projection);

  _clutter_stage_dirty_projection (stage);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_get_perspective:
 * @stage: A #ClutterStage
 * @perspective: (out caller-allocates) (allow-none): return location for a
 *   #ClutterPerspective
 *
 * Retrieves the stage perspective.
 */
void
clutter_stage_get_perspective (ClutterStage       *stage,
                               ClutterPerspective *perspective)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);

  *perspective = stage->priv->perspective;
}

/*
 * clutter_stage_get_projection_matrix:
 * @stage: A #ClutterStage
 * @projection: return location for a #graphene_matrix_t representing the
 *              perspective projection applied to actors on the given
 *              @stage.
 *
 * Retrieves the @stage's projection matrix. This is derived from the
 * current perspective.
 *
 * Since: 1.6
 */
void
_clutter_stage_get_projection_matrix (ClutterStage      *stage,
                                      graphene_matrix_t *projection)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (projection != NULL);

  *projection = stage->priv->projection;
}

/* This simply provides a simple mechanism for us to ensure that
 * the projection matrix gets re-asserted before painting.
 *
 * This is used when switching between multiple stages */
void
_clutter_stage_dirty_projection (ClutterStage *stage)
{
  ClutterStagePrivate *priv;
  GList *l;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  for (l = _clutter_stage_window_get_views (priv->impl); l; l = l->next)
    {
      ClutterStageView *view = l->data;

      clutter_stage_view_invalidate_projection (view);
    }
}

/*
 * clutter_stage_set_viewport:
 * @stage: A #ClutterStage
 * @width: The width to render the stage at, in window coordinates
 * @height: The height to render the stage at, in window coordinates
 *
 * Sets the stage viewport. The viewport defines a final scale and
 * translation of your rendered stage and actors. This lets you render
 * your stage into a subregion of the stage window or you could use it to
 * pan a subregion of the stage if your stage window is smaller then
 * the stage. (XXX: currently this isn't possible)
 *
 * Unlike a scale and translation done using the modelview matrix this
 * is done after everything has had perspective projection applied, so
 * for example if you were to pan across a subregion of the stage using
 * the viewport then you would not see a change in perspective for the
 * actors on the stage.
 *
 * Normally the stage viewport will automatically track the size of the
 * stage window with no offset so the stage will fill your window. This
 * behaviour can be changed with the "viewport-mimics-window" property
 * which will automatically be set to FALSE if you use this API. If
 * you want to revert to the original behaviour then you should set
 * this property back to %TRUE using
 * clutter_stage_set_viewport_mimics_window().
 * (XXX: If we were to make this API public then we might want to do
 *  add that property.)
 *
 * Note: currently this interface only support integer precision
 * offsets and sizes for viewports but the interface takes floats because
 * OpenGL 4.0 has introduced floating point viewports which we might
 * want to expose via this API eventually.
 *
 * Since: 1.6
 */
static void
clutter_stage_set_viewport (ClutterStage *stage,
                            float         width,
                            float         height)
{
  ClutterStagePrivate *priv;
  float x, y;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  x = 0.f;
  y = 0.f;
  width = roundf (width);
  height = roundf (height);

  if (x == priv->viewport[0] &&
      y == priv->viewport[1] &&
      width == priv->viewport[2] &&
      height == priv->viewport[3])
    return;

  priv->viewport[0] = x;
  priv->viewport[1] = y;
  priv->viewport[2] = width;
  priv->viewport[3] = height;

  clutter_stage_update_view_perspective (stage);
  _clutter_stage_dirty_viewport (stage);

  queue_full_redraw (stage);
}

/* This simply provides a simple mechanism for us to ensure that
 * the viewport gets re-asserted before next painting.
 *
 * This is used when switching between multiple stages */
void
_clutter_stage_dirty_viewport (ClutterStage *stage)
{
  ClutterStagePrivate *priv;
  GList *l;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  for (l = _clutter_stage_window_get_views (priv->impl); l; l = l->next)
    {
      ClutterStageView *view = l->data;

      clutter_stage_view_invalidate_viewport (view);
    }
}

/*
 * clutter_stage_get_viewport:
 * @stage: A #ClutterStage
 * @x: A location for the X position where the stage is rendered,
 *     in window coordinates.
 * @y: A location for the Y position where the stage is rendered,
 *     in window coordinates.
 * @width: A location for the width the stage is rendered at,
 *         in window coordinates.
 * @height: A location for the height the stage is rendered at,
 *          in window coordinates.
 *
 * Returns the viewport offset and size set using
 * clutter_stage_set_viewport() or if the "viewport-mimics-window" property
 * is TRUE then @x and @y will be set to 0 and @width and @height will equal
 * the width if the stage window.
 *
 * Since: 1.6
 */
void
_clutter_stage_get_viewport (ClutterStage *stage,
                             float        *x,
                             float        *y,
                             float        *width,
                             float        *height)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  *x = priv->viewport[0];
  *y = priv->viewport[1];
  *width = priv->viewport[2];
  *height = priv->viewport[3];
}

/**
 * clutter_stage_read_pixels:
 * @stage: A #ClutterStage
 * @x: x coordinate of the first pixel that is read from stage
 * @y: y coordinate of the first pixel that is read from stage
 * @width: Width dimension of pixels to be read, or -1 for the
 *   entire stage width
 * @height: Height dimension of pixels to be read, or -1 for the
 *   entire stage height
 *
 * Makes a screenshot of the stage in RGBA 8bit data, returns a
 * linear buffer with @width * 4 as rowstride.
 *
 * The alpha data contained in the returned buffer is driver-dependent,
 * and not guaranteed to hold any sensible value.
 *
 * Return value: (transfer full) (array): a pointer to newly allocated memory with the buffer
 *   or %NULL if the read failed. Use g_free() on the returned data
 *   to release the resources it has allocated.
 */
guchar *
clutter_stage_read_pixels (ClutterStage *stage,
                           gint          x,
                           gint          y,
                           gint          width,
                           gint          height)
{
  ClutterStagePrivate *priv;
  ClutterActorBox box;
  GList *l;
  ClutterStageView *view;
  cairo_region_t *clip;
  cairo_rectangle_int_t clip_rect;
  CoglFramebuffer *framebuffer;
  float view_scale;
  float pixel_width;
  float pixel_height;
  uint8_t *pixels;

  COGL_TRACE_BEGIN_SCOPED (ClutterStageReadPixels, "Read Pixels");

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  priv = stage->priv;

  clutter_actor_get_allocation_box (CLUTTER_ACTOR (stage), &box);

  if (width < 0)
    width = ceilf (box.x2 - box.x1);

  if (height < 0)
    height = ceilf (box.y2 - box.y1);

  l = _clutter_stage_window_get_views (priv->impl);

  if (!l)
    return NULL;

  /* XXX: We only read the first view. Needs different API for multi view screen
   * capture. */
  view = l->data;

  clutter_stage_view_get_layout (view, &clip_rect);
  clip = cairo_region_create_rectangle (&clip_rect);
  cairo_region_intersect_rectangle (clip,
                                    &(cairo_rectangle_int_t) {
                                      .x = x,
                                      .y = y,
                                      .width = width,
                                      .height = height,
                                    });
  cairo_region_get_extents (clip, &clip_rect);

  if (clip_rect.width == 0 || clip_rect.height == 0)
    {
      cairo_region_destroy (clip);
      return NULL;
    }

  framebuffer = clutter_stage_view_get_framebuffer (view);
  clutter_stage_do_paint_view (stage, view, clip);

  cairo_region_destroy (clip);

  view_scale = clutter_stage_view_get_scale (view);
  pixel_width = roundf (clip_rect.width * view_scale);
  pixel_height = roundf (clip_rect.height * view_scale);

  pixels = g_malloc0 (pixel_width * pixel_height * 4);
  cogl_framebuffer_read_pixels (framebuffer,
                                clip_rect.x * view_scale,
                                clip_rect.y * view_scale,
                                pixel_width, pixel_height,
                                COGL_PIXEL_FORMAT_RGBA_8888,
                                pixels);

  return pixels;
}

/**
 * clutter_stage_get_actor_at_pos:
 * @stage: a #ClutterStage
 * @pick_mode: how the scene graph should be painted
 * @x: X coordinate to check
 * @y: Y coordinate to check
 *
 * Checks the scene at the coordinates @x and @y and returns a pointer
 * to the #ClutterActor at those coordinates. The result is the actor which
 * would be at the specified location on the next redraw, and is not
 * necessarily that which was there on the previous redraw. This allows the
 * function to perform chronologically correctly after any queued changes to
 * the scene, and even if nothing has been drawn.
 *
 * By using @pick_mode it is possible to control which actors will be
 * painted and thus available.
 *
 * Return value: (transfer none): the actor at the specified coordinates,
 *   if any
 */
ClutterActor *
clutter_stage_get_actor_at_pos (ClutterStage    *stage,
                                ClutterPickMode  pick_mode,
                                float            x,
                                float            y)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return _clutter_stage_do_pick (stage, x, y, pick_mode, NULL);
}

/**
 * clutter_stage_set_title:
 * @stage: A #ClutterStage
 * @title: A utf8 string for the stage windows title.
 *
 * Sets the stage title.
 *
 * Since: 0.4
 **/
void
clutter_stage_set_title (ClutterStage       *stage,
			 const gchar        *title)
{
  ClutterStagePrivate *priv;
  ClutterStageWindow *impl;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  g_free (priv->title);
  priv->title = g_strdup (title);

  impl = CLUTTER_STAGE_WINDOW (priv->impl);
  if (CLUTTER_STAGE_WINDOW_GET_IFACE(impl)->set_title != NULL)
    CLUTTER_STAGE_WINDOW_GET_IFACE (impl)->set_title (impl, priv->title);

  g_object_notify_by_pspec (G_OBJECT (stage), obj_props[PROP_TITLE]);
}

/**
 * clutter_stage_get_title:
 * @stage: A #ClutterStage
 *
 * Gets the stage title.
 *
 * Return value: pointer to the title string for the stage. The
 * returned string is owned by the actor and should not
 * be modified or freed.
 *
 * Since: 0.4
 **/
const gchar *
clutter_stage_get_title (ClutterStage       *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return stage->priv->title;
}

/**
 * clutter_stage_set_key_focus:
 * @stage: the #ClutterStage
 * @actor: (allow-none): the actor to set key focus to, or %NULL
 *
 * Sets the key focus on @actor. An actor with key focus will receive
 * all the key events. If @actor is %NULL, the stage will receive
 * focus.
 *
 * Since: 0.6
 */
void
clutter_stage_set_key_focus (ClutterStage *stage,
			     ClutterActor *actor)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (actor == NULL || CLUTTER_IS_ACTOR (actor));

  priv = stage->priv;

  /* normalize the key focus. NULL == stage */
  if (actor == CLUTTER_ACTOR (stage))
    actor = NULL;

  /* avoid emitting signals and notifications if we're setting the same
   * actor as the key focus
   */
  if (priv->key_focused_actor == actor)
    return;

  if (priv->key_focused_actor != NULL)
    {
      ClutterActor *old_focused_actor;

      old_focused_actor = priv->key_focused_actor;

      /* set key_focused_actor to NULL before emitting the signal or someone
       * might hide the previously focused actor in the signal handler
       */
      priv->key_focused_actor = NULL;

      _clutter_actor_set_has_key_focus (old_focused_actor, FALSE);
    }
  else
    _clutter_actor_set_has_key_focus (CLUTTER_ACTOR (stage), FALSE);

  /* Note, if someone changes key focus in focus-out signal handler we'd be
   * overriding the latter call below moving the focus where it was originally
   * intended. The order of events would be:
   *   1st focus-out, 2nd focus-out (on stage), 2nd focus-in, 1st focus-in
   */
  priv->key_focused_actor = actor;

  /* If the key focused actor is allowed to receive key events according
   * to the given grab (or there is none) set key focus on it, otherwise
   * key focus is delayed until there are grabbing conditions that allow
   * it to get key focus.
   */
  if (!priv->topmost_grab ||
      priv->topmost_grab->actor == CLUTTER_ACTOR (stage) ||
      priv->topmost_grab->actor == actor ||
      (actor && clutter_actor_contains (priv->topmost_grab->actor, actor)))
    {
      if (actor != NULL)
        _clutter_actor_set_has_key_focus (actor, TRUE);
      else
        _clutter_actor_set_has_key_focus (CLUTTER_ACTOR (stage), TRUE);
    }

  g_object_notify_by_pspec (G_OBJECT (stage), obj_props[PROP_KEY_FOCUS]);
}

/**
 * clutter_stage_get_key_focus:
 * @stage: the #ClutterStage
 *
 * Retrieves the actor that is currently under key focus.
 *
 * Return value: (transfer none): the actor with key focus, or the stage
 *
 * Since: 0.6
 */
ClutterActor *
clutter_stage_get_key_focus (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  if (stage->priv->key_focused_actor)
    return stage->priv->key_focused_actor;

  return CLUTTER_ACTOR (stage);
}

/*** Perspective boxed type ******/

static gpointer
clutter_perspective_copy (gpointer data)
{
  if (G_LIKELY (data))
    return g_memdup2 (data, sizeof (ClutterPerspective));

  return NULL;
}

static void
clutter_perspective_free (gpointer data)
{
  if (G_LIKELY (data))
    g_free (data);
}

G_DEFINE_BOXED_TYPE (ClutterPerspective, clutter_perspective,
                     clutter_perspective_copy,
                     clutter_perspective_free);

/**
 * clutter_stage_ensure_viewport:
 * @stage: a #ClutterStage
 *
 * Ensures that the GL viewport is updated with the current
 * stage window size.
 *
 * This function will queue a redraw of @stage.
 *
 * This function should not be called by applications; it is used
 * when embedding a #ClutterStage into a toolkit with another
 * windowing system, like GTK+.
 *
 * Since: 1.0
 */
void
clutter_stage_ensure_viewport (ClutterStage *stage)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  _clutter_stage_dirty_viewport (stage);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

# define _DEG_TO_RAD(d)         ((d) * ((float) G_PI / 180.0f))

/* This calculates a distance into the view frustum to position the
 * stage so there is a decent amount of space to position geometry
 * between the stage and the near clipping plane.
 *
 * Some awkward issues with this problem are:
 * - It's not possible to have a gap as large as the stage size with
 *   a fov > 53° which is basically always the case since the default
 *   fov is 60°.
 *    - This can be deduced if you consider that this requires a
 *      triangle as wide as it is deep to fit in the frustum in front
 *      of the z_near plane. That triangle will always have an angle
 *      of 53.13° at the point sitting on the z_near plane, but if the
 *      frustum has a wider fov angle the left/right clipping planes
 *      can never converge with the two corners of our triangle no
 *      matter what size the triangle has.
 * - With a fov > 53° there is a trade off between maximizing the gap
 *   size relative to the stage size but not losing depth precision.
 * - Perhaps ideally we wouldn't just consider the fov on the y-axis
 *   that is usually used to define a perspective, we would consider
 *   the fov of the axis with the largest stage size so the gap would
 *   accommodate that size best.
 *
 * After going around in circles a few times with how to handle these
 * issues, we decided in the end to go for the simplest solution to
 * start with instead of an elaborate function that handles arbitrary
 * fov angles that we currently have no use-case for.
 *
 * The solution assumes a fovy of 60° and for that case gives a gap
 * that's 85% of the stage height. We can consider more elaborate
 * functions if necessary later.
 *
 * One guide we had to steer the gap size we support is the
 * interactive test, test-texture-quality which expects to animate an
 * actor to +400 on the z axis with a stage size of 640x480. A gap
 * that's 85% of the stage height gives a gap of 408 in that case.
 */
static float
calculate_z_translation (float z_near)
{
  /* This solution uses fairly basic trigonometry, but is seems worth
   * clarifying the particular geometry we are looking at in-case
   * anyone wants to develop this further later. Not sure how well an
   * ascii diagram is going to work :-)
   *
   *    |--- stage_height ---|
   *    |     stage line     |
   *   ╲━━━━━━━━━━━━━━━━━━━━━╱------------
   *    ╲.  (2)   │        .╱       |   |
   *   C ╲ .      │      . ╱     gap|   |
   * =0.5°╲  . a  │    .  ╱         |   |
   *      b╲(1). D│  .   ╱          |   |
   *        ╲   B.│.    ╱near plane |   |
   *      A= ╲━━━━━━━━━╱-------------   |
   *     120° ╲ c │   ╱  |            z_2d
   *           ╲  │  ╱  z_near          |
   *       left ╲ │ ╱    |              |
   *       clip  60°fovy |              |
   *       plane  ╳----------------------
   *              |
   *              |
   *         origin line
   *
   * The area of interest is the triangle labeled (1) at the top left
   * marked with the ... line (a) from where the origin line crosses
   * the near plane to the top left where the stage line cross the
   * left clip plane.
   *
   * The sides of the triangle are a, b and c and the corresponding
   * angles opposite those sides are A, B and C.
   *
   * The angle of C is what trades off the gap size we have relative
   * to the stage size vs the depth precision we have.
   *
   * As mentioned above we arove at the angle for C is by working
   * backwards from how much space we want for test-texture-quality.
   * With a stage_height of 480 we want a gap > 400, ideally we also
   * wanted a somewhat round number as a percentage of the height for
   * documentation purposes. ~87% or a gap of ~416 is the limit
   * because that's where we approach a C angle of 0° and effectively
   * loose all depth precision.
   *
   * So for our test app with a stage_height of 480 if we aim for a
   * gap of 408 (85% of 480) we can get the angle D as
   * atan (stage_height/2/408) = 30.5°.
   *
   * That gives us the angle for B as 90° - 30.5° = 59.5°
   *
   * We can already determine that A has an angle of (fovy/2 + 90°) =
   * 120°
   *
   * Therefore C = 180 - A - B = 0.5°
   *
   * The length of c = z_near * tan (30°)
   *
   * Now we can use the rule a/SinA = c/SinC to calculate the
   * length of a. After some rearranging that gives us:
   *
   *      a              c
   *  ----------  =  ----------
   *  sin (120°)     sin (0.5°)
   *
   *      c * sin (120°)
   *  a = --------------
   *        sin (0.5°)
   *
   * And with that we can determine z_2d = cos (D) * a =
   * cos (30.5°) * a + z_near:
   *
   *         c * sin (120°) * cos (30.5°)
   *  z_2d = --------------------------- + z_near
   *                 sin (0.5°)
   */

   /* We expect the compiler should boil this down to z_near * CONSTANT
    * already, but just in case we use precomputed constants
    */
#if 0
# define A      tanf (_DEG_TO_RAD (30.f))
# define B      sinf (_DEG_TO_RAD (120.f))
# define C      cosf (_DEG_TO_RAD (30.5f))
# define D      sinf (_DEG_TO_RAD (.5f))
#else
# define A      0.57735025882720947265625f
# define B      0.866025388240814208984375f
# define C      0.86162912845611572265625f
# define D      0.00872653536498546600341796875f
#endif

  return z_near
       * A * B * C
       / D
       + z_near;
}

static void
view_2d_in_perspective (graphene_matrix_t *matrix,
                        float              fov_y,
                        float              aspect,
                        float              z_near,
                        float              z_2d,
                        float              width_2d,
                        float              height_2d)
{
  float top = z_near * tan (fov_y * G_PI / 360.0);
  float left = -top * aspect;
  float right = top * aspect;
  float bottom = -top;

  float left_2d_plane = left / z_near * z_2d;
  float right_2d_plane = right / z_near * z_2d;
  float bottom_2d_plane = bottom / z_near * z_2d;
  float top_2d_plane = top / z_near * z_2d;

  float width_2d_start = right_2d_plane - left_2d_plane;
  float height_2d_start = top_2d_plane - bottom_2d_plane;

  /* Factors to scale from framebuffer geometry to frustum
   * cross-section geometry. */
  float width_scale = width_2d_start / width_2d;
  float height_scale = height_2d_start / height_2d;

  graphene_matrix_init_scale (matrix, width_scale, -height_scale, width_scale);
  graphene_matrix_translate (matrix,
                             &GRAPHENE_POINT3D_INIT (left_2d_plane,
                                                     top_2d_plane,
                                                     -z_2d));
}

static void
clutter_stage_update_view_perspective (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;
  ClutterPerspective perspective;
  float z_2d;

  perspective = priv->perspective;

  perspective.fovy = 60.0; /* 60 Degrees */
  perspective.z_near = 1.0;
  perspective.aspect = priv->viewport[2] / priv->viewport[3];
  z_2d = calculate_z_translation (perspective.z_near);

  /* NB: z_2d is only enough room for 85% of the stage_height between
   * the stage and the z_near plane. For behind the stage plane we
   * want a more consistent gap of 10 times the stage_height before
   * hitting the far plane so we calculate that relative to the final
   * height of the stage plane at the z_2d_distance we got... */
  perspective.z_far = z_2d +
    tanf (_DEG_TO_RAD (perspective.fovy / 2.0f)) * z_2d * 20.0f;

  clutter_stage_set_perspective (stage, &perspective);

  view_2d_in_perspective (&priv->view,
                          perspective.fovy,
                          perspective.aspect,
                          perspective.z_near,
                          z_2d,
                          priv->viewport[2],
                          priv->viewport[3]);

  clutter_actor_invalidate_transform (CLUTTER_ACTOR (stage));
}

void
_clutter_stage_maybe_setup_viewport (ClutterStage     *stage,
                                     ClutterStageView *view)
{
  ClutterStagePrivate *priv = stage->priv;

  if (clutter_stage_view_is_dirty_viewport (view))
    {
      cairo_rectangle_int_t view_layout;
      float fb_scale;
      float viewport_offset_x;
      float viewport_offset_y;
      float viewport_x;
      float viewport_y;
      float viewport_width;
      float viewport_height;

      CLUTTER_NOTE (PAINT,
                    "Setting up the viewport { w:%f, h:%f }",
                    priv->viewport[2],
                    priv->viewport[3]);

      fb_scale = clutter_stage_view_get_scale (view);
      clutter_stage_view_get_layout (view, &view_layout);

      viewport_offset_x = view_layout.x * fb_scale;
      viewport_offset_y = view_layout.y * fb_scale;
      viewport_x = roundf (priv->viewport[0] * fb_scale - viewport_offset_x);
      viewport_y = roundf (priv->viewport[1] * fb_scale - viewport_offset_y);
      viewport_width = roundf (priv->viewport[2] * fb_scale);
      viewport_height = roundf (priv->viewport[3] * fb_scale);

      clutter_stage_view_set_viewport (view,
                                       viewport_x, viewport_y,
                                       viewport_width, viewport_height);
    }

  if (clutter_stage_view_is_dirty_projection (view))
    clutter_stage_view_set_projection (view, &priv->projection);
}

#undef _DEG_TO_RAD

/**
 * clutter_stage_is_redraw_queued_on_view: (skip)
 */
gboolean
clutter_stage_is_redraw_queued_on_view (ClutterStage     *stage,
                                        ClutterStageView *view)
{
  clutter_stage_maybe_finish_queue_redraws (stage);

  return clutter_stage_view_has_redraw_clip (view);
}

void
_clutter_stage_set_window (ClutterStage       *stage,
                           ClutterStageWindow *stage_window)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (stage_window));

  if (stage->priv->impl != NULL)
    g_object_unref (stage->priv->impl);

  stage->priv->impl = stage_window;
}

ClutterStageWindow *
_clutter_stage_get_window (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return CLUTTER_STAGE_WINDOW (stage->priv->impl);
}

ClutterStageWindow *
_clutter_stage_get_default_window (void)
{
  ClutterStageManager *manager = clutter_stage_manager_get_default ();
  ClutterStage *stage;

  stage = clutter_stage_manager_get_default_stage (manager);
  if (stage == NULL)
    return NULL;

  return _clutter_stage_get_window (stage);
}

/**
 * clutter_stage_schedule_update:
 * @stage: a #ClutterStage actor
 *
 * Schedules a redraw of the #ClutterStage at the next optimal timestamp.
 */
void
clutter_stage_schedule_update (ClutterStage *stage)
{
  ClutterStageWindow *stage_window;
  GList *l;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  stage_window = _clutter_stage_get_window (stage);
  if (stage_window == NULL)
    return;

  for (l = clutter_stage_peek_stage_views (stage); l; l = l->next)
    {
      ClutterStageView *view = l->data;

      clutter_stage_view_schedule_update (view);
    }
}

ClutterPaintVolume *
_clutter_stage_paint_volume_stack_allocate (ClutterStage *stage)
{
  GArray *paint_volume_stack = stage->priv->paint_volume_stack;

  g_array_set_size (paint_volume_stack,
                    paint_volume_stack->len+1);

  return &g_array_index (paint_volume_stack,
                         ClutterPaintVolume,
                         paint_volume_stack->len - 1);
}

void
_clutter_stage_paint_volume_stack_free_all (ClutterStage *stage)
{
  GArray *paint_volume_stack = stage->priv->paint_volume_stack;
  int i;

  for (i = 0; i < paint_volume_stack->len; i++)
    {
      ClutterPaintVolume *pv =
        &g_array_index (paint_volume_stack, ClutterPaintVolume, i);
      clutter_paint_volume_free (pv);
    }

  g_array_set_size (paint_volume_stack, 0);
}

/* When an actor queues a redraw we add it to a list on the stage that
 * gets processed once all updates to the stage have been finished.
 *
 * This deferred approach to processing queue_redraw requests means
 * that we can avoid redundant transformations of clip volumes if
 * something later triggers a full stage redraw anyway. It also means
 * we can be more sure that all the referenced actors will have valid
 * allocations improving the chance that we can determine the actors
 * paint volume so we can clip the redraw request even if the user
 * didn't explicitly do so.
 */
void
clutter_stage_queue_actor_redraw (ClutterStage             *stage,
                                  ClutterActor             *actor,
                                  const ClutterPaintVolume *clip)
{
  ClutterStagePrivate *priv = stage->priv;
  QueueRedrawEntry *entry = NULL;

  CLUTTER_NOTE (CLIPPING, "stage_queue_actor_redraw (actor=%s, clip=%p): ",
                _clutter_actor_get_debug_name (actor), clip);

  if (!priv->pending_finish_queue_redraws)
    {
      GList *l;

      for (l = clutter_stage_peek_stage_views (stage); l; l = l->next)
        {
          ClutterStageView *view = l->data;

          clutter_stage_view_schedule_update (view);
        }

      priv->pending_finish_queue_redraws = TRUE;
    }

  entry = g_hash_table_lookup (priv->pending_queue_redraws, actor);

  if (entry)
    {
      /* Ignore all requests to queue a redraw for an actor if a full
       * (non-clipped) redraw of the actor has already been queued. */
      if (!entry->has_clip)
        {
          CLUTTER_NOTE (CLIPPING, "Bail from stage_queue_actor_redraw (%s): "
                        "Unclipped redraw of actor already queued",
                        _clutter_actor_get_debug_name (actor));
          return;
        }

      /* If queuing a clipped redraw and a clipped redraw has
       * previously been queued for this actor then combine the latest
       * clip together with the existing clip */
      if (clip)
        clutter_paint_volume_union (&entry->clip, clip);
      else
        {
          clutter_paint_volume_free (&entry->clip);
          entry->has_clip = FALSE;
        }
    }
  else
    {
      entry = g_new0 (QueueRedrawEntry, 1);

      if (clip)
        {
          entry->has_clip = TRUE;
          _clutter_paint_volume_init_static (&entry->clip, actor);
          _clutter_paint_volume_set_from_volume (&entry->clip, clip);
        }
      else
        entry->has_clip = FALSE;

      g_hash_table_insert (priv->pending_queue_redraws,
                           g_object_ref (actor), entry);
    }
}

static void
free_queue_redraw_entry (QueueRedrawEntry *entry)
{
  if (entry->has_clip)
    clutter_paint_volume_free (&entry->clip);
  g_free (entry);
}

void
clutter_stage_dequeue_actor_redraw (ClutterStage *self,
                                    ClutterActor *actor)
{
  g_hash_table_remove (self->priv->pending_queue_redraws, actor);
}

static void
add_to_stage_clip (ClutterStage       *stage,
                   ClutterPaintVolume *redraw_clip)
{
  ClutterStageWindow *stage_window;
  ClutterActorBox bounding_box;
  ClutterActorBox intersection_box;
  cairo_rectangle_int_t geom, stage_clip;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (CLUTTER_ACTOR (stage)))
    return;

  stage_window = _clutter_stage_get_window (stage);
  if (stage_window == NULL)
    return;

  if (is_full_stage_redraw_queued (stage))
    return;

  if (redraw_clip == NULL)
    {
      clutter_stage_add_redraw_clip (stage, NULL);
      return;
    }

  if (redraw_clip->is_empty)
    return;

  /* Now transform and project the clip volume to view coordinates and get
   * the axis aligned bounding box that's aligned to the pixel grid.
   */
  _clutter_paint_volume_get_stage_paint_box (redraw_clip,
                                             stage,
                                             &bounding_box);

  _clutter_stage_window_get_geometry (stage_window, &geom);

  intersection_box.x1 = MAX (bounding_box.x1, 0);
  intersection_box.y1 = MAX (bounding_box.y1, 0);
  intersection_box.x2 = MIN (bounding_box.x2, geom.width);
  intersection_box.y2 = MIN (bounding_box.y2, geom.height);

  /* There is no need to track degenerate/empty redraw clips */
  if (intersection_box.x2 <= intersection_box.x1 ||
      intersection_box.y2 <= intersection_box.y1)
    return;

  stage_clip.x = intersection_box.x1;
  stage_clip.y = intersection_box.y1;
  stage_clip.width = intersection_box.x2 - stage_clip.x;
  stage_clip.height = intersection_box.y2 - stage_clip.y;

  clutter_stage_add_redraw_clip (stage, &stage_clip);
}

void
clutter_stage_maybe_finish_queue_redraws (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;
  GHashTableIter iter;
  gpointer key, value;

  COGL_TRACE_BEGIN_SCOPED (ClutterStageFinishQueueRedraws, "FinishQueueRedraws");

  if (!priv->pending_finish_queue_redraws)
    return;

  priv->pending_finish_queue_redraws = FALSE;

  g_hash_table_iter_init (&iter, priv->pending_queue_redraws);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ClutterActor *redraw_actor = key;
      QueueRedrawEntry *entry = value;

      g_hash_table_iter_steal (&iter);

      if (clutter_actor_is_mapped (redraw_actor))
        {
          ClutterPaintVolume old_actor_pv, new_actor_pv;

          _clutter_paint_volume_init_static (&old_actor_pv, NULL);
          _clutter_paint_volume_init_static (&new_actor_pv, NULL);

          if (entry->has_clip)
            {
              add_to_stage_clip (stage, &entry->clip);
            }
          else if (clutter_actor_get_redraw_clip (redraw_actor,
                                                  &old_actor_pv,
                                                  &new_actor_pv))
            {
              /* Add both the old paint volume of the actor (which is
               * currently visible on the screen) and the new paint volume
               * (which will be visible on the screen after this redraw)
               * to the redraw clip.
               * The former we do to ensure the old texture on the screen
               * will be fully painted over in case the actor was moved.
               */
              add_to_stage_clip (stage, &old_actor_pv);
              add_to_stage_clip (stage, &new_actor_pv);
            }
          else
            {
              /* If there's no clip we can use, we have to trigger an
               * unclipped full stage redraw.
               */
              add_to_stage_clip (stage, NULL);
            }
        }

      g_object_unref (redraw_actor);
      free_queue_redraw_entry (entry);

      /* get_paint_volume() vfuncs might queue redraws and can cause our
       * iterator to now be invalidated. So start over. This isn't wasting
       * any time since we already stole (removed) the elements previously
       * visited.
       */
      g_hash_table_iter_init (&iter, priv->pending_queue_redraws);
    }
}

void
_clutter_stage_add_pointer_drag_actor (ClutterStage       *stage,
                                       ClutterInputDevice *device,
                                       ClutterActor       *actor)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_pointer_drag_actors");
  if (drag_actors == NULL)
    {
      drag_actors = g_hash_table_new (NULL, NULL);
      g_object_set_data_full (G_OBJECT (stage),
                              "__clutter_stage_pointer_drag_actors",
                              drag_actors,
                              (GDestroyNotify) g_hash_table_destroy);
    }

  g_hash_table_replace (drag_actors, device, actor);
}

ClutterActor *
_clutter_stage_get_pointer_drag_actor (ClutterStage       *stage,
                                       ClutterInputDevice *device)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_pointer_drag_actors");
  if (drag_actors == NULL)
    return NULL;

  return g_hash_table_lookup (drag_actors, device);
}

void
_clutter_stage_remove_pointer_drag_actor (ClutterStage       *stage,
                                          ClutterInputDevice *device)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_pointer_drag_actors");
  if (drag_actors == NULL)
    return;

  g_hash_table_remove (drag_actors, device);

  if (g_hash_table_size (drag_actors) == 0)
    g_object_set_data (G_OBJECT (stage),
                       "__clutter_stage_pointer_drag_actors",
                       NULL);
}

void
_clutter_stage_add_touch_drag_actor (ClutterStage         *stage,
                                     ClutterEventSequence *sequence,
                                     ClutterActor         *actor)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_touch_drag_actors");
  if (drag_actors == NULL)
    {
      drag_actors = g_hash_table_new (NULL, NULL);
      g_object_set_data_full (G_OBJECT (stage),
                              "__clutter_stage_touch_drag_actors",
                              drag_actors,
                              (GDestroyNotify) g_hash_table_destroy);
    }

  g_hash_table_replace (drag_actors, sequence, actor);
}

ClutterActor *
_clutter_stage_get_touch_drag_actor (ClutterStage         *stage,
                                     ClutterEventSequence *sequence)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_touch_drag_actors");
  if (drag_actors == NULL)
    return NULL;

  return g_hash_table_lookup (drag_actors, sequence);
}

void
_clutter_stage_remove_touch_drag_actor (ClutterStage         *stage,
                                        ClutterEventSequence *sequence)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_touch_drag_actors");
  if (drag_actors == NULL)
    return;

  g_hash_table_remove (drag_actors, sequence);

  if (g_hash_table_size (drag_actors) == 0)
    g_object_set_data (G_OBJECT (stage),
                       "__clutter_stage_touch_drag_actors",
                       NULL);
}

int64_t
clutter_stage_get_frame_counter (ClutterStage          *stage)
{
  ClutterStageWindow *stage_window;

  stage_window = _clutter_stage_get_window (stage);
  return _clutter_stage_window_get_frame_counter (stage_window);
}

void
clutter_stage_presented (ClutterStage     *stage,
                         ClutterStageView *view,
                         ClutterFrameInfo *frame_info)
{
  g_signal_emit (stage, stage_signals[PRESENTED], 0, view, frame_info);
}

/**
 * clutter_stage_get_capture_final_size:
 * @stage: a #ClutterStage actor
 * @rect: a #cairo_rectangle_int_t
 * @out_width: (out) (optional): the final width
 * @out_height: (out) (optional): the final height
 * @out_scale: (out) (optional): the final scale factor
 *
 * Get the size of the framebuffer one must pass to
 * clutter_stage_paint_to_buffer() or clutter_stage_paint_to_framebuffer()
 * would be used with the same @rect.
 *
 * Returns: %TRUE if the size has been retrieved, %FALSE otherwise.
 */
gboolean
clutter_stage_get_capture_final_size (ClutterStage          *stage,
                                      cairo_rectangle_int_t *rect,
                                      int                   *out_width,
                                      int                   *out_height,
                                      float                 *out_scale)
{
  float max_scale = 1.0;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  if (rect)
    {
      graphene_rect_t capture_rect;
      g_autoptr (GList) views = NULL;
      GList *l;

      _clutter_util_rect_from_rectangle (rect, &capture_rect);
      views = clutter_stage_get_views_for_rect (stage, &capture_rect);

      if (!views)
        return FALSE;

      for (l = views; l; l = l->next)
        {
          ClutterStageView *view = l->data;

          max_scale = MAX (clutter_stage_view_get_scale (view), max_scale);
        }

      if (out_width)
        *out_width = (gint) roundf (rect->width * max_scale);

      if (out_height)
        *out_height = (gint) roundf (rect->height * max_scale);
    }
  else
    {
      ClutterActorBox alloc;
      float stage_width, stage_height;

      clutter_actor_get_allocation_box (CLUTTER_ACTOR (stage), &alloc);
      clutter_actor_box_get_size (&alloc, &stage_width, &stage_height);
      max_scale = clutter_actor_get_real_resource_scale (CLUTTER_ACTOR (stage));

      if (out_width)
        *out_width = (gint) roundf (stage_width * max_scale);

      if (out_height)
        *out_height = (gint) roundf (stage_height * max_scale);
    }

  if (out_scale)
    *out_scale = max_scale;

  return TRUE;
}

void
clutter_stage_paint_to_framebuffer (ClutterStage                *stage,
                                    CoglFramebuffer             *framebuffer,
                                    const cairo_rectangle_int_t *rect,
                                    float                        scale,
                                    ClutterPaintFlag             paint_flags)
{
  ClutterStagePrivate *priv = stage->priv;
  ClutterPaintContext *paint_context;
  cairo_region_t *redraw_clip;

  if (paint_flags & CLUTTER_PAINT_FLAG_CLEAR)
    {
      CoglColor clear_color;

      cogl_color_init_from_4ub (&clear_color, 0, 0, 0, 0);
      cogl_framebuffer_clear (framebuffer, COGL_BUFFER_BIT_COLOR, &clear_color);
    }

  redraw_clip = cairo_region_create_rectangle (rect);
  paint_context =
    clutter_paint_context_new_for_framebuffer (framebuffer,
                                               redraw_clip,
                                               paint_flags);
  cairo_region_destroy (redraw_clip);

  cogl_framebuffer_push_matrix (framebuffer);
  cogl_framebuffer_set_projection_matrix (framebuffer, &priv->projection);
  cogl_framebuffer_set_viewport (framebuffer,
                                 -(rect->x * scale),
                                 -(rect->y * scale),
                                 priv->viewport[2] * scale,
                                 priv->viewport[3] * scale);
  clutter_actor_paint (CLUTTER_ACTOR (stage), paint_context);
  cogl_framebuffer_pop_matrix (framebuffer);

  clutter_paint_context_destroy (paint_context);
}

/**
 * clutter_stage_paint_to_buffer:
 * @stage: a #ClutterStage actor
 * @rect: a #cairo_rectangle_int_t
 * @scale: the scale
 * @data: (array) (element-type guint8): a pointer to the data
 * @stride: stride of the image surface
 * @format: the pixel format
 * @paint_flags: the #ClutterPaintFlag
 * @error: the error
 *
 * Take a snapshot of the stage to a provided buffer.
 *
 * Returns: %TRUE is the buffer has been paint successfully, %FALSE otherwise.
 */
gboolean
clutter_stage_paint_to_buffer (ClutterStage                 *stage,
                               const cairo_rectangle_int_t  *rect,
                               float                         scale,
                               uint8_t                      *data,
                               int                           stride,
                               CoglPixelFormat               format,
                               ClutterPaintFlag              paint_flags,
                               GError                      **error)
{
  ClutterBackend *clutter_backend = clutter_get_default_backend ();
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  int texture_width, texture_height;
  CoglTexture2D *texture;
  CoglOffscreen *offscreen;
  CoglFramebuffer *framebuffer;
  CoglBitmap *bitmap;

  texture_width = (int) roundf (rect->width * scale);
  texture_height = (int) roundf (rect->height * scale);
  texture = cogl_texture_2d_new_with_size (cogl_context,
                                           texture_width,
                                           texture_height);
  if (!texture)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create %dx%d texture",
                   texture_width, texture_height);
      return FALSE;
    }

  offscreen = cogl_offscreen_new_with_texture (COGL_TEXTURE (texture));
  framebuffer = COGL_FRAMEBUFFER (offscreen);

  cogl_object_unref (texture);

  if (!cogl_framebuffer_allocate (framebuffer, error))
    return FALSE;

  clutter_stage_paint_to_framebuffer (stage, framebuffer,
                                      rect, scale, paint_flags);

  bitmap = cogl_bitmap_new_for_data (cogl_context,
                                     texture_width, texture_height,
                                     format,
                                     stride,
                                     data);

  cogl_framebuffer_read_pixels_into_bitmap (framebuffer,
                                            0, 0,
                                            COGL_READ_PIXELS_COLOR_BUFFER,
                                            bitmap);

  cogl_object_unref (bitmap);
  g_object_unref (framebuffer);

  return TRUE;
}

/**
 * clutter_stage_paint_to_content:
 * @stage: a #ClutterStage actor
 * @rect: a #cairo_rectangle_int_t
 * @scale: the scale
 * @paint_flags: the #ClutterPaintFlag
 * @error: the error
 *
 * Take a snapshot of the stage to a #ClutterContent.
 *
 * Returns: (transfer full): the #ClutterContent or %NULL on error.
 */
ClutterContent *
clutter_stage_paint_to_content (ClutterStage                 *stage,
                                const cairo_rectangle_int_t  *rect,
                                float                         scale,
                                ClutterPaintFlag              paint_flags,
                                GError                      **error)
{
  ClutterBackend *clutter_backend = clutter_get_default_backend ();
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  int texture_width, texture_height;
  CoglTexture2D *texture;
  CoglOffscreen *offscreen;
  g_autoptr (CoglFramebuffer) framebuffer = NULL;

  texture_width = (int) roundf (rect->width * scale);
  texture_height = (int) roundf (rect->height * scale);
  texture = cogl_texture_2d_new_with_size (cogl_context,
                                           texture_width,
                                           texture_height);
  if (!texture)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create %dx%d texture",
                   texture_width, texture_height);
      return NULL;
    }

  offscreen = cogl_offscreen_new_with_texture (COGL_TEXTURE (texture));
  framebuffer = COGL_FRAMEBUFFER (offscreen);

  cogl_object_unref (texture);

  if (!cogl_framebuffer_allocate (framebuffer, error))
    return NULL;

  clutter_stage_paint_to_framebuffer (stage, framebuffer,
                                      rect, scale, paint_flags);

  return clutter_texture_content_new_from_texture (cogl_offscreen_get_texture (offscreen),
                                                   NULL);
}

void
clutter_stage_capture_view_into (ClutterStage          *stage,
                                 ClutterStageView      *view,
                                 cairo_rectangle_int_t *rect,
                                 uint8_t               *data,
                                 int                    stride)
{
  CoglFramebuffer *framebuffer;
  ClutterBackend *backend;
  CoglContext *context;
  CoglBitmap *bitmap;
  cairo_rectangle_int_t view_layout;
  float view_scale;
  float texture_width;
  float texture_height;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  framebuffer = clutter_stage_view_get_framebuffer (view);

  clutter_stage_view_get_layout (view, &view_layout);

  if (!rect)
    rect = &view_layout;

  view_scale = clutter_stage_view_get_scale (view);
  texture_width = roundf (rect->width * view_scale);
  texture_height = roundf (rect->height * view_scale);

  backend = clutter_get_default_backend ();
  context = clutter_backend_get_cogl_context (backend);
  bitmap = cogl_bitmap_new_for_data (context,
                                     texture_width, texture_height,
                                     CLUTTER_CAIRO_FORMAT_ARGB32,
                                     stride,
                                     data);

  cogl_framebuffer_read_pixels_into_bitmap (framebuffer,
                                            roundf ((rect->x - view_layout.x) * view_scale),
                                            roundf ((rect->y - view_layout.y) * view_scale),
                                            COGL_READ_PIXELS_COLOR_BUFFER,
                                            bitmap);

  cogl_object_unref (bitmap);
}

/**
 * clutter_stage_peek_stage_views: (skip)
 */
GList *
clutter_stage_peek_stage_views (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;

  return _clutter_stage_window_get_views (priv->impl);
}

void
clutter_stage_clear_stage_views (ClutterStage *stage)
{
  clutter_actor_clear_stage_views_recursive (CLUTTER_ACTOR (stage), FALSE);
}

GList *
clutter_stage_get_views_for_rect (ClutterStage          *stage,
                                  const graphene_rect_t *rect)
{
  ClutterStagePrivate *priv = stage->priv;
  GList *views_for_rect = NULL;
  GList *l;

  for (l = _clutter_stage_window_get_views (priv->impl); l; l = l->next)
    {
      ClutterStageView *view = l->data;
      cairo_rectangle_int_t view_layout;
      graphene_rect_t view_rect;

      clutter_stage_view_get_layout (view, &view_layout);
      _clutter_util_rect_from_rectangle (&view_layout, &view_rect);

      if (graphene_rect_intersection (&view_rect, rect, NULL))
        views_for_rect = g_list_prepend (views_for_rect, view);
    }

  return views_for_rect;
}

void
clutter_stage_set_actor_needs_immediate_relayout (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;

  priv->actor_needs_immediate_relayout = TRUE;
}

void
clutter_stage_maybe_invalidate_focus (ClutterStage *self,
                                      ClutterActor *actor)
{
  ClutterStagePrivate *priv = self->priv;
  GHashTableIter iter;
  gpointer value;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (self))
    return;

  g_hash_table_iter_init (&iter, priv->pointer_devices);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      PointerDeviceEntry *entry = value;

      if (entry->current_actor != actor)
        continue;

      clutter_stage_pick_and_update_device (self,
                                            entry->device,
                                            NULL,
                                            CLUTTER_DEVICE_UPDATE_IGNORE_CACHE |
                                            CLUTTER_DEVICE_UPDATE_EMIT_CROSSING,
                                            entry->coords,
                                            CLUTTER_CURRENT_TIME);
    }

  g_hash_table_iter_init (&iter, priv->touch_sequences);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      PointerDeviceEntry *entry = value;

      if (entry->current_actor != actor)
        continue;

      clutter_stage_pick_and_update_device (self,
                                            entry->device,
                                            entry->sequence,
                                            CLUTTER_DEVICE_UPDATE_IGNORE_CACHE |
                                            CLUTTER_DEVICE_UPDATE_EMIT_CROSSING,
                                            entry->coords,
                                            CLUTTER_CURRENT_TIME);
    }
}

void
clutter_stage_invalidate_focus (ClutterStage *self,
                                ClutterActor *actor)
{
  if (CLUTTER_ACTOR_IN_DESTRUCTION (self))
    return;

  g_assert (!clutter_actor_is_mapped (actor) || !clutter_actor_get_reactive (actor));

  clutter_stage_maybe_invalidate_focus (self, actor);

  if (actor != CLUTTER_ACTOR (self))
    g_assert (!clutter_actor_has_pointer (actor));
}

static void
free_pointer_device_entry (PointerDeviceEntry *entry)
{
  if (entry->current_actor)
    _clutter_actor_set_has_pointer (entry->current_actor, FALSE);

  g_clear_pointer (&entry->clear_area, cairo_region_destroy);

  g_free (entry);
}

void
clutter_stage_update_device_entry (ClutterStage         *self,
                                   ClutterInputDevice   *device,
                                   ClutterEventSequence *sequence,
                                   graphene_point_t      coords,
                                   ClutterActor         *actor,
                                   cairo_region_t       *clear_area)
{
  ClutterStagePrivate *priv = self->priv;
  PointerDeviceEntry *entry = NULL;

  g_assert (device != NULL);

  if (sequence != NULL)
    entry = g_hash_table_lookup (priv->touch_sequences, sequence);
  else
    entry = g_hash_table_lookup (priv->pointer_devices, device);

  if (!entry)
    {
      entry = g_new0 (PointerDeviceEntry, 1);

      if (sequence != NULL)
        g_hash_table_insert (priv->touch_sequences, sequence, entry);
      else
        g_hash_table_insert (priv->pointer_devices, device, entry);

      entry->stage = self;
      entry->device = device;
      entry->sequence = sequence;
    }

  entry->coords = coords;

  if (entry->current_actor != actor)
    {
      if (entry->current_actor)
        _clutter_actor_set_has_pointer (entry->current_actor, FALSE);

      entry->current_actor = actor;

      if (actor)
        _clutter_actor_set_has_pointer (actor, TRUE);
    }

  g_clear_pointer (&entry->clear_area, cairo_region_destroy);
  if (clear_area)
    entry->clear_area = cairo_region_reference (clear_area);
}

void
clutter_stage_remove_device_entry (ClutterStage         *self,
                                   ClutterInputDevice   *device,
                                   ClutterEventSequence *sequence)
{
  ClutterStagePrivate *priv = self->priv;
  gboolean removed;

  g_assert (device != NULL);

  if (sequence != NULL)
    removed = g_hash_table_remove (priv->touch_sequences, sequence);
  else
    removed = g_hash_table_remove (priv->pointer_devices, device);

  g_assert (removed);
}

/**
 * clutter_stage_get_device_actor:
 * @stage: a #ClutterStage
 * @device: a #ClutterInputDevice
 * @sequence: (allow-none): an optional #ClutterEventSequence
 *
 * Retrieves the #ClutterActor underneath the pointer or touch point
 * of @device and @sequence.
 *
 * Return value: (transfer none): a pointer to the #ClutterActor or %NULL
 */
ClutterActor *
clutter_stage_get_device_actor (ClutterStage         *stage,
                                ClutterInputDevice   *device,
                                ClutterEventSequence *sequence)
{
  ClutterStagePrivate *priv = stage->priv;
  PointerDeviceEntry *entry = NULL;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);
  g_return_val_if_fail (device != NULL, NULL);

  if (sequence != NULL)
    entry = g_hash_table_lookup (priv->touch_sequences, sequence);
  else
    entry = g_hash_table_lookup (priv->pointer_devices, device);

  if (entry)
    return entry->current_actor;

  return NULL;
}

/**
 * clutter_stage_get_device_coords: (skip):
 */
void
clutter_stage_get_device_coords (ClutterStage         *stage,
                                 ClutterInputDevice   *device,
                                 ClutterEventSequence *sequence,
                                 graphene_point_t     *coords)
{
  ClutterStagePrivate *priv = stage->priv;
  PointerDeviceEntry *entry = NULL;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (device != NULL);

  if (sequence != NULL)
    entry = g_hash_table_lookup (priv->touch_sequences, sequence);
  else
    entry = g_hash_table_lookup (priv->pointer_devices, device);

  if (entry && coords)
    *coords = entry->coords;
}

static void
clutter_stage_set_device_coords (ClutterStage         *stage,
                                 ClutterInputDevice   *device,
                                 ClutterEventSequence *sequence,
                                 graphene_point_t      coords)
{
  ClutterStagePrivate *priv = stage->priv;
  PointerDeviceEntry *entry = NULL;

  if (sequence != NULL)
    entry = g_hash_table_lookup (priv->touch_sequences, sequence);
  else
    entry = g_hash_table_lookup (priv->pointer_devices, device);

  if (entry)
    entry->coords = coords;
}

static ClutterActor *
find_common_root_actor (ClutterStage *stage,
                        ClutterActor *a,
                        ClutterActor *b)
{
  if (a && b)
    {
      while (a)
        {
          if (a == b || clutter_actor_contains (a, b))
            return a;

          a = clutter_actor_get_parent (a);
        }
    }

  return CLUTTER_ACTOR (stage);
}

static ClutterEvent *
create_crossing_event (ClutterStage         *stage,
                       ClutterInputDevice   *device,
                       ClutterEventSequence *sequence,
                       ClutterEventType      event_type,
                       ClutterEventFlags     flags,
                       ClutterActor         *source,
                       ClutterActor         *related,
                       graphene_point_t      coords,
                       uint32_t              time_ms)
{
  ClutterEvent *event;

  event = clutter_event_new (event_type);
  event->crossing.time = time_ms;
  event->crossing.flags = flags;
  event->crossing.stage = stage;
  event->crossing.x = coords.x;
  event->crossing.y = coords.y;
  event->crossing.related = related;
  event->crossing.sequence = sequence;
  clutter_event_set_device (event, device);

  return event;
}

void
clutter_stage_update_device (ClutterStage         *stage,
                             ClutterInputDevice   *device,
                             ClutterEventSequence *sequence,
                             graphene_point_t      point,
                             uint32_t              time_ms,
                             ClutterActor         *new_actor,
                             cairo_region_t       *clear_area,
                             gboolean              emit_crossing)
{
  ClutterInputDeviceType device_type;
  ClutterActor *old_actor, *root;
  gboolean device_actor_changed;
  ClutterEvent *event;

  device_type = clutter_input_device_get_device_type (device);

  g_assert (device_type != CLUTTER_KEYBOARD_DEVICE &&
            device_type != CLUTTER_PAD_DEVICE);

  old_actor = clutter_stage_get_device_actor (stage, device, sequence);
  device_actor_changed = new_actor != old_actor;

  clutter_stage_update_device_entry (stage,
                                     device, sequence,
                                     point,
                                     new_actor,
                                     clear_area);

  if (device_actor_changed)
    {
      CLUTTER_NOTE (EVENT,
                    "Updating actor under cursor (device %s, at %.2f, %.2f): %s",
                    clutter_input_device_get_device_name (device),
                    point.x,
                    point.y,
                    _clutter_actor_get_debug_name (new_actor));

      if (emit_crossing)
        {
          ClutterActor *grab_actor;

          root = find_common_root_actor (stage, new_actor, old_actor);

          grab_actor = clutter_stage_get_grab_actor (stage);

          /* If the common root is outside the currently effective grab,
           * it involves actors outside the grabbed actor hierarchy, the
           * events should be propagated from/inside the grab actor.
           */
          if (grab_actor &&
              root != grab_actor &&
              !clutter_actor_contains (grab_actor, root))
            root = grab_actor;
        }

      /* we need to make sure that this event is processed
       * before any other event we might have queued up until
       * now, so we go on, and synthesize the event emission
       * ourselves
       */
      if (old_actor && emit_crossing)
        {
          event = create_crossing_event (stage,
                                         device, sequence,
                                         CLUTTER_LEAVE,
                                         CLUTTER_EVENT_NONE,
                                         old_actor, new_actor,
                                         point, time_ms);
          if (!_clutter_event_process_filters (event, old_actor))
            _clutter_actor_handle_event (old_actor, root, event);

          clutter_event_free (event);
        }

      if (new_actor && emit_crossing)
        {
          event = create_crossing_event (stage,
                                         device, sequence,
                                         CLUTTER_ENTER,
                                         CLUTTER_EVENT_NONE,
                                         new_actor, old_actor,
                                         point, time_ms);
          if (!_clutter_event_process_filters (event, new_actor))
            _clutter_actor_handle_event (new_actor, root, event);

          clutter_event_free (event);
        }
    }
}

void
clutter_stage_repick_device (ClutterStage       *stage,
                             ClutterInputDevice *device)
{
  graphene_point_t point;

  clutter_stage_get_device_coords (stage, device, NULL, &point);
  clutter_stage_pick_and_update_device (stage,
                                        device,
                                        NULL,
                                        CLUTTER_DEVICE_UPDATE_IGNORE_CACHE |
                                        CLUTTER_DEVICE_UPDATE_EMIT_CROSSING,
                                        point,
                                        CLUTTER_CURRENT_TIME);
}

static gboolean
clutter_stage_check_in_clear_area (ClutterStage         *stage,
                                   ClutterInputDevice   *device,
                                   ClutterEventSequence *sequence,
                                   graphene_point_t      point)
{
  ClutterStagePrivate *priv = stage->priv;
  PointerDeviceEntry *entry = NULL;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (device != NULL, FALSE);

  if (sequence != NULL)
    entry = g_hash_table_lookup (priv->touch_sequences, sequence);
  else
    entry = g_hash_table_lookup (priv->pointer_devices, device);

  if (!entry)
    return FALSE;
  if (!entry->clear_area)
    return FALSE;

  return cairo_region_contains_point (entry->clear_area,
                                      point.x, point.y);
}

ClutterActor *
clutter_stage_pick_and_update_device (ClutterStage             *stage,
                                      ClutterInputDevice       *device,
                                      ClutterEventSequence     *sequence,
                                      ClutterDeviceUpdateFlags  flags,
                                      graphene_point_t          point,
                                      uint32_t                  time_ms)
{
  ClutterActor *new_actor;
  cairo_region_t *clear_area = NULL;

  if ((flags & CLUTTER_DEVICE_UPDATE_IGNORE_CACHE) == 0)
    {
      if (clutter_stage_check_in_clear_area (stage, device,
                                             sequence, point))
        {
          clutter_stage_set_device_coords (stage, device,
                                           sequence, point);
          return clutter_stage_get_device_actor (stage, device, sequence);
        }
    }

  new_actor = _clutter_stage_do_pick (stage,
                                      point.x,
                                      point.y,
                                      CLUTTER_PICK_REACTIVE,
                                      &clear_area);

  /* Picking should never fail, but if it does, we bail out here */
  g_return_val_if_fail (new_actor != NULL, NULL);

  clutter_stage_update_device (stage,
                               device, sequence,
                               point,
                               time_ms,
                               new_actor,
                               clear_area,
                               !!(flags & CLUTTER_DEVICE_UPDATE_EMIT_CROSSING));

  g_clear_pointer (&clear_area, cairo_region_destroy);

  return new_actor;
}

static void
clutter_stage_notify_grab_on_pointer_entry (ClutterStage       *stage,
                                            PointerDeviceEntry *entry,
                                            ClutterActor       *grab_actor,
                                            ClutterActor       *old_grab_actor)
{
  gboolean pointer_in_grab, pointer_in_old_grab;
  ClutterEventType event_type = CLUTTER_NOTHING;
  ClutterActor *topmost, *deepmost;

  if (!entry->current_actor)
    return;

  pointer_in_grab =
    !grab_actor ||
    grab_actor == entry->current_actor ||
    clutter_actor_contains (grab_actor, entry->current_actor);
  pointer_in_old_grab =
    !old_grab_actor ||
    old_grab_actor == entry->current_actor ||
    clutter_actor_contains (old_grab_actor, entry->current_actor);

  /* Equate NULL actors to the stage here, to ease calculations further down. */
  if (!grab_actor)
    grab_actor = CLUTTER_ACTOR (stage);
  if (!old_grab_actor)
    old_grab_actor = CLUTTER_ACTOR (stage);

  if (grab_actor == old_grab_actor)
    return;

  if (pointer_in_grab && pointer_in_old_grab)
    {
      /* Both grabs happen to contain the pointer actor, we have to figure out
       * which is topmost, and emit ENTER/LEAVE events accordingly on the actors
       * between old/new grabs.
       */
      if (clutter_actor_contains (grab_actor, old_grab_actor))
        {
          /* grab_actor is above old_grab_actor, emit ENTER events in the
           * line between those two actors.
           */
          event_type = CLUTTER_ENTER;
          deepmost = clutter_actor_get_parent (old_grab_actor);
          topmost = grab_actor;
        }
      else if (clutter_actor_contains (old_grab_actor, grab_actor))
        {
          /* old_grab_actor is above grab_actor, emit LEAVE events in the
           * line between those two actors.
           */
          event_type = CLUTTER_LEAVE;
          deepmost = clutter_actor_get_parent (grab_actor);
          topmost = old_grab_actor;
        }
    }
  else if (pointer_in_grab)
    {
      /* Pointer is somewhere inside the grab_actor hierarchy. Emit ENTER events
       * from the current grab actor to the pointer actor.
       */
      event_type = CLUTTER_ENTER;
      deepmost = entry->current_actor;
      topmost = grab_actor;
    }
  else if (pointer_in_old_grab)
    {
      /* Pointer is somewhere inside the old_grab_actor hierarchy. Emit LEAVE
       * events from the common root of old/cur grab actors to the pointer
       * actor.
       */
      event_type = CLUTTER_LEAVE;
      deepmost = entry->current_actor;
      topmost = find_common_root_actor (stage, grab_actor, old_grab_actor);
    }

  if (event_type != CLUTTER_NOTHING)
    {
      ClutterEvent *event;

      event = create_crossing_event (stage,
                                     entry->device,
                                     entry->sequence,
                                     event_type,
                                     CLUTTER_EVENT_FLAG_GRAB_NOTIFY,
                                     entry->current_actor,
                                     event_type == CLUTTER_LEAVE ?
                                     grab_actor : old_grab_actor,
                                     entry->coords,
                                     CLUTTER_CURRENT_TIME);
      if (!_clutter_event_process_filters (event, entry->current_actor))
        _clutter_actor_handle_event (deepmost, topmost, event);
      clutter_event_free (event);
    }
}

static void
clutter_stage_notify_grab_on_key_focus (ClutterStage *stage,
                                        ClutterActor *grab_actor,
                                        ClutterActor *old_grab_actor)
{
  ClutterStagePrivate *priv = stage->priv;
  ClutterActor *key_focus;
  gboolean focus_in_grab, focus_in_old_grab;

  key_focus = priv->key_focused_actor ?
    priv->key_focused_actor : CLUTTER_ACTOR (stage);

  focus_in_grab =
    !grab_actor ||
    grab_actor == key_focus ||
    clutter_actor_contains (grab_actor, key_focus);
  focus_in_old_grab =
    !old_grab_actor ||
    old_grab_actor == key_focus ||
    clutter_actor_contains (old_grab_actor, key_focus);

  if (focus_in_grab && !focus_in_old_grab)
    _clutter_actor_set_has_key_focus (CLUTTER_ACTOR (key_focus), TRUE);
  else if (!focus_in_grab && focus_in_old_grab)
    _clutter_actor_set_has_key_focus (CLUTTER_ACTOR (key_focus), FALSE);
}

static void
clutter_stage_notify_grab (ClutterStage *stage,
                           ClutterGrab  *cur,
                           ClutterGrab  *old)
{
  ClutterStagePrivate *priv = stage->priv;
  ClutterActor *cur_actor = NULL, *old_actor = NULL;
  PointerDeviceEntry *entry;
  GHashTableIter iter;

  if (cur)
    cur_actor = cur->actor;
  if (old)
    old_actor = old->actor;

  /* Nothing to notify */
  if (cur_actor == old_actor)
    return;

  g_hash_table_iter_init (&iter, priv->pointer_devices);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &entry))
    {
      /* Update pointers */
      clutter_stage_notify_grab_on_pointer_entry (stage,
                                                  entry,
                                                  cur_actor,
                                                  old_actor);
    }

  g_hash_table_iter_init (&iter, priv->touch_sequences);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &entry))
    {
      /* Update touch sequences */
      clutter_stage_notify_grab_on_pointer_entry (stage,
                                                  entry,
                                                  cur_actor,
                                                  old_actor);
    }

  clutter_stage_notify_grab_on_key_focus (stage, cur_actor, old_actor);
}

ClutterGrab *
clutter_grab_ref (ClutterGrab *grab)
{
  g_ref_count_inc (&grab->ref_count);
  return grab;
}

void
clutter_grab_unref (ClutterGrab *grab)
{
  if (g_ref_count_dec (&grab->ref_count))
    {
      clutter_grab_dismiss (grab);
      g_free (grab);
    }
}

G_DEFINE_BOXED_TYPE (ClutterGrab, clutter_grab,
                     clutter_grab_ref, clutter_grab_unref)

/**
 * clutter_stage_grab:
 * @stage: The #ClutterStage
 * @actor: The actor grabbing input
 *
 * Grabs input onto a certain actor. Events will be propagated as
 * usual inside its hierarchy.
 *
 * Returns: (transfer full): an opaque #ClutterGrab handle, drop
 *   with clutter_grab_dismiss()
 **/
ClutterGrab *
clutter_stage_grab (ClutterStage *stage,
                    ClutterActor *actor)
{
  ClutterStagePrivate *priv;
  ClutterGrab *grab;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), NULL);

  priv = stage->priv;

  if (!priv->topmost_grab)
    {
      ClutterMainContext *context;
      ClutterSeat *seat;

      /* First grab in the chain, trigger a backend grab too */
      context = _clutter_context_get_default ();
      seat = clutter_backend_get_default_seat (context->backend);
      priv->grab_state =
        clutter_seat_grab (seat, clutter_get_current_event_time ());
    }

  grab = g_new0 (ClutterGrab, 1);
  g_ref_count_init (&grab->ref_count);
  grab->stage = stage;
  grab->actor = actor;
  grab->prev = NULL;
  grab->next = priv->topmost_grab;

  if (priv->topmost_grab)
    priv->topmost_grab->prev = grab;

  priv->topmost_grab = grab;
  clutter_actor_attach_grab (actor, grab);
  clutter_stage_notify_grab (stage, grab, grab->next);

  return grab;
}

void
clutter_stage_unlink_grab (ClutterStage *stage,
                           ClutterGrab  *grab)
{
  ClutterStagePrivate *priv = stage->priv;
  ClutterGrab *prev, *next;

  /* This grab is already detached */
  if (!grab->prev && !grab->next && priv->topmost_grab != grab)
    return;

  prev = grab->prev;
  next = grab->next;

  if (prev)
    prev->next = next;
  if (next)
    next->prev = prev;

  if (priv->topmost_grab == grab)
    {
      /* This is the active grab */
      g_assert (prev == NULL);
      priv->topmost_grab = next;
      clutter_stage_notify_grab (stage, next, grab);
    }

  clutter_actor_detach_grab (grab->actor, grab);

  if (!priv->topmost_grab)
    {
      ClutterMainContext *context;
      ClutterSeat *seat;

      /* This was the last remaining grab, trigger a backend ungrab */
      context = _clutter_context_get_default ();
      seat = clutter_backend_get_default_seat (context->backend);
      clutter_seat_ungrab (seat, clutter_get_current_event_time ());
      priv->grab_state = CLUTTER_GRAB_STATE_NONE;
    }

  grab->next = NULL;
  grab->prev = NULL;
}

/**
 * clutter_grab_dismiss:
 * @grab: Grab to undo
 *
 * Removes a grab. If this grab is effective, crossing events
 * will be generated to indicate the change in event redirection.
 **/
void
clutter_grab_dismiss (ClutterGrab *grab)
{
  g_return_if_fail (grab != NULL);

  clutter_stage_unlink_grab (grab->stage, grab);
}

/**
 * clutter_grab_get_seat_state:
 * @grab: a Grab handle
 *
 * Returns the windowing-level state of the
 * grab, the devices that are guaranteed to be
 * grabbed.
 *
 * Returns: The state of the grab.
 **/
ClutterGrabState
clutter_grab_get_seat_state (ClutterGrab *grab)
{
  g_return_val_if_fail (grab != NULL, CLUTTER_GRAB_STATE_NONE);

  return grab->stage->priv->grab_state;
}

/**
 * clutter_stage_get_grab_actor:
 * @stage: a #ClutterStage
 *
 * Gets the actor that currently holds a grab.
 *
 * Returns: (transfer none): The grabbing actor
 **/
ClutterActor *
clutter_stage_get_grab_actor (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;

  if (!priv->topmost_grab)
    return NULL;

  /* Return active grab */
  return priv->topmost_grab->actor;
}

/**
 * clutter_stage_get_event_actor:
 * @stage: a #ClutterStage
 * @event: an event received on the stage
 *
 * Retrieves the current focus actor for an event. This is
 * the key focus for key events and other events directed
 * to the key focus, or the actor directly under the
 * coordinates of a device or touch sequence.
 *
 * The actor is looked up at the time of calling this function,
 * and may differ from the actor that the stage originally
 * delivered the event to.
 *
 * Return value: (transfer none): a pointer to the #ClutterActor or %NULL
 **/
ClutterActor *
clutter_stage_get_event_actor (ClutterStage       *stage,
                               const ClutterEvent *event)
{
  ClutterInputDevice *device;
  ClutterEventSequence *sequence;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);
  g_return_val_if_fail (event != NULL, NULL);

  switch (event->type)
    {
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
    case CLUTTER_PAD_BUTTON_PRESS:
    case CLUTTER_PAD_BUTTON_RELEASE:
    case CLUTTER_PAD_RING:
    case CLUTTER_PAD_STRIP:
    case CLUTTER_IM_COMMIT:
    case CLUTTER_IM_DELETE:
    case CLUTTER_IM_PREEDIT:
      return clutter_stage_get_key_focus (stage);
    case CLUTTER_MOTION:
    case CLUTTER_ENTER:
    case CLUTTER_LEAVE:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_SCROLL:
    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
    case CLUTTER_TOUCHPAD_PINCH:
    case CLUTTER_TOUCHPAD_SWIPE:
    case CLUTTER_TOUCHPAD_HOLD:
    case CLUTTER_PROXIMITY_IN:
    case CLUTTER_PROXIMITY_OUT:
      device = clutter_event_get_device (event);
      sequence = clutter_event_get_event_sequence (event);

      return clutter_stage_get_device_actor (stage, device, sequence);
    case CLUTTER_DEVICE_ADDED:
    case CLUTTER_DEVICE_REMOVED:
    case CLUTTER_NOTHING:
    case CLUTTER_EVENT_LAST:
      g_warn_if_reached ();
    }

  return NULL;
}
