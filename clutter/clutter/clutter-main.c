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
 * SECTION:clutter-main
 * @short_description: Various 'global' Clutter functions.
 *
 * Functions to retrieve various global Clutter resources and other utility
 * functions for mainloops, events and threads
 *
 * ## The Clutter Threading Model
 *
 * Clutter is *thread-aware*: all operations performed by Clutter are assumed
 * to be under the Big Clutter Lock, which is created when the threading is
 * initialized through clutter_init(), and entered when calling user-related
 * code during event handling and actor drawing.
 *
 * The only safe and portable way to use the Clutter API in a multi-threaded
 * environment is to only access the Clutter API from a thread that did called
 * clutter_init() and clutter_main().
 *
 * The common pattern for using threads with Clutter is to use worker threads
 * to perform blocking operations and then install idle or timeout sources with
 * the result when the thread finishes, and update the UI from those callbacks.
 *
 * For a working example of how to use a worker thread to update the UI, see
 * [threads.c](https://git.gnome.org/browse/clutter/tree/examples/threads.c?h=clutter-1.18)
 */

#include "clutter-build-config.h"

#include <stdlib.h>

#include "clutter-actor-private.h"
#include "clutter-backend-private.h"
#include "clutter-debug.h"
#include "clutter-event-private.h"
#include "clutter-feature.h"
#include "clutter-input-device-private.h"
#include "clutter-input-pointer-a11y-private.h"
#include "clutter-graphene.h"
#include "clutter-main.h"
#include "clutter-mutter.h"
#include "clutter-paint-node-private.h"
#include "clutter-private.h"
#include "clutter-settings-private.h"
#include "clutter-stage.h"
#include "clutter-stage-manager.h"
#include "clutter-stage-private.h"
#include "clutter-backend-private.h"

#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

#include "cally/cally.h" /* For accessibility support */

/* main context */
static ClutterMainContext *ClutterCntx       = NULL;

/* command line options */
static gboolean clutter_is_initialized       = FALSE;
static gboolean clutter_show_fps             = FALSE;
static gboolean clutter_disable_mipmap_text  = FALSE;
static gboolean clutter_enable_accessibility = TRUE;
static gboolean clutter_sync_to_vblank       = TRUE;

static ClutterTextDirection clutter_text_direction = CLUTTER_TEXT_DIRECTION_LTR;

/* debug flags */
guint clutter_debug_flags       = 0;
guint clutter_paint_debug_flags = 0;
guint clutter_pick_debug_flags  = 0;

/* A constant added to heuristic max render time to account for variations
 * in the estimates.
 */
int clutter_max_render_time_constant_us = 2000;

#ifdef CLUTTER_ENABLE_DEBUG
static const GDebugKey clutter_debug_keys[] = {
  { "misc", CLUTTER_DEBUG_MISC },
  { "actor", CLUTTER_DEBUG_ACTOR },
  { "texture", CLUTTER_DEBUG_TEXTURE },
  { "event", CLUTTER_DEBUG_EVENT },
  { "paint", CLUTTER_DEBUG_PAINT },
  { "pick", CLUTTER_DEBUG_PICK },
  { "pango", CLUTTER_DEBUG_PANGO },
  { "backend", CLUTTER_DEBUG_BACKEND },
  { "scheduler", CLUTTER_DEBUG_SCHEDULER },
  { "script", CLUTTER_DEBUG_SCRIPT },
  { "shader", CLUTTER_DEBUG_SHADER },
  { "animation", CLUTTER_DEBUG_ANIMATION },
  { "layout", CLUTTER_DEBUG_LAYOUT },
  { "clipping", CLUTTER_DEBUG_CLIPPING },
  { "oob-transforms", CLUTTER_DEBUG_OOB_TRANSFORMS },
  { "frame-timings", CLUTTER_DEBUG_FRAME_TIMINGS },
  { "detailed-trace", CLUTTER_DEBUG_DETAILED_TRACE },
};
#endif /* CLUTTER_ENABLE_DEBUG */

static const GDebugKey clutter_pick_debug_keys[] = {
  { "nop-picking", CLUTTER_DEBUG_NOP_PICKING },
};

static const GDebugKey clutter_paint_debug_keys[] = {
  { "disable-swap-events", CLUTTER_DEBUG_DISABLE_SWAP_EVENTS },
  { "disable-clipped-redraws", CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS },
  { "redraws", CLUTTER_DEBUG_REDRAWS },
  { "paint-volumes", CLUTTER_DEBUG_PAINT_VOLUMES },
  { "disable-culling", CLUTTER_DEBUG_DISABLE_CULLING },
  { "disable-offscreen-redirect", CLUTTER_DEBUG_DISABLE_OFFSCREEN_REDIRECT },
  { "continuous-redraw", CLUTTER_DEBUG_CONTINUOUS_REDRAW },
  { "paint-deform-tiles", CLUTTER_DEBUG_PAINT_DEFORM_TILES },
  { "damage-region", CLUTTER_DEBUG_PAINT_DAMAGE_REGION },
  { "disable-dynamic-max-render-time", CLUTTER_DEBUG_DISABLE_DYNAMIC_MAX_RENDER_TIME },
  { "max-render-time", CLUTTER_DEBUG_PAINT_MAX_RENDER_TIME },
};

gboolean
_clutter_context_get_show_fps (void)
{
  ClutterMainContext *context = _clutter_context_get_default ();

  return context->show_fps;
}

/**
 * clutter_get_accessibility_enabled:
 *
 * Returns whether Clutter has accessibility support enabled.  As
 * least, a value of TRUE means that there are a proper AtkUtil
 * implementation available
 *
 * Return value: %TRUE if Clutter has accessibility support enabled
 *
 * Since: 1.4
 */
gboolean
clutter_get_accessibility_enabled (void)
{
  return cally_get_cally_initialized ();
}

/**
 * clutter_disable_accessibility:
 *
 * Disable loading the accessibility support. It has the same effect
 * as setting the environment variable
 * CLUTTER_DISABLE_ACCESSIBILITY. For the same reason, this method
 * should be called before clutter_init().
 *
 * Since: 1.14
 */
void
clutter_disable_accessibility (void)
{
  if (clutter_is_initialized)
    {
      g_warning ("clutter_disable_accessibility() can only be called before "
                 "initializing Clutter.");
      return;
    }

  clutter_enable_accessibility = FALSE;
}

static CoglPangoFontMap *
clutter_context_get_pango_fontmap (void)
{
  ClutterMainContext *self;
  CoglPangoFontMap *font_map;
  gdouble resolution;
  gboolean use_mipmapping;

  self = _clutter_context_get_default ();
  if (G_LIKELY (self->font_map != NULL))
    return self->font_map;

  font_map = COGL_PANGO_FONT_MAP (cogl_pango_font_map_new ());

  resolution = clutter_backend_get_resolution (self->backend);
  cogl_pango_font_map_set_resolution (font_map, resolution);

  use_mipmapping = !clutter_disable_mipmap_text;
  cogl_pango_font_map_set_use_mipmapping (font_map, use_mipmapping);

  self->font_map = font_map;

  return self->font_map;
}

static ClutterTextDirection
clutter_get_text_direction (void)
{
  ClutterTextDirection dir = CLUTTER_TEXT_DIRECTION_LTR;
  const gchar *direction;

  direction = g_getenv ("CLUTTER_TEXT_DIRECTION");
  if (direction && *direction != '\0')
    {
      if (strcmp (direction, "rtl") == 0)
        dir = CLUTTER_TEXT_DIRECTION_RTL;
      else if (strcmp (direction, "ltr") == 0)
        dir = CLUTTER_TEXT_DIRECTION_LTR;
    }
  else
    {
      /* Re-use GTK+'s LTR/RTL handling */
      const char *e = g_dgettext ("gtk30", "default:LTR");

      if (strcmp (e, "default:RTL") == 0)
        dir = CLUTTER_TEXT_DIRECTION_RTL;
      else if (strcmp (e, "default:LTR") == 0)
        dir = CLUTTER_TEXT_DIRECTION_LTR;
      else
        g_warning ("Whoever translated default:LTR did so wrongly.");
    }

  CLUTTER_NOTE (MISC, "Text direction: %s",
                dir == CLUTTER_TEXT_DIRECTION_RTL ? "rtl" : "ltr");

  return dir;
}

gboolean
_clutter_threads_dispatch (gpointer data)
{
  ClutterThreadsDispatch *dispatch = data;
  gboolean ret = FALSE;

  if (!g_source_is_destroyed (g_main_current_source ()))
    ret = dispatch->func (dispatch->data);

  return ret;
}

void
_clutter_threads_dispatch_free (gpointer data)
{
  ClutterThreadsDispatch *dispatch = data;

  /* XXX - we cannot hold the thread lock here because the main loop
   * might destroy a source while still in the dispatcher function; so
   * knowing whether the lock is being held or not is not known a priori.
   *
   * see bug: http://bugzilla.gnome.org/show_bug.cgi?id=459555
   */
  if (dispatch->notify)
    dispatch->notify (dispatch->data);

  g_free (dispatch);
}

/**
 * clutter_threads_add_idle_full: (rename-to clutter_threads_add_idle)
 * @priority: the priority of the timeout source. Typically this will be in the
 *    range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE
 * @func: function to call
 * @data: data to pass to the function
 * @notify: functio to call when the idle source is removed
 *
 * Adds a function to be called whenever there are no higher priority
 * events pending. If the function returns %FALSE it is automatically
 * removed from the list of event sources and will not be called again.
 *
 * This function can be considered a thread-safe variant of g_idle_add_full():
 * it will call @function while holding the Clutter lock. It is logically
 * equivalent to the following implementation:
 *
 * |[
 * static gboolean
 * idle_safe_callback (gpointer data)
 * {
 *    SafeClosure *closure = data;
 *    gboolean res = FALSE;
 *
 *    // the callback does not need to acquire the Clutter
 *     / lock itself, as it is held by the this proxy handler
 *     //
 *    res = closure->callback (closure->data);
 *
 *    return res;
 * }
 * static gulong
 * add_safe_idle (GSourceFunc callback,
 *                gpointer    data)
 * {
 *   SafeClosure *closure = g_new0 (SafeClosure, 1);
 *
 *   closure->callback = callback;
 *   closure->data = data;
 *
 *   return g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
 *                           idle_safe_callback,
 *                           closure,
 *                           g_free)
 * }
 *]|
 *
 * This function should be used by threaded applications to make sure
 * that @func is emitted under the Clutter threads lock and invoked
 * from the same thread that started the Clutter main loop. For instance,
 * it can be used to update the UI using the results from a worker
 * thread:
 *
 * |[
 * static gboolean
 * update_ui (gpointer data)
 * {
 *   SomeClosure *closure = data;
 *
 *   // it is safe to call Clutter API from this function because
 *    / it is invoked from the same thread that started the main
 *    / loop and under the Clutter thread lock
 *    //
 *   clutter_label_set_text (CLUTTER_LABEL (closure->label),
 *                           closure->text);
 *
 *   g_object_unref (closure->label);
 *   g_free (closure);
 *
 *   return FALSE;
 * }
 *
 *   // within another thread //
 *   closure = g_new0 (SomeClosure, 1);
 *   // always take a reference on GObject instances //
 *   closure->label = g_object_ref (my_application->label);
 *   closure->text = g_strdup (processed_text_to_update_the_label);
 *
 *   clutter_threads_add_idle_full (G_PRIORITY_HIGH_IDLE,
 *                                  update_ui,
 *                                  closure,
 *                                  NULL);
 * ]|
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.4
 */
guint
clutter_threads_add_idle_full (gint           priority,
                               GSourceFunc    func,
                               gpointer       data,
                               GDestroyNotify notify)
{
  ClutterThreadsDispatch *dispatch;

  g_return_val_if_fail (func != NULL, 0);

  dispatch = g_new0 (ClutterThreadsDispatch, 1);
  dispatch->func = func;
  dispatch->data = data;
  dispatch->notify = notify;

  return g_idle_add_full (priority,
                          _clutter_threads_dispatch, dispatch,
                          _clutter_threads_dispatch_free);
}

/**
 * clutter_threads_add_idle: (skip)
 * @func: function to call
 * @data: data to pass to the function
 *
 * Simple wrapper around clutter_threads_add_idle_full() using the
 * default priority.
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.4
 */
guint
clutter_threads_add_idle (GSourceFunc func,
                          gpointer    data)
{
  g_return_val_if_fail (func != NULL, 0);

  return clutter_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE,
                                        func, data,
                                        NULL);
}

