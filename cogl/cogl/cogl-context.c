/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#include "cogl-config.h"

#include "cogl-mutter.h"
#include "cogl-object.h"
#include "cogl-private.h"
#include "cogl-profile.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-display-private.h"
#include "cogl-renderer-private.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-attribute-private.h"
#include "cogl1-context.h"
#include "cogl-gtype-private.h"
#include "winsys/cogl-winsys-private.h"

#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>

static void _cogl_context_free (CoglContext *context);

COGL_OBJECT_DEFINE (Context, context);
COGL_GTYPE_DEFINE_CLASS (Context, context);

extern void
_cogl_create_context_driver (CoglContext *context);

static CoglContext *_cogl_context = NULL;

static void
_cogl_init_feature_overrides (CoglContext *ctx)
{
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_PBOS)))
    COGL_FLAGS_SET (ctx->private_features, COGL_PRIVATE_FEATURE_PBOS, FALSE);
}

const CoglWinsysVtable *
_cogl_context_get_winsys (CoglContext *context)
{
  return context->display->renderer->winsys_vtable;
}

static const CoglDriverVtable *
_cogl_context_get_driver (CoglContext *context)
{
  return context->driver_vtable;
}

/* For reference: There was some deliberation over whether to have a
 * constructor that could throw an exception but looking at standard
 * practices with several high level OO languages including python, C++,
 * C# Java and Ruby they all support exceptions in constructors and the
 * general consensus appears to be that throwing an exception is neater
 * than successfully constructing with an internal error status that
 * would then have to be explicitly checked via some form of ::is_ok()
 * method.
 */
