/*
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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
 *
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "backends/meta-stage-private.h"
#include "backends/x11/cm/meta-backend-x11-cm.h"
#include "backends/x11/cm/meta-renderer-x11-cm.h"
#include "backends/x11/meta-backend-x11.h"
#include "backends/x11/meta-clutter-backend-x11.h"
#include "backends/x11/meta-seat-x11.h"
#include "backends/x11/meta-stage-x11.h"
#include "backends/x11/nested/meta-stage-x11-nested.h"
#include "clutter/clutter-mutter.h"
#include "cogl/cogl-mutter.h"
#include "cogl/cogl.h"
#include "core/display-private.h"
#include "meta/meta-context.h"
#include "meta/meta-x11-errors.h"

#define STAGE_X11_IS_MAPPED(s)  ((((MetaStageX11 *) (s))->wm_state & STAGE_X11_WITHDRAWN) == 0)

static ClutterStageWindowInterface *clutter_stage_window_parent_iface = NULL;

static void
clutter_stage_window_iface_init (ClutterStageWindowInterface *iface);

static MetaStageImpl *meta_x11_get_stage_window_from_window (Window win);

static GHashTable *clutter_stages_by_xid = NULL;

G_DEFINE_TYPE_WITH_CODE (MetaStageX11,
                         meta_stage_x11,
                         META_TYPE_STAGE_IMPL,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

#define META_STAGE_X11_EVENT_MASK \
  StructureNotifyMask | \
  FocusChangeMask | \
  ExposureMask | \
  PropertyChangeMask | \
  EnterWindowMask | \
  LeaveWindowMask | \
  KeyPressMask | \
  KeyReleaseMask | \
  ButtonPressMask | \
  ButtonReleaseMask | \
  PointerMotionMask

static MetaClutterBackendX11 *
clutter_backend_x11_from_stage (MetaStageX11 *stage_x11)
{
  MetaBackend *backend =
    meta_stage_impl_get_backend (META_STAGE_IMPL (stage_x11));

  return META_CLUTTER_BACKEND_X11 (meta_backend_get_clutter_backend (backend));
}

static Display *
xdisplay_from_stage (MetaStageX11 *stage_x11)
{
  MetaBackend *backend =
    meta_stage_impl_get_backend (META_STAGE_IMPL (stage_x11));

  return meta_backend_x11_get_xdisplay (META_BACKEND_X11 (backend));
}

static void
meta_stage_x11_fix_window_size (MetaStageX11 *stage_x11,
                                int           new_width,
                                int           new_height)
{
  g_return_if_fail (new_width > 0);
  g_return_if_fail (new_height > 0);

  if (stage_x11->xwin != None)
    {
      Display *xdisplay = xdisplay_from_stage (stage_x11);
      XSizeHints *size_hints;

      size_hints = XAllocSizeHints();

      size_hints->min_width = new_width;
      size_hints->min_height = new_height;
      size_hints->max_width = new_width;
      size_hints->max_height = new_height;
      size_hints->flags = PMinSize | PMaxSize;

      XSetWMNormalHints (xdisplay, stage_x11->xwin, size_hints);

      XFree(size_hints);
    }
}

static void
meta_stage_x11_set_wm_protocols (MetaStageX11 *stage_x11)
{
  MetaClutterBackendX11 *clutter_backend_x11 =
    clutter_backend_x11_from_stage (stage_x11);
  Display *xdisplay = xdisplay_from_stage (stage_x11);
  Atom protocols[2];
  int n = 0;

  protocols[n++] = clutter_backend_x11->atom_WM_DELETE_WINDOW;
  protocols[n++] = clutter_backend_x11->atom_NET_WM_PING;

  XSetWMProtocols (xdisplay, stage_x11->xwin, protocols, n);
}

static void
meta_stage_x11_get_geometry (ClutterStageWindow    *stage_window,
                             cairo_rectangle_int_t *geometry)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);

  geometry->x = geometry->y = 0;
  geometry->width = stage_x11->xwin_width;
  geometry->height = stage_x11->xwin_height;
}

static void
meta_stage_x11_resize (ClutterStageWindow *stage_window,
                       int                 width,
                       int                 height)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);

  if (width == 0 || height == 0)
    {
      /* Should not happen, if this turns up we need to debug it and
       * determine the cleanest way to fix.
       */
      g_warning ("X11 stage not allowed to have 0 width or height");
      width = 1;
      height = 1;
    }

  if (stage_x11->xwin != None)
    {
      meta_stage_x11_fix_window_size (stage_x11, width, height);

      if (width != stage_x11->xwin_width ||
          height != stage_x11->xwin_height)
        {
          Display *xdisplay = xdisplay_from_stage (stage_x11);

          /* XXX: in this case we can rely on a subsequent
           * ConfigureNotify that will result in the stage
           * being reallocated so we don't actively do anything
           * to affect the stage allocation here. */
          XResizeWindow (xdisplay,
                         stage_x11->xwin,
                         width,
                         height);
        }
    }
  else
    {
      /* if the backing window hasn't been created yet, we just
       * need to store the new window size
       */
      stage_x11->xwin_width = width;
      stage_x11->xwin_height = height;
    }
}

