#!/usr/bin/python
# Example demonstrating how to use the configuration/commandline system
# for object setup.

# This parses the given config file in 'ISC' style where the sections
# represent object instances and shows how to iterate over the sections.
# Pass it the sample apt-ftparchive configuration,
# doc/examples/ftp-archive.conf
# or a bind8 config file..

import apt_pkg
import sys

ConfigFile = apt_pkg.parse_commandline(apt_pkg.config, [], sys.argv)

if len(ConfigFile) != 1:
    print "Must have exactly 1 file name"
    sys.exit(0)

Cnf = apt_pkg.Configuration()
apt_pkg.read_config_file_isc(Cnf, ConfigFile[0])

# Print the configuration space
#print "The Configuration space looks like:"
#for I in Cnf.keys():
#   print "%s \"%s\";" % (I, Cnf[I])

# bind8 config file..
if "Zone" in Cnf:
    print "Zones: ", Cnf.sub_tree("zone").list()
    for I in Cnf.list("zone"):
        SubCnf = Cnf.sub_tree(I)
        if SubCnf.find("type") == "slave":
            print "Masters for %s: %s" % (
                SubCnf.my_tag(), SubCnf.value_list("masters"))
else:
    print "Tree definitions:"
    for I in Cnf.list("tree"):
        SubCnf = Cnf.sub_tree(I)
        # This could use Find which would eliminate the possibility of
        # exceptions.
        print "Subtree %s with sections '%s' and architectures '%s'" % (
            SubCnf.my_tag(), SubCnf["Sections"], SubCnf["Architectures"])
