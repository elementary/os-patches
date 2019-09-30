/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: William Hua <william.hua@canonical.com>
 */

const int TIMEOUT_S = 1;
const int TIMEOUT_MS = 1000;
const int LONG_TIMEOUT_S = 10;

static string display;

[DBus (name = "com.canonical.indicator.keyboard.test")]
public class Service : Object {

	[DBus (visible = false)]
	private string? _command;

	[DBus (visible = false)]
	public string? command {
		get { return _command; }
	}

	public void execute (string command) {
		_command = command;

		var pspec = get_class ().find_property ("command");

		if (pspec != null) {
			notify["command"] ((!) pspec);
		}
	}
}

public class Tests : Object, Fixture {

	private TestDBus? _bus;
	private uint _service_name;
	private DBusConnection? _connection;
	private Service? _service;
	private uint _object_name;

	public void start_service () {
		if (_connection != null) {
			try {
				_service = new Service ();
				_object_name = ((!) _connection).register_object ("/com/canonical/indicator/keyboard/test", _service);
			} catch (IOError error) {
				_connection = null;
				_service = null;
				_object_name = 0;

				Test.message ("error: %s", error.message);
				Test.fail ();
			}
		}
	}

	public void setup () {
		Environment.set_variable ("DCONF_PROFILE", DCONF_PROFILE, true);
		Environment.set_variable ("DISPLAY", display, true);
		Environment.set_variable ("LC_ALL", "C", true);

		_bus = new TestDBus (TestDBusFlags.NONE);
		((!) _bus).add_service_dir (SERVICE_DIR);
		((!) _bus).up ();

		var loop = new MainLoop (null, false);

		_service_name = Bus.own_name (BusType.SESSION,
		                              "com.canonical.indicator.keyboard.test",
		                              BusNameOwnerFlags.ALLOW_REPLACEMENT | BusNameOwnerFlags.REPLACE,
		                              (connection, name) => {
		                                      if (loop.is_running ()) {
		                                              _connection = connection;
		                                              start_service ();
		                                              loop.quit ();
		                                      }
		                              },
		                              null,
		                              (connection, name) => {
		                                      if (loop.is_running ()) {
		                                              _connection = null;
		                                              _service = null;
		                                              _object_name = 0;
		                                              loop.quit ();
		                                      }
		                              });

		loop.run ();

		if (_connection == null) {
			Test.message ("error: Unable to connect to com.canonical.indicator.keyboard.test.");
			Test.fail ();
		}

		if (_object_name == 0) {
			Test.message ("error: Test fixture not initialized.");
			Test.fail ();
			return;
		}
	}

	public void teardown () {
		if (_object_name != 0) {
			((!) _connection).unregister_object (_object_name);
			_object_name = 0;
		}

		if (_service_name != 0) {
			Bus.unown_name (_service_name);
			_service_name = 0;
		}

		_service = null;
		_connection = null;

		if (_bus != null) {
			((!) _bus).down ();
			_bus = null;
		}
	}

	public void test_activate_input_source () {
		try {
			var current = 0;
			var sources = "[('xkb', 'us'), ('xkb', 'ca+eng'), ('xkb', 'epo'), ('ibus', 'pinyin')]";
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources current $current");
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources sources \"$sources\"");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		var action_group = DBusActionGroup.get ((!) _connection,
		                                        "com.canonical.indicator.keyboard",
		                                        "/com/canonical/indicator/keyboard");
		action_group.list_actions ();
		action_group.activate_action ("current", new Variant.uint32 (2));

		var loop = new MainLoop (null, false);
		Timeout.add_seconds (TIMEOUT_S, () => { loop.quit (); return false; });
		loop.run ();

		var state = action_group.get_action_state ("current");
		var current = state.get_uint32 ();
		stderr.printf ("current = %u\n", current);
		assert (current == 2);

		try {
			string output;
			Process.spawn_command_line_sync ("gsettings get org.gnome.desktop.input-sources current", out output);
			stderr.printf ("output = \"%s\"\n", output);
			assert (strcmp (output, "uint32 2\n") == 0);
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}
	}

	public void test_activate_character_map () {
		var action_group = DBusActionGroup.get ((!) _connection,
		                                        "com.canonical.indicator.keyboard",
		                                        "/com/canonical/indicator/keyboard");
		var loop = new MainLoop (null, false);
		var signal_name = ((!) _service).notify["command"].connect ((pspec) => {
		        loop.quit ();
		});

		action_group.activate_action ("map", null);

		var source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		((!) _service).disconnect (signal_name);

		stderr.printf ("_service.command = \"%s\"\n", (!) ((!) _service).command);
		assert (strcmp ((!) ((!) _service).command, "'gucharmap '") == 0);
	}

