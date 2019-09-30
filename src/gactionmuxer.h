/*
 * Copyright 2012 Canonical Ltd.
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
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 *     Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_ACTION_MUXER_H__
#define __G_ACTION_MUXER_H__

#include <gio/gio.h>

#define G_TYPE_ACTION_MUXER    (g_action_muxer_get_type ())
#define G_ACTION_MUXER(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_ACTION_MUXER, GActionMuxer))
#define G_IS_ACTION_MUXER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_ACTION_MUXER))

typedef struct _GActionMuxer GActionMuxer;

GType          g_action_muxer_get_type (void) G_GNUC_CONST;

GActionMuxer * g_action_muxer_new      (void);

void           g_action_muxer_insert   (GActionMuxer *muxer,
                                        const gchar  *prefix,
                                        GActionGroup *group);

void           g_action_muxer_remove   (GActionMuxer *muxer,
                                        const gchar  *prefix);

GActionGroup * g_action_muxer_get_group (GActionMuxer *muxer,
                                         const gchar  *prefix);

#endif