/**
 * clutter_threads_add_timeout_full: (rename-to clutter_threads_add_timeout)
 * @priority: the priority of the timeout source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT and #G_PRIORITY_HIGH.
 * @interval: the time between calls to the function, in milliseconds
 * @func: function to call
 * @data: data to pass to the function
 * @notify: function to call when the timeout source is removed
 *
 * Sets a function to be called at regular intervals holding the Clutter
 * threads lock, with the given priority. The function is called repeatedly
 * until it returns %FALSE, at which point the timeout is automatically
 * removed and the function will not be called again. The @notify function
 * is called when the timeout is removed.
 *
 * The first call to the function will be at the end of the first @interval.
 *
 * It is important to note that, due to how the Clutter main loop is
 * implemented, the timing will not be accurate and it will not try to
 * "keep up" with the interval.
 *
 * See also clutter_threads_add_idle_full().
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.4
 */
guint
clutter_threads_add_timeout_full (gint           priority,
                                  guint          interval,
                                  GSourceFunc    func,
                                  gpointer       data,
                                  GDestroyNotify notify)
{
  ClutterThreadsDispatch *dispatch;

  g_return_val_if_fail (func != NULL, 0);

  dispatch = g_new0 (ClutterThreadsDispatch, 1);
  dispatch->func = func;
  dispatch->data = data;
  dispatch->notify = notify;

  return g_timeout_add_full (priority,
                             interval,
                             _clutter_threads_dispatch, dispatch,
                             _clutter_threads_dispatch_free);
}