	public void test_activate_keyboard_layout_chart () {
		try {
			var current = 1;
			var sources = "[('xkb', 'us'), ('xkb', 'ca+eng'), ('xkb', 'epo'), ('ibus', 'pinyin')]";
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources current $current");
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources sources \"$sources\"");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		var action_group = DBusActionGroup.get ((!) _connection,
		                                        "com.canonical.indicator.keyboard",
		                                        "/com/canonical/indicator/keyboard");
		var loop = new MainLoop (null, false);
		var signal_name = ((!) _service).notify["command"].connect ((pspec) => {
		        loop.quit ();
		});

		action_group.activate_action ("chart", null);

		var source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		((!) _service).disconnect (signal_name);

		stderr.printf ("_service.command = \"%s\"\n", (!) ((!) _service).command);
		assert (strcmp ((!) ((!) _service).command, "'gkbd-keyboard-display -l ca\teng'") == 0);
	}

	public void test_activate_text_entry_settings () {
		var action_group = DBusActionGroup.get ((!) _connection,
		                                        "com.canonical.indicator.keyboard",
		                                        "/com/canonical/indicator/keyboard");
		var loop = new MainLoop (null, false);
		var signal_name = ((!) _service).notify["command"].connect ((pspec) => {
		        loop.quit ();
		});

		action_group.activate_action ("settings", null);

		var source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		((!) _service).disconnect (signal_name);

		stderr.printf ("_service.command = \"%s\"\n", (!) ((!) _service).command);
		assert (strcmp ((!) ((!) _service).command, "'unity-control-center region layouts'") == 0);
	}

	public void test_migration () {
		try {
			var migrated = false;
			var sources = "[('xkb', 'us')]";
			var layouts = "['us', 'ca\teng', 'epo']";
			Process.spawn_command_line_sync (@"gsettings set com.canonical.indicator.keyboard migrated $migrated");
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources sources \"$sources\"");
			Process.spawn_command_line_sync (@"gsettings set org.gnome.libgnomekbd.keyboard layouts \"$layouts\"");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		try {
			var cancellable = new Cancellable ();

			var source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { cancellable.cancel (); return true; });

			var dbus_proxy = new DBusProxy.sync ((!) _connection,
			                                     DBusProxyFlags.NONE,
			                                     null,
			                                     "org.freedesktop.DBus",
			                                     "/",
			                                     "org.freedesktop.DBus",
			                                     cancellable);

			Source.remove (source);

			if (cancellable.is_cancelled ()) {
				Test.message ("error: Unable to connect to org.freedesktop.DBus.");
				Test.fail ();
				return;
			}

			dbus_proxy.call_sync ("StartServiceByName", new Variant ("(su)", "com.canonical.indicator.keyboard", 0), DBusCallFlags.NONE, TIMEOUT_MS);
		} catch (Error error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		var loop = new MainLoop (null, false);
		Timeout.add_seconds (TIMEOUT_S, () => { loop.quit (); return false; });
		loop.run ();

		try {
			string sources;
			Process.spawn_command_line_sync ("gsettings get org.gnome.desktop.input-sources sources", out sources);
			stderr.printf ("sources = \"%s\"\n", sources);
			assert (strcmp (sources, "[('xkb', 'us'), ('xkb', 'ca+eng'), ('xkb', 'epo')]\n") == 0);
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}
	}

	public void test_no_migration () {
		try {
			var migrated = true;
			var sources = "[('xkb', 'us')]";
			var layouts = "['us', 'ca\teng', 'epo']";
			Process.spawn_command_line_sync (@"gsettings set com.canonical.indicator.keyboard migrated $migrated");
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources sources \"$sources\"");
			Process.spawn_command_line_sync (@"gsettings set org.gnome.libgnomekbd.keyboard layouts \"$layouts\"");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		try {
			var cancellable = new Cancellable ();

			var source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { cancellable.cancel (); return true; });

			var dbus_proxy = new DBusProxy.sync ((!) _connection,
			                                     DBusProxyFlags.NONE,
			                                     null,
			                                     "org.freedesktop.DBus",
			                                     "/",
			                                     "org.freedesktop.DBus",
			                                     cancellable);

			Source.remove (source);

			if (cancellable.is_cancelled ()) {
				Test.message ("error: Unable to connect to org.freedesktop.DBus.");
				Test.fail ();
				return;
			}

			dbus_proxy.call_sync ("StartServiceByName", new Variant ("(su)", "com.canonical.indicator.keyboard", 0), DBusCallFlags.NONE, TIMEOUT_MS);
		} catch (Error error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		var loop = new MainLoop (null, false);
		Timeout.add_seconds (TIMEOUT_S, () => { loop.quit (); return false; });
		loop.run ();

		try {
			string sources;
			Process.spawn_command_line_sync ("gsettings get org.gnome.desktop.input-sources sources", out sources);
			stderr.printf ("sources = \"%s\"\n", sources);
			assert (strcmp (sources, "[('xkb', 'us')]\n") == 0);
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}
	}

