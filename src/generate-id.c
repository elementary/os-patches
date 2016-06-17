/*
Quick litte lack to generate the ordering ID.

Copyright 2010 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

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

#include "generate-id.h"

#define MULTIPLIER 32

guint32
generate_id (const AppIndicatorCategory catenum, const gchar * id)
{
	guint32 category = 0;
	guint32 first = 0;
	guint32 second = 0;
	guint32 third = 0;

	switch (catenum) {
	case APP_INDICATOR_CATEGORY_OTHER:
		category = MULTIPLIER * 5;
		break;
	case APP_INDICATOR_CATEGORY_APPLICATION_STATUS:
		category = MULTIPLIER * 4;
		break;
	case APP_INDICATOR_CATEGORY_COMMUNICATIONS:
		category = MULTIPLIER * 3;
		break;
	case APP_INDICATOR_CATEGORY_SYSTEM_SERVICES:
		category = MULTIPLIER * 2;
		break;
	case APP_INDICATOR_CATEGORY_HARDWARE:
		category = MULTIPLIER * 1;
		break;
	default:
		g_warning("Got an undefined category: %d", category);
		category = 0;
		break;
	}
	
	if (id != NULL) {
		if (id[0] != '\0') {
			first = id[0];
			if (id[1] != '\0') {
				second = id[1];
				if (id[2] != '\0') {
					third = id[2];
				}
			}
		}
	}

	return (((((category * 256) + first) * 256) + second) * 256) + third;
}
