Geoclue: The Geoinformation Service
===================================

Geoclue is a D-Bus geoinformation service. The goal of the Geoclue project
is to make creating location-aware applications as simple as possible.

Geoclue is Free Software, licensed under GNU GPLv2+.

Geoclue comprises the following functionalities : 
- WiFi-based geolocation (accuracy: in meters)
- GPS(A) receivers (accuracy: in centimeters)
- GPS of other devices on the local network, e.g smartphones (accuracy: 
  in centimeters)
- 3G modems (accuracy: in kilometers, unless modem has GPS)
- GeoIP (accuracy: city-level)

WiFi-based geolocation makes use of 
[Mozilla Location Service](https://wiki.mozilla.org/CloudServices/Location). 

If geoclue is unable to find you, you can easily fix that by installing 
and running a 
[simple app](https://wiki.mozilla.org/CloudServices/Location#Contributing) on 
your phone. For using phone GPS, you'll need to install the latest version of 
[GeoclueShare app](https://github.com/ankitstarski/GeoclueShare/releases)
on your phone (currently, this is supported only on Android devices).

Geoclue was also used for (reverse-)geocoding but that functionality has 
been dropped in favour of the 
[geocode-glib library](http://ftp.gnome.org/pub/GNOME/sources/geocode-glib/).

# History
Geoclue was started during the GNOME Summit 2006 in Boston. At least 
Keith Preston and Tuomas Kuosmanen can be blamed. There was a total rewrite 
after version 0.9. 

Use tag "0.9" (as in git checkout 0.9) if you need the old code.

There was yet another rewrite that we call geoclue2. The first version to 
introduce the re-write was version 1.99.

# Communication and Contribution

- Discussions take place on the 
[GNOME Discourse](https://discourse.gnome.org/c/platform).
- The IRC chat for geoclue is on __#gnome-maps__ at irc.gimp.org .
- Issues are tracked on 
[Gitlab](https://gitlab.freedesktop.org/geoclue/geoclue/issues).

# Get Source Code
The source code is available as a tar-ball and in a Git repository.

For latest release tarballs, use the `Download` option of Gitlab on the 
[tag of your choice](https://gitlab.freedesktop.org/geoclue/geoclue/tags/).

Older (than 2.4.13) releases are available 
[here](http://www.freedesktop.org/software/geoclue/releases/2.4/).

Git repository for Geoclue: https://gitlab.freedesktop.org/geoclue/geoclue
  
# Building Geoclue

The guidelines for building geoclue have been documented 
[here](https://gitlab.freedesktop.org/geoclue/geoclue/blob/master/HACKING.md). 

# Using Geoclue in an application
 
- __D-Bus API__: The documentation for using geoclue with D-Bus API is 
[here](http://www.freedesktop.org/software/geoclue/docs/).
- __Libgeoclue API documentation__:  The documentation is available 
[here](https://www.freedesktop.org/software/geoclue/docs/libgeoclue/).
- __C user application__: 
[Here](https://gitlab.freedesktop.org/geoclue/geoclue/blob/master/demo/where-am-i.c)
is an example showing a C application that uses 
geoclue to locate its user. 
