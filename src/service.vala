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

public class IndicatorSound.Service: Object {
	public Service (MediaPlayerList playerlist) {
		this.settings = new Settings ("com.canonical.indicator.sound");
		this.sharedsettings = new Settings ("com.ubuntu.sound");

		this.settings.bind ("visible", this, "visible", SettingsBindFlags.GET);
		this.notify["visible"].connect ( () => this.update_root_icon () );

		this.volume_control = new VolumeControl ();

		this.players = playerlist;
		this.players.player_added.connect (this.player_added);
		this.players.player_removed.connect (this.player_removed);

		this.actions = new SimpleActionGroup ();
		this.actions.add_action_entries (action_entries, this);
		this.actions.add_action (this.create_mute_action ());
		this.actions.add_action (this.create_volume_action ());
		this.actions.add_action (this.create_mic_volume_action ());

		this.menus = new HashTable<string, SoundMenu> (str_hash, str_equal);
		this.menus.insert ("desktop_greeter", new SoundMenu (null, SoundMenu.DisplayFlags.SHOW_MUTE | SoundMenu.DisplayFlags.HIDE_PLAYERS));
		this.menus.insert ("phone_greeter", new SoundMenu (null, SoundMenu.DisplayFlags.HIDE_INACTIVE_PLAYERS));
		this.menus.insert ("desktop", new SoundMenu ("indicator.desktop-settings", SoundMenu.DisplayFlags.SHOW_MUTE));
		this.menus.insert ("phone", new SoundMenu ("indicator.phone-settings", SoundMenu.DisplayFlags.HIDE_INACTIVE_PLAYERS));

		this.menus.@foreach ( (profile, menu) => {
			this.volume_control.bind_property ("active-mic", menu, "show-mic-volume", BindingFlags.SYNC_CREATE);
		});

		/* Setup handling for the greeter-export setting */
		this.settings.changed["greeter-export"].connect( () => this.build_accountsservice() );
		build_accountsservice();

		this.sync_preferred_players ();
		this.settings.changed["interested-media-players"].connect ( () => {
			this.sync_preferred_players ();
		});

		if (settings.get_boolean ("show-notify-osd-on-scroll")) {
			List<string> caps = Notify.get_server_caps ();
			if (caps.find_custom ("x-canonical-private-synchronous", strcmp) != null) {
				this.notification = new Notify.Notification ("indicator-sound", "", "");
				this.notification.set_hint_string ("x-canonical-private-synchronous", "indicator-sound");
			}
		}

		sharedsettings.bind ("allow-amplified-volume", this, "allow-amplified-volume", SettingsBindFlags.GET);
	}

	~Service() {
		if (this.sound_was_blocked_timeout_id > 0) {
			Source.remove (this.sound_was_blocked_timeout_id);
			this.sound_was_blocked_timeout_id = 0;
		}
	}

	void build_accountsservice () {
		clear_acts_player();
		this.accounts_service = null;

		/* If we're not exporting, don't build anything */
		if (!this.settings.get_boolean("greeter-export")) {
			debug("Accounts service export disabled due to user setting");
			return;
		}

		/* If we're on the greeter, don't export */
		if (GLib.Environment.get_user_name() == "lightdm") {
			debug("Accounts service export disabled due to being used on the greeter");
			return;
		}

		this.accounts_service = new AccountsServiceUser();

		this.eventually_update_player_actions();
	}

	void clear_acts_player () {
		/* NOTE: This is a bit of a hack to ensure that accounts service doesn't
		   continue to export the player by keeping a ref in the timer */
		if (this.accounts_service != null)
			this.accounts_service.player = null;
	}

	public int run () {
		if (this.loop != null) {
			warning ("service is already running");
			return 1;
		}

		Bus.own_name (BusType.SESSION, "com.canonical.indicator.sound", BusNameOwnerFlags.NONE,
			this.bus_acquired, null, this.name_lost);

		this.loop = new MainLoop (null, false);

		GLib.Unix.signal_add(GLib.ProcessSignal.TERM, () => {
			debug("SIGTERM recieved, stopping our mainloop");
			this.loop.quit();
			return false;
		});

		this.loop.run ();

		clear_acts_player();

		return 0;
	}

