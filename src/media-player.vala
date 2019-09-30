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

public abstract class MediaPlayer : Object {
	public virtual string id { get { not_implemented(); return ""; } }
	public virtual string name { get { not_implemented(); return ""; } }
	public virtual string state { get { not_implemented(); return ""; } set { }}
	public virtual Icon? icon { get { not_implemented(); return null; } }
	public virtual string dbus_name { get { not_implemented(); return ""; } }

	public virtual bool is_running { get { not_implemented(); return false; } }
	public virtual bool can_raise { get { not_implemented(); return false; } }

	public class Track : Object {
		public string artist { get; construct; }
		public string title { get; construct; }
		public string album { get; construct; }
		public string art_url { get; construct; }

		public Track (string artist, string title, string album, string art_url) {
			Object (artist: artist, title: title, album: album, art_url: art_url);
		}
	}

	public virtual Track? current_track {
		get { not_implemented(); return null; }
		set { not_implemented(); }
	}

	public signal void playlists_changed ();

	public abstract void activate ();
	public abstract void play_pause ();
	public abstract void next ();
	public abstract void previous ();

	public abstract uint get_n_playlists();
	public abstract string get_playlist_id (int index);
	public abstract string get_playlist_name (int index);
	public abstract void activate_playlist_by_name (string playlist);

	private void not_implemented () {
		warning("Property not implemented");
	}
}
