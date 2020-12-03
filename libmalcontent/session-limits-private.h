/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2019 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <libmalcontent/session-limits.h>

G_BEGIN_DECLS

/**
 * MctSessionLimitsType:
 * @MCT_SESSION_LIMITS_TYPE_NONE: No session limits are imposed.
 * @MCT_SESSION_LIMITS_TYPE_DAILY_SCHEDULE: Sessions are limited to between a
 *     pair of given times each day.
 *
 * Types of session limit which can be imposed on an account. Additional types
 * may be added in future.
 *
 * Since: 0.5.0
 */
typedef enum
{
  /* these values are used in the com.endlessm.ParentalControls.SessionLimits
   * D-Bus interface, so must not be changed */
  MCT_SESSION_LIMITS_TYPE_NONE = 0,
  MCT_SESSION_LIMITS_TYPE_DAILY_SCHEDULE = 1,
} MctSessionLimitsType;

struct _MctSessionLimits
{
  gint ref_count;

  uid_t user_id;

  MctSessionLimitsType limit_type;
  guint daily_start_time;  /* seconds since midnight */
  guint daily_end_time;  /* seconds since midnight */
};

G_END_DECLS