	public bool visible { get; set; }

	public bool allow_amplified_volume {
		get {
			return this.max_volume > 1.0;
		}

		set {
			if (value) {
				/* from pulse/volume.h: #define PA_VOLUME_UI_MAX (pa_sw_volume_from_dB(+11.0)) */
				this.max_volume = (double)PulseAudio.Volume.sw_from_dB(11.0) / PulseAudio.Volume.NORM;
			}
			else {
				this.max_volume = 1.0;
			}

			/* Normalize volume, because the volume action's state is [0.0, 1.0], see create_volume_action() */
			this.actions.change_action_state ("volume", this.volume_control.get_volume () / this.max_volume);
		}
	}

	const ActionEntry[] action_entries = {
		{ "root", null, null, "@a{sv} {}", null },
		{ "scroll", activate_scroll_action, "i", null, null },
		{ "desktop-settings", activate_desktop_settings, null, null, null },
		{ "phone-settings", activate_phone_settings, null, null, null },
	};

	MainLoop loop;
	SimpleActionGroup actions;
	HashTable<string, SoundMenu> menus;
	Settings settings;
	Settings sharedsettings;
	VolumeControl volume_control;
	MediaPlayerList players;
	uint player_action_update_id;
	bool mute_blocks_sound;
	uint sound_was_blocked_timeout_id;
	Notify.Notification notification;
	bool syncing_preferred_players = false;
	AccountsServiceUser? accounts_service = null;

	/* Maximum volume as a scaling factor between the volume action's state and the value in
	 * this.volume_control. See create_volume_action().
	 */
	double max_volume = 1.0;

	const double volume_step_percentage = 0.06;

	void activate_scroll_action (SimpleAction action, Variant? param) {
		int delta = param.get_int32(); /* positive for up, negative for down */

		double v = this.volume_control.get_volume () + volume_step_percentage * delta;
		this.volume_control.set_volume (v.clamp (0.0, this.max_volume));

		if (this.notification != null) {
			string icon;
			if (v <= 0.0)
				icon = "notification-audio-volume-off";
			else if (v <= 0.3)
				icon = "notification-audio-volume-low";
			else if (v <= 0.7)
				icon = "notification-audio-volume-medium";
			else
				icon = "notification-audio-volume-high";

			this.notification.update ("indicator-sound", "", icon);
			this.notification.set_hint_int32 ("value", ((int32) (100 * v / this.max_volume)).clamp (-1, 101));
			try {
				this.notification.show ();
			}
			catch (Error e) {
				warning ("unable to show notification: %s", e.message);
			}
		}
	}

	void activate_desktop_settings (SimpleAction action, Variant? param) {
		var env = Environment.get_variable ("DESKTOP_SESSION");
		string cmd;
		if (env == "xubuntu" || env == "ubuntustudio")
			cmd = "pavucontrol";
		else
		{
			if (Environment.get_variable ("XDG_CURRENT_DESKTOP") == "Unity" && Environment.find_program_in_path ("unity-control-center") != null)
				cmd = "unity-control-center sound";
			else
				cmd = "gnome-control-center sound";
		}

		try {
			Process.spawn_command_line_async (cmd);
		} catch (Error e) {
			warning ("unable to launch sound settings: %s", e.message);
		}
	}

	void activate_phone_settings (SimpleAction action, Variant? param) {
		UrlDispatch.send ("settings:///system/sound");
	}

	/* Returns a serialized version of @icon_name suited for the panel */
	static Variant serialize_themed_icon (string icon_name)
	{
		var icon = new ThemedIcon.with_default_fallbacks (icon_name);
		return icon.serialize ();
	}