/**
 * clutter_threads_add_timeout: (skip)
 * @interval: the time between calls to the function, in milliseconds
 * @func: function to call
 * @data: data to pass to the function
 *
 * Simple wrapper around clutter_threads_add_timeout_full().
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.4
 */
guint
clutter_threads_add_timeout (guint       interval,
                             GSourceFunc func,
                             gpointer    data)
{
  g_return_val_if_fail (func != NULL, 0);

  return clutter_threads_add_timeout_full (G_PRIORITY_DEFAULT,
                                           interval,
                                           func, data,
                                           NULL);
}

gboolean
_clutter_context_is_initialized (void)
{
  if (ClutterCntx == NULL)
    return FALSE;

  return ClutterCntx->is_initialized;
}

ClutterMainContext *
_clutter_context_get_default (void)
{
  g_assert (ClutterCntx);
  return ClutterCntx;
}

static gboolean
clutter_init_real (ClutterMainContext  *clutter_context,
                   GError             **error)
{
  ClutterBackend *backend;

  /* Note, creates backend if not already existing, though parse args will
   * have likely created it
   */
  backend = clutter_context->backend;

  if (!_clutter_backend_finish_init (backend, error))
    return FALSE;

  /* If we are displaying the regions that would get redrawn with clipped
   * redraws enabled we actually have to disable the clipped redrawing
   * because otherwise we end up with nasty trails of rectangles everywhere.
   */
  if (clutter_paint_debug_flags & CLUTTER_DEBUG_REDRAWS)
    clutter_paint_debug_flags |= CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS;

  /* The same is true when drawing the outlines of paint volumes... */
  if (clutter_paint_debug_flags & CLUTTER_DEBUG_PAINT_VOLUMES)
    {
      clutter_paint_debug_flags |=
        CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS | CLUTTER_DEBUG_DISABLE_CULLING;
    }

  if (clutter_paint_debug_flags & CLUTTER_DEBUG_PAINT_DAMAGE_REGION)
    g_message ("Enabling damaged region");

  /* this will take care of initializing Cogl's state and
   * query the GL machinery for features
   */
  if (!clutter_feature_init (clutter_context, error))
    return FALSE;

  clutter_text_direction = clutter_get_text_direction ();

  clutter_is_initialized = TRUE;
  clutter_context->is_initialized = TRUE;

  /* Initialize a11y */
  if (clutter_enable_accessibility)
    cally_accessibility_init ();

  /* Initialize types required for paint nodes */
  clutter_paint_node_init_types (clutter_context->backend);

  return TRUE;
}

