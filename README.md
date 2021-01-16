malcontent
==========

malcontent implements support for restricting the type of content accessible to
non-administrator accounts on a Linux system. Typically, when this is
used, a non-administrator account will be for a child using the system; and the
administrator accounts will be for the parents; and the content being filtered
will be apps which are not suitable for the child to use, due to (for example)
being too violent.

It provides an
[accounts-service](https://gitlab.freedesktop.org/accountsservice/accountsservice)
vendor extension for storing an app filter to
restrict the child’s access to certain applications; a simple library for
accessing and applying the app filter; and a UI program (`malcontent-control`)
for viewing and changing the parental controls settings on users.

The parental controls policy is stored in `/var/lib/AccountsService/users/${user}`,
which is a key file readable and writable only by the accounts-service daemon.
Access to the data is mediated through accounts-service’s D-Bus interface, which
libmalcontent is a client library for.

All the library APIs are currently unstable and are likely to change wildly.

Two kinds of policy are currently supported:
 * A filter specifying whether installed applications are allowed to be run;
   this is typically set up to restrict access to a limited set of
   already-installed applications — but it can be set up to only allow access
   to a fixed list of applications and deny access to all others.
   Applications which are not currently installed are not subject to this
   filter.
 * A set of mappings from [OARS categories](https://hughsie.github.io/oars/) to
   the maximum ratings for those categories which are permissible for a user to
   install apps with. For example, a mapping of `violence-realistic=mild` would
   prevent any applications containing more than ‘mild’ violence from being
   installed. Applications which are already installed are not subject to this
   filter.

Additional policies may be added in future, such as filtering by content type
or limiting the amount of time a user is allowed to use the system for.

Any application or service which provides the user with access to content which
should be parentally filtered is responsible for querying the user’s parental
controls filter and refusing to provide the content if not permitted by the
filter. This could mean refusing to launch a flatpak app, hiding a search
result in gnome-shell, or hiding an app in gnome-software because of its high
OARS rating.

A sufficiently technically advanced user may always work around these parental
controls. malcontent is not a mandatory access control (MAC) system like
AppArmor or SELinux. However, its correct use by applications should provide
enough of an obstacle to prevent users easily or accidentally having access to
content which they shouldn’t.

Development principles
---

malcontent exists to make it easier for parents to responsibly set limits on
what their children can do on the computer (similarly, for other carer/caree
relationships). It should provide guidance on, and nudge parents towards,
following the best practice for discussing, choosing and setting restrictions on
their child’s computer use.

Restrictions should be supported which allow giving children access to material
which is appropriate to their age/stage of development, but which can restrict
access to other material so they are not overwhelmed with too many, or too
complex, ideas for their stage of development. Similarly, restrictions should be
supported to control unsupervised communication with unknown people online, as
being coerced or bullied is one of the more common harms for children online.

Best practices should be research and evidence-based, and supported by large
international or national childrens’ organisations or charities. They may change
over time.

malcontent is a tool to help with a social problem. In the hands of a bad
parent, it can be used to do bad things. Features in malcontent should be
designed with that in mind, such that if they are used maliciously, they should
not give the parent radically more power over their child than they would have
had without malcontent.

Example usage
---

malcontent ships a `malcontent-client` application which can be used to get and
set parental controls policies for users.

```
$ # This sets the parental controls policy for user ‘philip’ to allow no \\
    installation of apps with anything more than ‘none’ for realistic violence, \\
    and to blocklist running the org.freedesktop.Bustle flatpak:
$ malcontent-client set philip \\
    violence-realistic=none \\
    app/org.freedesktop.Bustle/x86_64/stable
App filter for user 1000 set
```

With that policy in place, other applications which are aware of malcontent will
apply the policy:

```
$ flatpak run org.freedesktop.Bustle
error: Running app/org.freedesktop.Bustle/x86_64/stable is not allowed by the policy set by your administrator
```

Development
-----------

When developing malcontent, you should be able to run an uninstalled version of
`malcontent-client` or `malcontent-control`, as long as the polkit files from
`accounts-service/` and `malcontent-control/org.freedesktop.MalcontentControl.policy.in`
have been installed system-wide (typically under `/usr/share/polkit-1`) where
your system copy of polkitd can find them.

Dependencies
------------

 * accounts-service
 * appstream-glib
 * dbus-daemon
 * flatpak
 * gio-2.0 ≥ 2.60
 * gio-unix-2.0 ≥ 2.60
 * glib-2.0 ≥ 2.60
 * gobject-2.0 ≥ 2.60
 * gtk+-3.0
 * polkit-gobject-1

Licensing
---------

All code in the libraries in this project is licensed under LGPL-2.1+. Code in the
`malcontent-control` application is licensed under GPL-2.0+. See `COPYING` and the
copyright headers in individual files for more details.

Bugs
----

Bug reports and patches should be filed in
[GitLab](https://gitlab.freedesktop.org/pwithnall/malcontent).
