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

#include <datetime/timezone-file.h>

#include <cerrno>
#include <cstdlib>

namespace unity {
namespace indicator {
namespace datetime {

FileTimezone::FileTimezone()
{
}

FileTimezone::FileTimezone(const std::string& filename)
{
    set_filename(filename);
}

FileTimezone::~FileTimezone()
{
    clear();
}

void
FileTimezone::clear()
{
    if (m_monitor_handler_id)
        g_signal_handler_disconnect(m_monitor, m_monitor_handler_id);

    g_clear_object (&m_monitor);

    m_filename.clear();
}

void
FileTimezone::set_filename(const std::string& filename)
{
    clear();

    auto tmp = realpath(filename.c_str(), nullptr);
    if(tmp != nullptr)
      {
        m_filename = tmp;
        free(tmp);
      }
    else
      {
        g_warning("Unable to resolve path '%s': %s", filename.c_str(), g_strerror(errno));
        m_filename = filename; // better than nothing?
      }

    auto file = g_file_new_for_path(m_filename.c_str());
    GError * err = nullptr;
    m_monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, nullptr, &err);
    g_object_unref(file);
    if (err)
      {
        g_warning("%s Unable to monitor timezone file '%s': %s", G_STRLOC, TIMEZONE_FILE, err->message);
        g_error_free(err);
      }
    else
      {
        m_monitor_handler_id = g_signal_connect_swapped(m_monitor, "changed", G_CALLBACK(on_file_changed), this);
        g_debug("%s Monitoring timezone file '%s'", G_STRLOC, m_filename.c_str());
      }

    reload();
}

void
FileTimezone::on_file_changed(gpointer gself)
{
    static_cast<FileTimezone*>(gself)->reload();
}

void
FileTimezone::reload()
{
    GError * err = nullptr;
    gchar * str = nullptr;

    if (!g_file_get_contents(m_filename.c_str(), &str, nullptr, &err))
    {
        g_warning("%s Unable to read timezone file '%s': %s", G_STRLOC, m_filename.c_str(), err->message);
        g_error_free(err);
    }
    else
    {
        g_strstrip(str);
        timezone.set(str);
        g_free(str);
    }
}

} // namespace datetime
} // namespace indicator
} // namespace unity
