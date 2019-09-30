/*
 * Copyright 2010, 2014 Canonical Ltd.
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
 *   Michael Terry <michael.terry@canonical.com>
 *   Charles Kerr <charles.kerr@canonical.com>
 */


#include <datetime/utils.h>
#include <datetime/settings-shared.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <locale.h>
#include <langinfo.h>
#include <string.h>

/* Check the system locale setting to see if the format is 24-hour
   time or 12-hour time */
gboolean
is_locale_12h(void)
{
    int i;
    static const char *formats_24h[] = {"%H", "%R", "%T", "%OH", "%k", NULL};
    const char* t_fmt = nl_langinfo(T_FMT);

    for (i=0; formats_24h[i]!=NULL; i++)
        if (strstr(t_fmt, formats_24h[i]) != NULL)
            return FALSE;

    return TRUE;
}

void
split_settings_location(const gchar* location, gchar** zone, gchar** name)
{
    gchar* location_dup = g_strdup(location);
    if(location_dup != NULL)
        g_strstrip(location_dup);

    gchar* first = NULL;
    if(location_dup && (first = strchr(location_dup, ' ')))
        *first = '\0';

    if(zone)
        *zone = location_dup;

    if(name != NULL)
    {
        gchar* after = first ? g_strstrip(first + 1) : NULL;

        if(after && *after)
        {
            *name = g_strdup(after);
        }
        else if (location_dup) // make the name from zone
        {
            gchar * chr = strrchr(location_dup, '/');
            after = g_strdup(chr ? chr + 1 : location_dup);

            // replace underscores with spaces
            for(chr=after; chr && *chr; chr++)
                if(*chr == '_')
                    *chr = ' ';

            *name = after;
        }
        else
        {
            *name = NULL;
        }
    }
}

/**
 * Our Locations come from two places: (1) direct user input and (2) ones
 * guessed by the system, such as from geoclue or timedate1.
 *
 * Since the latter only have a timezone (eg, "America/Chicago") and the
 * former have a descriptive name provided by the end user (eg,
 * "America/Chicago Oklahoma City"), this function tries to make a
 * more human-readable name by using the user-provided name if the guessed
 * timezone matches the last one the user manually clicked on.
 * 
 * In the example above, this allows the menuitem for the system-guessed
 * timezone ("America/Chicago") to read "Oklahoma City" after the user clicks
 * on the "Oklahoma City" menuitem.
 */
gchar*
get_beautified_timezone_name(const char* timezone_, const char* saved_location)
{
    gchar* zone;
    gchar* name;
    split_settings_location(timezone_, &zone, &name);

    gchar* saved_zone;
    gchar* saved_name;
    split_settings_location(saved_location, &saved_zone, &saved_name);

    gchar* rv;
    if (g_strcmp0(zone, saved_zone) == 0)
    {
        rv = saved_name;
        saved_name = NULL;
    }
    else
    {
        rv = name;
        name = NULL;
    }

    g_free(zone);
    g_free(name);
    g_free(saved_zone);
    g_free(saved_name);
    return rv;
}