static void
init_clutter_debug (ClutterMainContext *clutter_context)
{
  const char *env_string;

#ifdef CLUTTER_ENABLE_DEBUG
  env_string = g_getenv ("CLUTTER_DEBUG");
  if (env_string != NULL)
    {
      clutter_debug_flags =
        g_parse_debug_string (env_string,
                              clutter_debug_keys,
                              G_N_ELEMENTS (clutter_debug_keys));
      env_string = NULL;
    }
#endif /* CLUTTER_ENABLE_DEBUG */

  env_string = g_getenv ("CLUTTER_PICK");
  if (env_string != NULL)
    {
      clutter_pick_debug_flags =
        g_parse_debug_string (env_string,
                              clutter_pick_debug_keys,
                              G_N_ELEMENTS (clutter_pick_debug_keys));
      env_string = NULL;
    }

  env_string = g_getenv ("CLUTTER_PAINT");
  if (env_string != NULL)
    {
      clutter_paint_debug_flags =
        g_parse_debug_string (env_string,
                              clutter_paint_debug_keys,
                              G_N_ELEMENTS (clutter_paint_debug_keys));
      env_string = NULL;
    }

  env_string = g_getenv ("CLUTTER_SHOW_FPS");
  if (env_string)
    clutter_show_fps = TRUE;

  env_string = g_getenv ("CLUTTER_DISABLE_MIPMAPPED_TEXT");
  if (env_string)
    clutter_disable_mipmap_text = TRUE;
}

ClutterContext *
clutter_context_new (ClutterBackendConstructor   backend_constructor,
                     gpointer                    user_data,
                     GError                    **error)
{
  ClutterMainContext *clutter_context;

  if (ClutterCntx)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Currently only creating one clutter context is supported");
      return NULL;
    }

  clutter_graphene_init ();

  clutter_context = g_new0 (ClutterMainContext, 1);
  init_clutter_debug (clutter_context);
  clutter_context->show_fps = clutter_show_fps;
  clutter_context->is_initialized = FALSE;

  clutter_context->backend = backend_constructor (user_data);
  clutter_context->settings = clutter_settings_get_default ();
  _clutter_settings_set_backend (clutter_context->settings,
                                 clutter_context->backend);

  clutter_context->events_queue = g_async_queue_new ();
  clutter_context->last_repaint_id = 1;

  if (!clutter_init_real (clutter_context, error))
    {
      g_free (clutter_context);
      return NULL;
    }

  ClutterCntx = clutter_context;

  return clutter_context;
}

void
clutter_context_free (ClutterMainContext *clutter_context)
{
  g_clear_pointer (&clutter_context->events_queue, g_async_queue_unref);
  g_clear_pointer (&clutter_context->backend, clutter_backend_destroy);
  ClutterCntx = NULL;
  g_free (clutter_context);
}

ClutterBackend *
clutter_context_get_backend (ClutterContext *clutter_context)
{
  return clutter_context->backend;
}

gboolean
_clutter_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                      GValue                *return_accu,
                                      const GValue          *handler_return,
                                      gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;

  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;

  return continue_emission;
}

gboolean
_clutter_boolean_continue_accumulator (GSignalInvocationHint *ihint,
                                       GValue                *return_accu,
                                       const GValue          *handler_return,
                                       gpointer               dummy)
{
  gboolean continue_emission;

  continue_emission = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, continue_emission);

  return continue_emission;
}

static inline void
emit_event_chain (ClutterActor *target,
                  ClutterEvent *event)
{
  _clutter_actor_handle_event (target,
                               clutter_stage_get_grab_actor (event->any.stage),
                               event);
}

/*
 * Emits a pointer event after having prepared the event for delivery (setting
 * source, generating enter/leave etc.).
 */

static inline void
emit_event (ClutterActor *target,
            ClutterEvent *event)
{
  if (event->type == CLUTTER_KEY_PRESS ||
      event->type == CLUTTER_KEY_RELEASE)
    cally_snoop_key_event ((ClutterKeyEvent *) event);

  emit_event_chain (target, event);
}

