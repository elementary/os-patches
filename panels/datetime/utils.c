/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

A dialog for setting time and date preferences.

Copyright 2010 Canonical Ltd.

Authors:
    Michael Terry <michael.terry@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <locale.h>
#include <langinfo.h>
#include <string.h>
#include "utils.h"
#include "settings-shared.h"

/* Check the system locale setting to see if the format is 24-hour
   time or 12-hour time */
gboolean
is_locale_12h (void)
{
	static const char *formats_24h[] = {"%H", "%R", "%T", "%OH", "%k", NULL};
	const char *t_fmt = nl_langinfo (T_FMT);
	int i;

	for (i = 0; formats_24h[i]; ++i) {
		if (strstr (t_fmt, formats_24h[i])) {
			return FALSE;
		}
	}

	return TRUE;
}

void
split_settings_location (const gchar * location, gchar ** zone, gchar ** name)
{
  gchar * location_dup;
  gchar * first;

  location_dup = g_strdup (location);
  g_strstrip (location_dup);

  if ((first = strchr (location_dup, ' ')))
    *first = '\0';

  if (zone != NULL)
    {
      *zone = location_dup;
    }

  if (name != NULL)
    {
      gchar * after = first ? g_strstrip (first + 1) : NULL;

      if (after && *after)
        {
          *name = g_strdup (after);
        }
      else /* make the name from zone */
        {
          gchar * chr = strrchr (location_dup, '/');
          after = g_strdup (chr ? chr + 1 : location_dup);

          /* replace underscores with spaces */
          for (chr=after; chr && *chr; chr++)
            if (*chr == '_')
              *chr = ' ';

          *name = after;
        }
    }
}

gchar *
get_current_zone_name (const gchar * location, GSettings * settings)
{
  gchar * new_zone, * new_name;
  gchar * tz_name;
  gchar * old_zone, * old_name;
  gchar * rv;

  split_settings_location (location, &new_zone, &new_name);

  tz_name = g_settings_get_string (settings, SETTINGS_TIMEZONE_NAME_S);
  split_settings_location (tz_name, &old_zone, &old_name);
  g_free (tz_name);

  /* new_name is always just a sanitized version of a timezone.
     old_name is potentially a saved "pretty" version of a timezone name from
     geonames.  So we prefer to use it if available and the zones match. */

  if (g_strcmp0 (old_zone, new_zone) == 0) {
    rv = old_name;
    old_name = NULL;
  }
  else {
    rv = new_name;
    new_name = NULL;
  }

  g_free (new_zone);
  g_free (old_zone);
  g_free (new_name);
  g_free (old_name);

  return rv;
}

/* Translate msg according to the locale specified by LC_TIME */
static const char *
T_(const char *msg)
{
	/* General strategy here is to make sure LANGUAGE is empty (since that
	   trumps all LC_* vars) and then to temporarily swap LC_TIME and
	   LC_MESSAGES.  Then have gettext translate msg.

	   We strdup the strings because the setlocale & *env functions do not
	   guarantee anything about the storage used for the string, and thus
	   the string may not be portably safe after multiple calls.

	   Note that while you might think g_dcgettext would do the trick here,
	   that actually looks in /usr/share/locale/XX/LC_TIME, not the
	   LC_MESSAGES directory, so we won't find any translation there.
	*/
	char *message_locale = g_strdup(setlocale(LC_MESSAGES, NULL));
	const char *time_locale = setlocale (LC_TIME, NULL);
	char *language = g_strdup(g_getenv("LANGUAGE"));
	const char *rv;
	if (language)
		g_unsetenv("LANGUAGE");
	setlocale(LC_MESSAGES, time_locale);

	/* Get the LC_TIME version */
	rv = _(msg);

	/* Put everything back the way it was */
	setlocale(LC_MESSAGES, message_locale);
	if (language)
		g_setenv("LANGUAGE", language, TRUE);
	g_free(message_locale);
	g_free(language);
	return rv;
}

