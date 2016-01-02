Contributing to python-apt
==========================
:Author: Julian Andres Klode <jak@debian.org>
:Release: |release|
:Date: |today|

Let's say you need a new feature, you can develop it, and you want to get it
included in python-apt. Then be sure to follow the following guidelines.

Available branches
-------------------
First of all, let's talk a bit about the git branches of python-apt. In the
following parts, we will assume that you use git to create your changes and
submit them.

Repositories
^^^^^^^^^^^^

https://anonscm.debian.org/cgit/apt/python-apt.git

    This is the official Debian repository of python-apt.
    You can clone it using git by doing::

      git clone git://anonscm.debian.org/apt/python-apt.git


    All code which will be uploaded to Debian is here.
    There are also branches for Ubuntu releases, but those may not be up-to-date.

    Branch names consist of the distribution vendor, followed by a slash,
    followed by the release of that distribution, for example: ``debian/sid``.

    The current working branch is usually pointed to by ``HEAD``, it is
    either ``debian/sid`` or ``debian/experimental``.

    If both sid and experimental are active, bug fixes are either cherry-picked from
    ``debian/experimental`` to ``debian/sid``, or a new release is cut on the sid branch
    and then merged into experimental.

    Updates to stable release branches, such as ``debian/wheezy``, are almost always
    cherry-picked or backported from the ``debian/sid`` branch.


.. highlightlang:: c

C++ Coding style
----------------
This document gives coding conventions for the C++ code comprising
the C++ extensions of Python APT.  Please see the companion
informational PEP describing style guidelines for Python code (:PEP:`8`).

Note, rules are there to be broken.  Two good reasons to break a
particular rule:

    (1) When applying the rule would make the code less readable, even
        for someone who is used to reading code that follows the rules.

    (2) To be consistent with surrounding code that also breaks it
        (maybe for historic reasons) -- although this is also an
        opportunity to clean up someone else's mess (in true XP style).

This part of the document is derived from :PEP:`7` which was written by
Guido van Rossum.


C++ dialect
^^^^^^^^^^^

- Use ISO standard C++ (the 2011 version of the standard), headers
  should also adhere to the 1998 version of the standard.

- Use C++ style // one-line comments for single-line comments.

- No compiler warnings with ``gcc -std=c++11 -Wall -Wno-write-strings``. There
  should also be no errors with ``-pedantic`` added.


Code lay-out
^^^^^^^^^^^^

- Use 3-space indents, in files that already use them. In new source files,
  that were created after this rule was introduced, use 4-space indents.

  At some point, the whole codebase may be converted to use only
  4-space indents.

- No line should be longer than 79 characters.  If this and the
  previous rule together don't give you enough room to code, your
  code is too complicated -- consider using subroutines.

- No line should end in whitespace.  If you think you need
  significant trailing whitespace, think again -- somebody's
  editor might delete it as a matter of routine.

- Function definition style: function name in column 2, outermost
  curly braces in column 1, blank line after local variable
  declarations::

    static int extra_ivars(PyTypeObject *type, PyTypeObject *base)
    {
        int t_size = PyType_BASICSIZE(type);
        int b_size = PyType_BASICSIZE(base);

        assert(t_size >= b_size); /* type smaller than base! */
        ...
        return 1;
    }

- Code structure: one space between keywords like 'if', 'for' and
  the following left paren; no spaces inside the paren; braces as
  shown::

    if (mro != NULL) {
        ...
    }
    else {
        ...
    }

- The return statement should *not* get redundant parentheses::

    return Py_None; /* correct */
    return(Py_None); /* incorrect */

- Function and macro call style: ``foo(a, b, c)`` -- no space before
  the open paren, no spaces inside the parens, no spaces before
  commas, one space after each comma.

- Always put spaces around assignment, Boolean and comparison
  operators.  In expressions using a lot of operators, add spaces
  around the outermost (lowest-priority) operators.

- Breaking long lines: if you can, break after commas in the
  outermost argument list.  Always indent continuation lines
  appropriately, e.g.::

    PyErr_Format(PyExc_TypeError,
            "cannot create '%.100s' instances",
            type->tp_name);

- When you break a long expression at a binary operator, the
  operator goes at the end of the previous line, e.g.::

    if (type->tp_dictoffset != 0 && base->tp_dictoffset == 0 &&
        type->tp_dictoffset == b_size &&
        (size_t)t_size == b_size + sizeof(PyObject *))
        return 0; /* "Forgive" adding a __dict__ only */

- Put blank lines around functions, structure definitions, and
  major sections inside functions.

- Comments go before the code they describe.

- All functions and global variables should be declared static
  unless they are to be part of a published interface


Naming conventions
^^^^^^^^^^^^^^^^^^

- Use a ``Py`` prefix for public functions; never for static
  functions.  The ``Py_`` prefix is reserved for global service
  routines like ``Py_FatalError``; specific groups of routines
  (e.g. specific object type APIs) use a longer prefix,
  e.g. ``PyString_`` for string functions.

- Public functions and variables use MixedCase with underscores,
  like this: ``PyObject_GetAttr``, ``Py_BuildValue``, ``PyExc_TypeError``.

