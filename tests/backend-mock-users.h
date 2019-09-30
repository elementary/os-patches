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

#ifndef __USERS_MOCK_H__
#define __USERS_MOCK_H__

#include <glib.h>
#include <glib-object.h>

#include "users.h" /* parent class */

G_BEGIN_DECLS

#define INDICATOR_TYPE_SESSION_USERS_MOCK          (indicator_session_users_mock_get_type())
#define INDICATOR_SESSION_USERS_MOCK(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_USERS_MOCK, IndicatorSessionUsersMock))
#define INDICATOR_SESSION_USERS_MOCK_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_USERS_MOCK, IndicatorSessionUsersMockClass))
#define INDICATOR_IS_SESSION_USERS_MOCK(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_USERS_MOCK))

typedef struct _IndicatorSessionUsersMock        IndicatorSessionUsersMock;
typedef struct _IndicatorSessionUsersMockPriv    IndicatorSessionUsersMockPriv;
typedef struct _IndicatorSessionUsersMockClass   IndicatorSessionUsersMockClass;

/**
 * An implementation of IndicatorSessionUsers that lies about everything.
 */
struct _IndicatorSessionUsersMock
{
  /*< private >*/
  IndicatorSessionUsers parent;
  IndicatorSessionUsersMockPriv * priv;
};

struct _IndicatorSessionUsersMockClass
{
  IndicatorSessionUsersClass parent_class;
};

GType indicator_session_users_mock_get_type (void);

IndicatorSessionUsers * indicator_session_users_mock_new (void);

void indicator_session_users_mock_add_user (IndicatorSessionUsersMock * self,
                                            IndicatorSessionUser      * user);

void indicator_session_users_mock_remove_user (IndicatorSessionUsersMock * self,
                                               guint                       uid);



G_END_DECLS

#endif