gchar *
join_date_and_time_format_strings (const char * date_string,
                                   const char * time_string)
{
  gchar * str;

  if (date_string && time_string)
    {
      /* TRANSLATORS: This is a format string passed to strftime to combine the
       * date and the time.  The value of "%s\xE2\x80\x82%s" will result in a
       * string like this in US English 12-hour time: 'Fri Jul 16 11:50 AM'.
       * The space in between date and time is a Unicode en space
       * (E28082 in UTF-8 hex). */
      str =  g_strdup_printf (T_("%s\xE2\x80\x82%s"), date_string, time_string);
    }
  else if (date_string)
    {
      str = g_strdup_printf (T_("%s"), date_string);
    }
  else /* time_string */
    {
      str = g_strdup_printf (T_("%s"), time_string);
    }

  return str;
}

/***
****
***/

static const gchar *
get_default_header_time_format (gboolean twelvehour, gboolean show_seconds)
{
  const gchar * fmt;

  if (twelvehour && show_seconds)
    /* TRANSLATORS: a strftime(3) format for 12hr time w/seconds */
    fmt = T_("%l:%M:%S %p");
  else if (twelvehour)
    /* TRANSLATORS: a strftime(3) format for 12hr time */
    fmt = T_("%l:%M %p");
  else if (show_seconds)
    /* TRANSLATORS: a strftime(3) format for 24hr time w/seconds */
    fmt = T_("%H:%M:%S");
  else
    /* TRANSLATORS: a strftime(3) format for 24hr time */
    fmt = T_("%H:%M");

  return fmt;
}

/***
****
***/

typedef enum
{
  DATE_PROXIMITY_TODAY,
  DATE_PROXIMITY_TOMORROW,
  DATE_PROXIMITY_WEEK,
  DATE_PROXIMITY_FAR
}
date_proximity_t;

static date_proximity_t
get_date_proximity (GDateTime * now, GDateTime * time)
{
  date_proximity_t prox = DATE_PROXIMITY_FAR;
  gint now_year, now_month, now_day;
  gint time_year, time_month, time_day;

  /* does it happen today? */
  g_date_time_get_ymd (now, &now_year, &now_month, &now_day);
  g_date_time_get_ymd (time, &time_year, &time_month, &time_day);
  if ((now_year == time_year) && (now_month == time_month) && (now_day == time_day))
    prox = DATE_PROXIMITY_TODAY;

  /* does it happen tomorrow? */
  if (prox == DATE_PROXIMITY_FAR)
    {
      GDateTime * tomorrow;
      gint tom_year, tom_month, tom_day;

      tomorrow = g_date_time_add_days (now, 1);
      g_date_time_get_ymd (tomorrow, &tom_year, &tom_month, &tom_day);
      if ((tom_year == time_year) && (tom_month == time_month) && (tom_day == time_day))
        prox = DATE_PROXIMITY_TOMORROW;

      g_date_time_unref (tomorrow);
    }

  /* does it happen this week? */
  if (prox == DATE_PROXIMITY_FAR)
    {
      GDateTime * week;
      GDateTime * week_bound;

      week = g_date_time_add_days (now, 6);
      week_bound = g_date_time_new_local (g_date_time_get_year(week),
                                          g_date_time_get_month (week),
                                          g_date_time_get_day_of_month(week),
                                          23, 59, 59.9);

      if (g_date_time_compare (time, week_bound) <= 0)
        prox = DATE_PROXIMITY_WEEK;

      g_date_time_unref (week_bound);
      g_date_time_unref (week);
    }

  return prox;
}


/*
 * "Terse" time & date format strings
 * 
 * Used on the phone menu where space is at a premium, these strings
 * express the time and date in as brief a form as possible.
 *
 * Examples from spec:
 *  1. "Daily 6:30 AM"
 *  2. "5:07 PM" (note date is omitted; today's date is implicit)
 *  3. "Daily 12 PM" (note minutes are omitted for on-the-hour times)
 *  4. "Tomorrow 7 AM" (note "Tomorrow" is used instead of a day of week)
 */

static const gchar *
get_terse_date_format_string (date_proximity_t proximity)
{
  const gchar * fmt;

  switch (proximity)
    {
      case DATE_PROXIMITY_TODAY:
        /* 'Today' is implicit in the terse case, so no string needed */
        fmt = NULL;
        break;

      case DATE_PROXIMITY_TOMORROW:
        fmt = T_("Tomorrow");
        break;

      case DATE_PROXIMITY_WEEK:
        /* a strftime(3) fmt string for abbreviated day of week */
        fmt = T_("%a");
        break;

      default:
        /* a strftime(3) fmt string for day-of-month and abbreviated month */
        fmt = T_("%d %b");
        break;
    }

  return fmt;
}