- Internal functions and variables use lowercase with underscores, like
  this: ``hashes_get_sha1.``

- Occasionally an "internal" function has to be visible to the
  loader; we use the _Py prefix for this, e.g.: ``_PyObject_Dump``.

- Macros should have a MixedCase prefix and then use upper case,
  for example: ``PyString_AS_STRING``, ``Py_PRINT_RAW``.


Documentation Strings
^^^^^^^^^^^^^^^^^^^^^
- The first line of each function docstring should be a "signature
  line" that gives a brief synopsis of the arguments and return
  value.  For example::

    PyDoc_STRVAR(myfunction__doc__,
    "myfunction(name: str, value) -> bool\n\n"
    "Determine whether name and value make a valid pair.");

  The signature line should be formatted using the format for function
  annotations described in :PEP:`3107`, whereas the annotations shall reflect
  the name of the type (e.g. ``str``). The leading ``def`` and the trailing
  ``:`` as used for function definitions must not be included.

  Always include a blank line between the signature line and the
  text of the description.

  If the return value for the function is always ``None`` (because
  there is no meaningful return value), do not include the
  indication of the return type.

- When writing multi-line docstrings, be sure to always use
  string literal concatenation::

    PyDoc_STRVAR(myfunction__doc__,
    "myfunction(name, value) -> bool\n\n"
    "Determine whether name and value make a valid pair.");


Python Coding Style
-------------------
The coding style for all code written in python is :PEP:`8`. Exceptions from
this rule are the documentation, where code is sometimes formatted differently
to explain aspects.

When writing code, use tools like pylint, pyflakes, pychecker and pep8.py
(all available from Debian/Ubuntu) to verify that your code is
OK. Fix all the problems which seem reasonable, and mention the unfixed issues
when asking for merge.

All code must work on both Python 2 and Python 3.

Submitting your patch
---------------------
First of all, the patch you create should be based against the most current
branch of python-apt (debian/sid or debian/experimental). If it is a bugfix,
you should probably use debian/sid. If you choose the wrong branch, we will
ask you to rebase your patches against the correct one.

Once you have made your change, check that it:

    * conforms to :PEP:`8` (checked with pep8.py). It should, at least not
      introduce new errors. (and never have whitespace at end of line)
    * produces no new errors in pychecker, pyflakes and pylint (unless you
      can't fix them, but please tell so when requesting the merge, so it can
      be fixed before hitting one of the main branches).
    * does not change the behaviour of existing code in a non-compatible way.
    * works on both Python 2 and Python 3.

If your change follows all points of the checklist, you can commit it to your
repository. (You could commit it first, and check later, and then commit the
fixes, but commits should be logical and it makes no sense to have to commits
for one logical unit).

The changelog message should follow standard git format. At the end of the
message, tags understood by gbp-dch and other tags may be added. An example
commit message could be::

  apt.package: Fix blah blah

  Fix a small bug where foo is doing bar, but should be doing baz
  instead.

  Closes: #bugnumber
  LP: #ubuntu-bug-number
  Reported-By: Bug Reporter Name <email@example.com>


Once you have made all your changes,  you can run ``git format-patch``,
specifying the upstream commit or branch you want to create patches
against. Then you can either:

* report a bug against the python-apt package, attach the patches
  you created in the previous step, and tag it with 'patch'. It might also be
  a good idea to prefix the bug report with '[PATCH]'.

* send the patches via ``git send-email``.

For larger patch series, you can also publish a git branch on a
public repository and request it to be pulled.

If you choose that approach, you may want to base your patches against
the latest release, and not against some random commit, for the sake of
preserving a sane git history.

Be prepared to rebase such a branch, and close any bugs you fix in the
branch by mentioning them in the commit message using a Closes or LP
tag.


Documentation updates
---------------------
If you want to update the documentation, please follow the procedure as written
above. You can send your content in plain text, but reStructuredText is the
preferred format. I (Julian Andres Klode) will review your patch and include
it.

.. highlightlang:: sh

Example patch session
----------------------
In the following example, we edit a file, create a patch (an enhanced
patch), and report a wishlist bug with this patch against the python-apt
package::

    user@ pc:~$ git clone git://anonscm.debian.org/apt/python-apt.git
    user@pc:~$ cd python-apt
    user@pc:~/python-apt$ editor FILES
    user@pc:~/python-apt$ pep8 FILES # PEP 8 check, see above.
    user@pc:~/python-apt$ pylint -e FILES # Check with pylint
    user@pc:~/python-apt$ pyflakes FILES  # Check with pyflakes
    user@pc:~/python-apt$ pychecker FILES # Check with pychecker
    user@pc:~/python-apt$ git commit -p
    user@pc:~/python-apt$ git format-patch origin/HEAD
    user@pc:~/python-apt$ reportbug --severity=wishlist --tag=patch --attach=<patch> ... python-apt

You may also send the patches to the mailing list instead of
reporting the bug::

    user@pc:~/python-apt$ git send-email --to=deity@lists.debian.org <patches created by format-patch>

You can even push your changes to your own repository and request
a pull request.
