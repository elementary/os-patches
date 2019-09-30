/*
 * Copyright 2013 Canonical Ltd.
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
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#ifndef INDICATOR_DATETIME_DATETIME_H
#define INDICATOR_DATETIME_DATETIME_H

#include <glib.h> // GDateTime

#include <ctime> // time_t
#include <memory> // std::shared_ptr

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A simple C++ wrapper for GDateTime to simplify ownership/refcounts
 */
class DateTime
{
public:
    static DateTime NowLocal();
    static DateTime Local(int years, int months, int days, int hours, int minutes, int seconds);

    explicit DateTime(time_t t);
    explicit DateTime(GDateTime* in=nullptr);
    DateTime& operator=(GDateTime* in);
    DateTime& operator=(const DateTime& in);
    DateTime to_timezone(const std::string& zone) const;
    DateTime add_full(int years, int months, int days, int hours, int minutes, double seconds) const;
    void reset(GDateTime* in=nullptr);

    GDateTime* get() const;
    GDateTime* operator()() const {return get();}

    std::string format(const std::string& fmt) const;
    void ymd(int& year, int& month, int& day) const;
    int day_of_month() const;
    int hour() const;
    int minute() const;
    double seconds() const;
    int64_t to_unix() const;

    bool operator<(const DateTime& that) const;
    bool operator<=(const DateTime& that) const;
    bool operator!=(const DateTime& that) const;
    bool operator==(const DateTime& that) const;

    static bool is_same_day(const DateTime& a, const DateTime& b);
    static bool is_same_minute(const DateTime& a, const DateTime& b);

private:
    std::shared_ptr<GDateTime> m_dt;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_DATETIME_H
