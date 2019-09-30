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

#ifndef __INDICATOR_SESSION_SERVICE_H__
#define __INDICATOR_SESSION_SERVICE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* standard GObject macros */
#define INDICATOR_TYPE_SESSION_SERVICE          (indicator_session_service_get_type())
#define INDICATOR_SESSION_SERVICE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_SERVICE, IndicatorSessionService))
#define INDICATOR_SESSION_SERVICE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_SERVICE, IndicatorSessionServiceClass))
#define INDICATOR_SESSION_SERVICE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), INDICATOR_TYPE_SESSION_SERVICE, IndicatorSessionServiceClass))
#define INDICATOR_IS_SESSION_SERVICE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_SERVICE))

typedef struct _IndicatorSessionService         IndicatorSessionService;
typedef struct _IndicatorSessionServiceClass    IndicatorSessionServiceClass;
typedef struct _IndicatorSessionServicePrivate  IndicatorSessionServicePrivate;

/* signal keys */
#define INDICATOR_SESSION_SERVICE_SIGNAL_NAME_LOST   "name-lost"

/**
 * The Indicator Session Service.
 */
struct _IndicatorSessionService
{
  /*< private >*/
  GObject parent;
  IndicatorSessionServicePrivate * priv;
};

struct _IndicatorSessionServiceClass
{
  GObjectClass parent_class;

  /* signals */

  void (* name_lost)(IndicatorSessionService * self);
};

/***
****
***/

GType indicator_session_service_get_type (void);

IndicatorSessionService * indicator_session_service_new (void);

G_END_DECLS

#endif /* __INDICATOR_SESSION_SERVICE_H__ */
