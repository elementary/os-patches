/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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
 */

#ifndef __INDICATOR_SESSION_ACTIONS_MOCK_H__
#define __INDICATOR_SESSION_ACTIONS_MOCK_H__

#include <glib.h>
#include <glib-object.h>

#include "actions.h" /* parent class */

G_BEGIN_DECLS

#define INDICATOR_TYPE_SESSION_ACTIONS_MOCK          (indicator_session_actions_mock_get_type())
#define INDICATOR_SESSION_ACTIONS_MOCK(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_ACTIONS_MOCK, IndicatorSessionActionsMock))
#define INDICATOR_SESSION_ACTIONS_MOCK_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_ACTIONS_MOCK, IndicatorSessionActionsMockClass))
#define INDICATOR_IS_SESSION_ACTIONS_MOCK(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_ACTIONS_MOCK))

typedef struct _IndicatorSessionActionsMock        IndicatorSessionActionsMock;
typedef struct _IndicatorSessionActionsMockPriv    IndicatorSessionActionsMockPriv;
typedef struct _IndicatorSessionActionsMockClass   IndicatorSessionActionsMockClass;

/**
 * An implementation of IndicatorSessionActions that lies about everything.
 */
struct _IndicatorSessionActionsMock
{
  /*< private >*/
  IndicatorSessionActions parent;
  IndicatorSessionActionsMockPriv * priv;
};

struct _IndicatorSessionActionsMockClass
{
  IndicatorSessionActionsClass parent_class;
};

GType indicator_session_actions_mock_get_type (void);

IndicatorSessionActions * indicator_session_actions_mock_new (void);

G_END_DECLS

#endif /* __INDICATOR_SESSION_ACTIONS_MOCK_H__ */
