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

#include "gtest-dbus-fixture.h"

#include "mock-accounts.h"
#include "mock-login1-manager.h"
#include "mock-login1-seat.h"
#include "mock-display-manager-seat.h"
#include "mock-end-session-dialog.h"
#include "mock-screen-saver.h"
#include "mock-unity-session.h"
#include "mock-session-manager.h"
#include "mock-user.h"
#include "mock-webcredentials.h"

/***
****
***/

class GTestMockDBusFixture: public GTestDBusFixture
{
  private:

    typedef GTestDBusFixture super;

  protected:

    MockScreenSaver * screen_saver;
    MockDisplayManagerSeat * dm_seat;
    MockAccounts * accounts;
    MockLogin1Manager * login1_manager;
    MockLogin1Seat * login1_seat;
    MockEndSessionDialog * end_session_dialog;
    MockWebcredentials * webcredentials;

  protected:

    virtual void SetUp ()
    {
      super :: SetUp ();

      webcredentials = new MockWebcredentials (loop, conn);
      end_session_dialog = new MockEndSessionDialog (loop, conn);
      screen_saver = new MockScreenSaver (loop, conn);
      dm_seat = new MockDisplayManagerSeat (loop, conn);
      g_setenv ("XDG_SEAT_PATH", dm_seat->path(), TRUE);
      dm_seat->set_guest_allowed (false);
      login1_manager = new MockLogin1Manager (loop, conn);
      login1_seat = new MockLogin1Seat (loop, conn, true);
      g_setenv ("XDG_SEAT", login1_seat->seat_id(), TRUE);
      login1_manager->add_seat (login1_seat);
      accounts = build_accounts_mock ();
      MockUser * user = accounts->find_by_username ("msmith");
      const int session_tag = login1_manager->add_session (login1_seat, user);
      dm_seat->set_login1_seat (login1_seat);
      dm_seat->switch_to_user (user->username());
      ASSERT_EQ (session_tag, login1_seat->active_session());
    }

  protected:

    virtual void TearDown ()
    {
      delete accounts;
      delete login1_manager;
      delete dm_seat;
      delete screen_saver;
      delete end_session_dialog;
      delete webcredentials;

      super :: TearDown ();
    }

  private:

    MockAccounts * build_accounts_mock ()
    {
      struct {
        guint64 login_frequency;
        const gchar * user_name;
        const gchar * real_name;
      } users[] = {
        { 134, "whartnell",  "First Doctor"    },
        { 119, "ptroughton", "Second Doctor"   },
        { 128, "jpertwee",   "Third Doctor"    },
        { 172, "tbaker",     "Fourth Doctor"   },
        {  69, "pdavison",   "Fifth Doctor"    },
        {  31, "cbaker",     "Sixth Doctor"    },
        {  42, "smccoy",     "Seventh Doctor"  },
        {   1, "pmcgann",    "Eigth Doctor"    },
        {  13, "ceccleston", "Ninth Doctor"    },
        {  47, "dtennant",   "Tenth Doctor"    },
        {  34, "msmith",     "Eleventh Doctor" },
        {   1, "rhurndall",  "First Doctor"    }
      };

      MockAccounts * a = new MockAccounts (loop, conn);
      for (int i=0, n=G_N_ELEMENTS(users); i<n; ++i)
        a->add_user (new MockUser (loop, conn,
                                   users[i].user_name,
                                   users[i].real_name,
                                   users[i].login_frequency));
      return a;
    }
};

