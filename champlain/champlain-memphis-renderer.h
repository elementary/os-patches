/*
 * Copyright (C) 2009 Simon Wenner <simon@wenner.ch>
 * Copyright (C) 2010-2013 Jiri Techet <techet@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined (__CHAMPLAIN_CHAMPLAIN_H_INSIDE__) && !defined (CHAMPLAIN_COMPILATION)
#error "Only <champlain/champlain.h> can be included directly."
#endif

#ifndef _CHAMPLAIN_MEMPHIS_RENDERER
#define _CHAMPLAIN_MEMPHIS_RENDERER

#include <champlain/champlain-renderer.h>
#include <champlain/champlain-bounding-box.h>

G_BEGIN_DECLS

#define CHAMPLAIN_TYPE_MEMPHIS_RENDERER champlain_memphis_renderer_get_type ()

#define CHAMPLAIN_MEMPHIS_RENDERER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CHAMPLAIN_TYPE_MEMPHIS_RENDERER, ChamplainMemphisRenderer))

#define CHAMPLAIN_MEMPHIS_RENDERER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CHAMPLAIN_TYPE_MEMPHIS_RENDERER, ChamplainMemphisRendererClass))

#define CHAMPLAIN_IS_MEMPHIS_RENDERER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CHAMPLAIN_TYPE_MEMPHIS_RENDERER))

#define CHAMPLAIN_IS_MEMPHIS_RENDERER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CHAMPLAIN_TYPE_MEMPHIS_RENDERER))

#define CHAMPLAIN_MEMPHIS_RENDERER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CHAMPLAIN_TYPE_MEMPHIS_RENDERER, ChamplainMemphisRendererClass))

typedef struct _ChamplainMemphisRendererPrivate ChamplainMemphisRendererPrivate;

typedef struct _ChamplainMemphisRenderer ChamplainMemphisRenderer;
typedef struct _ChamplainMemphisRendererClass ChamplainMemphisRendererClass;

typedef struct _ChamplainMemphisRule ChamplainMemphisRule;
typedef struct _ChamplainMemphisRuleAttr ChamplainMemphisRuleAttr;

/**
 * ChamplainMemphisRuleType:
 * @CHAMPLAIN_MEMPHIS_RULE_TYPE_UNKNOWN: MEMPHIS_RULE_TYPE_UNKNOWN
 * @CHAMPLAIN_MEMPHIS_RULE_TYPE_NODE: MEMPHIS_RULE_TYPE_NODE
 * @CHAMPLAIN_MEMPHIS_RULE_TYPE_WAY: MEMPHIS_RULE_TYPE_WAY
 * @CHAMPLAIN_MEMPHIS_RULE_TYPE_RELATION: MEMPHIS_RULE_TYPE_RELATION
 *
 * A wrapper of the MemphisRuleType union. For details see the libmemphis
 * documentation.
 *
 * Since: 0.8
 */
typedef enum
{
  CHAMPLAIN_MEMPHIS_RULE_TYPE_UNKNOWN,
  CHAMPLAIN_MEMPHIS_RULE_TYPE_NODE,
  CHAMPLAIN_MEMPHIS_RULE_TYPE_WAY,
  CHAMPLAIN_MEMPHIS_RULE_TYPE_RELATION
} ChamplainMemphisRuleType;

/**
 * ChamplainMemphisRenderer:
 *
 * The #ChamplainMemphisRenderer structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.8
 */
struct _ChamplainMemphisRenderer
{
  ChamplainRenderer parent;

  ChamplainMemphisRendererPrivate *priv;
};

struct _ChamplainMemphisRendererClass
{
  ChamplainRendererClass parent_class;
};

/**
 * ChamplainMemphisRuleAttr:
 * @z_min: z_min
 * @z_max: z_max
 * @color_red: color_red
 * @color_green: color_green
 * @color_blue: color_blue
 * @color_alpha: color_alpha
 * @style: style
 * @size: size
 *
 * A wrapper of the MemphisRuleAttr structure. For details see the libmemphis
 * documentation.
 *
 * Since: 0.8
 */
struct _ChamplainMemphisRuleAttr
{
  guint8 z_min;
  guint8 z_max;
  guint8 color_red;
  guint8 color_green;
  guint8 color_blue;
  guint8 color_alpha;
  gchar *style;
  gdouble size;
};

/**
 * ChamplainMemphisRule:
 * @keys: keys
 * @values: values
 * @type: type
 * @polygon: polygon
 * @line: line
 * @border: border
 * @text: text
 *
 * A wrapper of the MemphisRule structure. For details see the libmemphis
 * documentation.
 *
 * Since: 0.8
 */
struct _ChamplainMemphisRule
{
  gchar **keys;
  gchar **values;
  ChamplainMemphisRuleType type;
  ChamplainMemphisRuleAttr *polygon;
  ChamplainMemphisRuleAttr *line;
  ChamplainMemphisRuleAttr *border;
  ChamplainMemphisRuleAttr *text;
};

GType champlain_memphis_renderer_get_type (void);

ChamplainMemphisRenderer *champlain_memphis_renderer_new_full (guint tile_size);

void champlain_memphis_renderer_load_rules (
    ChamplainMemphisRenderer *renderer,
    const gchar *rules_path);

ClutterColor *champlain_memphis_renderer_get_background_color (
    ChamplainMemphisRenderer *renderer);

void champlain_memphis_renderer_set_background_color (
    ChamplainMemphisRenderer *renderer,
    const ClutterColor *color);

GList *champlain_memphis_renderer_get_rule_ids (
    ChamplainMemphisRenderer *renderer);

void champlain_memphis_renderer_set_rule (
    ChamplainMemphisRenderer *renderer,
    ChamplainMemphisRule *rule);

ChamplainMemphisRule *champlain_memphis_renderer_get_rule (
    ChamplainMemphisRenderer *renderer,
    const gchar *id);

void champlain_memphis_renderer_remove_rule (
    ChamplainMemphisRenderer *renderer,
    const gchar *id);

ChamplainBoundingBox *champlain_memphis_renderer_get_bounding_box (ChamplainMemphisRenderer *renderer);

void champlain_memphis_renderer_set_tile_size (ChamplainMemphisRenderer *renderer,
    guint size);

guint champlain_memphis_renderer_get_tile_size (ChamplainMemphisRenderer *renderer);

G_END_DECLS

#endif /* _CHAMPLAIN_MEMPHIS_RENDERER */
