public class Dist : GLib.Object {
    public string name;
    Gee.LinkedList<string> packages;
    GLib.File dist_folder;
    public Dist (string name, File src_list, File pkg_to_import) {
        this.name = name;

        packages = new Gee.LinkedList<string> ();
        try {
            var dis = new DataInputStream (pkg_to_import.read ());
            string line;

            while ((line = dis.read_line ()) != null) {
                packages.add (line);
            }
        } catch (Error e) {
            critical (e.message);
        }

        try {
            dist_folder = main_folder.get_child ("dists").get_child (name);
            if (!dist_folder.query_exists ()) {
                dist_folder.make_directory_with_parents ();
            }
            src_list.copy (dist_folder.get_child ("sources.list"), GLib.FileCopyFlags.OVERWRITE);
        } catch (Error e) {
            critical (e.message);
        }
    }

    public void setup_apt () {
        // Prepare all the required folders for a "sandboxed" apt
        try {
            var apt_folder = dist_folder.get_child ("apt");
            if (!apt_folder.query_exists ()) {
                apt_folder.make_directory_with_parents ();
            }

            var dpkg_folder = dist_folder.get_child ("dpkg");
            if (!dpkg_folder.query_exists ()) {
                dpkg_folder.make_directory_with_parents ();
            }

            var dpkg_status = dpkg_folder.get_child ("status");
            if (!dpkg_status.query_exists ()) {
                dpkg_status.create (GLib.FileCreateFlags.PRIVATE);
            }

            var apt_state_folder = dist_folder.get_child ("apt_state");
            if (!apt_state_folder.query_exists ()) {
                apt_state_folder.make_directory_with_parents ();
            }
        } catch (Error e) {
            critical (e.message);
        }

        // Updating the "sandboxed" apt
        string[] apt_options = {
            "-o",
            "Dir::Etc::SourceList=%s".printf (dist_folder.get_child ("sources.list").get_path ()),
            "-o",
            "Dir::Etc::SourceParts=None",
            "-o",
            "Dir::Etc::Main=None",
            "-o",
            "Dir::Etc::Parts=None",
            "-o",
            "Dir::Etc::Preferences=None",
            "-o",
            "Dir::Etc::PreferencesParts=None",
            "-o",
            "Dir::Cache=%s/apt".printf (dist_folder.get_path ()),
            "-o",
            "Dir::State=%s/apt_state".printf (dist_folder.get_path ()),
            "-o",
            "Dir::State::Status=%s/dpkg/status".printf (dist_folder.get_path ())
        };

        string[] spawn_env = Environ.get ();
        string ls_stdout;
        string ls_stderr;
        int ls_status;
        try {
            string[] spawn_args = {"apt"};
            foreach (var option in apt_options) {
                spawn_args += option;
            }

            spawn_args += "update";
            Process.spawn_sync ("/",
                spawn_args,
                spawn_env,
                SpawnFlags.SEARCH_PATH,
                null,
                out ls_stdout,
                out ls_stderr,
                out ls_status);
        } catch (Error e) {
            critical (e.message);
        }

        foreach (var package in packages) {
            string[] spawn_args = {"apt-cache"};
            foreach (var option in apt_options) {
                spawn_args += option;
            }

            spawn_args += "showsrc";
            spawn_args += package;
            Process.spawn_sync ("/",
                spawn_args,
                spawn_env,
                SpawnFlags.SEARCH_PATH,
                null,
                out ls_stdout,
                out ls_stderr,
                out ls_status
            );

            if (ls_status != 0) {
                critical ("%s", ls_stderr);
                continue;
            }

            var lines = ls_stdout.split ("\n");
            var available_versions = new Gee.LinkedList<string> ();
            foreach (var line in lines) {
                if (line.has_prefix ("Version: ")) {
                    available_versions.add (line.replace ("Version: ", ""));
                }
            }

            string greatest_version = available_versions.first ();
            foreach (var version in available_versions) {
                if (version == greatest_version) {
                    continue;
                }

                Process.spawn_sync ("/",
                    {"dpkg", "--compare-versions", version, ">>", greatest_version},
                    spawn_env,
                    SpawnFlags.SEARCH_PATH,
                    null,
                    out ls_stdout,
                    out ls_stderr,
                    out ls_status
                );

                if (ls_status == 0) {
                    greatest_version = version;
                }
            }

            if ("-elementary" in greatest_version) {
                critical ("looks like the version of %s you're trying to import is already patched.", package);
            }

            bool branch_need_update = true;
            string branch_name = "%s-%s".printf (name, package);
            if (check_branch_exists (branch_name)) {
                
            } else {
                create_branch (branch_name);
            }
        }
    }

    
}
