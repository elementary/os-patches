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

#ifndef __CLUTTER_BACKEND_PRIVATE_H__
#define __CLUTTER_BACKEND_PRIVATE_H__

#include <clutter/clutter-backend.h>
#include <clutter/clutter-seat.h>
#include <clutter/clutter-stage-window.h>

#define CLUTTER_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND, ClutterBackendClass))
#define CLUTTER_IS_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND))
#define CLUTTER_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND, ClutterBackendClass))

G_BEGIN_DECLS

typedef struct _ClutterBackendPrivate   ClutterBackendPrivate;

struct _ClutterBackend
{
  /*< private >*/
  GObject parent_instance;

  CoglRenderer *cogl_renderer;
  CoglDisplay *cogl_display;
  CoglContext *cogl_context;
  GSource *cogl_source;

  CoglOnscreen *dummy_onscreen;

  cairo_font_options_t *font_options;

  gchar *font_name;

  gfloat units_per_em;
  gint32 units_serial;

  float fallback_resource_scale;

  ClutterStageWindow *stage_window;

  ClutterInputMethod *input_method;
};

struct _ClutterBackendClass
{
  /*< private >*/
  GObjectClass parent_class;

  /* vfuncs */
  gboolean              (* finish_init)        (ClutterBackend  *backend,
                                                GError         **error);
  ClutterStageWindow *  (* create_stage)       (ClutterBackend  *backend,
                                                ClutterStage    *wrapper,
                                                GError         **error);
  void                  (* init_features)      (ClutterBackend  *backend);
  CoglRenderer *        (* get_renderer)       (ClutterBackend  *backend,
                                                GError         **error);
  CoglDisplay *         (* get_display)        (ClutterBackend  *backend,
                                                CoglRenderer    *renderer,
                                                CoglSwapChain   *swap_chain,
                                                GError         **error);
  gboolean              (* create_context)     (ClutterBackend  *backend,
                                                GError         **error);

  gboolean              (* translate_event)    (ClutterBackend     *backend,
                                                gpointer            native,
                                                ClutterEvent       *event);

  ClutterSeat *         (* get_default_seat)   (ClutterBackend *backend);

  gboolean              (* is_display_server)  (ClutterBackend *backend);

  /* signals */
  void (* resolution_changed) (ClutterBackend *backend);
  void (* font_changed)       (ClutterBackend *backend);
  void (* settings_changed)   (ClutterBackend *backend);
};

ClutterStageWindow *    _clutter_backend_create_stage                   (ClutterBackend         *backend,
                                                                         ClutterStage           *wrapper,
                                                                         GError                **error);
gboolean                _clutter_backend_create_context                 (ClutterBackend         *backend,
                                                                         GError                **error);

gboolean                _clutter_backend_finish_init                    (ClutterBackend         *backend,
                                                                         GError                **error);

CLUTTER_EXPORT
gboolean                _clutter_backend_translate_event                (ClutterBackend         *backend,
                                                                         gpointer                native,
                                                                         ClutterEvent           *event);

gfloat                  _clutter_backend_get_units_per_em               (ClutterBackend         *backend,
                                                                         PangoFontDescription   *font_desc);
gint32                  _clutter_backend_get_units_serial               (ClutterBackend         *backend);

void                    clutter_set_allowed_drivers                     (const char             *drivers);

CLUTTER_EXPORT
ClutterStageWindow *    clutter_backend_get_stage_window                (ClutterBackend         *backend);

CLUTTER_EXPORT
void clutter_backend_set_fallback_resource_scale (ClutterBackend *backend,
                                                  float           fallback_resource_scale);

float clutter_backend_get_fallback_resource_scale (ClutterBackend *backend);

gboolean clutter_backend_is_display_server (ClutterBackend *backend);

CLUTTER_EXPORT
void clutter_backend_destroy (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_PRIVATE_H__ */
