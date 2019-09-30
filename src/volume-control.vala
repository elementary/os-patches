/*
 * -*- Mode:Vala; indent-tabs-mode:t; tab-width:4; encoding:utf8 -*-
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
 *      Alberto Ruiz <alberto.ruiz@canonical.com>
 */

using PulseAudio;

[CCode(cname="pa_cvolume_set", cheader_filename = "pulse/volume.h")]
extern unowned PulseAudio.CVolume? vol_set (PulseAudio.CVolume? cv, uint channels, PulseAudio.Volume v);

[DBus (name="com.canonical.UnityGreeter.List")]
interface GreeterListInterface : Object
{
    public abstract async string get_active_entry () throws IOError;
    public signal void entry_selected (string entry_name);
}

public class VolumeControl : Object
{
	/* this is static to ensure it being freed after @context (loop does not have ref counting) */
	private static PulseAudio.GLibMainLoop loop;

	private uint _reconnect_timer = 0;

	private PulseAudio.Context context;
	private bool   _mute = true;
	private bool   _is_playing = false;
	private double _volume = 0.0;
	private double _mic_volume = 0.0;

	private DBusProxy _user_proxy;
	private GreeterListInterface _greeter_proxy;
	private Cancellable _mute_cancellable;
	private Cancellable _volume_cancellable;

	public signal void volume_changed (double v);
	public signal void mic_volume_changed (double v);

	/** true when connected to the pulse server */
	public bool ready { get; set; }

	/** true when a microphone is active **/
	public bool active_mic { get; private set; default = false; }

	public VolumeControl ()
	{
		if (loop == null)
			loop = new PulseAudio.GLibMainLoop ();

		_mute_cancellable = new Cancellable ();
		_volume_cancellable = new Cancellable ();
		setup_accountsservice.begin ();

		this.reconnect_to_pulse ();
	}

	~VolumeControl ()
	{
		if (_reconnect_timer != 0) {
			Source.remove (_reconnect_timer);
			_reconnect_timer = 0;
		}
	}

	/* PulseAudio logic*/
	private void context_events_cb (Context c, Context.SubscriptionEventType t, uint32 index)
	{
		switch (t & Context.SubscriptionEventType.FACILITY_MASK)
		{
			case Context.SubscriptionEventType.SINK:
				update_sink ();
				break;

			case Context.SubscriptionEventType.SOURCE:
				update_source ();
				break;

			case Context.SubscriptionEventType.SOURCE_OUTPUT:
				switch (t & Context.SubscriptionEventType.TYPE_MASK)
				{
					case Context.SubscriptionEventType.NEW:
						c.get_source_output_info (index, source_output_info_cb);
						break;

					case Context.SubscriptionEventType.REMOVE:
						this.active_mic = false;
						break;
				}
				break;
		}
	}

	private void sink_info_cb_for_props (Context c, SinkInfo? i, int eol)
	{
		if (i == null)
			return;

		if (_mute != (bool)i.mute)
		{
			_mute = (bool)i.mute;
			this.notify_property ("mute");
		}

		var playing = (i.state == PulseAudio.SinkState.RUNNING);
		if (_is_playing != playing)
		{
			_is_playing = playing;
			this.notify_property ("is-playing");
		}

		if (_volume != volume_to_double (i.volume.values[0]))
		{
			_volume = volume_to_double (i.volume.values[0]);
			volume_changed (_volume);
		}
	}

	private void source_info_cb (Context c, SourceInfo? i, int eol)
	{
		if (i == null)
			return;

		if (_mic_volume != volume_to_double (i.volume.values[0]))
		{
			_mic_volume = volume_to_double (i.volume.values[0]);
			mic_volume_changed (_mic_volume);
		}
	}

	private void server_info_cb_for_props (Context c, ServerInfo? i)
	{
		if (i == null)
			return;
		context.get_sink_info_by_name (i.default_sink_name, sink_info_cb_for_props);
	}

	private void update_sink ()
	{
		context.get_server_info (server_info_cb_for_props);
	}

	private void update_source_get_server_info_cb (PulseAudio.Context c, PulseAudio.ServerInfo? i) {
		if (i != null)
			context.get_source_info_by_name (i.default_source_name, source_info_cb);
	}

	private void update_source ()
	{
		context.get_server_info (update_source_get_server_info_cb);
	}

	private void source_output_info_cb (Context c, SourceOutputInfo? i, int eol)
	{
		if (i == null)
			return;

		var role = i.proplist.gets (PulseAudio.Proplist.PROP_MEDIA_ROLE);
		if (role == "phone" || role == "production")
			this.active_mic = true;
	}

	private void context_state_callback (Context c)
	{
		switch (c.get_state ()) {
			case Context.State.READY:
				c.subscribe (PulseAudio.Context.SubscriptionMask.SINK |
							 PulseAudio.Context.SubscriptionMask.SOURCE |
							 PulseAudio.Context.SubscriptionMask.SOURCE_OUTPUT);
				c.set_subscribe_callback (context_events_cb);
				update_sink ();
				update_source ();
				this.ready = true;
				break;

			case Context.State.FAILED:
			case Context.State.TERMINATED:
				if (_reconnect_timer == 0)
					_reconnect_timer = Timeout.add_seconds (2, reconnect_timeout);
				break;

			default: 
				this.ready = false;
				break;
		}
	}