static ClutterActor *
update_device_for_event (ClutterStage *stage,
                         ClutterEvent *event,
                         gboolean      emit_crossing)
{
  ClutterInputDevice *device = clutter_event_get_device (event);
  ClutterEventSequence *sequence = clutter_event_get_event_sequence (event);
  ClutterDeviceUpdateFlags flags = CLUTTER_DEVICE_UPDATE_NONE;
  graphene_point_t point;
  uint32_t time_ms;

  clutter_event_get_coords (event, &point.x, &point.y);
  time_ms = clutter_event_get_time (event);

  if (emit_crossing)
    flags |= CLUTTER_DEVICE_UPDATE_EMIT_CROSSING;

  return clutter_stage_pick_and_update_device (stage,
                                               device,
                                               sequence,
                                               flags,
                                               point,
                                               time_ms);
}

static void
remove_device_for_event (ClutterStage *stage,
                         ClutterEvent *event,
                         gboolean      emit_crossing)
{
  ClutterInputDevice *device = clutter_event_get_device (event);
  ClutterEventSequence *sequence = clutter_event_get_event_sequence (event);
  graphene_point_t point;
  uint32_t time;

  clutter_event_get_coords (event, &point.x, &point.y);
  time = clutter_event_get_time (event);

  clutter_stage_update_device (stage,
                               device, sequence,
                               point,
                               time,
                               NULL,
                               NULL,
                               TRUE);

  clutter_stage_remove_device_entry (stage, device, sequence);
}

/**
 * clutter_do_event:
 * @event: a #ClutterEvent.
 *
 * Processes an event.
 *
 * The @event must be a valid #ClutterEvent and have a #ClutterStage
 * associated to it.
 *
 * This function is only useful when embedding Clutter inside another
 * toolkit, and it should never be called by applications.
 *
 * Since: 0.4
 */
void
clutter_do_event (ClutterEvent *event)
{
  ClutterContext *context = _clutter_context_get_default();
  ClutterActor *event_actor = NULL;

  /* we need the stage for the event */
  if (event->any.stage == NULL)
    {
      g_warning ("%s: Event does not have a stage: discarding.", G_STRFUNC);
      return;
    }

  /* stages in destruction do not process events */
  if (CLUTTER_ACTOR_IN_DESTRUCTION (event->any.stage))
    return;

  switch (event->any.type)
    {
    case CLUTTER_ENTER:
    case CLUTTER_MOTION:
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
      update_device_for_event (event->any.stage, event, TRUE);
      break;
    default:
      break;
    }

  if (event->any.type != CLUTTER_DEVICE_ADDED &&
      event->any.type != CLUTTER_DEVICE_REMOVED &&
      event->any.type != CLUTTER_NOTHING &&
      event->any.type != CLUTTER_EVENT_LAST)
    {
      event_actor = clutter_stage_get_event_actor (event->any.stage, event);
    }

  context->current_event = g_slist_prepend (context->current_event, event);

  if (_clutter_event_process_filters (event, event_actor))
    {
      context->current_event =
        g_slist_delete_link (context->current_event, context->current_event);

      if (event->type == CLUTTER_TOUCH_END ||
          event->type == CLUTTER_TOUCH_CANCEL)
        {
          _clutter_stage_process_queued_events (event->any.stage);
          remove_device_for_event (event->any.stage, event, TRUE);
        }

      return;
    }

  context->current_event = g_slist_delete_link (context->current_event, context->current_event);

  /* Instead of processing events when received, we queue them up to
   * handle per-frame before animations, layout, and drawing.
   *
   * This gives us the chance to reliably compress motion events
   * because we've "looked ahead" and know all motion events that
   * will occur before drawing the frame.
   */
  _clutter_stage_queue_event (event->any.stage, event, TRUE);
}