static inline void
set_wm_pid (MetaStageX11 *stage_x11)
{
  MetaStageImpl *stage_impl = META_STAGE_IMPL (stage_x11);
  MetaBackend *backend = meta_stage_impl_get_backend (stage_impl);
  MetaBackendX11 *backend_x11 = META_BACKEND_X11 (backend);
  MetaClutterBackendX11 *clutter_backend_x11 =
    clutter_backend_x11_from_stage (stage_x11);
  Display *xdisplay = meta_backend_x11_get_xdisplay (backend_x11);
  long pid;

  if (stage_x11->xwin == None)
    return;

  /* this will take care of WM_CLIENT_MACHINE and WM_LOCALE_NAME */
  XSetWMProperties (xdisplay, stage_x11->xwin,
                    NULL,
                    NULL,
                    NULL, 0,
                    NULL, NULL, NULL);

  pid = getpid ();
  XChangeProperty (xdisplay,
                   stage_x11->xwin,
                   clutter_backend_x11->atom_NET_WM_PID, XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &pid, 1);
}

static inline void
set_wm_title (MetaStageX11 *stage_x11)
{
  MetaClutterBackendX11 *clutter_backend_x11 =
    clutter_backend_x11_from_stage (stage_x11);
  Display *xdisplay = xdisplay_from_stage (stage_x11);

  if (stage_x11->xwin == None)
    return;

  if (stage_x11->title == NULL)
    {
      XDeleteProperty (xdisplay,
                       stage_x11->xwin,
                       clutter_backend_x11->atom_NET_WM_NAME);
    }
  else
    {
      XChangeProperty (xdisplay,
                       stage_x11->xwin,
                       clutter_backend_x11->atom_NET_WM_NAME,
                       clutter_backend_x11->atom_UTF8_STRING,
                       8,
                       PropModeReplace,
                       (unsigned char *) stage_x11->title,
                       (int) strlen (stage_x11->title));
    }
}

static void
meta_stage_x11_unrealize (ClutterStageWindow *stage_window)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);

  if (clutter_stages_by_xid != NULL)
    {
      g_hash_table_remove (clutter_stages_by_xid,
                           GINT_TO_POINTER (stage_x11->xwin));
    }

  clutter_stage_window_parent_iface->unrealize (stage_window);

  g_clear_object (&stage_x11->onscreen);
}