	bool reconnect_timeout ()
	{
		_reconnect_timer = 0;
		reconnect_to_pulse ();
		return false; // G_SOURCE_REMOVE
	}

	void reconnect_to_pulse ()
	{
		if (this.ready) {
			this.context.disconnect ();
			this.context = null;
			this.ready = false;
		}

		var props = new Proplist ();
		props.sets (Proplist.PROP_APPLICATION_NAME, "Ubuntu Audio Settings");
		props.sets (Proplist.PROP_APPLICATION_ID, "com.canonical.settings.sound");
		props.sets (Proplist.PROP_APPLICATION_ICON_NAME, "multimedia-volume-control");
		props.sets (Proplist.PROP_APPLICATION_VERSION, "0.1");

		this.context = new PulseAudio.Context (loop.get_api(), null, props);
		this.context.set_state_callback (context_state_callback);

		if (context.connect(null, Context.Flags.NOFAIL, null) < 0)
			warning( "pa_context_connect() failed: %s\n", PulseAudio.strerror(context.errno()));
	}

	void sink_info_list_callback_set_mute (PulseAudio.Context context, PulseAudio.SinkInfo? sink, int eol) {
		if (sink != null)
			context.set_sink_mute_by_index (sink.index, true, null);
	}

	void sink_info_list_callback_unset_mute (PulseAudio.Context context, PulseAudio.SinkInfo? sink, int eol) {
		if (sink != null)
			context.set_sink_mute_by_index (sink.index, false, null);
	}

	/* Mute operations */
	bool set_mute_internal (bool mute)
	{
		return_val_if_fail (context.get_state () == Context.State.READY, false);

		if (_mute != mute) {
			if (mute)
				context.get_sink_info_list (sink_info_list_callback_set_mute);
			else
				context.get_sink_info_list (sink_info_list_callback_unset_mute);
			return true;
		} else {
			return false;
		}
	}

	public void set_mute (bool mute)
	{
		if (set_mute_internal (mute))
			sync_mute_to_accountsservice.begin (mute);
	}

	public void toggle_mute ()
	{
		this.set_mute (!this._mute);
	}

	public bool mute
	{
		get
		{
			return this._mute;
		}
	}

	public bool is_playing
	{
		get
		{
			return this._is_playing;
		}
	}

	/* Volume operations */
	private static PulseAudio.Volume double_to_volume (double vol)
	{
		double tmp = (double)(PulseAudio.Volume.NORM - PulseAudio.Volume.MUTED) * vol;
		return (PulseAudio.Volume)tmp + PulseAudio.Volume.MUTED;
	}

	private static double volume_to_double (PulseAudio.Volume vol)
	{
		double tmp = (double)(vol - PulseAudio.Volume.MUTED);
		return tmp / (double)(PulseAudio.Volume.NORM - PulseAudio.Volume.MUTED);
	}

	private void set_volume_success_cb (Context c, int success)
	{
		if ((bool)success)
			volume_changed (_volume);
	}

	private void sink_info_set_volume_cb (Context c, SinkInfo? i, int eol)
	{
		if (i == null)
			return;

		unowned CVolume cvol = vol_set (i.volume, 1, double_to_volume (_volume));
		c.set_sink_volume_by_index (i.index, cvol, set_volume_success_cb);
	}

	private void server_info_cb_for_set_volume (Context c, ServerInfo? i)
	{
		if (i == null)
		{
			warning ("Could not get PulseAudio server info");
			return;
		}

		context.get_sink_info_by_name (i.default_sink_name, sink_info_set_volume_cb);
	}

	bool set_volume_internal (double volume)
	{
		return_val_if_fail (context.get_state () == Context.State.READY, false);

		if (_volume != volume) {
			_volume = volume;
			context.get_server_info (server_info_cb_for_set_volume);
			return true;
		} else {
			return false;
		}
	}

	public void set_volume (double volume)
	{
		if (set_volume_internal (volume))
			sync_volume_to_accountsservice.begin (volume);
	}

	void set_mic_volume_success_cb (Context c, int success)
	{
		if ((bool)success)
			mic_volume_changed (_mic_volume);
	}

	void set_mic_volume_get_server_info_cb (PulseAudio.Context c, PulseAudio.ServerInfo? i) {
		if (i != null) {
			unowned CVolume cvol = CVolume ();
			cvol = vol_set (cvol, 1, double_to_volume (_mic_volume));
			c.set_source_volume_by_name (i.default_source_name, cvol, set_mic_volume_success_cb);
		}
	}

	public void set_mic_volume (double volume)
	{
		return_if_fail (context.get_state () == Context.State.READY);

		_mic_volume = volume;

		context.get_server_info (set_mic_volume_get_server_info_cb);
	}

