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

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <string.h> /* strstr() */

#include <gtk/gtk.h>

#include "idoactionhelper.h"
#include "idolocationmenuitem.h"

enum
{
  PROP_0,
  PROP_TIMEZONE,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _IdoLocationMenuItemPrivate
{
  char * timezone;

  guint timestamp_timer;
};

typedef IdoLocationMenuItemPrivate priv_t;

G_DEFINE_TYPE (IdoLocationMenuItem, ido_location_menu_item, IDO_TYPE_TIME_STAMP_MENU_ITEM);

/***
****  Timestamp Label
***/

static void
update_timestamp (IdoLocationMenuItem * self)
{
  GTimeZone * tz;
  GDateTime * date_time;

  tz = g_time_zone_new (self->priv->timezone);
  if (tz == NULL)
    tz = g_time_zone_new_local ();
  date_time = g_date_time_new_now (tz);

  ido_time_stamp_menu_item_set_date_time (IDO_TIME_STAMP_MENU_ITEM(self),
                                          date_time);

  g_date_time_unref (date_time);
  g_time_zone_unref (tz);
}

static void
stop_timestamp_timer (IdoLocationMenuItem * self)
{
  priv_t * p = self->priv;

  if (p->timestamp_timer != 0)
    {
      g_source_remove (p->timestamp_timer);
      p->timestamp_timer = 0;
    }
}

static void restart_timestamp_timer (IdoLocationMenuItem * self);

static gboolean
on_timestamp_timer (gpointer gself)
{
  IdoLocationMenuItem * self = IDO_LOCATION_MENU_ITEM (gself);

  update_timestamp (self);

  restart_timestamp_timer (self);
  return G_SOURCE_REMOVE;
}

static guint
calculate_seconds_until_next_minute (void)
{
  guint seconds;
  GTimeSpan diff;
  GDateTime * now;
  GDateTime * next;
  GDateTime * start_of_next;

  now = g_date_time_new_now_local ();
  next = g_date_time_add_minutes (now, 1);
  start_of_next = g_date_time_new_local (g_date_time_get_year (next),
                                         g_date_time_get_month (next),
                                         g_date_time_get_day_of_month (next),
                                         g_date_time_get_hour (next),
                                         g_date_time_get_minute (next),
                                         1);

  diff = g_date_time_difference (start_of_next, now);
  seconds = (diff + (G_TIME_SPAN_SECOND - 1)) / G_TIME_SPAN_SECOND;

  /* cleanup */
  g_date_time_unref (start_of_next);
  g_date_time_unref (next);
  g_date_time_unref (now);

  return seconds;
}

static void
restart_timestamp_timer (IdoLocationMenuItem * self)
{
  const char * fmt = ido_time_stamp_menu_item_get_format (IDO_TIME_STAMP_MENU_ITEM (self));
  gboolean timestamp_shows_seconds;
  int interval_sec;

  stop_timestamp_timer (self);

  timestamp_shows_seconds = fmt && (strstr(fmt,"%s") || strstr(fmt,"%S") ||
                                    strstr(fmt,"%T") || strstr(fmt,"%X") ||
                                    strstr(fmt,"%c"));

  if (timestamp_shows_seconds)
    interval_sec = 1;
  else
    interval_sec = calculate_seconds_until_next_minute();

  self->priv->timestamp_timer = g_timeout_add_seconds (interval_sec,
                                                       on_timestamp_timer,
                                                       self);
}

/***
****  GObject Virtual Functions
***/

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IdoLocationMenuItem * self = IDO_LOCATION_MENU_ITEM (o);
  priv_t * p = self->priv;

  switch (property_id)
    {
      case PROP_TIMEZONE:
        g_value_set_string (value, p->timezone);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
        break;
    }
}

