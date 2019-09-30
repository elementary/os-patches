
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

#include "glib-fixture.h"

#include <datetime/timezone-file.h>

//#include <condition_variable>
//#include <mutex>
//#include <queue>
//#include <string>
//#include <thread>
//#include <iostream>
//#include <istream>
//#include <fstream>

#include <cstdio> // fopen()
//#include <sys/stat.h> // chmod()
#include <unistd.h> // sync()

using unity::indicator::datetime::FileTimezone;


/***
****
***/

#define TIMEZONE_FILE (SANDBOX"/timezone")

class TimezoneFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;

  protected:

    virtual void SetUp()
    {
      super::SetUp();
    }

    virtual void TearDown()
    {
      super::TearDown();
    }

  public:

    /* convenience func to set the timezone file */
    void set_file(const std::string& text)
    {
      auto fp = fopen(TIMEZONE_FILE, "w+");
      fprintf(fp, "%s\n", text.c_str());
      fclose(fp);
      sync();
    }
};


/**
 * Test that timezone-file warns, but doesn't crash, if the timezone file doesn't exist
 */
TEST_F(TimezoneFixture, NoFile)
{
  remove(TIMEZONE_FILE);
  ASSERT_FALSE(g_file_test(TIMEZONE_FILE, G_FILE_TEST_EXISTS));

  FileTimezone tz(TIMEZONE_FILE);
  testLogCount(G_LOG_LEVEL_WARNING, 1);
}


/**
 * Test that timezone-file picks up the initial value
 */
TEST_F(TimezoneFixture, InitialValue)
{
  const std::string expected_timezone = "America/Chicago";
  set_file(expected_timezone);
  FileTimezone tz(TIMEZONE_FILE);
  ASSERT_EQ(expected_timezone, tz.timezone.get());
}


/**
 * Test that clearing the timezone results in an empty string
 */
TEST_F(TimezoneFixture, ChangedValue)
{
  const std::string initial_timezone = "America/Chicago";
  const std::string changed_timezone = "America/New_York";
  set_file(initial_timezone);

  FileTimezone tz(TIMEZONE_FILE);
  ASSERT_EQ(initial_timezone, tz.timezone.get());

  bool changed = false;
  auto connection = tz.timezone.changed().connect(
        [&changed, this](const std::string& s){
          g_message("timezone changed to %s", s.c_str());
          changed = true;
          g_main_loop_quit(loop);
        });

  g_idle_add([](gpointer gself){
    static_cast<TimezoneFixture*>(gself)->set_file("America/New_York");
  //  static_cast<FileTimezone*>(gtz)->timezone.set("America/New_York");
    return G_SOURCE_REMOVE;
  }, this);//&tz);

  g_main_loop_run(loop);

  ASSERT_TRUE(changed);
  ASSERT_EQ(changed_timezone, tz.timezone.get());
}
