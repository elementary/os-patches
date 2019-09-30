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

/**
 * MediaPlayerList is a list of media players that should appear in the sound menu.  Its main responsibility is
 * to listen for MPRIS players on the bus and attach them to the corresponding %Player objects.
 */
public class MediaPlayerListMpris : MediaPlayerList {

	public MediaPlayerListMpris () {
		this._players = new HashTable<string, MediaPlayerMpris> (str_hash, str_equal);

		BusWatcher.watch_namespace (BusType.SESSION, "org.mpris.MediaPlayer2", this.player_appeared, this.player_disappeared);
	}

	/* only valid while the list is not changed */
	public class Iterator : MediaPlayerList.Iterator {
		HashTableIter<string, MediaPlayerMpris> iter;

		public Iterator (MediaPlayerListMpris list) {
			this.iter = HashTableIter<string, MediaPlayerMpris> (list._players);
		}

		public override MediaPlayer? next_value () {
			MediaPlayerMpris? player;

			if (this.iter.next (null, out player))
				return player as MediaPlayer;
			else
				return null;
		}
	}

	public override MediaPlayerList.Iterator iterator () {
		return new Iterator (this) as MediaPlayerList.Iterator;
	}

	/**
	 * Adds the player associated with @desktop_id.  Does nothing if such a player already exists.
	 */
	MediaPlayerMpris? insert (string desktop_id) {
		debug("Inserting player: %s", desktop_id);

		var id = desktop_id.has_suffix (".desktop") ? desktop_id : desktop_id + ".desktop";
		MediaPlayerMpris? player = this._players.lookup (id);

		if (player == null) {
			var appinfo = new DesktopAppInfo (id);
			if (appinfo == null) {
				warning ("unable to find application '%s'", id);
				return null;
			}

			player = new MediaPlayerMpris (appinfo);
			this._players.insert (player.id, player);
			this.player_added (player);
		}

		return player;
	}

	/**
	 * Removes the player associated with @desktop_id, unless it is currently running.
	 */
	void remove (string desktop_id) {
		MediaPlayer? player = this._players.lookup (desktop_id);

		if (player != null && !player.is_running) {
			this._players.remove (desktop_id);
			this.player_removed (player);
		}
	}

	/**
	 * Synchronizes the player list with @desktop_ids.  After this call, this list will only contain the players
	 * in @desktop_ids.  Players that were running but are not in @desktop_ids will remain in the list.
	 */
	public override void sync (string[] desktop_ids) {

		/* hash desktop_ids for faster lookup */
		var hash = new HashTable<string, unowned string> (str_hash, str_equal);
		foreach (var id in desktop_ids)
			hash.add (id);

		/* remove players that are not desktop_ids */
		foreach (var id in this._players.get_keys ()) {
			if (!hash.contains (id))
				this.remove (id);
		}

		/* insert all players (insert() takes care of not adding a player twice */
		foreach (var id in desktop_ids)
			this.insert (id);
	}

	HashTable<string, MediaPlayerMpris> _players;

	void player_appeared (DBusConnection connection, string name, string owner) {
		try {
			MprisRoot mpris2_root = Bus.get_proxy_sync (BusType.SESSION, name, MPRIS_MEDIA_PLAYER_PATH);

			var player = this.insert (mpris2_root.DesktopEntry);
			if (player != null)
				player.attach (mpris2_root, name);
		}
		catch (Error e) {
			warning ("unable to create mpris proxy for '%s': %s", name, e.message);
		}
	}

	void player_disappeared (DBusConnection connection, string dbus_name) {
		MediaPlayerMpris? player = this._players.find ( (name, player) => {
			return player.dbus_name == dbus_name;
		});

		if (player != null)
			player.detach ();
	}
}