static CoglOnscreen *
create_onscreen (CoglContext *cogl_context,
                 int          width,
                 int          height)
{
  CoglDisplay *cogl_display = cogl_context_get_display (cogl_context);
  CoglRenderer *cogl_renderer = cogl_display_get_renderer (cogl_display);

  switch (cogl_renderer_get_winsys_id (cogl_renderer))
    {
    case COGL_WINSYS_ID_GLX:
#ifdef COGL_HAS_GLX_SUPPORT
      return COGL_ONSCREEN (cogl_onscreen_glx_new (cogl_context,
                                                   width, height));
#else
      g_assert_not_reached ();
      break;
#endif
    case COGL_WINSYS_ID_EGL_XLIB:
#ifdef COGL_HAS_EGL_SUPPORT
      return COGL_ONSCREEN (cogl_onscreen_xlib_new (cogl_context,
                                                    width, height));
#else
      g_assert_not_reached ();
      break;
#endif
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static gboolean
meta_stage_x11_realize (ClutterStageWindow *stage_window)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);
  MetaStageImpl *stage_impl = META_STAGE_IMPL (stage_window);
  MetaBackend *backend = meta_stage_impl_get_backend (stage_impl);
  MetaBackendX11 *backend_x11 = META_BACKEND_X11 (backend);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  MetaSeatX11 *seat_x11 =
    META_SEAT_X11 (meta_backend_get_default_seat (backend));
  Display *xdisplay = meta_backend_x11_get_xdisplay (backend_x11);
  float width, height;
  GError *error = NULL;

  clutter_actor_get_size (CLUTTER_ACTOR (stage_impl->wrapper), &width, &height);

  stage_x11->onscreen = create_onscreen (clutter_backend->cogl_context,
                                         width, height);

  if (META_IS_BACKEND_X11_CM (backend))
    {
      MetaRenderer *renderer = meta_backend_get_renderer (backend);
      MetaRendererX11Cm *renderer_x11_cm = META_RENDERER_X11_CM (renderer);

      meta_renderer_x11_cm_init_screen_view (renderer_x11_cm,
                                             stage_x11->onscreen,
                                             stage_x11->xwin_width,
                                             stage_x11->xwin_height);
    }

  /* We just created a window of the size of the actor. No need to fix
     the size of the stage, just update it. */
  stage_x11->xwin_width = width;
  stage_x11->xwin_height = height;

  if (!cogl_framebuffer_allocate (COGL_FRAMEBUFFER (stage_x11->onscreen), &error))
    {
      g_warning ("Failed to allocate stage: %s", error->message);
      g_error_free (error);
      g_object_unref (stage_x11->onscreen);
      abort();
    }

  if (!(clutter_stage_window_parent_iface->realize (stage_window)))
    return FALSE;

  stage_x11->xwin =
    cogl_x11_onscreen_get_x11_window (COGL_X11_ONSCREEN (stage_x11->onscreen));

  if (clutter_stages_by_xid == NULL)
    clutter_stages_by_xid = g_hash_table_new (NULL, NULL);

  g_hash_table_insert (clutter_stages_by_xid,
                       GINT_TO_POINTER (stage_x11->xwin),
                       stage_x11);

  set_wm_pid (stage_x11);
  set_wm_title (stage_x11);

  /* we unconditionally select input events even with event retrieval
   * disabled because we need to guarantee that the Clutter internal
   * state is maintained when calling meta_clutter_x11_handle_event() without
   * requiring applications or embedding toolkits to select events
   * themselves. if we did that, we'd have to document the events to be
   * selected, and also update applications and embedding toolkits each
   * time we added a new mask, or a new class of events.
   *
   * see: http://bugzilla.clutter-project.org/show_bug.cgi?id=998
   * for the rationale of why we did conditional selection. it is now
   * clear that a compositor should clear out the input region, since
   * it cannot assume a perfectly clean slate coming from us.
   *
   * see: http://bugzilla.clutter-project.org/show_bug.cgi?id=2228
   * for an example of things that break if we do conditional event
   * selection.
   */
  XSelectInput (xdisplay, stage_x11->xwin, META_STAGE_X11_EVENT_MASK);

  meta_seat_x11_select_stage_events (seat_x11, stage_impl->wrapper);

  meta_stage_x11_fix_window_size (stage_x11,
                                  stage_x11->xwin_width,
                                  stage_x11->xwin_height);
  meta_stage_x11_set_wm_protocols (stage_x11);

  return TRUE;
}

static void
meta_stage_x11_set_title (ClutterStageWindow *stage_window,
                          const char         *title)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);

  g_free (stage_x11->title);
  stage_x11->title = g_strdup (title);
  set_wm_title (stage_x11);
}

static inline void
update_wm_hints (MetaStageX11 *stage_x11)
{
  Display *xdisplay = xdisplay_from_stage (stage_x11);
  XWMHints wm_hints;

  if (stage_x11->wm_state & STAGE_X11_WITHDRAWN)
    return;

  wm_hints.flags = StateHint | InputHint;
  wm_hints.initial_state = NormalState;
  wm_hints.input = True;

  XSetWMHints (xdisplay, stage_x11->xwin, &wm_hints);
}

static void
set_stage_x11_state (MetaStageX11      *stage_x11,
                     MetaStageX11State  unset_flags,
                     MetaStageX11State  set_flags)
{
  MetaStageX11State new_stage_state, old_stage_state;

  old_stage_state = stage_x11->wm_state;

  new_stage_state = old_stage_state;
  new_stage_state |= set_flags;
  new_stage_state &= ~unset_flags;

  if (new_stage_state == old_stage_state)
    return;

  stage_x11->wm_state = new_stage_state;
}

