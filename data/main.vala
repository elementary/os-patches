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

int main (string[] args) {
	var force = false;
	var width = 22.0;
	var height = 22.0;
	var icon_width = 20.0;
	var icon_height = 20.0;
	var radius = 2.0;
	var colour = "black";
	var font = "Ubuntu";
	var weight = 500;
	var layout_size = 12;
	var subscript_size = 8;
	string? output_path = null;
	string? no_subscript_path = null;
	string? with_subscript_path = null;

	OptionEntry[] options = new OptionEntry[15];
	options[0] = { "force", 'f', 0, OptionArg.NONE, ref force, "Overwrite existing files" };
	options[1] = { "width", 'w', 0, OptionArg.DOUBLE, ref width, "Template width", "DOUBLE" };
	options[2] = { "height", 'h', 0, OptionArg.DOUBLE, ref height, "Template height", "DOUBLE" };
	options[3] = { "icon-width", 'W', 0, OptionArg.DOUBLE, ref icon_width, "Icon width", "DOUBLE" };
	options[4] = { "icon-height", 'H', 0, OptionArg.DOUBLE, ref icon_height, "Icon height", "DOUBLE" };
	options[5] = { "radius", 'r', 0, OptionArg.DOUBLE, ref radius, "Icon radius", "DOUBLE" };
	options[6] = { "colour", 'c', 0, OptionArg.STRING, ref colour, "Icon colour", "COLOUR" };
	options[7] = { "font", 'F', 0, OptionArg.STRING, ref font, "Font family", "NAME" };
	options[8] = { "weight", 'G', 0, OptionArg.INT, ref weight, "Font weight (100 to 1000)", "INT" };
	options[9] = { "layout-size", 's', 0, OptionArg.INT, ref layout_size, "Layout font size", "INT" };
	options[10] = { "subscript-size", 'S', 0, OptionArg.INT, ref subscript_size, "Subscript font size", "INT" };
	options[11] = { "output", 'o', 0, OptionArg.FILENAME, ref output_path, "Output directory", "PATH" };
	options[12] = { "no-subscript", 'i', 0, OptionArg.FILENAME, ref no_subscript_path, "Icon template", "PATH" };
	options[13] = { "with-subscript", 'I', 0, OptionArg.FILENAME, ref with_subscript_path, "Subscript icon template", "PATH" };
	options[14] = { };

	try {
		var context = new OptionContext ("- generate keyboard layout icons");
		context.add_main_entries (options, null);
		context.set_help_enabled (true);
		context.parse (ref args);
	} catch (OptionError error) {
		GLib.error ("error: %s", error.message);
	}

	if (no_subscript_path == null && with_subscript_path == null) {
		error ("error: No icon template");
	} else if (no_subscript_path == null) {
		no_subscript_path = with_subscript_path;
	} else if (with_subscript_path == null) {
		with_subscript_path = no_subscript_path;
	}

	if (output_path != null) {
		var file = File.new_for_path ((!) output_path);

		if (!file.query_exists (null)) {
			try {
				file.make_directory_with_parents (null);
			} catch (Error error) {
				GLib.error ("error: %s", error.message);
			}
		}
	} else {
		output_path = ".";
	}

	Gtk.init (ref args);

	var info = new Gnome.XkbInfo ();
	var layouts = info.get_all_layouts ();
	var occurrences = new Gee.HashMap<string, int> ();

	layouts.foreach ((name) => {
		string? short_name;

		info.get_layout_info (name, null, out short_name, null, null);

		var abbreviation = abbreviate (short_name);
		var has_abbreviation = abbreviation != null && ((!) abbreviation).get_char () != '\0';

		if (has_abbreviation) {
			if (!occurrences.has_key ((!) abbreviation)) {
				occurrences[(!) abbreviation] = 1;
			} else {
				occurrences[(!) abbreviation] = occurrences[(!) abbreviation] + 1;
			}
		}
	});

	string no_subscript_data;
	string with_subscript_data;

	try {
		uint8[] contents;

		var icon_x = 0.5 * (width - icon_width);
		var icon_y = 0.5 * (height - icon_height);
		var layout_font = @"font-family:$font;font-weight:$weight;font-size:$layout_size";
		var subscript_font = @"font-family:$font;font-weight:$weight;font-size:$subscript_size";

		File.new_for_path ((!) no_subscript_path).load_contents (null, out contents, null);
		no_subscript_data = (string) contents;
		no_subscript_data = no_subscript_data.replace ("@WIDTH@", @"$width");
		no_subscript_data = no_subscript_data.replace ("@HEIGHT@", @"$height");
		no_subscript_data = no_subscript_data.replace ("@ICON_X@", @"$icon_x");
		no_subscript_data = no_subscript_data.replace ("@ICON_Y@", @"$icon_y");
		no_subscript_data = no_subscript_data.replace ("@ICON_WIDTH@", @"$icon_width");
		no_subscript_data = no_subscript_data.replace ("@ICON_HEIGHT@", @"$icon_height");
		no_subscript_data = no_subscript_data.replace ("@RADIUS@", @"$radius");
		no_subscript_data = no_subscript_data.replace ("@COLOUR@", colour);
		no_subscript_data = no_subscript_data.replace ("@LAYOUT_FONT@", layout_font);
		no_subscript_data = no_subscript_data.replace ("@SUBSCRIPT_FONT@", subscript_font);

		File.new_for_path ((!) with_subscript_path).load_contents (null, out contents, null);
		with_subscript_data = (string) contents;
		with_subscript_data = with_subscript_data.replace ("@WIDTH@", @"$width");
		with_subscript_data = with_subscript_data.replace ("@HEIGHT@", @"$height");
		with_subscript_data = with_subscript_data.replace ("@ICON_X@", @"$icon_x");
		with_subscript_data = with_subscript_data.replace ("@ICON_Y@", @"$icon_y");
		with_subscript_data = with_subscript_data.replace ("@ICON_WIDTH@", @"$icon_width");
		with_subscript_data = with_subscript_data.replace ("@ICON_HEIGHT@", @"$icon_height");
		with_subscript_data = with_subscript_data.replace ("@RADIUS@", @"$radius");
		with_subscript_data = with_subscript_data.replace ("@COLOUR@", colour);
		with_subscript_data = with_subscript_data.replace ("@LAYOUT_FONT@", layout_font);
		with_subscript_data = with_subscript_data.replace ("@SUBSCRIPT_FONT@", subscript_font);
	} catch (Error error) {
		GLib.error ("error: %s", error.message);
	}

	var font_map = new PangoFT2.FontMap ();
	var layout_layout = new Pango.Layout (font_map.create_context ());
	var subscript_layout = new Pango.Layout (font_map.create_context ());

	var font_description = new Pango.FontDescription ();
	font_description.set_family (font);
	font_description.set_weight ((Pango.Weight) weight);
	font_description.set_size (layout_size * Pango.SCALE);
	layout_layout.set_font_description (font_description);

	font_description = new Pango.FontDescription ();
	font_description.set_family (font);
	font_description.set_weight ((Pango.Weight) weight);
	font_description.set_size (subscript_size * Pango.SCALE);
	subscript_layout.set_font_description (font_description);

	foreach (var entry in occurrences.entries) {
		var layout = entry.key;
		var count = entry.value;
		var file = File.new_for_path (@"$((!) output_path)/indicator-keyboard-$layout.svg");

		if (force || !file.query_exists (null)) {
			int layout_width;
			int layout_height;

			layout_layout.set_text (layout, -1);
			layout_layout.get_size (out layout_width, out layout_height);
			var layout_baseline = layout_layout.get_baseline ();

			var layout_x = 0.5 * (width - 1.0 * layout_width / Pango.SCALE);
			var layout_y = 0.5 * (height - 1.0 * layout_height / Pango.SCALE) + 1.0 * layout_baseline / Pango.SCALE;

			var output_data = no_subscript_data;
			output_data = output_data.replace ("@LAYOUT@", layout);
			output_data = output_data.replace ("@LAYOUT_X@", @"$layout_x");
			output_data = output_data.replace ("@LAYOUT_Y@", @"$layout_y");
			output_data = output_data.replace ("@SUBSCRIPT@", "");
			output_data = output_data.replace ("@SUBSCRIPT_X@", "0");
			output_data = output_data.replace ("@SUBSCRIPT_Y@", "0");

			try {
				file.replace_contents (output_data.data, null, false, FileCreateFlags.REPLACE_DESTINATION, null, null);
			} catch (Error error) {
				GLib.error ("error: %s", error.message);
			}
		}

		if (count > 1) {
			int layout_width;
			int layout_height;

			layout_layout.set_text (layout, -1);
			layout_layout.get_size (out layout_width, out layout_height);
			var layout_baseline = layout_layout.get_baseline ();

			var layout_y = 0.5 * (height - 1.0 * layout_height / Pango.SCALE) + 1.0 * layout_baseline / Pango.SCALE;

			var partial_data = with_subscript_data;
			partial_data = partial_data.replace ("@LAYOUT@", layout);
			partial_data = partial_data.replace ("@LAYOUT_Y@", @"$layout_y");

			for (var i = 1; i <= count; i++) {
				file = File.new_for_path (@"$((!) output_path)/indicator-keyboard-$layout-$i.svg");

				if (force || !file.query_exists (null)) {
					var subscript = @"$i";
					int subscript_width;
					int subscript_height;

					subscript_layout.set_text (subscript, -1);
					subscript_layout.get_size (out subscript_width, out subscript_height);
					var subscript_baseline = subscript_layout.get_baseline ();

					var layout_x = 0.5 * (width - 1.0 * (layout_width + subscript_width) / Pango.SCALE);
					var subscript_x = layout_x + 1.0 * layout_width / Pango.SCALE;
					var subscript_y = layout_y - 0.5 * subscript_height / Pango.SCALE + 1.0 * subscript_baseline / Pango.SCALE;

					var output_data = partial_data;
					output_data = output_data.replace ("@LAYOUT_X@", @"$layout_x");
					output_data = output_data.replace ("@LAYOUT_Y@", @"$layout_y");
					output_data = output_data.replace ("@SUBSCRIPT@", subscript);
					output_data = output_data.replace ("@SUBSCRIPT_X@", @"$subscript_x");
					output_data = output_data.replace ("@SUBSCRIPT_Y@", @"$subscript_y");

					try {
						file.replace_contents (output_data.data, null, false, FileCreateFlags.REPLACE_DESTINATION, null, null);
					} catch (Error error) {
						GLib.error ("error: %s", error.message);
					}
				}
			}
		}
	}

	return 0;
}
