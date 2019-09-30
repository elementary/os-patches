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
 * MediaPlayerMpris represents an MRPIS-capable media player.
 */
public class MediaPlayerMpris: MediaPlayer {

	public MediaPlayerMpris (DesktopAppInfo appinfo) {
		this.appinfo = appinfo;
	}

	/** Desktop id of the player */
	public override string id {
		get {
			return this.appinfo.get_id ();
		}
	}

	/** Display name of the player */
	public override string name {
		get {
			return this.appinfo.get_name ();
		}
	}

	/** Application icon of the player */
	public override Icon? icon {
		get {
			return this.appinfo.get_icon ();
		}
	}

	/**
	 * True if an instance of the player is currently running.
	 *
	 * See also: attach(), detach()
	 */
	public override bool is_running {
		get {
			return this.proxy != null;
		}
	}

	/** Name of the player on the bus, if an instance is currently running */
	public override string dbus_name {
		get {
			return this._dbus_name;
		}
	}

	public override string state {
		get; private set; default = "Paused";
	}

	public override MediaPlayer.Track? current_track {
		get; set;
	}

	public override bool can_raise {
		get {
			return this.root != null ? this.root.CanRaise : true;
		}
	}

	/**
	 * Attach this object to a process of the associated media player.  The player must own @dbus_name and
	 * implement the org.mpris.MediaPlayer2.Player interface.
	 *
	 * Only one player can be attached at any given time.  Use detach() to detach a player.
	 *
	 * This method does not block.  If it is successful, "is-running" will be set to %TRUE.
	 */
	public void attach (MprisRoot root, string dbus_name) {
		return_if_fail (this._dbus_name == null && this.proxy == null);

		this.root = root;
		this.notify_property ("can-raise");

		this._dbus_name = dbus_name;
		Bus.get_proxy.begin<MprisPlayer> (BusType.SESSION, dbus_name, "/org/mpris/MediaPlayer2",
										  DBusProxyFlags.GET_INVALIDATED_PROPERTIES, null, got_proxy);
		Bus.get_proxy.begin<MprisPlaylists> (BusType.SESSION, dbus_name, "/org/mpris/MediaPlayer2",
											 DBusProxyFlags.GET_INVALIDATED_PROPERTIES, null, got_playlists_proxy);
	}

	/**
	 * Detach this object from a process running the associated media player.
	 *
	 * See also: attach()
	 */
	public void detach () {
		this.root = null;
		this.proxy = null;
		this._dbus_name = null;
		this.notify_property ("is-running");
		this.notify_property ("can-raise");
		this.state = "Paused";
		this.current_track = null;
	}

	/**
	 * Activate the associated media player.
	 *
	 * Note: this will _not_ call attach(), because it doesn't know on which dbus-name the player will appear.
	 * Use attach() to attach this object to a running instance of the player.
	 */
	public override void activate () {
		try {
			if (this.proxy == null) {
				this.appinfo.launch (null, null);
				this.state = "Launching";
			}
			else if (this.root != null && this.root.CanRaise) {
				this.root.Raise ();
			}
		}
		catch (Error e) {
			warning ("unable to activate %s: %s", appinfo.get_name (), e.message);
		}
	}

	/**
	 * Toggles playing status.
	 */
	public override void play_pause () {
		if (this.proxy != null) {
			this.proxy.PlayPause.begin ();
		}
		else if (this.state != "Launching") {
			this.play_when_attached = true;
			this.activate ();
		}
	}

	/**
	 * Skips to the next track.
	 */
	public override void next () {
		if (this.proxy != null)
			this.proxy.Next.begin ();
	}

	/**
	 * Skips to the previous track.
	 */
	public override void previous () {
		if (this.proxy != null)
			this.proxy.Previous.begin ();
	}

	public override uint get_n_playlists () {
		return this.playlists != null ? this.playlists.length : 0;
	}

	public override string get_playlist_id (int index) {
		return_val_if_fail (index < this.playlists.length, "");
		return this.playlists[index].path;
	}

	public override string get_playlist_name (int index) {
		return_val_if_fail (index < this.playlists.length, "");
		return this.playlists[index].name;
	}

