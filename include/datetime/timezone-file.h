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

#ifndef INDICATOR_DATETIME_FILE_TIMEZONE_H
#define INDICATOR_DATETIME_FILE_TIMEZONE_H

#include <datetime/timezone.h> // base class

#include <string> // std::string

#include <glib.h>
#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A #Timezone that gets its information from monitoring a file, such as /etc/timezone
 */
class FileTimezone: public Timezone
{
public:
    FileTimezone();
    FileTimezone(const std::string& filename);
    ~FileTimezone();

private:
    void set_filename(const std::string& filename);
    static void on_file_changed(gpointer gself);
    void clear();
    void reload();

    std::string m_filename;
    GFileMonitor * m_monitor = nullptr;
    unsigned long m_monitor_handler_id = 0;

    // we have raw pointers and glib tags in here, so disable copying
    FileTimezone(const FileTimezone&) =delete;
    FileTimezone& operator=(const FileTimezone&) =delete;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_FILE_TIMEZONE_H
