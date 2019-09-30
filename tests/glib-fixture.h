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

#include <map>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <gtest/gtest.h>

#include <locale.h> // setlocale()

class GlibFixture : public ::testing::Test
{
  private:

    //GLogFunc realLogHandler;

  protected:

    std::map<GLogLevelFlags,int> logCounts;

    void testLogCount(GLogLevelFlags log_level, int /*expected*/)
    {
#if 0
      EXPECT_EQ(expected, logCounts[log_level]);
#endif

      logCounts.erase(log_level);
    }

  private:

    static void default_log_handler(const gchar    * log_domain,
                                    GLogLevelFlags   log_level,
                                    const gchar    * message,
                                    gpointer         self)
    {
      g_print("%s - %d - %s\n", log_domain, (int)log_level, message);
      static_cast<GlibFixture*>(self)->logCounts[log_level]++;
    }

  protected:

    virtual void SetUp()
    {
      setlocale(LC_ALL, "C.UTF-8");

      loop = g_main_loop_new(nullptr, false);

      //g_log_set_default_handler(default_log_handler, this);

      // only use local, temporary settings
      g_assert(g_setenv("GSETTINGS_SCHEMA_DIR", SCHEMA_DIR, true));
      g_assert(g_setenv("GSETTINGS_BACKEND", "memory", true));
      g_debug("SCHEMA_DIR is %s", SCHEMA_DIR);

      g_unsetenv("DISPLAY");

    }

    virtual void TearDown()
    {
#if 0
      // confirm there aren't any unexpected log messages
      EXPECT_EQ(0, logCounts[G_LOG_LEVEL_ERROR]);
      EXPECT_EQ(0, logCounts[G_LOG_LEVEL_CRITICAL]);
      EXPECT_EQ(0, logCounts[G_LOG_LEVEL_WARNING]);
      EXPECT_EQ(0, logCounts[G_LOG_LEVEL_MESSAGE]);
      EXPECT_EQ(0, logCounts[G_LOG_LEVEL_INFO]);
#endif

      // revert to glib's log handler
      //g_log_set_default_handler(realLogHandler, this);

      g_clear_pointer(&loop, g_main_loop_unref);
    }

  private:

    static gboolean
    wait_for_signal__timeout(gpointer name)
    {
      g_error("%s: timed out waiting for signal '%s'", G_STRLOC, (char*)name);
      return G_SOURCE_REMOVE;
    }

    static gboolean
    wait_msec__timeout(gpointer loop)
    {
      g_main_loop_quit(static_cast<GMainLoop*>(loop));
      return G_SOURCE_CONTINUE;
    }

  protected:

    /* convenience func to loop while waiting for a GObject's signal */
    void wait_for_signal(gpointer o, const gchar * signal, const int timeout_seconds=5)
    {
      // wait for the signal or for timeout, whichever comes first
      const auto handler_id = g_signal_connect_swapped(o, signal,
                                                       G_CALLBACK(g_main_loop_quit),
                                                       loop);
      const auto timeout_id = g_timeout_add_seconds(timeout_seconds,
                                                    wait_for_signal__timeout,
                                                    loop);
      g_main_loop_run(loop);
      g_source_remove(timeout_id);
      g_signal_handler_disconnect(o, handler_id);
    }

    /* convenience func to loop for N msec */
    void wait_msec(int msec=50)
    {
      const auto id = g_timeout_add(msec, wait_msec__timeout, loop);
      g_main_loop_run(loop);
      g_source_remove(id);
    }

    GMainLoop * loop;
};