	public double get_volume ()
	{
		return _volume;
	}

	public double get_mic_volume ()
	{
		return _mic_volume;
	}

	/* AccountsService operations */
	private void accountsservice_props_changed_cb (DBusProxy proxy, Variant changed_properties, string[] invalidated_properties)
	{
		Variant volume_variant = changed_properties.lookup_value ("Volume", new VariantType ("d"));
		if (volume_variant != null) {
			var volume = volume_variant.get_double ();
			if (volume >= 0)
				set_volume_internal (volume);
		}

		Variant mute_variant = changed_properties.lookup_value ("Muted", new VariantType ("b"));
		if (mute_variant != null) {
			var mute = mute_variant.get_boolean ();
			set_mute_internal (mute);
		}
	}

	private async void setup_user_proxy (string? username_in = null)
	{
		var username = username_in;
		_user_proxy = null;

		// Look up currently selected greeter user, if asked
		if (username == null) {
			try {
				username = yield _greeter_proxy.get_active_entry ();
				if (username == "" || username == null)
					return;
			} catch (GLib.Error e) {
				warning ("unable to find Accounts path for user %s: %s", username, e.message);
				return;
			}
		}

		// Get master AccountsService object
		DBusProxy accounts_proxy;
		try {
			accounts_proxy = yield DBusProxy.create_for_bus (BusType.SYSTEM, DBusProxyFlags.DO_NOT_LOAD_PROPERTIES | DBusProxyFlags.DO_NOT_CONNECT_SIGNALS, null, "org.freedesktop.Accounts", "/org/freedesktop/Accounts", "org.freedesktop.Accounts");
		} catch (GLib.Error e) {
			warning ("unable to get greeter proxy: %s", e.message);
			return;
		}

		// Find user's AccountsService object
		try {
			var user_path_variant = yield accounts_proxy.call ("FindUserByName", new Variant ("(s)", username), DBusCallFlags.NONE, -1);
			string user_path;
			user_path_variant.get ("(o)", out user_path);
			_user_proxy = yield DBusProxy.create_for_bus (BusType.SYSTEM, DBusProxyFlags.GET_INVALIDATED_PROPERTIES, null, "org.freedesktop.Accounts", user_path, "com.ubuntu.AccountsService.Sound");
		} catch (GLib.Error e) {
			warning ("unable to find Accounts path for user %s: %s", username, e.message);
			return;
		}

		// Get current values and listen for changes
		_user_proxy.g_properties_changed.connect (accountsservice_props_changed_cb);
		var props_variant = yield _user_proxy.get_connection ().call (_user_proxy.get_name (), _user_proxy.get_object_path (), "org.freedesktop.DBus.Properties", "GetAll", new Variant ("(s)", _user_proxy.get_interface_name ()), null, DBusCallFlags.NONE, -1);
		Variant props;
		props_variant.get ("(@a{sv})", out props);
		accountsservice_props_changed_cb(_user_proxy, props, null);
	}

	private void greeter_user_changed (string username)
	{
		setup_user_proxy.begin (username);
	}

	private async void setup_accountsservice ()
	{
		if (Environment.get_variable ("XDG_SESSION_CLASS") == "greeter") {
			try {
				_greeter_proxy = yield Bus.get_proxy (BusType.SESSION, "com.canonical.UnityGreeter", "/list");
			} catch (GLib.Error e) {
				warning ("unable to get greeter proxy: %s", e.message);
				return;
			}
			_greeter_proxy.entry_selected.connect (greeter_user_changed);
			yield setup_user_proxy ();
		} else {
			// We are in a user session.  We just need our own proxy
			var username = Environment.get_variable ("USER");
			if (username != "" && username != null) {
				yield setup_user_proxy (username);
			}
		}
	}

	private async void sync_mute_to_accountsservice (bool mute)
	{
		if (_user_proxy == null)
			return;

		_mute_cancellable.cancel ();
		_mute_cancellable.reset ();

		try {
			yield _user_proxy.get_connection ().call (_user_proxy.get_name (), _user_proxy.get_object_path (), "org.freedesktop.DBus.Properties", "Set", new Variant ("(ssv)", _user_proxy.get_interface_name (), "Muted", new Variant ("b", mute)), null, DBusCallFlags.NONE, -1, _mute_cancellable);
		} catch (GLib.Error e) {
			warning ("unable to sync mute to AccountsService: %s", e.message);
		}
	}

	private async void sync_volume_to_accountsservice (double volume)
	{
		if (_user_proxy == null)
			return;

		_volume_cancellable.cancel ();
		_volume_cancellable.reset ();

		try {
			yield _user_proxy.get_connection ().call (_user_proxy.get_name (), _user_proxy.get_object_path (), "org.freedesktop.DBus.Properties", "Set", new Variant ("(ssv)", _user_proxy.get_interface_name (), "Volume", new Variant ("d", volume)), null, DBusCallFlags.NONE, -1, _volume_cancellable);
		} catch (GLib.Error e) {
			warning ("unable to sync volume to AccountsService: %s", e.message);
		}
	}
}