gchar*
get_timezone_name(const gchar* timezone_, GSettings* settings)
{
    gchar* saved_location = g_settings_get_string(settings, SETTINGS_TIMEZONE_NAME_S);
    gchar* rv = get_beautified_timezone_name(timezone_, saved_location);
    g_free(saved_location);
    return rv;
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
getDateProximity(GDateTime* now, GDateTime* time)
{
    date_proximity_t prox = DATE_PROXIMITY_FAR;
    gint now_year, now_month, now_day;
    gint time_year, time_month, time_day;

    // did it already happen?
    if (g_date_time_difference(time, now) < -G_USEC_PER_SEC)
        return DATE_PROXIMITY_FAR;

    // does it happen today?
    g_date_time_get_ymd(now, &now_year, &now_month, &now_day);
    g_date_time_get_ymd(time, &time_year, &time_month, &time_day);
    if ((now_year == time_year) && (now_month == time_month) && (now_day == time_day))
        prox = DATE_PROXIMITY_TODAY;

    // does it happen tomorrow?
    if (prox == DATE_PROXIMITY_FAR)
    {
        GDateTime* tomorrow = g_date_time_add_days(now, 1);

        gint tom_year, tom_month, tom_day;
        g_date_time_get_ymd(tomorrow, &tom_year, &tom_month, &tom_day);
        if ((tom_year == time_year) && (tom_month == time_month) && (tom_day == time_day))
            prox = DATE_PROXIMITY_TOMORROW;

        g_date_time_unref(tomorrow);
    }

    // does it happen this week?
    if (prox == DATE_PROXIMITY_FAR)
    {
        GDateTime* week = g_date_time_add_days(now, 6);
        GDateTime* week_bound = g_date_time_new_local(g_date_time_get_year(week),
                                                g_date_time_get_month(week),
                                                g_date_time_get_day_of_month(week),
                                                23, 59, 59.9);

        if (g_date_time_compare(time, week_bound) <= 0)
            prox = DATE_PROXIMITY_WEEK;

        g_date_time_unref(week_bound);
        g_date_time_unref(week);
    }

    return prox;
}

const char*
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

    gchar* message_locale = g_strdup(setlocale(LC_MESSAGES, NULL));
    const char* time_locale = setlocale(LC_TIME, NULL);
    gchar* language = g_strdup(g_getenv("LANGUAGE"));

    if (language)
        g_unsetenv("LANGUAGE");
    setlocale(LC_MESSAGES, time_locale);

    /* Get the LC_TIME version */
    const char* rv = _(msg);

    /* Put everything back the way it was */
    setlocale(LC_MESSAGES, message_locale);
    if (language)
        g_setenv("LANGUAGE", language, TRUE);

    g_free(message_locale);
    g_free(language);
    return rv;
}


/**
 * _ a time today should be shown as just the time (e.g. “3:55 PM”)
 * _ a full-day event today should be shown as “Today”
 * _ a time any other day this week should be shown as the short version of the
 *   day and time (e.g. “Wed 3:55 PM”)
 * _ a full-day event tomorrow should be shown as “Tomorrow”
 * _ a full-day event another day this week should be shown as the
 *   weekday (e.g. “Friday”)
 * _ a time after this week should be shown as the short version of the day,
 *   date, and time (e.g. “Wed 21 Apr 3:55 PM”)
 * _ a full-day event after this week should be shown as the short version of
 *   the day and date (e.g. “Wed 21 Apr”). 
 * _ in addition, when presenting the times of upcoming events, the time should
 *   be followed by the timezone if it is different from the one the computer
 *   is currently set to. For example, “Wed 3:55 PM UTC−5”. 
 */
