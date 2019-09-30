/*
 * Copyright (C) 2012 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Marco Trevisan (Treviño) <marco.trevisan@canonical.com>
 *
 */

#ifndef __BAMF_LEGACY_SCREEN_PRIVATE_H__
#define __BAMF_LEGACY_SCREEN_PRIVATE_H__

#include "bamf-legacy-screen.h"
#include "bamf-legacy-window-test.h"

void _bamf_legacy_screen_open_test_window (BamfLegacyScreen *self, BamfLegacyWindowTest *window);
void _bamf_legacy_screen_close_test_window (BamfLegacyScreen *self, BamfLegacyWindowTest *window);

#endif