CoglContext *
cogl_context_new (CoglDisplay *display,
                  GError **error)
{
  CoglContext *context;
  uint8_t white_pixel[] = { 0xff, 0xff, 0xff, 0xff };
  const CoglWinsysVtable *winsys;
  int i;
  GError *local_error = NULL;

  _cogl_init ();

#ifdef COGL_ENABLE_PROFILE
  /* We need to be absolutely sure that uprof has been initialized
   * before calling _cogl_uprof_init. uprof_init (NULL, NULL)
   * will be a NOP if it has been initialized but it will also
   * mean subsequent parsing of the UProf GOptionGroup will have no
   * affect.
   *
   * Sadly GOptionGroup based library initialization is extremely
   * fragile by design because GOptionGroups have no notion of
   * dependencies and so the order things are initialized isn't
   * currently under tight control.
   */
  uprof_init (NULL, NULL);
  _cogl_uprof_init ();
#endif

  /* Allocate context memory */
  context = g_malloc0 (sizeof (CoglContext));

  /* Convert the context into an object immediately in case any of the
     code below wants to verify that the context pointer is a valid
     object */
  _cogl_context_object_new (context);

  /* XXX: Gross hack!
   * Currently everything in Cogl just assumes there is a default
   * context which it can access via _COGL_GET_CONTEXT() including
   * code used to construct a CoglContext. Until all of that code
   * has been updated to take an explicit context argument we have
   * to immediately make our pointer the default context.
   */
  _cogl_context = context;

  /* Init default values */
  memset (context->features, 0, sizeof (context->features));
  memset (context->private_features, 0, sizeof (context->private_features));
  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  if (!display)
    {
      CoglRenderer *renderer = cogl_renderer_new ();
      if (!cogl_renderer_connect (renderer, error))
        {
          cogl_object_unref (renderer);
          g_free (context);
          return NULL;
        }

      display = cogl_display_new (renderer, NULL);
      cogl_object_unref(renderer);
    }
  else
    cogl_object_ref (display);

  if (!cogl_display_setup (display, error))
    {
      cogl_object_unref (display);
      g_free (context);
      return NULL;
    }

  context->display = display;

  /* This is duplicated data, but it's much more convenient to have
     the driver attached to the context and the value is accessed a
     lot throughout Cogl */
  context->driver = display->renderer->driver;

  /* Again this is duplicated data, but it convenient to be able
   * access these from the context. */
  context->driver_vtable = display->renderer->driver_vtable;
  context->texture_driver = display->renderer->texture_driver;

  for (i = 0; i < G_N_ELEMENTS (context->private_features); i++)
    context->private_features[i] |= display->renderer->private_features[i];

  winsys = _cogl_context_get_winsys (context);
  if (!winsys->context_init (context, error))
    {
      cogl_object_unref (display);
      g_free (context);
      return NULL;
    }

  if (!context->driver_vtable->context_init (context))
    {
      cogl_object_unref (display);
      g_free (context);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to initialize context");
      return NULL;
    }

  context->attribute_name_states_hash =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  context->attribute_name_index_map = NULL;
  context->n_attribute_names = 0;

  /* The "cogl_color_in" attribute needs a deterministic name_index
   * so we make sure it's the first attribute name we register */
  _cogl_attribute_register_attribute_name (context, "cogl_color_in");


  context->uniform_names =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
  context->uniform_name_hash = g_hash_table_new (g_str_hash, g_str_equal);
  context->n_uniform_names = 0;

  /* Initialise the driver specific state */
  _cogl_init_feature_overrides (context);

  context->sampler_cache = _cogl_sampler_cache_new (context);

  _cogl_pipeline_init_default_pipeline ();
  _cogl_pipeline_init_default_layers ();
  _cogl_pipeline_init_state_hash_functions ();
  _cogl_pipeline_init_layer_state_hash_functions ();

  context->current_clip_stack_valid = FALSE;
  context->current_clip_stack = NULL;

  graphene_matrix_init_identity (&context->identity_matrix);
  graphene_matrix_init_identity (&context->y_flip_matrix);
  graphene_matrix_scale (&context->y_flip_matrix, 1, -1, 1);

  context->opaque_color_pipeline = cogl_pipeline_new (context);

  context->codegen_header_buffer = g_string_new ("");
  context->codegen_source_buffer = g_string_new ("");
  context->codegen_boilerplate_buffer = g_string_new ("");

  context->default_gl_texture_2d_tex = NULL;

  context->framebuffers = NULL;
  context->current_draw_buffer = NULL;
  context->current_read_buffer = NULL;
  context->current_draw_buffer_state_flushed = 0;
  context->current_draw_buffer_changes = COGL_FRAMEBUFFER_STATE_ALL;

  context->swap_callback_closures =
    g_hash_table_new (g_direct_hash, g_direct_equal);

  _cogl_list_init (&context->onscreen_events_queue);
  _cogl_list_init (&context->onscreen_dirty_queue);

  context->journal_flush_attributes_array =
    g_array_new (TRUE, FALSE, sizeof (CoglAttribute *));
  context->journal_clip_bounds = NULL;

  context->current_pipeline = NULL;
  context->current_pipeline_changes_since_flush = 0;
  context->current_pipeline_with_color_attrib = FALSE;

  _cogl_bitmask_init (&context->enabled_custom_attributes);
  _cogl_bitmask_init (&context->enable_custom_attributes_tmp);
  _cogl_bitmask_init (&context->changed_bits_tmp);

  context->max_texture_units = -1;
  context->max_activateable_texture_units = -1;

  context->current_gl_program = 0;

  context->current_gl_dither_enabled = TRUE;

  context->gl_blend_enable_cache = FALSE;

  context->depth_test_enabled_cache = FALSE;
  context->depth_test_function_cache = COGL_DEPTH_TEST_FUNCTION_LESS;
  context->depth_writing_enabled_cache = TRUE;
  context->depth_range_near_cache = 0;
  context->depth_range_far_cache = 1;

  context->pipeline_cache = _cogl_pipeline_cache_new ();

  for (i = 0; i < COGL_BUFFER_BIND_TARGET_COUNT; i++)
    context->current_buffer[i] = NULL;

  context->stencil_pipeline = cogl_pipeline_new (context);

  context->rectangle_byte_indices = NULL;
  context->rectangle_short_indices = NULL;
  context->rectangle_short_indices_len = 0;

  context->blit_texture_pipeline = NULL;

  context->current_modelview_entry = NULL;
  context->current_projection_entry = NULL;
  _cogl_matrix_entry_identity_init (&context->identity_entry);

  /* Create default textures used for fall backs */
  context->default_gl_texture_2d_tex =
    cogl_texture_2d_new_from_data (context,
                                   1, 1,
                                   COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                   0, /* rowstride */
                                   white_pixel,
                                   &local_error);
  if (!context->default_gl_texture_2d_tex)
    {
      cogl_object_unref (display);
      g_free (context);
      g_propagate_prefixed_error (error, local_error,
                                  "Failed to create 1x1 fallback texture: ");
      return NULL;
    }

  context->atlases = NULL;
  g_hook_list_init (&context->atlas_reorganize_callbacks, sizeof (GHook));

  context->buffer_map_fallback_array = g_byte_array_new ();
  context->buffer_map_fallback_in_use = FALSE;

  _cogl_list_init (&context->fences);

  context->named_pipelines =
    g_hash_table_new_full (NULL, NULL, NULL, cogl_object_unref);

  return context;
}