static void
my_set_property (GObject       * o,
                 guint           property_id,
                 const GValue  * value,
                 GParamSpec    * pspec)
{
  IdoLocationMenuItem * self = IDO_LOCATION_MENU_ITEM (o);

  switch (property_id)
    {
      case PROP_TIMEZONE:
        ido_location_menu_item_set_timezone (self, g_value_get_string (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
        break;
    }
}

static void
my_dispose (GObject * object)
{
  stop_timestamp_timer (IDO_LOCATION_MENU_ITEM (object));

  G_OBJECT_CLASS (ido_location_menu_item_parent_class)->dispose (object);
}

static void
my_finalize (GObject * object)
{
  IdoLocationMenuItem * self = IDO_LOCATION_MENU_ITEM (object);

  g_free (self->priv->timezone);

  G_OBJECT_CLASS (ido_location_menu_item_parent_class)->finalize (object);
}

/***
****  Instantiation
***/

static void
ido_location_menu_item_class_init (IdoLocationMenuItemClass *klass)
{
  GObjectClass * gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (IdoLocationMenuItemPrivate));

  gobject_class->get_property = my_get_property;
  gobject_class->set_property = my_set_property;
  gobject_class->dispose = my_dispose;
  gobject_class->finalize = my_finalize;

  properties[PROP_TIMEZONE] = g_param_spec_string (
    "timezone",
    "timezone identifier",
    "string used to identify a timezone; eg, 'America/Chicago'",
    NULL,
    G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, properties);
}

static void
ido_location_menu_item_init (IdoLocationMenuItem *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            IDO_LOCATION_MENU_ITEM_TYPE,
                                            IdoLocationMenuItemPrivate);

  /* Update the timer whenever the format string changes
     because it determines whether we update once per second or per minute */
  g_signal_connect (self, "notify::format",
                    G_CALLBACK(restart_timestamp_timer), NULL);
}

/***
****  Public API
***/

/* create a new IdoLocationMenuItemType */
GtkWidget *
ido_location_menu_item_new (void)
{
  return GTK_WIDGET (g_object_new (IDO_LOCATION_MENU_ITEM_TYPE, NULL));
}

/**
 * ido_location_menu_item_set_timezone:
 * @timezone: timezone identifier (eg: "America/Chicago")
 *
 * Set this location's timezone. This will be used to show the location's
 * current time in menuitem's right-justified secondary label.
 */
void
ido_location_menu_item_set_timezone (IdoLocationMenuItem   * self,
                                     const char            * timezone)
{
  priv_t * p;

  g_return_if_fail (IDO_IS_LOCATION_MENU_ITEM (self));
  p = self->priv;

  g_free (p->timezone);
  p->timezone = g_strdup (timezone);
  update_timestamp (self);
}

/**
 * ido_location_menu_item_new_from_model:
 * @menu_item: the corresponding menuitem
 * @actions: action group to tell when this GtkMenuItem is activated
 *
 * Creates a new IdoLocationMenuItem with properties initialized from
 * the menuitem's attributes.
 *
 * If the menuitem's 'action' attribute is set, trigger that action
 * in @actions when this IdoLocationMenuItem is activated.
 */
GtkMenuItem *
ido_location_menu_item_new_from_model (GMenuItem    * menu_item,
                                       GActionGroup * actions)
{
  guint i;
  guint n;
  gchar * str;
  IdoLocationMenuItem * ido_location;
  GParameter parameters[4];

  /* create the ido_location */

  n = 0;

  if (g_menu_item_get_attribute (menu_item, "label", "s", &str))
    {
      GParameter p = { "text", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_STRING);
      g_value_take_string (&p.value, str);
      parameters[n++] = p;
    }

  if (g_menu_item_get_attribute (menu_item, "x-canonical-timezone", "s", &str))
    {
      GParameter p = { "timezone", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_STRING);
      g_value_take_string (&p.value, str);
      parameters[n++] = p;
    }

  if (g_menu_item_get_attribute (menu_item, "x-canonical-time-format", "s", &str))
    {
      GParameter p = { "format", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_STRING);
      g_value_take_string (&p.value, str);
      parameters[n++] = p;
    }

  g_assert (n <= G_N_ELEMENTS (parameters));
  ido_location = g_object_newv (IDO_LOCATION_MENU_ITEM_TYPE, n, parameters);

  for (i=0; i<n; i++)
    g_value_unset (&parameters[i].value);


  /* give it an ActionHelper */

  if (g_menu_item_get_attribute (menu_item, "action", "s", &str))
    {
      GVariant * target;
      IdoActionHelper * helper;

      target = g_menu_item_get_attribute_value (menu_item, "target",
                                                G_VARIANT_TYPE_ANY);
      helper = ido_action_helper_new (GTK_WIDGET(ido_location), actions,
                                      str, target);
      g_signal_connect_swapped (ido_location, "activate",
                                G_CALLBACK (ido_action_helper_activate), helper);
      g_signal_connect_swapped (ido_location, "destroy",
                                G_CALLBACK (g_object_unref), helper);

      if (target)
        g_variant_unref (target);
      g_free (str);
    }

  return GTK_MENU_ITEM (ido_location);
}
