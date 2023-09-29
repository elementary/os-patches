/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

#ifndef __CLUTTER_ACTOR_PRIVATE_H__
#define __CLUTTER_ACTOR_PRIVATE_H__

#include <clutter/clutter-actor.h>
#include <clutter/clutter-grab.h>

G_BEGIN_DECLS

/*< private >
 * ClutterActorTraverseFlags:
 * CLUTTER_ACTOR_TRAVERSE_DEPTH_FIRST: Traverse the graph in
 *   a depth first order.
 * CLUTTER_ACTOR_TRAVERSE_BREADTH_FIRST: Traverse the graph in a
 *   breadth first order.
 *
 * Controls some options for how clutter_actor_traverse() iterates
 * through the graph.
 */
typedef enum
{
  CLUTTER_ACTOR_TRAVERSE_DEPTH_FIRST   = 1L<<0,
  CLUTTER_ACTOR_TRAVERSE_BREADTH_FIRST = 1L<<1
} ClutterActorTraverseFlags;

/*< private >
 * ClutterActorTraverseVisitFlags:
 * CLUTTER_ACTOR_TRAVERSE_VISIT_CONTINUE: Continue traversing as
 *   normal
 * CLUTTER_ACTOR_TRAVERSE_VISIT_SKIP_CHILDREN: Don't traverse the
 *   children of the last visited actor. (Not applicable when using
 *   %CLUTTER_ACTOR_TRAVERSE_DEPTH_FIRST_POST_ORDER since the children
 *   are visited before having an opportunity to bail out)
 * CLUTTER_ACTOR_TRAVERSE_VISIT_BREAK: Immediately bail out without
 *   visiting any more actors.
 *
 * Each time an actor is visited during a scenegraph traversal the
 * ClutterTraverseCallback can return a set of flags that may affect
 * the continuing traversal. It may stop traversal completely, just
 * skip over children for the current actor or continue as normal.
 */
typedef enum
{
  CLUTTER_ACTOR_TRAVERSE_VISIT_CONTINUE       = 1L<<0,
  CLUTTER_ACTOR_TRAVERSE_VISIT_SKIP_CHILDREN  = 1L<<1,
  CLUTTER_ACTOR_TRAVERSE_VISIT_BREAK          = 1L<<2
} ClutterActorTraverseVisitFlags;

/*< private >
 * ClutterTraverseCallback:
 *
 * The callback prototype used with clutter_actor_traverse. The
 * returned flags can be used to affect the continuing traversal
 * either by continuing as normal, skipping over children of an
 * actor or bailing out completely.
 */
typedef ClutterActorTraverseVisitFlags (*ClutterTraverseCallback) (ClutterActor *actor,
                                                                   gint          depth,
                                                                   gpointer      user_data);

/*< private >
 * ClutterForeachCallback:
 * @actor: The actor being iterated
 * @user_data: The private data specified when starting the iteration
 *
 * A generic callback for iterating over actor, such as with
 * _clutter_actor_foreach_child. The difference when compared to
 * #ClutterCallback is that it returns a boolean so it is possible to break
 * out of an iteration early.
 *
 * Return value: %TRUE to continue iterating or %FALSE to break iteration
 * early.
 */
typedef gboolean (*ClutterForeachCallback) (ClutterActor *actor,
                                            gpointer      user_data);

typedef struct _SizeRequest             SizeRequest;

typedef struct _ClutterLayoutInfo       ClutterLayoutInfo;
typedef struct _ClutterTransformInfo    ClutterTransformInfo;
typedef struct _ClutterAnimationInfo    ClutterAnimationInfo;

struct _SizeRequest
{
  guint  age;
  gfloat for_size;
  gfloat min_size;
  gfloat natural_size;
};

/*< private >
 * ClutterLayoutInfo:
 * @fixed_pos: the fixed position of the actor
 * @margin: the composed margin of the actor
 * @x_align: the horizontal alignment, if the actor expands horizontally
 * @y_align: the vertical alignment, if the actor expands vertically
 * @x_expand: whether the actor should expand horizontally
 * @y_expand: whether the actor should expand vertically
 * @minimum: the fixed minimum size
 * @natural: the fixed natural size
 *
 * Ancillary layout information for an actor.
 */
struct _ClutterLayoutInfo
{
  /* fixed position coordinates */
  graphene_point_t fixed_pos;

  ClutterMargin margin;

  guint x_align : 4;
  guint y_align : 4;

  guint x_expand : 1;
  guint y_expand : 1;

  graphene_size_t minimum;
  graphene_size_t natural;
};

const ClutterLayoutInfo *       _clutter_actor_get_layout_info_or_defaults      (ClutterActor *self);
ClutterLayoutInfo *             _clutter_actor_get_layout_info                  (ClutterActor *self);
ClutterLayoutInfo *             _clutter_actor_peek_layout_info                 (ClutterActor *self);

struct _ClutterTransformInfo
{
  /* rotation */
  gdouble rx_angle;
  gdouble ry_angle;
  gdouble rz_angle;

  /* scaling */
  gdouble scale_x;
  gdouble scale_y;
  gdouble scale_z;

