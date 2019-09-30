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

public class MediaPlayerUser : MediaPlayer {
	Act.UserManager accounts_manager = Act.UserManager.get_default();
	string username;
	Act.User? actuser = null;
	AccountsServiceSoundSettings? proxy = null;
	GreeterBroadcast? greeter = null;

	HashTable<string, bool> properties_queued = new HashTable<string, bool>(str_hash, str_equal);
	uint properties_timeout = 0;

	/* Grab the user from the Accounts service and, when it is loaded then
	   set up a proxy to its sound settings */
	public MediaPlayerUser(string user) {
		username = user;

		actuser = accounts_manager.get_user(user);
		actuser.notify["is-loaded"].connect(() => {
			debug("User loaded");

			this.proxy = null;

			Bus.get_proxy.begin<AccountsServiceSoundSettings> (
				BusType.SYSTEM,
				"org.freedesktop.Accounts",
				actuser.get_object_path(),
				DBusProxyFlags.GET_INVALIDATED_PROPERTIES,
				null,
				new_proxy);
		});

		Bus.get_proxy.begin<GreeterBroadcast> (
			BusType.SYSTEM,
			"com.canonical.Unity.Greeter.Broadcast",
			"/com/canonical/Unity/Greeter/Broadcast",
			DBusProxyFlags.NONE,
			null,
			greeter_proxy_new);
	}

	~MediaPlayerUser () {
		if (properties_timeout != 0) {
			Source.remove(properties_timeout);
			properties_timeout = 0;
		}
	}

	/* Ensure that we've collected all the changes so that we only signal
	   once for variables like 'track' */
	bool properties_idle () {
		properties_timeout = 0;

		properties_queued.@foreach((key, value) => {
			debug("Notifying '%s' changed", key);
			this.notify_property(key);
		});

		properties_queued.remove_all();

		/* Remove source */
		return false;
	}

	/* Turns the DBus names into the object properties */
	void queue_property_notification (string dbus_property_name) {
		if (properties_timeout == 0) {
			properties_timeout = Idle.add(properties_idle);
		}

		switch (dbus_property_name) {
		case "Timestamp":
			properties_queued.insert("name", true);
			properties_queued.insert("icon", true);
			properties_queued.insert("state", true);
			properties_queued.insert("current-track", true);
			properties_queued.insert("is-running", true);
			break;
		case "PlayerName":
			properties_queued.insert("name", true);
			break;
		case "PlayerIcon":
			properties_queued.insert("icon", true);
			break;
		case "State":
			properties_queued.insert("state", true);
			break;
		case "Title":
		case "Artist":
		case "Album":
		case "ArtUrl":
			properties_queued.insert("current-track", true);
			break;
		}
	}

	void new_proxy (GLib.Object? obj, AsyncResult res) {
		try {
			this.proxy = Bus.get_proxy.end (res);

			var gproxy = this.proxy as DBusProxy;
			gproxy.g_properties_changed.connect ((proxy, changed, invalidated) => {
				string key = "";
				Variant value;
				VariantIter iter = new VariantIter(changed);

				while (iter.next("{sv}", &key, &value)) {
					queue_property_notification(key);
				}

				foreach (var invalid in invalidated) {
					queue_property_notification(invalid);
				}
			});

			debug("Notifying player is ready for user: %s", this.username);
			this.notify_property("is-running");
		} catch (Error e) {
			this.proxy = null;
			warning("Unable to get proxy to user '%s' sound settings: %s", username, e.message);
		}
	}

	bool proxy_is_valid () {
		if (this.proxy == null) {
			return false;
		}

		/* More than 10 minutes old */
		if (this.proxy.timestamp < GLib.get_monotonic_time() - 10 * 60 * 1000 * 1000) {
			return false;
		}

		return true;
	}

	public override string id {
		get { return username; }
	}

	/* These values come from the proxy */
	string name_cache;
	public override string name { 
		get {
			if (proxy_is_valid()) {
				name_cache = this.proxy.player_name;
				debug("Player Name: %s", name_cache);
				return name_cache;
			} else {
				return "";
			}
		}
	}
	string state_cache;
	public override string state {
		get {
			if (proxy_is_valid()) {
				state_cache = this.proxy.state;
				debug("State: %s", state_cache);
				return state_cache;
			} else {
				return "";
			}
		}
		set { }
	}
	Icon icon_cache;
	public override Icon? icon { 
		get { 
			if (proxy_is_valid()) {
				icon_cache = Icon.deserialize(this.proxy.player_icon);
				return icon_cache;
			} else {
				return null;
			}
		}
	}

	/* Placeholder */
	public override string dbus_name { get { return ""; } }

	/* If it's shown externally it's running */
	public override bool is_running { get { return proxy_is_valid(); } }
	/* A bit weird.  Not sure how we should handle this. */
	public override bool can_raise { get { return true; } }

	/* Fill out the track based on the values in the proxy */
	MediaPlayer.Track track_cache;
	public override MediaPlayer.Track? current_track {
		get { 
			if (proxy_is_valid()) {
				track_cache = new MediaPlayer.Track(
					this.proxy.artist,
					this.proxy.title,
					this.proxy.album,
					this.proxy.art_url
				);
				return track_cache;
			} else {
				return null;
			}
		}
		set { }
	}

	void greeter_proxy_new (GLib.Object? obj, AsyncResult res) {
		try {
			this.greeter = Bus.get_proxy.end (res);
		} catch (Error e) {
			this.greeter = null;
			warning("Unable to get greeter proxy: %s", e.message);
		}
	}

	/* Control functions through unity-greeter-session-broadcast */
	public override void activate () {
		/* TODO: */
	}
	public override void play_pause () {
		debug("Play Pause for user: %s", this.username);

		if (this.greeter != null) {
			this.greeter.RequestSoundPlayPause.begin(this.username, (obj, res) => {
				try {
					(obj as GreeterBroadcast).RequestSoundPlayPause.end(res);
				} catch (Error e) {
					warning("Unable to send play pause: %s", e.message);
				}
			});
		} else {
			warning("No unity-greeter-session-broadcast to send play-pause");
		}
	}
	public override void next () {
		debug("Next for user: %s", this.username);

		if (this.greeter != null) {
			this.greeter.RequestSoundNext.begin(this.username, (obj, res) => {
				try {
					(obj as GreeterBroadcast).RequestSoundNext.end(res);
				} catch (Error e) {
					warning("Unable to send next: %s", e.message);
				}
			});
		} else {
			warning("No unity-greeter-session-broadcast to send next");
		}
	}
	public override void previous () {
		debug("Previous for user: %s", this.username);

		if (this.greeter != null) {
			this.greeter.RequestSoundPrev.begin(this.username, (obj, res) => {
				try {
					(obj as GreeterBroadcast).RequestSoundPrev.end(res);
				} catch (Error e) {
					warning("Unable to send previous: %s", e.message);
				}
			});
		} else {
			warning("No unity-greeter-session-broadcast to send previous");
		}
	}

	/* Play list functions are all null as we don't support the
	   playlist feature on the greeter */
	public override uint get_n_playlists() {
		return 0;
	}
	public override string get_playlist_id (int index) {
		return "";
	}
	public override string get_playlist_name (int index) {
		return "";
	}
	public override void activate_playlist_by_name (string playlist) {
	}
}