static void
meta_stage_x11_show (ClutterStageWindow *stage_window,
                     gboolean            do_raise)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);
  MetaStageImpl *stage_impl = META_STAGE_IMPL (stage_x11);

  if (stage_x11->xwin != None)
    {
      Display *xdisplay = xdisplay_from_stage (stage_x11);

      if (do_raise)
        {
          XRaiseWindow (xdisplay, stage_x11->xwin);
        }

      if (!STAGE_X11_IS_MAPPED (stage_x11))
        {
          set_stage_x11_state (stage_x11, STAGE_X11_WITHDRAWN, 0);

          update_wm_hints (stage_x11);
        }

      g_assert (STAGE_X11_IS_MAPPED (stage_x11));

      clutter_actor_map (CLUTTER_ACTOR (stage_impl->wrapper));

      XMapWindow (xdisplay, stage_x11->xwin);
    }
}

static void
meta_stage_x11_hide (ClutterStageWindow *stage_window)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);
  MetaStageImpl *stage_impl = META_STAGE_IMPL (stage_x11);

  if (stage_x11->xwin != None)
    {
      Display *xdisplay = xdisplay_from_stage (stage_x11);

      if (STAGE_X11_IS_MAPPED (stage_x11))
        set_stage_x11_state (stage_x11, 0, STAGE_X11_WITHDRAWN);

      g_assert (!STAGE_X11_IS_MAPPED (stage_x11));

      clutter_actor_unmap (CLUTTER_ACTOR (stage_impl->wrapper));

      XWithdrawWindow (xdisplay, stage_x11->xwin, 0);
    }
}

static gboolean
meta_stage_x11_can_clip_redraws (ClutterStageWindow *stage_window)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);

  /* while resizing a window, clipped redraws are disabled in order to
   * avoid artefacts.
   */
  return stage_x11->clipped_redraws_cool_off == 0;
}

static GList *
meta_stage_x11_get_views (ClutterStageWindow *stage_window)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (stage_window);
  MetaRenderer *renderer = meta_backend_get_renderer (stage_x11->backend);

  return meta_renderer_get_views (renderer);
}

static void
meta_stage_x11_redraw_view (ClutterStageWindow *stage_window,
                            ClutterStageView   *view,
                            ClutterFrame       *frame)
{
  clutter_stage_window_parent_iface->redraw_view (stage_window, view, frame);
  clutter_frame_set_result (frame, CLUTTER_FRAME_RESULT_PENDING_PRESENTED);
}

static void
meta_stage_x11_finalize (GObject *object)
{
  MetaStageX11 *stage_x11 = META_STAGE_X11 (object);

  g_free (stage_x11->title);

  G_OBJECT_CLASS (meta_stage_x11_parent_class)->finalize (object);
}

static void
meta_stage_x11_class_init (MetaStageX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = meta_stage_x11_finalize;
}

static void
meta_stage_x11_init (MetaStageX11 *stage)
{
  stage->xwin = None;
  stage->xwin_width = 640;
  stage->xwin_height = 480;

  stage->wm_state = STAGE_X11_WITHDRAWN;

  stage->title = NULL;

  stage->backend = meta_get_backend ();
  g_assert (stage->backend);
}

static void
clutter_stage_window_iface_init (ClutterStageWindowInterface *iface)
{
  clutter_stage_window_parent_iface = g_type_interface_peek_parent (iface);

  iface->set_title = meta_stage_x11_set_title;
  iface->show = meta_stage_x11_show;
  iface->hide = meta_stage_x11_hide;
  iface->resize = meta_stage_x11_resize;
  iface->get_geometry = meta_stage_x11_get_geometry;
  iface->realize = meta_stage_x11_realize;
  iface->unrealize = meta_stage_x11_unrealize;
  iface->can_clip_redraws = meta_stage_x11_can_clip_redraws;
  iface->get_views = meta_stage_x11_get_views;
  iface->redraw_view = meta_stage_x11_redraw_view;
}

static inline void
set_user_time (MetaStageX11 *stage_x11,
               long          timestamp)
{
  MetaClutterBackendX11 *clutter_backend_x11 =
    clutter_backend_x11_from_stage (stage_x11);

  if (timestamp != CLUTTER_CURRENT_TIME)
    {
      Display *xdisplay = xdisplay_from_stage (stage_x11);

      XChangeProperty (xdisplay,
                       stage_x11->xwin,
                       clutter_backend_x11->atom_NET_WM_USER_TIME,
                       XA_CARDINAL, 32,
                       PropModeReplace,
                       (unsigned char *) &timestamp, 1);
    }
}

