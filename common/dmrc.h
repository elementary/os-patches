/*
 * Copyright (C) 2010-2011 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 * 
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See http://www.gnu.org/copyleft/gpl.html the full text of the
 * license.
 */

#ifndef DMRC_H_
#define DMRC_H_

#include <glib.h>
#include "user-list.h"

G_BEGIN_DECLS

GKeyFile *dmrc_load (CommonUser *user);

void dmrc_save (GKeyFile *dmrc_file, CommonUser *user);

G_END_DECLS

#endif /* DMRC_H_ */