	public void test_update_visible () {
		bool visible;

		try {
			visible = true;
			Process.spawn_command_line_sync (@"gsettings set com.canonical.indicator.keyboard visible $visible");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		var action_group = DBusActionGroup.get ((!) _connection,
		                                        "com.canonical.indicator.keyboard",
		                                        "/com/canonical/indicator/keyboard");
		var loop = new MainLoop (null, false);
		var signal_name = action_group.action_added["indicator"].connect ((action) => {
		        loop.quit ();
		});

		action_group.list_actions ();

		var source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		action_group.disconnect (signal_name);

		var state = action_group.get_action_state ("indicator");
		assert (state.lookup ("visible", "b", out visible));
		stderr.printf ("visible = %s\n", visible ? "true" : "false");
		assert (visible);

		loop = new MainLoop (null, false);
		signal_name = action_group.action_state_changed["indicator"].connect ((action, state) => {
		        loop.quit ();
		});

		try {
			visible = false;
			Process.spawn_command_line_sync (@"gsettings set com.canonical.indicator.keyboard visible $visible");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		action_group.disconnect (signal_name);

		state = action_group.get_action_state ("indicator");
		assert (state.lookup ("visible", "b", out visible));
		stderr.printf ("visible = %s\n", visible ? "true" : "false");
		assert (!visible);

		loop = new MainLoop (null, false);
		signal_name = action_group.action_state_changed["indicator"].connect ((action, state) => {
		        loop.quit ();
		});

		try {
			visible = true;
			Process.spawn_command_line_sync (@"gsettings set com.canonical.indicator.keyboard visible $visible");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		action_group.disconnect (signal_name);

		state = action_group.get_action_state ("indicator");
		assert (state.lookup ("visible", "b", out visible));
		stderr.printf ("visible = %s\n", visible ? "true" : "false");
		assert (visible);
	}

	public void test_update_input_source () {
		try {
			var current = 0;
			var sources = "[('xkb', 'us'), ('xkb', 'ca+eng'), ('xkb', 'epo'), ('ibus', 'pinyin')]";
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources current $current");
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources sources \"$sources\"");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		var action_group = DBusActionGroup.get ((!) _connection,
		                                        "com.canonical.indicator.keyboard",
		                                        "/com/canonical/indicator/keyboard");
		var loop = new MainLoop (null, false);
		var signal_name = action_group.action_state_changed["current"].connect ((action, state) => {
		        loop.quit ();
		});

		action_group.list_actions ();

		try {
			var current = 1;
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources current $current");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		var source = Timeout.add_seconds (TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		action_group.disconnect (signal_name);

		var state = action_group.get_action_state ("current");
		var current = state.get_uint32 ();
		stderr.printf ("current = %u\n", current);
		assert (current == 1);

		try {
			string output;
			Process.spawn_command_line_sync ("gsettings get org.gnome.desktop.input-sources current", out output);
			stderr.printf ("output = \"%s\"\n", output);
			assert (strcmp (output, "uint32 1\n") == 0);
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		loop = new MainLoop (null, false);
		signal_name = action_group.action_state_changed["current"].connect ((action, state) => {
		        loop.quit ();
		});

		try {
			current = 0;
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources current $current");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		action_group.disconnect (signal_name);

		state = action_group.get_action_state ("current");
		current = state.get_uint32 ();
		stderr.printf ("current = %u\n", current);
		assert (current == 0);

		try {
			string output;
			Process.spawn_command_line_sync ("gsettings get org.gnome.desktop.input-sources current", out output);
			stderr.printf ("output = \"%s\"\n", output);
			assert (strcmp (output, "uint32 0\n") == 0);
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}
	}

	public void test_update_input_sources () {
		try {
			var current = 0;
			var sources = "[('xkb', 'us')]";
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources current $current");
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources sources \"$sources\"");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		var menu_model = DBusMenuModel.get ((!) _connection,
		                                    "com.canonical.indicator.keyboard",
		                                    "/com/canonical/indicator/keyboard/desktop");
		var loop = new MainLoop (null, false);
		var signal_name = menu_model.items_changed.connect ((position, removed, added) => {
		        loop.quit ();
		});

		menu_model.get_n_items ();

		var source = Timeout.add_seconds (TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		menu_model.disconnect (signal_name);

		var menu = menu_model.get_item_link (0, Menu.LINK_SUBMENU);
		loop = new MainLoop (null, false);
		signal_name = menu.items_changed.connect ((position, removed, added) => {
		        loop.quit ();
		});

		menu.get_n_items ();

		source = Timeout.add_seconds (TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		menu.disconnect (signal_name);

		var section = menu.get_item_link (0, Menu.LINK_SECTION);
		loop = new MainLoop (null, false);
		signal_name = section.items_changed.connect ((position, removed, added) => {
		        loop.quit ();
		});

		section.get_n_items ();

		source = Timeout.add_seconds (TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		section.disconnect (signal_name);

		string label;

		stderr.printf ("section.get_n_items () = %d\n", section.get_n_items ());
		assert (section.get_n_items () == 1);
		section.get_item_attribute (0, Menu.ATTRIBUTE_LABEL, "s", out label);
		stderr.printf ("label = \"%s\"\n", label);
		assert (strcmp (label, "English (US)") == 0);

		loop = new MainLoop (null, false);
		signal_name = section.items_changed.connect ((position, removed, added) => {
		        if (section.get_n_items () == 4) {
		                loop.quit ();
		        }
		});

		try {
			var sources = "[('xkb', 'us'), ('xkb', 'ca+eng'), ('xkb', 'epo'), ('ibus', 'pinyin')]";
			Process.spawn_command_line_sync (@"gsettings set org.gnome.desktop.input-sources sources \"$sources\"");
		} catch (SpawnError error) {
			Test.message ("error: %s", error.message);
			Test.fail ();
			return;
		}

		source = Timeout.add_seconds (LONG_TIMEOUT_S, () => { loop.quit (); return true; });
		loop.run ();
		Source.remove (source);
		section.disconnect (signal_name);

		stderr.printf ("section.get_n_items () = %d\n", section.get_n_items ());
		assert (section.get_n_items () == 4);
		section.get_item_attribute (0, Menu.ATTRIBUTE_LABEL, "s", out label);
		stderr.printf ("label = \"%s\"\n", label);
		assert (strcmp (label, "English (US)") == 0);
		section.get_item_attribute (1, Menu.ATTRIBUTE_LABEL, "s", out label);
		stderr.printf ("label = \"%s\"\n", label);
		assert (strcmp (label, "English (Canada)") == 0);
		section.get_item_attribute (2, Menu.ATTRIBUTE_LABEL, "s", out label);
		stderr.printf ("label = \"%s\"\n", label);
		assert (strcmp (label, "Esperanto") == 0);
		section.get_item_attribute (3, Menu.ATTRIBUTE_LABEL, "s", out label);
		stderr.printf ("label = \"%s\"\n", label);
		assert (label.ascii_casecmp ("Pinyin") == 0);
	}
}

public int main (string[] args) {
	display = Environment.get_variable ("DISPLAY");

	Test.init (ref args);

	Test.add_data_func ("/indicator-keyboard-service/activate-input-source", Fixture.create<Tests> (Tests.test_activate_input_source));
	Test.add_data_func ("/indicator-keyboard-service/activate-character-map", Fixture.create<Tests> (Tests.test_activate_character_map));
	Test.add_data_func ("/indicator-keyboard-service/activate-keyboard-layout-chart", Fixture.create<Tests> (Tests.test_activate_keyboard_layout_chart));
	Test.add_data_func ("/indicator-keyboard-service/activate-text-entry-settings", Fixture.create<Tests> (Tests.test_activate_text_entry_settings));
	Test.add_data_func ("/indicator-keyboard-service/migration", Fixture.create<Tests> (Tests.test_migration));
	Test.add_data_func ("/indicator-keyboard-service/no-migration", Fixture.create<Tests> (Tests.test_no_migration));
	Test.add_data_func ("/indicator-keyboard-service/update-visible", Fixture.create<Tests> (Tests.test_update_visible));
	Test.add_data_func ("/indicator-keyboard-service/update-input-source", Fixture.create<Tests> (Tests.test_update_input_source));
	Test.add_data_func ("/indicator-keyboard-service/update-input-sources", Fixture.create<Tests> (Tests.test_update_input_sources));

	return Test.run ();
}