char* generate_full_format_string_at_time (GDateTime* now,
                                           GDateTime* then,
                                           GDateTime* then_end)
{
    GString* ret = g_string_new (NULL);

    if (then != NULL)
    {
        const gboolean full_day = then_end && (g_date_time_difference(then_end, then) >= G_TIME_SPAN_DAY);
        const date_proximity_t prox = getDateProximity(now, then);

        if (full_day)
        {
            switch (prox)
            {
                case DATE_PROXIMITY_TODAY:
                    g_string_assign (ret, T_("Today"));
                    break;

                case DATE_PROXIMITY_TOMORROW:
                    g_string_assign (ret, T_("Tomorrow"));
                    break;

                case DATE_PROXIMITY_WEEK:
                    /* This is a strftime(3) format string indicating the unabbreviated weekday. */
                    g_string_assign (ret, T_("%A"));
                    break;

                case DATE_PROXIMITY_FAR:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing full-day events that are over a week away.
                       en_US example: "%a %b %d" --> "Sat Oct 31"
                       en_GB example: "%a %d %b" --> "Sat 31 Oct"
                       zh_CN example(?): "%m月%d日 周%a" --> "10月31日 周六" */
                    g_string_assign (ret, T_("%a %d %b"));
                    break;
            }
        }
        else if (is_locale_12h())
        {
            switch (prox)
            {
                case DATE_PROXIMITY_TODAY:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing, on a 12-hour clock, events/appointments that happen today.
                       en_US example: "%l:%M %p" --> "1:00 PM" */
                    g_string_assign (ret, T_("%l:%M %p"));
                    break;

                case DATE_PROXIMITY_TOMORROW:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing, on a 12-hour clock, events/appointments that happen tomorrow.
                       (\u2003 is a unicode em space which is slightly wider than a normal space.)
                       en_US example: "Tomorrow\u2003%l:%M %p" --> "Tomorrow  1:00 PM" */
                    g_string_assign (ret, T_("Tomorrow\u2003%l:%M %p"));
                    break;

                case DATE_PROXIMITY_WEEK:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing, on a 12-hour clock, events/appointments that happen this week.
                       (\u2003 is a unicode em space which is slightly wider than a normal space.)
                       en_US example: "Tomorrow\u2003%l:%M %p" --> "Fri  1:00 PM" */
                    g_string_assign (ret, T_("%a\u2003%l:%M %p"));
                    break;

                case DATE_PROXIMITY_FAR:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing, on a 12-hour clock, events/appointments that happen over a week from now.
                       (\u2003 is a unicode em space which is slightly wider than a normal space.)
                       en_US example: "%a %d %b\u2003%l:%M %p" --> "Fri Oct 31  1:00 PM"
                       en_GB example: "%a %b %d\u2003%l:%M %p" --> "Fri 31 Oct  1:00 PM" */
                    g_string_assign (ret, T_("%a %d %b\u2003%l:%M %p"));
                    break;
            }
        }
        else
        {
            switch (prox)
            {
                case DATE_PROXIMITY_TODAY:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing, on a 24-hour clock, events/appointments that happen today.
                       en_US example: "%H:%M" --> "13:00" */
                    g_string_assign (ret, T_("%H:%M"));
                    break;

                case DATE_PROXIMITY_TOMORROW:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing, on a 24-hour clock, events/appointments that happen tomorrow.
                       (\u2003 is a unicode em space which is slightly wider than a normal space.)
                       en_US example: "Tomorrow\u2003%l:%M %p" --> "Tomorrow  13:00" */
                    g_string_assign (ret, T_("Tomorrow\u2003%H:%M"));
                    break;

                case DATE_PROXIMITY_WEEK:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing, on a 24-hour clock, events/appointments that happen this week.
                       (\u2003 is a unicode em space which is slightly wider than a normal space.)
                       en_US example: "%a\u2003%H:%M" --> "Fri  13:00" */
                    g_string_assign (ret, T_("%a\u2003%H:%M"));
                    break;

                case DATE_PROXIMITY_FAR:
                    /* Translators, please edit/rearrange these strftime(3) tokens to suit your locale!
                       This format string is used for showing, on a 24-hour clock, events/appointments that happen over a week from now.
                       (\u2003 is a unicode em space which is slightly wider than a normal space.)
                       en_US example: "%a %d %b\u2003%H:%M" --> "Fri Oct 31  13:00"
                       en_GB example: "%a %b %d\u2003%H:%M" --> "Fri 31 Oct  13:00" */
                    g_string_assign (ret, T_("%a %d %b\u2003%H:%M"));
                    break;
            }
        }

        /* if it's an appointment in a different timezone (and doesn't run for a full day)
           then the time should be followed by its timezone. */
        if ((then_end != NULL) &&
            (!full_day) &&
            ((g_date_time_get_utc_offset(now) != g_date_time_get_utc_offset(then))))
        {
            g_string_append_printf (ret, " %s", g_date_time_get_timezone_abbreviation(then));
        }
    }

    return g_string_free (ret, FALSE);
}
