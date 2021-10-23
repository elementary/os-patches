Dynamic Kernel Module System (DKMS)
==
This intention of this README is to explain how a DKMS enabled module RPM
functions and also how DKMS can be used in conjunction with tarballs which
contain a dkms.conf file within them.

The DKMS project (and any updates) can be found at: https://github.com/dell/dkms

How To Build RPM & DEB Package
--

If you want to create rpm or deb package, then you can install the dkms package in your system.

1. Install all the build dependency packages
2. Run: `make rpm` to create rpm package
3. Run: `make debs` to create deb package
4. You can find the built package in `dkms/dist/`


Installation of DKMS via RPM
--

To install DKMS itself, you simply install (or upgrade) it like any
other RPM:

`rpm -ivh dkms-<version>-<release>.noarch.rpm`

This is a prerequisite to installing DKMS-enabled module RPMs.


Installation via RPM
--

To install a DKMS enabled module RPM, you simply install it like any other RPM:

`rpm -ivh <module>-<version>-<rpmversion>.noarch.rpm`

With a DKMS enabled module RPM, most of the installation work done by the RPM
is actually handed off to DKMS within the RPM. Generally it does the following:

1. Installs module source into `/usr/src/<module>-<moduleversion>/`
2. Places a dkms.conf file into `/usr/src/<module>-<moduleversion>/`
3. Runs: `# dkms add -m <module> -v <version>`
4. Runs: `# dkms build -m <module> -v <version>`
5. Runs: `# dkms install -m <module> -v <version>`

Once the RPM installation is complete, you can use DKMS to understand which
module and which moduleversion is installed on which kernels.  This can be
accomplished via the command:

`# dkms status`

From here, you can then use the various DKMS commands (eg. add, build, install,
uninstall) to load that module onto other kernels.


Installation via DKMS Tarballs
--

DKMS is not limited to installation via RPM only.  In fact, DKMS can also
install directly from the following:

1. Generic module source tarballs which contain a dkms.conf file
2. Specially created DKMS tarballs with module source, pre-built module
   binaries and a dkms.conf file
3. Specially created DKMS tarballs with pre-built module binaries and a
   dkms.conf file
4. Manual placement of module source and dkms.conf file into
   `/usr/src/<module>-<moduleversion>/` directory

In order to load any tarball into the DKMS tree, you must use the following
command:

`# dkms ldtarball /path/to/dkms_enabled.tar.gz`

This command will first inspect the tarball to ensure that it contains a
dkms.conf configuration file for that module.  If it cannot find this file
anywhere within the archive, then the ldtarball will fail.

From here, it will place the source in the tarball into `/usr/src/<module>-<moduleversion>/`.  If source already exists in the directory, it will not
overwrite it unless the --force option is specified.  If the tarball is of type
"c" above and does not contain source, it will only continue to load the
tarball if existing module source is found in `/usr/src/<module>-<moduleversion>/`
or if the --force option is specified.

Continuing on, if the tarball is of type "b" or "c" it will then load any
pre-built binaries found within the tarball into the dkms tree, but will stop
short of installing them.  Thus, all pre-built binaries will then be of in the
*built* state when checked from the `dkms status` command.  You can then use the
`dkms install` command to install any of these binaries.

To create a tarball of type "1" above, you need only to take module source and a
dkms.conf file for that module and create a tarball from them.  Tarballs of
type *2* or type *3* are created with the `dkms mktarball` command.  To create
a type *3* tarball, you must specify the flag `--binaries-only` with the
`mktarball`.



Installation on Systems with no Module Source and/or Compiler
--

If you choose not to load module source on your system or if you choose not to
load a compiler such as gcc onto your system, DKMS can still be used to install
modules.  It does this through use of DKMS binary only tarballs as explained in
this README under tarballs of type *c*.

If your system does not have module source, loading the dkms tarball will fail
because of this.  To avoid this, use the --force flag, as such:

`# dkms ldtarball /path/to/dkms_enabled.tar.gz --force`

This will load the pre-built binaries into the dkms tree, and create the
directory `/usr/src/<module>-<moduleversion>/` which will only contain the
module's dkms.conf configuration file.  Once the tarball is loaded, you can then
use `dkms install` to install any of the pre-built modules.

Of course, since module source will not be located in your dkms tree, you will
not be able to build any modules with DKMS for this package.


Further Documentation
--

Once DKMS is installed, you can reference its man page for further information
on different DKMS options and also to understand the formatting of a module's
dkms.conf configuration file.

You may also wish to join the dkms-devel public mailing-list at
http://lists.us.dell.com/.

The DKMS project is located at: https://github.com/dell/dkms
