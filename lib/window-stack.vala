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

public struct WindowInfo {

	public uint window_id;
	public string app_id;
	public bool focused;
	public uint stage;
}

[DBus (name="com.canonical.Unity.WindowStack")]
public interface WindowStack : Object {

	public abstract string get_app_id_from_pid (uint pid) throws IOError;
	public abstract string[] get_window_properties (uint window_id, string app_id, string[] property_names) throws IOError;
	public abstract WindowInfo[] get_window_stack () throws IOError;

	public signal void focused_window_changed (uint window_id, string app_id, uint stage);
	public signal void window_created (uint window_id, string app_id);
	public signal void window_destroyed (uint window_id, string app_id);
}
