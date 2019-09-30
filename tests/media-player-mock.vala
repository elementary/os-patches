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

public class MediaPlayerMock: MediaPlayer {

	/* Superclass variables */
	public override string id { get { return mock_id; } }
	public override string name { get { return mock_name; } }
	public override string state { get { return mock_state; } set { this.mock_state = value; }}
	public override Icon? icon { get { return mock_icon; } }
	public override string dbus_name { get { return mock_dbus_name; } }

	public override bool is_running { get { return mock_is_running; } }
	public override bool can_raise { get {  return mock_can_raise; } }

	public override MediaPlayer.Track? current_track { get { return mock_current_track; } set { this.mock_current_track = value; } }

	/* Mock values */
	public string mock_id { get; set; }
	public string mock_name { get; set; }
	public string mock_state { get; set; }
	public Icon? mock_icon { get; set; }
	public string mock_dbus_name { get; set; }

	public bool mock_is_running { get; set; }
	public bool mock_can_raise { get; set; }

	public MediaPlayer.Track? mock_current_track { get; set; } 

	/* Virtual functions */
	public override void activate () {
		debug("Mock activate");
	}
	public override void play_pause () {
		debug("Mock play_pause");
	}
	public override void next () {
		debug("Mock next");
	}
	public override void previous () {
		debug("Mock previous");
	}

	public override uint get_n_playlists() {
		debug("Mock get_n_playlists");
		return 0;
	}
	public override string get_playlist_id (int index) {
		debug("Mock get_playlist_id");
		return "";
	}
	public override string get_playlist_name (int index) {
		debug("Mock get_playlist_name");
		return "";
	}
	public override void activate_playlist_by_name (string playlist) {
		debug("Mock activate_playlist_by_name");
	}

}