static void
_clutter_process_event_details (ClutterActor        *stage,
                                ClutterMainContext  *context,
                                ClutterEvent        *event)
{
  ClutterInputDevice *device = clutter_event_get_device (event);
  ClutterEventSequence *sequence = clutter_event_get_event_sequence (event);
  ClutterActor *target;

  switch (event->type)
    {
      case CLUTTER_NOTHING:
        break;

      case CLUTTER_KEY_PRESS:
      case CLUTTER_KEY_RELEASE:
      case CLUTTER_PAD_BUTTON_PRESS:
      case CLUTTER_PAD_BUTTON_RELEASE:
      case CLUTTER_PAD_STRIP:
      case CLUTTER_PAD_RING:
      case CLUTTER_IM_COMMIT:
      case CLUTTER_IM_DELETE:
      case CLUTTER_IM_PREEDIT:
        {
          ClutterActor *actor = NULL;

          actor = clutter_stage_get_key_focus (CLUTTER_STAGE (stage));
          if (G_UNLIKELY (actor == NULL))
            {
              g_warning ("No key focus set, discarding");
              return;
            }

          emit_event (actor, event);
        }
        break;

      case CLUTTER_ENTER:
        target = clutter_stage_get_device_actor (CLUTTER_STAGE (stage),
                                                 device, sequence);
        emit_event (target, event);
        break;

      case CLUTTER_LEAVE:
        target = clutter_stage_get_device_actor (CLUTTER_STAGE (stage),
                                                 device, sequence);
        emit_event (target, event);
        break;

      case CLUTTER_MOTION:
      case CLUTTER_BUTTON_PRESS:
      case CLUTTER_BUTTON_RELEASE:
      case CLUTTER_SCROLL:
      case CLUTTER_TOUCHPAD_PINCH:
      case CLUTTER_TOUCHPAD_SWIPE:
      case CLUTTER_TOUCHPAD_HOLD:
        {
          gfloat x, y;

          target = clutter_stage_get_device_actor (CLUTTER_STAGE (stage),
                                                   device, sequence);
          clutter_event_get_coords (event, &x, &y);

          CLUTTER_NOTE (EVENT,
                        "Reactive event received at %.2f, %.2f - actor: %p",
                        x, y, target);

          emit_event (target, event);
          break;
        }

      case CLUTTER_TOUCH_UPDATE:
      case CLUTTER_TOUCH_BEGIN:
      case CLUTTER_TOUCH_CANCEL:
      case CLUTTER_TOUCH_END:
        {
          gfloat x, y;

          target = clutter_stage_get_device_actor (CLUTTER_STAGE (stage),
                                                   device, sequence);
          clutter_event_get_coords (event, &x, &y);

          CLUTTER_NOTE (EVENT,
                        "Reactive event received at %.2f, %.2f - actor: %p",
                        x, y, target);

          emit_event (target, event);

          if (event->type == CLUTTER_TOUCH_END ||
              event->type == CLUTTER_TOUCH_CANCEL)
            remove_device_for_event (CLUTTER_STAGE (stage), event, TRUE);

          break;
        }

      case CLUTTER_PROXIMITY_IN:
      case CLUTTER_PROXIMITY_OUT:
        if (!clutter_actor_event (stage, event, TRUE))
          {
            /* and bubbling phase */
            clutter_actor_event (stage, event, FALSE);
          }

        break;

      case CLUTTER_DEVICE_REMOVED:
        {
          ClutterInputDeviceType device_type;

          device_type = clutter_input_device_get_device_type (device);
          if (device_type == CLUTTER_POINTER_DEVICE ||
              device_type == CLUTTER_TABLET_DEVICE ||
              device_type == CLUTTER_PEN_DEVICE ||
              device_type == CLUTTER_ERASER_DEVICE ||
              device_type == CLUTTER_CURSOR_DEVICE)
            remove_device_for_event (CLUTTER_STAGE (stage), event, TRUE);

          break;
        }

      case CLUTTER_DEVICE_ADDED:
      case CLUTTER_EVENT_LAST:
        break;
    }
}

/*
 * _clutter_process_event
 * @event: a #ClutterEvent.
 *
 * Does the actual work of processing an event that was queued earlier
 * out of clutter_do_event().
 */
void
_clutter_process_event (ClutterEvent *event)
{
  ClutterMainContext *context;
  ClutterActor *stage;
  ClutterSeat *seat;

  context = _clutter_context_get_default ();
  seat = clutter_backend_get_default_seat (context->backend);

  stage = CLUTTER_ACTOR (event->any.stage);
  if (stage == NULL)
    {
      CLUTTER_NOTE (EVENT, "Discarding event without a stage set");
      return;
    }

  /* push events on a stack, so that we don't need to
   * add an event parameter to all signals that can be emitted within
   * an event chain
   */
  context->current_event = g_slist_prepend (context->current_event, event);

  clutter_seat_handle_event_post (seat, event);
  _clutter_process_event_details (stage, context, event);

  context->current_event = g_slist_delete_link (context->current_event, context->current_event);
}

/**
 * clutter_get_font_map:
 *
 * Retrieves the #PangoFontMap instance used by Clutter.
 * You can use the global font map object with the COGL
 * Pango API.
 *
 * Return value: (transfer none): the #PangoFontMap instance. The returned
 *   value is owned by Clutter and it should never be unreferenced.
 *
 * Since: 1.0
 */
PangoFontMap *
clutter_get_font_map (void)
{
  return PANGO_FONT_MAP (clutter_context_get_pango_fontmap ());
}

typedef struct _ClutterRepaintFunction
{
  guint id;
  ClutterRepaintFlags flags;
  GSourceFunc func;
  gpointer data;
  GDestroyNotify notify;
} ClutterRepaintFunction;

/**
 * clutter_threads_remove_repaint_func:
 * @handle_id: an unsigned integer greater than zero
 *
 * Removes the repaint function with @handle_id as its id
 *
 * Since: 1.0
 */
void
clutter_threads_remove_repaint_func (guint handle_id)
{
  ClutterRepaintFunction *repaint_func;
  ClutterMainContext *context;
  GList *l;

  g_return_if_fail (handle_id > 0);

  context = _clutter_context_get_default ();
  l = context->repaint_funcs;
  while (l != NULL)
    {
      repaint_func = l->data;

      if (repaint_func->id == handle_id)
        {
          context->repaint_funcs =
            g_list_remove_link (context->repaint_funcs, l);

          g_list_free (l);

          if (repaint_func->notify)
            repaint_func->notify (repaint_func->data);

          g_free (repaint_func);

          break;
        }

      l = l->next;
    }
}

