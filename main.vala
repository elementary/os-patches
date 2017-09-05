const string REPO_SSH = "git@github.com:elementary/ubuntu-package-imports.git";

static GLib.File main_folder;
static Ggit.Repository repository;

int main (string[] args) {
    message ("Cloning the repository");
    Ggit.init ();
    try {
        main_folder = File.new_for_path (GLib.Environment.get_tmp_dir ()).get_child ("io.elementary.ubuntu-package-imports");
        var location = main_folder.get_child ("repository");
        if (!location.query_exists ()) {
            location.make_directory_with_parents ();
            var fetch_options = new Ggit.FetchOptions ();
            fetch_options.set_remote_callbacks (new Cloner ());
            var clone_options = new Ggit.CloneOptions ();
            clone_options.set_fetch_options (fetch_options);
            repository = Ggit.Repository.clone (REPO_SSH, location, clone_options);
        } else {
            repository = Ggit.Repository.open (location);
        }

        message ("Going to the `import-lists` branch");
        var dists = get_dists ();
        foreach (var dist in dists) {
            message ("Working on %s", dist.name);
            dist.setup_apt ();
        }
    } catch (Error e) {
        critical (e.message);
    }

    return 0;
}

public Gee.LinkedList<Dist> get_dists () {
    /*var enumerator = repository.enumerate_branches (Ggit.BranchType.REMOTE);
    while (enumerator.next ()) {
        warning (enumerator.get ().get_name ());
    }*/
    var dists = new Gee.LinkedList<Dist> ();
    try {
        switch_to_branch ("import-lists");
        var location = repository.workdir;
        var children = location.enumerate_children (GLib.FileAttribute.STANDARD_NAME, GLib.FileQueryInfoFlags.NOFOLLOW_SYMLINKS);
        GLib.FileInfo info = null;
        while ((info = children.next_file ()) != null) {
            if (info.get_file_type () == FileType.DIRECTORY) {
                var name = info.get_name ();
                if (name.has_prefix (".")) {
                    continue;
                }

                var subdir = location.resolve_relative_path (name);
                var source_list_file = subdir.get_child ("sources.list");
                var packages_file = subdir.get_child ("packages_to_import");
                var dist = new Dist (name, source_list_file, packages_file);
                dists.add (dist);
            }
        }
    } catch (Error e) {
        critical (e.message);
    }

    return dists;
}

public void switch_to_branch (string branch_name) throws GLib.Error {
    try {
        var branch = repository.lookup_branch ("origin/%s".printf (branch_name), Ggit.BranchType.REMOTE);
        var commit = branch.resolve ().lookup () as Ggit.Commit;
        var tree = commit.get_tree ();

        var opts = new Ggit.CheckoutOptions ();
        opts.set_strategy (Ggit.CheckoutStrategy.FORCE);

        repository.checkout_tree (tree, opts);
    } catch (Error e) {
        throw e;
    }
}

public bool check_branch_exists (string branch_name) {
    try {
        var branch = repository.lookup_branch ("origin/%s".printf (branch_name), Ggit.BranchType.REMOTE);
        if (branch == null) {
            return false;
        } else {
            return true;
        }
    } catch (Error e) {
        return false;
    }
}

public void create_branch (string branch_name) {
    
}

public class Cloner : Ggit.RemoteCallbacks {
    public Cloner () {
        
    }

    public override Ggit.Cred? credentials (string url, string? username_from_url, Ggit.Credtype allowed_types) throws Error {
        return new Ggit.CredSshKeyFromAgent (username_from_url);
    }
}
