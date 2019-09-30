/*
 * Copyright © 2014 Canonical Ltd.
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
 *      Ted Gould <ted@canonical.com>
 */

public class MediaPlayerList {
	public class Iterator {
		public virtual MediaPlayer? next_value() {
			return null;
		}
	}
	public virtual Iterator iterator () { return new Iterator(); }

	public virtual void sync (string[] ids) { return; }

	public signal void player_added (MediaPlayer player);
	public signal void player_removed (MediaPlayer player);
}