/**
 * clutter_threads_add_repaint_func:
 * @func: the function to be called within the paint cycle
 * @data: data to be passed to the function, or %NULL
 * @notify: function to be called when removing the repaint
 *    function, or %NULL
 *
 * Adds a function to be called whenever Clutter is processing a new
 * frame.
 *
 * If the function returns %FALSE it is automatically removed from the
 * list of repaint functions and will not be called again.
 *
 * This function is guaranteed to be called from within the same thread
 * that called clutter_main(), and while the Clutter lock is being held;
 * the function will be called within the main loop, so it is imperative
 * that it does not block, otherwise the frame time budget may be lost.
 *
 * A repaint function is useful to ensure that an update of the scenegraph
 * is performed before the scenegraph is repainted. By default, a repaint
 * function added using this function will be invoked prior to the frame
 * being processed.
 *
 * Adding a repaint function does not automatically ensure that a new
 * frame will be queued.
 *
 * When the repaint function is removed (either because it returned %FALSE
 * or because clutter_threads_remove_repaint_func() has been called) the
 * @notify function will be called, if any is set.
 *
 * See also: clutter_threads_add_repaint_func_full()
 *
 * Return value: the ID (greater than 0) of the repaint function. You
 *   can use the returned integer to remove the repaint function by
 *   calling clutter_threads_remove_repaint_func().
 *
 * Since: 1.0
 */
guint
clutter_threads_add_repaint_func (GSourceFunc    func,
                                  gpointer       data,
                                  GDestroyNotify notify)
{
  return clutter_threads_add_repaint_func_full (CLUTTER_REPAINT_FLAGS_PRE_PAINT,
                                                func,
                                                data, notify);
}

/**
 * clutter_threads_add_repaint_func_full:
 * @flags: flags for the repaint function
 * @func: the function to be called within the paint cycle
 * @data: data to be passed to the function, or %NULL
 * @notify: function to be called when removing the repaint
 *    function, or %NULL
 *
 * Adds a function to be called whenever Clutter is processing a new
 * frame.
 *
 * If the function returns %FALSE it is automatically removed from the
 * list of repaint functions and will not be called again.
 *
 * This function is guaranteed to be called from within the same thread
 * that called clutter_main(), and while the Clutter lock is being held;
 * the function will be called within the main loop, so it is imperative
 * that it does not block, otherwise the frame time budget may be lost.
 *
 * A repaint function is useful to ensure that an update of the scenegraph
 * is performed before the scenegraph is repainted. The @flags passed to this
 * function will determine the section of the frame processing that will
 * result in @func being called.
 *
 * Adding a repaint function does not automatically ensure that a new
 * frame will be queued.
 *
 * When the repaint function is removed (either because it returned %FALSE
 * or because clutter_threads_remove_repaint_func() has been called) the
 * @notify function will be called, if any is set.
 *
 * Return value: the ID (greater than 0) of the repaint function. You
 *   can use the returned integer to remove the repaint function by
 *   calling clutter_threads_remove_repaint_func().
 *
 * Since: 1.10
 */
guint
clutter_threads_add_repaint_func_full (ClutterRepaintFlags flags,
                                       GSourceFunc         func,
                                       gpointer            data,
                                       GDestroyNotify      notify)
{
  ClutterMainContext *context;
  ClutterRepaintFunction *repaint_func;

  g_return_val_if_fail (func != NULL, 0);

  context = _clutter_context_get_default ();

  repaint_func = g_new0 (ClutterRepaintFunction, 1);

  repaint_func->id = context->last_repaint_id++;

  repaint_func->flags = flags;
  repaint_func->func = func;
  repaint_func->data = data;
  repaint_func->notify = notify;

  context->repaint_funcs = g_list_prepend (context->repaint_funcs,
                                           repaint_func);

  return repaint_func->id;
}

/*
 * _clutter_run_repaint_functions:
 * @flags: only run the repaint functions matching the passed flags
 *
 * Executes the repaint functions added using the
 * clutter_threads_add_repaint_func() function.
 *
 * Must be called with the Clutter thread lock held.
 */
void
_clutter_run_repaint_functions (ClutterRepaintFlags flags)
{
  ClutterMainContext *context = _clutter_context_get_default ();
  ClutterRepaintFunction *repaint_func;
  GList *invoke_list, *reinvoke_list, *l;

  if (context->repaint_funcs == NULL)
    return;

  /* steal the list */
  invoke_list = context->repaint_funcs;
  context->repaint_funcs = NULL;

  reinvoke_list = NULL;

  /* consume the whole list while we execute the functions */
  while (invoke_list != NULL)
    {
      gboolean res = FALSE;

      repaint_func = invoke_list->data;

      l = invoke_list;
      invoke_list = g_list_remove_link (invoke_list, invoke_list);

      g_list_free (l);

      if ((repaint_func->flags & flags) != 0)
        res = repaint_func->func (repaint_func->data);
      else
        res = TRUE;

      if (res)
        reinvoke_list = g_list_prepend (reinvoke_list, repaint_func);
      else
        {
          if (repaint_func->notify != NULL)
            repaint_func->notify (repaint_func->data);

          g_free (repaint_func);
        }
    }

  if (context->repaint_funcs != NULL)
    {
      context->repaint_funcs = g_list_concat (context->repaint_funcs,
                                              g_list_reverse (reinvoke_list));
    }
  else
    context->repaint_funcs = g_list_reverse (reinvoke_list);
}