static gboolean
handle_wm_protocols_event (MetaStageX11 *stage_x11,
                           XEvent       *xevent)
{
  MetaClutterBackendX11 *clutter_backend_x11 =
    clutter_backend_x11_from_stage (stage_x11);
  Atom atom = (Atom) xevent->xclient.data.l[0];

  if (atom == clutter_backend_x11->atom_WM_DELETE_WINDOW &&
      xevent->xany.window == stage_x11->xwin)
    {
      set_user_time (stage_x11, xevent->xclient.data.l[1]);

      return TRUE;
    }
  else if (atom == clutter_backend_x11->atom_NET_WM_PING &&
           xevent->xany.window == stage_x11->xwin)
    {
      MetaBackend *backend =
        meta_stage_impl_get_backend (META_STAGE_IMPL (stage_x11));
      MetaBackendX11 *backend_x11 = META_BACKEND_X11 (backend);
      Display *xdisplay = meta_backend_x11_get_xdisplay (backend_x11);
      Window root_xwindow = meta_backend_x11_get_root_xwindow (backend_x11);
      XClientMessageEvent xclient = xevent->xclient;

      xclient.window = root_xwindow;
      XSendEvent (xdisplay, xclient.window,
                  False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *) &xclient);
      return FALSE;
    }

  /* do not send any of the WM_PROTOCOLS events to the queue */
  return FALSE;
}

static gboolean
clipped_redraws_cool_off_cb (void *data)
{
  MetaStageX11 *stage_x11 = data;

  stage_x11->clipped_redraws_cool_off = 0;

  return G_SOURCE_REMOVE;
}