	void update_root_icon () {
		double volume = this.volume_control.get_volume ();
		string icon;
		if (this.volume_control.mute)
			icon = this.mute_blocks_sound ? "audio-volume-muted-blocking-panel" : "audio-volume-muted-panel";
		else if (volume <= 0.0)
			icon = "audio-volume-low-zero-panel";
		else if (volume <= 0.3)
			icon = "audio-volume-low-panel";
		else if (volume <= 0.7)
			icon = "audio-volume-medium-panel";
		else
			icon  = "audio-volume-high-panel";

		string accessible_name;
		if (this.volume_control.mute) {
			accessible_name = _("Volume (muted)");
		} else {
			int volume_int = (int)(volume * 100);
			accessible_name = "%s (%d%%)".printf (_("Volume"), volume_int);
		}

		var root_action = actions.lookup_action ("root") as SimpleAction;
		var builder = new VariantBuilder (new VariantType ("a{sv}"));
		builder.add ("{sv}", "title", new Variant.string (_("Sound")));
		builder.add ("{sv}", "accessible-desc", new Variant.string (accessible_name));
		builder.add ("{sv}", "icon", serialize_themed_icon (icon));
		builder.add ("{sv}", "visible", new Variant.boolean (this.visible));
		root_action.set_state (builder.end());
	}

	Action create_mute_action () {
		var mute_action = new SimpleAction.stateful ("mute", null, new Variant.boolean (this.volume_control.mute));

		mute_action.activate.connect ( (action, param) => {
			action.change_state (new Variant.boolean (!action.get_state ().get_boolean ()));
		});

		mute_action.change_state.connect ( (action, val) => {
			volume_control.set_mute (val.get_boolean ());
		});

		this.volume_control.notify["mute"].connect ( () => {
			mute_action.set_state (new Variant.boolean (this.volume_control.mute));
			this.update_root_icon ();
		});

		this.volume_control.notify["is-playing"].connect( () => {
			if (!this.volume_control.mute) {
				this.mute_blocks_sound = false;
				return;
			}

			if (this.volume_control.is_playing) {
				this.mute_blocks_sound = true;
			}
			else if (this.mute_blocks_sound) {
				/* Continue to show the blocking icon five seconds after a player has tried to play something */
				if (this.sound_was_blocked_timeout_id > 0)
					Source.remove (this.sound_was_blocked_timeout_id);

				this.sound_was_blocked_timeout_id = Timeout.add_seconds (5, () => {
					this.mute_blocks_sound = false;
					this.sound_was_blocked_timeout_id = 0;
					this.update_root_icon ();
					return false;
				});
			}

			this.update_root_icon ();
		});

		return mute_action;
	}

	void volume_changed (double volume) {
		var volume_action = this.actions.lookup_action ("volume") as SimpleAction;

		/* Normalize volume, because the volume action's state is [0.0, 1.0], see create_volume_action() */
		volume_action.set_state (new Variant.double (volume / this.max_volume));

		this.update_root_icon ();
	}

	Action create_volume_action () {
		/* The action's state is between be in [0.0, 1.0] instead of [0.0,
		 * max_volume], so that we don't need to update the slider menu item
		 * every time allow-amplified-volume is changed.  Convert between the
		 * two here, so that we always pass the full range into
		 * volume_control.set_volume().
		 */

		double volume = this.volume_control.get_volume () / this.max_volume;

		var volume_action = new SimpleAction.stateful ("volume", VariantType.INT32, new Variant.double (volume));

		volume_action.change_state.connect ( (action, val) => {
			double v = val.get_double () * this.max_volume;
			volume_control.set_volume (v.clamp (0.0, this.max_volume));
		});

		/* activating this action changes the volume by the amount given in the parameter */
		volume_action.activate.connect ( (action, param) => {
			int delta = param.get_int32 ();
			double v = volume_control.get_volume () + volume_step_percentage * delta;
			volume_control.set_volume (v.clamp (0.0, this.max_volume));
		});

		this.volume_control.volume_changed.connect (volume_changed);

		this.volume_control.bind_property ("ready", volume_action, "enabled", BindingFlags.SYNC_CREATE);

		return volume_action;
	}

	Action create_mic_volume_action () {
		var volume_action = new SimpleAction.stateful ("mic-volume", null, new Variant.double (this.volume_control.get_mic_volume ()));

		volume_action.change_state.connect ( (action, val) => {
			volume_control.set_mic_volume (val.get_double ());
		});

		this.volume_control.mic_volume_changed.connect ( (volume) => {
			volume_action.set_state (new Variant.double (volume));
		});

		this.volume_control.bind_property ("ready", volume_action, "enabled", BindingFlags.SYNC_CREATE);

		return volume_action;
	}