/**
 * clutter_get_default_text_direction:
 *
 * Retrieves the default direction for the text. The text direction is
 * determined by the locale and/or by the `CLUTTER_TEXT_DIRECTION`
 * environment variable.
 *
 * The default text direction can be overridden on a per-actor basis by using
 * clutter_actor_set_text_direction().
 *
 * Return value: the default text direction
 *
 * Since: 1.2
 */
ClutterTextDirection
clutter_get_default_text_direction (void)
{
  return clutter_text_direction;
}

/*< private >
 * clutter_clear_events_queue:
 *
 * Clears the events queue stored in the main context.
 */
void
_clutter_clear_events_queue (void)
{
  ClutterMainContext *context = _clutter_context_get_default ();
  ClutterEvent *event;
  GAsyncQueue *events_queue;

  if (!context->events_queue)
    return;

  g_async_queue_lock (context->events_queue);

  while ((event = g_async_queue_try_pop_unlocked (context->events_queue)))
    clutter_event_free (event);

  events_queue = context->events_queue;
  context->events_queue = NULL;

  g_async_queue_unlock (events_queue);
  g_async_queue_unref (events_queue);
}

/**
 * clutter_add_debug_flags: (skip)
 *
 * Adds the debug flags passed to the list of debug flags.
 */
void
clutter_add_debug_flags (ClutterDebugFlag     debug_flags,
                         ClutterDrawDebugFlag draw_flags,
                         ClutterPickDebugFlag pick_flags)
{
  clutter_debug_flags |= debug_flags;
  clutter_paint_debug_flags |= draw_flags;
  clutter_pick_debug_flags |= pick_flags;
}

/**
 * clutter_remove_debug_flags: (skip)
 *
 * Removes the debug flags passed from the list of debug flags.
 */
void
clutter_remove_debug_flags (ClutterDebugFlag     debug_flags,
                            ClutterDrawDebugFlag draw_flags,
                            ClutterPickDebugFlag pick_flags)
{
  clutter_debug_flags &= ~debug_flags;
  clutter_paint_debug_flags &= ~draw_flags;
  clutter_pick_debug_flags &= ~pick_flags;
}

void
clutter_debug_set_max_render_time_constant (int max_render_time_constant_us)
{
  clutter_max_render_time_constant_us = max_render_time_constant_us;
}

void
clutter_get_debug_flags (ClutterDebugFlag     *debug_flags,
                         ClutterDrawDebugFlag *draw_flags,
                         ClutterPickDebugFlag *pick_flags)
{
  if (debug_flags)
    *debug_flags = clutter_debug_flags;
  if (draw_flags)
    *draw_flags = clutter_paint_debug_flags;
  if (pick_flags)
    *pick_flags = clutter_pick_debug_flags;
}

void
_clutter_set_sync_to_vblank (gboolean sync_to_vblank)
{
  clutter_sync_to_vblank = !!sync_to_vblank;
}

void
_clutter_debug_messagev (const char *format,
                         va_list     var_args)
{
  static gint64 last_debug_stamp;
  gchar *stamp, *fmt;
  gint64 cur_time, debug_stamp;

  cur_time = g_get_monotonic_time ();

  /* if the last debug message happened less than a second ago, just
   * show the increments instead of the full timestamp
   */
  if (last_debug_stamp == 0 ||
      cur_time - last_debug_stamp >= G_USEC_PER_SEC)
    {
      debug_stamp = cur_time;
      last_debug_stamp = debug_stamp;

      stamp = g_strdup_printf ("[%16" G_GINT64_FORMAT "]", debug_stamp);
    }
  else
    {
      debug_stamp = cur_time - last_debug_stamp;

      stamp = g_strdup_printf ("[%+16" G_GINT64_FORMAT "]", debug_stamp);
    }

  fmt = g_strconcat (stamp, ":", format, NULL);
  g_free (stamp);

  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, var_args);

  g_free (fmt);
}

void
_clutter_debug_message (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  _clutter_debug_messagev (format, args);
  va_end (args);
}

gboolean
_clutter_diagnostic_enabled (void)
{
  static const char *clutter_enable_diagnostic = NULL;

  if (G_UNLIKELY (clutter_enable_diagnostic == NULL))
    {
      clutter_enable_diagnostic = g_getenv ("CLUTTER_ENABLE_DIAGNOSTIC");

      if (clutter_enable_diagnostic == NULL)
        clutter_enable_diagnostic = "0";
    }

  return *clutter_enable_diagnostic != '0';
}

void
_clutter_diagnostic_message (const char *format, ...)
{
  va_list args;
  char *fmt;

  fmt = g_strconcat ("[DIAGNOSTIC]: ", format, NULL);

  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, args);
  va_end (args);

  g_free (fmt);
}