gboolean
meta_stage_x11_translate_event (MetaStageX11 *stage_x11,
                                XEvent       *xevent,
                                ClutterEvent *event)
{
  MetaClutterBackendX11 *clutter_backend_x11 =
    clutter_backend_x11_from_stage (stage_x11);
  MetaBackend *backend;
  MetaStageImpl *stage_impl;
  gboolean res = FALSE;
  ClutterStage *stage;

  stage_impl = meta_x11_get_stage_window_from_window (xevent->xany.window);
  if (stage_impl == NULL)
    return FALSE;

  backend = meta_stage_impl_get_backend (stage_impl);
  stage = stage_impl->wrapper;

  switch (xevent->type)
    {
    case ConfigureNotify:
        {
          gboolean size_changed = FALSE;
          int stage_width;
          int stage_height;

          g_debug ("ConfigureNotify[%x] (%d, %d)",
                   (unsigned int) stage_x11->xwin,
                   xevent->xconfigure.width,
                   xevent->xconfigure.height);

          if ((stage_x11->xwin_width != xevent->xconfigure.width) ||
              (stage_x11->xwin_height != xevent->xconfigure.height))
            {
              size_changed = TRUE;
              stage_x11->xwin_width = xevent->xconfigure.width;
              stage_x11->xwin_height = xevent->xconfigure.height;
            }

          stage_width = xevent->xconfigure.width;
          stage_height = xevent->xconfigure.height;

          if (META_IS_BACKEND_X11_CM (backend))
            {
              clutter_actor_set_size (CLUTTER_ACTOR (stage),
                                      stage_width,
                                      stage_height);
            }

          if (size_changed)
            {
              /* XXX: This is a workaround for a race condition when
               * resizing windows while there are in-flight
               * glXCopySubBuffer blits happening.
               *
               * The problem stems from the fact that rectangles for the
               * blits are described relative to the bottom left of the
               * window and because we can't guarantee control over the X
               * window gravity used when resizing so the gravity is
               * typically NorthWest not SouthWest.
               *
               * This means if you grow a window vertically the server
               * will make sure to place the old contents of the window
               * at the top-left/north-west of your new larger window, but
               * that may happen asynchronous to GLX preparing to do a
               * blit specified relative to the bottom-left/south-west of
               * the window (based on the old smaller window geometry).
               *
               * When the GLX issued blit finally happens relative to the
               * new bottom of your window, the destination will have
               * shifted relative to the top-left where all the pixels you
               * care about are so it will result in a nasty artefact
               * making resizing look very ugly!
               *
               * We can't currently fix this completely, in-part because
               * the window manager tends to trample any gravity we might
               * set.  This workaround instead simply disables blits for a
               * while if we are notified of any resizes happening so if
               * the user is resizing a window via the window manager then
               * they may see an artefact for one frame but then we will
               * fallback to redrawing the full stage until the cooling
               * off period is over.
               */
              g_clear_handle_id (&stage_x11->clipped_redraws_cool_off,
                                 g_source_remove);

              stage_x11->clipped_redraws_cool_off =
                clutter_threads_add_timeout (1000,
                                             clipped_redraws_cool_off_cb,
                                             stage_x11);

              /* Queue a relayout - we want glViewport to be called
               * with the correct values, and this is done in ClutterStage
               * via cogl_onscreen_clutter_backend_set_size ().
               *
               * We queue a relayout, because if this ConfigureNotify is
               * in response to a size we set in the application, the
               * set_size() call above is essentially a null-op.
               *
               * Make sure we do this only when the size has changed,
               * otherwise we end up relayouting on window moves.
               */
              clutter_actor_queue_relayout (CLUTTER_ACTOR (stage));

              /* the resize process is complete, so we can ask the stage
               * to set up the GL viewport with the new size
               */
              clutter_stage_ensure_viewport (stage);

              /* If this was a result of the Xrandr change when running as a
               * X11 compositing manager, we need to reset the legacy
               * stage view, now that it has a new size.
               */
              if (META_IS_BACKEND_X11_CM (backend))
                {
                  MetaRenderer *renderer = meta_backend_get_renderer (backend);
                  MetaRendererX11Cm *renderer_x11_cm =
                    META_RENDERER_X11_CM (renderer);

                  meta_renderer_x11_cm_resize (renderer_x11_cm,
                                               stage_width,
                                               stage_height);
                }
            }
        }
      break;

    case FocusIn:
      meta_stage_set_active ((MetaStage *) stage_impl->wrapper, TRUE);
      break;

    case FocusOut:
      meta_stage_set_active ((MetaStage *) stage_impl->wrapper, FALSE);
      break;

    case Expose:
      {
        XExposeEvent *expose = (XExposeEvent *) xevent;
        cairo_rectangle_int_t clip;

        g_debug ("expose for stage: win:0x%x - "
                 "redrawing area (x: %d, y: %d, width: %d, height: %d)",
                 (unsigned int) xevent->xany.window,
                 expose->x,
                 expose->y,
                 expose->width,
                 expose->height);

        clip.x = expose->x;
        clip.y = expose->y;
        clip.width = expose->width;
        clip.height = expose->height;
        clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stage), &clip);
      }
      break;

    case DestroyNotify:
      g_debug ("Destroy notification received for stage, win:0x%x",
               (unsigned int) xevent->xany.window);

      g_return_val_if_fail (META_IS_STAGE_X11_NESTED (stage_x11),
                            FALSE);
      meta_context_terminate (meta_backend_get_context (backend));
      res = FALSE;
      break;

    case ClientMessage:
      g_debug ("Client message for stage, win:0x%x",
               (unsigned int) xevent->xany.window);

      if (xevent->xclient.message_type == clutter_backend_x11->atom_WM_PROTOCOLS)
        {
          if (handle_wm_protocols_event (stage_x11, xevent))
            {
              g_return_val_if_fail (META_IS_STAGE_X11_NESTED (stage_x11),
                                    FALSE);
              meta_context_terminate (meta_backend_get_context (backend));
              res = FALSE;
            }
        }

      break;

    default:
      res = FALSE;
      break;
    }

  return res;
}

Window
meta_x11_get_stage_window (ClutterStage *stage)
{
  ClutterStageWindow *impl;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), None);

  impl = _clutter_stage_get_window (stage);
  g_assert (META_IS_STAGE_X11 (impl));

  return META_STAGE_X11 (impl)->xwin;
}

static MetaStageImpl *
meta_x11_get_stage_window_from_window (Window win)
{
  if (clutter_stages_by_xid == NULL)
    return NULL;

  return g_hash_table_lookup (clutter_stages_by_xid,
                              GINT_TO_POINTER (win));
}

ClutterStage *
meta_x11_get_stage_from_window (Window win)
{
  MetaStageImpl *stage_impl;

  stage_impl = meta_x11_get_stage_window_from_window (win);

  if (stage_impl != NULL)
    return stage_impl->wrapper;

  return NULL;
}

void
meta_stage_x11_set_user_time (MetaStageX11 *stage_x11,
                              uint32_t      user_time)
{
  set_user_time (stage_x11, user_time);
}