	void bus_acquired (DBusConnection connection, string name) {
		try {
			connection.export_action_group ("/com/canonical/indicator/sound", this.actions);
		} catch (Error e) {
			critical ("%s", e.message);
		}

		this.menus.@foreach ( (profile, menu) => menu.export (connection, @"/com/canonical/indicator/sound/$profile"));
	}

	void name_lost (DBusConnection connection, string name) {
		this.loop.quit ();
	}

	Variant action_state_for_player (MediaPlayer player) {
		var builder = new VariantBuilder (new VariantType ("a{sv}"));
		builder.add ("{sv}", "running", new Variant ("b", player.is_running));
		builder.add ("{sv}", "state", new Variant ("s", player.state));
		if (player.current_track != null) {
			builder.add ("{sv}", "title", new Variant ("s", player.current_track.title));
			builder.add ("{sv}", "artist", new Variant ("s", player.current_track.artist));
			builder.add ("{sv}", "album", new Variant ("s", player.current_track.album));
			builder.add ("{sv}", "art-url", new Variant ("s", player.current_track.art_url));
		}
		return builder.end ();
	}

	bool update_player_actions () {
		bool clear_accounts_player = true;

		foreach (var player in this.players) {
			SimpleAction? action = this.actions.lookup_action (player.id) as SimpleAction;
			if (action != null) {
				action.set_state (this.action_state_for_player (player));
				action.set_enabled (player.can_raise);
			}
			
			/* If we're playing then put that data in accounts service */
			if (player.is_running && accounts_service != null) {
				accounts_service.player = player;
				clear_accounts_player = false;
			}
		}

		if (clear_accounts_player)
			clear_acts_player();

		this.player_action_update_id = 0;
		return false;
	}

	void eventually_update_player_actions () {
		if (player_action_update_id == 0)
			this.player_action_update_id = Idle.add (this.update_player_actions);
	}


	void sync_preferred_players () {
		this.syncing_preferred_players = true;
		this.players.sync (settings.get_strv ("interested-media-players"));
		this.syncing_preferred_players = false;
	}

	void update_preferred_players () {
		/* only write the key if we're not getting this call because we're syncing from the key right now */
		if (!this.syncing_preferred_players) {
			var builder = new VariantBuilder (VariantType.STRING_ARRAY);
			foreach (var player in this.players)
				builder.add ("s", player.id);
			this.settings.set_value ("interested-media-players", builder.end ());
		}
	}

	void player_added (MediaPlayer player) {
		this.menus.@foreach ( (profile, menu) => menu.add_player (player));

		SimpleAction action = new SimpleAction.stateful (player.id, null, this.action_state_for_player (player));
		action.set_enabled (player.can_raise);
		action.activate.connect ( () => { player.activate (); });
		this.actions.add_action (action);

		var play_action = new SimpleAction.stateful ("play." + player.id, null, player.state);
		play_action.activate.connect ( () => player.play_pause () );
		this.actions.add_action (play_action);
		player.notify["state"].connect ( (object, pspec) => {
			play_action.set_state (player.state);
		});

		var next_action = new SimpleAction ("next." + player.id, null);
		next_action.activate.connect ( () => player.next () );
		this.actions.add_action (next_action);

		var prev_action = new SimpleAction ("previous." + player.id, null);
		prev_action.activate.connect ( () => player.previous () );
		this.actions.add_action (prev_action);

		var playlist_action = new SimpleAction ("play-playlist." + player.id, VariantType.STRING);
		playlist_action.activate.connect ( (parameter) => player.activate_playlist_by_name (parameter.get_string ()) );
		this.actions.add_action (playlist_action);

		player.notify.connect (this.eventually_update_player_actions);

		this.update_preferred_players ();
	}

	void player_removed (MediaPlayer player) {
		this.actions.remove_action (player.id);
		this.actions.remove_action ("play." + player.id);
		this.actions.remove_action ("next." + player.id);
		this.actions.remove_action ("previous." + player.id);
		this.actions.remove_action ("play-playlist." + player.id);

		player.notify.disconnect (this.eventually_update_player_actions);

		this.menus.@foreach ( (profile, menu) => menu.remove_player (player));

		this.update_preferred_players ();
	}
}