static void
_cogl_context_free (CoglContext *context)
{
  const CoglWinsysVtable *winsys = _cogl_context_get_winsys (context);
  const CoglDriverVtable *driver = _cogl_context_get_driver (context);

  winsys->context_deinit (context);

  if (context->default_gl_texture_2d_tex)
    cogl_object_unref (context->default_gl_texture_2d_tex);

  if (context->opaque_color_pipeline)
    cogl_object_unref (context->opaque_color_pipeline);

  if (context->blit_texture_pipeline)
    cogl_object_unref (context->blit_texture_pipeline);

  if (context->swap_callback_closures)
    g_hash_table_destroy (context->swap_callback_closures);

  if (context->journal_flush_attributes_array)
    g_array_free (context->journal_flush_attributes_array, TRUE);
  if (context->journal_clip_bounds)
    g_array_free (context->journal_clip_bounds, TRUE);

  if (context->rectangle_byte_indices)
    cogl_object_unref (context->rectangle_byte_indices);
  if (context->rectangle_short_indices)
    cogl_object_unref (context->rectangle_short_indices);

  if (context->default_pipeline)
    cogl_object_unref (context->default_pipeline);

  if (context->dummy_layer_dependant)
    cogl_object_unref (context->dummy_layer_dependant);
  if (context->default_layer_n)
    cogl_object_unref (context->default_layer_n);
  if (context->default_layer_0)
    cogl_object_unref (context->default_layer_0);

  if (context->current_clip_stack_valid)
    _cogl_clip_stack_unref (context->current_clip_stack);

  g_slist_free (context->atlases);
  g_hook_list_clear (&context->atlas_reorganize_callbacks);

  _cogl_bitmask_destroy (&context->enabled_custom_attributes);
  _cogl_bitmask_destroy (&context->enable_custom_attributes_tmp);
  _cogl_bitmask_destroy (&context->changed_bits_tmp);

  if (context->current_modelview_entry)
    cogl_matrix_entry_unref (context->current_modelview_entry);
  if (context->current_projection_entry)
    cogl_matrix_entry_unref (context->current_projection_entry);

  _cogl_pipeline_cache_free (context->pipeline_cache);

  _cogl_sampler_cache_free (context->sampler_cache);

  g_ptr_array_free (context->uniform_names, TRUE);
  g_hash_table_destroy (context->uniform_name_hash);

  g_hash_table_destroy (context->attribute_name_states_hash);
  g_array_free (context->attribute_name_index_map, TRUE);

  g_byte_array_free (context->buffer_map_fallback_array, TRUE);

  driver->context_deinit (context);

  cogl_object_unref (context->display);

  g_hash_table_remove_all (context->named_pipelines);
  g_hash_table_destroy (context->named_pipelines);

  g_free (context);
}

CoglContext *
_cogl_context_get_default (void)
{
  GError *error = NULL;
  /* Create if doesn't exist yet */
  if (_cogl_context == NULL)
    {
      _cogl_context = cogl_context_new (NULL, &error);
      if (!_cogl_context)
        {
          g_warning ("Failed to create default context: %s",
                     error->message);
          g_error_free (error);
        }
    }

  return _cogl_context;
}

CoglDisplay *
cogl_context_get_display (CoglContext *context)
{
  return context->display;
}

CoglRenderer *
cogl_context_get_renderer (CoglContext *context)
{
  return context->display->renderer;
}

gboolean
_cogl_context_update_features (CoglContext *context,
                               GError **error)
{
  return context->driver_vtable->update_features (context, error);
}

void
_cogl_context_set_current_projection_entry (CoglContext *context,
                                            CoglMatrixEntry *entry)
{
  cogl_matrix_entry_ref (entry);
  if (context->current_projection_entry)
    cogl_matrix_entry_unref (context->current_projection_entry);
  context->current_projection_entry = entry;
}

void
_cogl_context_set_current_modelview_entry (CoglContext *context,
                                           CoglMatrixEntry *entry)
{
  cogl_matrix_entry_ref (entry);
  if (context->current_modelview_entry)
    cogl_matrix_entry_unref (context->current_modelview_entry);
  context->current_modelview_entry = entry;
}

CoglGraphicsResetStatus
cogl_get_graphics_reset_status (CoglContext *context)
{
  return context->driver_vtable->get_graphics_reset_status (context);
}

gboolean
cogl_context_is_hardware_accelerated (CoglContext *context)
{
  return context->driver_vtable->is_hardware_accelerated (context);
}

gboolean
cogl_context_format_supports_upload (CoglContext *ctx,
                                     CoglPixelFormat format)
{
  return ctx->texture_driver->format_supports_upload (ctx, format);
}

void
cogl_context_set_named_pipeline (CoglContext     *context,
                                 CoglPipelineKey *key,
                                 CoglPipeline    *pipeline)
{
  if (pipeline)
    {
      g_debug ("Adding named pipeline %s", *key);
      g_hash_table_insert (context->named_pipelines, (gpointer) key, pipeline);
    }
  else
    {
      g_debug ("Removing named pipeline %s", *key);
      g_hash_table_remove (context->named_pipelines, (gpointer) key);
    }
}

CoglPipeline *
cogl_context_get_named_pipeline (CoglContext     *context,
                                 CoglPipelineKey *key)
{
  return g_hash_table_lookup (context->named_pipelines, key);
}

/**
 * cogl_context_free_timestamp_query:
 * @context: a #CoglContext object
 * @query: (transfer full): the #CoglTimestampQuery to free
 *
 * Free the #CoglTimestampQuery
 */
void
cogl_context_free_timestamp_query (CoglContext        *context,
                                   CoglTimestampQuery *query)
{
  context->driver_vtable->free_timestamp_query (context, query);
}

int64_t
cogl_context_timestamp_query_get_time_ns (CoglContext        *context,
                                          CoglTimestampQuery *query)
{
  return context->driver_vtable->timestamp_query_get_time_ns (context, query);
}

int64_t
cogl_context_get_gpu_time_ns (CoglContext *context)
{
  g_return_val_if_fail (cogl_has_feature (context,
                                          COGL_FEATURE_ID_TIMESTAMP_QUERY),
                        0);

  return context->driver_vtable->get_gpu_time_ns (context);
}