  /* translation */
  graphene_point3d_t translation;

  /* z_position */
  gfloat z_position;

  /* transformation center */
  graphene_point_t pivot;
  gfloat pivot_z;

  graphene_matrix_t transform;
  guint transform_set : 1;

  graphene_matrix_t child_transform;
  guint child_transform_set : 1;
};

const ClutterTransformInfo *    _clutter_actor_get_transform_info_or_defaults   (ClutterActor *self);
ClutterTransformInfo *          _clutter_actor_get_transform_info               (ClutterActor *self);

typedef struct _AState {
  guint easing_duration;
  guint easing_delay;
  ClutterAnimationMode easing_mode;
} AState;

struct _ClutterAnimationInfo
{
  GArray *states;
  AState *cur_state;

  GHashTable *transitions;
};

const ClutterAnimationInfo *    _clutter_actor_get_animation_info_or_defaults           (ClutterActor *self);
ClutterAnimationInfo *          _clutter_actor_get_animation_info                       (ClutterActor *self);

ClutterTransition *             _clutter_actor_create_transition                        (ClutterActor *self,
                                                                                         GParamSpec   *pspec,
                                                                                         ...);
gboolean                        _clutter_actor_foreach_child                            (ClutterActor *self,
                                                                                         ClutterForeachCallback callback,
                                                                                         gpointer user_data);
void                            _clutter_actor_traverse                                 (ClutterActor *actor,
                                                                                         ClutterActorTraverseFlags flags,
                                                                                         ClutterTraverseCallback before_children_callback,
                                                                                         ClutterTraverseCallback after_children_callback,
                                                                                         gpointer user_data);
ClutterActor *                  _clutter_actor_get_stage_internal                       (ClutterActor *actor);

void                            _clutter_actor_apply_modelview_transform                (ClutterActor      *self,
                                                                                         graphene_matrix_t *matrix);
void                            _clutter_actor_apply_relative_transformation_matrix     (ClutterActor      *self,
                                                                                         ClutterActor      *ancestor,
                                                                                         graphene_matrix_t *matrix);

void                            _clutter_actor_rerealize                                (ClutterActor    *self,
                                                                                         ClutterCallback  callback,
                                                                                         gpointer         data);

void                            _clutter_actor_set_in_clone_paint                       (ClutterActor *self,
                                                                                         gboolean      is_in_clone_paint);

void                            _clutter_actor_set_enable_model_view_transform          (ClutterActor *self,
                                                                                         gboolean      enable);

void                            _clutter_actor_set_enable_paint_unmapped                (ClutterActor *self,
                                                                                         gboolean      enable);

void                            _clutter_actor_set_has_pointer                          (ClutterActor *self,
                                                                                         gboolean      has_pointer);

void                            _clutter_actor_set_has_key_focus                        (ClutterActor *self,
                                                                                         gboolean      has_key_focus);

void                            _clutter_actor_queue_redraw_full                        (ClutterActor             *self,
                                                                                         const ClutterPaintVolume *volume,
                                                                                         ClutterEffect            *effect);

void                            _clutter_actor_finish_queue_redraw                      (ClutterActor *self);

gboolean                        _clutter_actor_set_default_paint_volume                 (ClutterActor       *self,
                                                                                         GType               check_gtype,
                                                                                         ClutterPaintVolume *volume);

const char *                    _clutter_actor_get_debug_name                           (ClutterActor *self);

void                            _clutter_actor_push_clone_paint                         (void);
void                            _clutter_actor_pop_clone_paint                          (void);

ClutterActorAlign               _clutter_actor_get_effective_x_align                    (ClutterActor *self);

void                            _clutter_actor_handle_event                             (ClutterActor       *actor,
                                                                                         ClutterActor       *root,
                                                                                         const ClutterEvent *event);

void                            _clutter_actor_attach_clone                             (ClutterActor *actor,
                                                                                         ClutterActor *clone);
void                            _clutter_actor_detach_clone                             (ClutterActor *actor,
                                                                                         ClutterActor *clone);
void                            _clutter_actor_queue_only_relayout                      (ClutterActor *actor);
void                            clutter_actor_clear_stage_views_recursive               (ClutterActor *actor,
                                                                                         gboolean      stop_transitions);

float                           clutter_actor_get_real_resource_scale                   (ClutterActor *actor);

ClutterPaintNode *              clutter_actor_create_texture_paint_node                 (ClutterActor *self,
                                                                                         CoglTexture  *texture);

void clutter_actor_finish_layout (ClutterActor *self,
                                  int           phase);

void clutter_actor_queue_immediate_relayout (ClutterActor *self);

gboolean clutter_actor_is_painting_unmapped (ClutterActor *self);

gboolean clutter_actor_get_redraw_clip (ClutterActor       *self,
                                        ClutterPaintVolume *dst_old_pv,
                                        ClutterPaintVolume *dst_new_pv);

void clutter_actor_attach_grab (ClutterActor *actor,
                                ClutterGrab  *grab);
void clutter_actor_detach_grab (ClutterActor *actor,
                                ClutterGrab  *grab);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_PRIVATE_H__ */