	public override void activate_playlist_by_name (string name) {
		if (this.playlists_proxy != null)
			this.playlists_proxy.ActivatePlaylist.begin (new ObjectPath (name));
	}

	DesktopAppInfo appinfo;
	MprisPlayer? proxy;
	MprisPlaylists ?playlists_proxy;
	string _dbus_name;
	bool play_when_attached = false;
	MprisRoot root;
	PlaylistDetails[] playlists = null;

	void got_proxy (Object? obj, AsyncResult res) {
		try {
			this.proxy = Bus.get_proxy.end (res);

			/* Connecting to GDBusProxy's "g-properties-changed" signal here, because vala's dbus objects don't
			 * emit notify signals */
			var gproxy = this.proxy as DBusProxy;
			gproxy.g_properties_changed.connect (this.proxy_properties_changed);

			this.notify_property ("is-running");
			this.state = this.proxy.PlaybackStatus;
			this.update_current_track (gproxy.get_cached_property ("Metadata"));

			if (this.play_when_attached) {
				/* wait a little before calling PlayPause, some players need some time to
				   set themselves up */
				Timeout.add (1000, () => { proxy.PlayPause.begin (); return false; } );
				this.play_when_attached = false;
			}
		}
		catch (Error e) {
			this._dbus_name = null;
			warning ("unable to attach to media player: %s", e.message);
		}
	}

	void fetch_playlists () {
		/* The proxy is created even when the interface is not supported. GDBusProxy will
		   return 0 for the PlaylistCount property in that case. */
		if (this.playlists_proxy != null && this.playlists_proxy.PlaylistCount > 0) {
			this.playlists_proxy.GetPlaylists.begin (0, 100, "Alphabetical", false, (obj, res) => {
				try {
					this.playlists = playlists_proxy.GetPlaylists.end (res);
					this.playlists_changed ();
				}
				catch (Error e) {
					warning ("could not fetch playlists: %s", e.message);
					this.playlists = null;
				}
			});
		}
		else {
			this.playlists = null;
			this.playlists_changed ();
		}
	}

	void got_playlists_proxy (Object? obj, AsyncResult res) {
		try {
			this.playlists_proxy = Bus.get_proxy.end (res);

			var gproxy = this.proxy as DBusProxy;
			gproxy.g_properties_changed.connect (this.playlists_proxy_properties_changed);
		}
		catch (Error e) {
			warning ("unable to create mpris plalists proxy: %s", e.message);
			return;
		}

		Timeout.add (500, () => { this.fetch_playlists (); return false; } );
	}

	/* some players (e.g. Spotify) don't follow the spec closely and pass single strings in metadata fields
	 * where an array of string is expected */
	static string sanitize_metadata_value (Variant? v) {
		if (v == null)
			return "";
		else if (v.is_of_type (VariantType.STRING))
			return v.get_string ();
		else if (v.is_of_type (VariantType.STRING_ARRAY))
			return string.joinv (",", v.get_strv ());

		warn_if_reached ();
		return "";
	}

	void proxy_properties_changed (DBusProxy proxy, Variant changed_properties, string[] invalidated_properties) {
		if (changed_properties.lookup ("PlaybackStatus", "s", null)) {
			this.state = this.proxy.PlaybackStatus;
		}

		var metadata = changed_properties.lookup_value ("Metadata", new VariantType ("a{sv}"));
		if (metadata != null)
			this.update_current_track (metadata);
	}

	void playlists_proxy_properties_changed (DBusProxy proxy, Variant changed_properties, string[] invalidated_properties) {
		if (changed_properties.lookup ("PlaylistCount", "u", null))
			this.fetch_playlists ();
	}

	void update_current_track (Variant metadata) {
		if (metadata != null) {
			this.current_track = new Track (
				sanitize_metadata_value (metadata.lookup_value ("xesam:artist", null)),
				sanitize_metadata_value (metadata.lookup_value ("xesam:title", null)),
				sanitize_metadata_value (metadata.lookup_value ("xesam:album", null)),
				sanitize_metadata_value (metadata.lookup_value ("mpris:artUrl", null))
			);
		}
		else {
			this.current_track = null;
		}
	}
}
