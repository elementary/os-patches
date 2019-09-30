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

#include "glib-fixture.h"

#include "actions-mock.h"
#include "state-mock.h"

using namespace unity::indicator::datetime;

class StateFixture: public GlibFixture
{
private:
    typedef GlibFixture super;

protected:
    std::shared_ptr<MockState> m_mock_state;
    std::shared_ptr<State> m_state;
    std::shared_ptr<MockActions> m_mock_actions;
    std::shared_ptr<Actions> m_actions;

    virtual void SetUp()
    {
        super::SetUp();

        m_mock_state.reset(new MockState);
        m_state = std::dynamic_pointer_cast<State>(m_mock_state);

        m_mock_actions.reset(new MockActions(m_state));
        m_actions = std::dynamic_pointer_cast<Actions>(m_mock_actions);
    }

    virtual void TearDown()
    {
        m_actions.reset();
        m_mock_actions.reset();

        m_state.reset();
        m_mock_state.reset();

        super::TearDown();
    }
};