const gchar*
get_terse_header_time_format_string (void)
{
  const gboolean twelvehour = is_locale_12h ();
  const gboolean show_seconds = FALSE;

  return get_default_header_time_format (twelvehour, show_seconds);
}

const gchar *
get_terse_time_format_string (GDateTime * time)
{
  const gchar * fmt;

  if (g_date_time_get_minute (time) != 0)
    {
      fmt = get_terse_header_time_format_string ();
    }
  else
    {
      /* a strftime(3) fmt string for a 12 hour on-the-hour time, eg "7 PM" */
      fmt = T_("%l %p");
    }

  return fmt;
}

gchar *
generate_terse_format_string_at_time (GDateTime * now, GDateTime * time)
{
  const date_proximity_t prox = get_date_proximity (now, time);
  const gchar * date_fmt = get_terse_date_format_string (prox);
  const gchar * time_fmt = get_terse_time_format_string (time);
  return join_date_and_time_format_strings (date_fmt, time_fmt);
}

/***
****  FULL
***/

static const gchar *
get_full_date_format_string (gboolean show_day, gboolean show_date, gboolean show_year)
{
  const char * fmt;

  if (show_day && show_date && show_year)
    /* TRANSLATORS: a strftime(3) format showing the weekday, date, and year */
    fmt = T_("%a %b %e %Y");
  else if (show_day && show_date)
    /* TRANSLATORS: a strftime(3) format showing the weekday and date */
    fmt = T_("%a %b %e");
  else if (show_day && show_year)
    /* TRANSLATORS: a strftime(3) format showing the weekday and year. */
    fmt = T_("%a %Y");
  else if (show_day)
    /* TRANSLATORS: a strftime(3) format showing the weekday. */
    fmt = T_("%a");
  else if (show_date && show_year)
    /* TRANSLATORS: a strftime(3) format showing the date and year */
    fmt = T_("%b %e %Y");
  else if (show_date)
    /* TRANSLATORS: a strftime(3) format showing the date */
    fmt = T_("%b %e");
  else if (show_year)
    /* TRANSLATORS: a strftime(3) format showing the year */
    fmt = T_("%Y");
  else
    fmt = NULL;

  return fmt;
}


/*
 * "Full" time & date format strings
 * 
 * These are used on the desktop menu & header and honors the
 * GSettings entries for 12/24hr mode and whether or not to show seconds.
 *
 */

const gchar *
get_full_time_format_string (GSettings * settings)
{
  gboolean twelvehour;
  gboolean show_seconds;

  g_return_val_if_fail (settings != NULL, NULL);

  show_seconds = g_settings_get_boolean (settings, SETTINGS_SHOW_SECONDS_S);

  switch (g_settings_get_enum (settings, SETTINGS_TIME_FORMAT_S))
    {
      case TIME_FORMAT_MODE_LOCALE_DEFAULT:
        twelvehour = is_locale_12h();
        break;

      case TIME_FORMAT_MODE_24_HOUR:
        twelvehour = FALSE;
        break;

      default:
        twelvehour = TRUE;
        break;
    }

  return get_default_header_time_format (twelvehour, show_seconds);
}

gchar *
generate_full_format_string (gboolean show_day, gboolean show_date, gboolean show_year, GSettings * settings)
{
  const gchar * date_fmt = get_full_date_format_string (show_day, show_date, show_year);
  const gchar * time_fmt = get_full_time_format_string (settings);
  return join_date_and_time_format_strings (date_fmt, time_fmt);
}
  
gchar *
generate_full_format_string_at_time (GDateTime * now, GDateTime * time, GSettings * settings)
{
  gboolean show_day;
  gboolean show_date;

  g_return_val_if_fail (now != NULL, NULL);
  g_return_val_if_fail (time != NULL, NULL);
  g_return_val_if_fail (settings != NULL, NULL);

  switch (get_date_proximity (now, time))
    {
      case DATE_PROXIMITY_TODAY:
        show_day = FALSE;
        show_date = FALSE;
        break;

      case DATE_PROXIMITY_TOMORROW:  
      case DATE_PROXIMITY_WEEK:
        show_day = FALSE;
        show_date = TRUE;
        break;

      default:
        show_day = TRUE;
        show_date = TRUE;
        break;
    }

  return generate_full_format_string (show_day, show_date, FALSE, settings);
}

