:mod:`apt.package` --- Classes for package handling
====================================================


.. automodule:: apt.package


The Package class
-----------------
.. autoclass:: Package
    :members:

    .. note::

        Several methods have been deprecated in version 0.7.9 of python-apt,
        please see the :class:`Version` class for the new alternatives.

The Version class
-----------------
.. autoclass:: Version
    :members:


Dependency Information
----------------------
.. autoclass:: BaseDependency
    :members:

.. class:: Dependency

    The dependency class represents a Or-Group of dependencies. It provides
    an attribute to access the :class:`BaseDependency` object for the available
    choices.

    .. attribute:: or_dependencies

        A list of :class:`BaseDependency` objects which could satisfy the
        requirement of the Or-Group.


Origin Information
-------------------
.. class:: Origin

    The :class:`Origin` class provides access to the origin of the package.
    It allows you to check the component, archive, the hostname, and even if
    this package can be trusted.

    .. attribute:: archive

        The archive (eg. unstable)

    .. attribute:: component

        The component (eg. main)

    .. attribute:: label

        The Label, as set in the Release file

    .. attribute:: origin

        The Origin, as set in the Release file

    .. attribute:: site

        The hostname of the site.

    .. attribute:: trusted

       Boolean value whether this is trustworthy. An origin can be trusted, if
       it provides a GPG-signed Release file and the GPG-key used is in the
       keyring used by apt (see apt-key).



The Record class
-----------------
.. autoclass:: Record
    :members:

    .. note::
        .. versionchanged:: 0.7.100
            This class is a subclass of :class:`collections.Mapping` when used
            in Python 2.6 or newer.

    .. describe:: record[name]

        Return the value of the field with the name *name*.

    .. describe:: name in record

        Return whether a field *name* exists in record.

    .. describe:: len(record)

        The number of fields in the record

    .. describe:: str(record)

        Display the record as a string


Examples
---------
.. code-block:: python

    import apt

    cache = apt.Cache()
    pkg = cache['python-apt'] # Access the Package object for python-apt
    print('python-apt is trusted:', pkg.candidate.origins[0].trusted)

    # Mark python-apt for install
    pkg.mark_install()

    print('python-apt is marked for install:', pkg.marked_install)

    print('python-apt is (summary):', pkg.candidate.summary)

    # Now, really install it
    cache.commit()
