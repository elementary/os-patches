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

[DBus (name="com.canonical.UnityGreeter.List")]
public interface UnityGreeterList : Object {
	public abstract async string get_active_entry () throws IOError;
	public signal void entry_selected (string entry_name);
}

public class MediaPlayerListGreeter : MediaPlayerList {
	string? selected_user = null;
	UnityGreeterList? proxy = null;
	HashTable<string, MediaPlayerUser> players = new HashTable<string, MediaPlayerUser>(str_hash, str_equal);

	public MediaPlayerListGreeter () {
		Bus.get_proxy.begin<UnityGreeterList> (
			BusType.SESSION,
			"com.canonical.UnityGreeter",
			"/list",
			DBusProxyFlags.NONE,
			null,
			new_proxy);
	}

	void new_proxy (GLib.Object? obj, AsyncResult res) {
		try {
			this.proxy = Bus.get_proxy.end(res);

			this.proxy.entry_selected.connect(active_user_changed);
			this.proxy.get_active_entry.begin ((obj, res) => {
				try {
					var value = (obj as UnityGreeterList).get_active_entry.end(res);
					active_user_changed(value);
				} catch (Error e) {
					warning("Unable to get active entry: %s", e.message);
				}
			});
		} catch (Error e) {
			this.proxy = null;
			warning("Unable to create proxy to the greeter: %s", e.message);
		}
	}

	void active_user_changed (string active_user) {
		/* No change, move along */
		if (selected_user == active_user) {
			return;
		}

		debug(@"Active user changed to: $active_user");

		var old_user = selected_user;

		/* Protect against a null user */
		if (active_user != "" && active_user[0] != '*') {
			selected_user = active_user;
		} else {
			debug(@"Blocking active user change for '$active_user'");
			selected_user = null;
		}

		if (selected_user != null && !players.contains(selected_user)) {
			players.insert(selected_user, new MediaPlayerUser(selected_user));
		}

		if (old_user != null) {
			var old_player = players.lookup(old_user);
			debug("Removing player for user: %s", old_user);
			player_removed(old_player);
		}

		if (selected_user != null) {
			var new_player = players.lookup(selected_user);

			if (new_player != null) {
				debug("Adding player for user: %s", selected_user);
				player_added(new_player);
			}
		}
	}

	/* We need to have an iterator for the interface, but eh, we can
	   only ever have one player for the current user */
	public class Iterator : MediaPlayerList.Iterator {
		int i = 0;
		MediaPlayerListGreeter list;

		public Iterator (MediaPlayerListGreeter in_list) {
			list = in_list;
		}

		public override MediaPlayer? next_value () {
			MediaPlayer? retval = null;

			if (i == 0 && list.selected_user != null) {
				retval = list.players.lookup(list.selected_user);
			}
			i++;

			return retval;
		}
	}

	public override MediaPlayerList.Iterator iterator() {
		return new Iterator(this) as MediaPlayerList.Iterator;
	}
}
