/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Lars Uebernickel <lars.uebernickel@canonical.com>
 */

namespace BusWatcher {
	[CCode (cheader_filename = "bus-watch-namespace.h", cname = "bus_watch_namespace")]
	public static uint watch_namespace (GLib.BusType bus_type, string name_space,
			[CCode (delegate_target_pos = 4.9)] owned GLib.BusNameAppearedCallback? name_appeared,
			[CCode (delegate_target_pos = 4.9)] owned GLib.BusNameVanishedCallback? name_vanished);
}
