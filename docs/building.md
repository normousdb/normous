Building Normous
================

To build Normous, you will need:

* A modern C++ compiler. One of the following is required.
    * GCC 5.4.0 or newer
    * Clang 3.8 (or Apple XCode 8.3.2 Clang) or newer
    * Visual Studio 2015 Update 3 or newer (See Windows section below for details)
* On Linux and macOS, the libcurl library and header is required. MacOS includes libcurl.
    * Fedora/RHEL - dnf install libcurl-devel
    * Ubuntu/Debian - apt-get install libcurl-dev
* Python 2.7.x and Pip modules:
  * pyyaml
  * typing
  * cheetah

Normous supports the following architectures: arm64, ppc64le, s390x, and x86-64.
More detailed platform instructions can be found below.


Normous Tools
--------------

The Normous command line tools (normousdump, normousrestore, normousimport, normousexport, etc)
have been rewritten in [Go](http://golang.org/) and are no longer included in this repository.

The source for the tools is now available at [normousdb/normous-tools](https://github.com/normousdb/normous-tools).

Python Prerequisites
---------------

In order to build Normous, Python 2.7.x is required, and several Python modules. To install
the required Python modules, run:

    $ pip2 install -r etc/pip/compile-requirements.txt

Note: If the `pip2` command is not available, `pip` without a suffix may be the pip command
associated with Python 2.7.x.

SCons
---------------

For detail information about building, please see [the build manual](https://github.com/normousdb/normous/wiki/Build-normousdb-From-Source)

If you want to build everything (normousd, normous, tests, etc):

    $ python2 buildscripts/scons.py all

If you only want to build the database:

    $ python2 buildscripts/scons.py normousd

***Note***: For C++ compilers that are newer than the supported version, the compiler may issue new warnings that cause Normous to fail to build since the build system treats compiler warnings as errors. To ignore the warnings, pass the switch --disable-warnings-as-errors to scons.

    $ python2 buildscripts/scons.py normousd --disable-warnings-as-errors

To install

    $ python2 buildscripts/scons.py --prefix=/opt/normous install

Please note that prebuilt binaries are available on [github.com](https://github.com/normousdb/normous/releases) and may be the easiest way to get started.

SCons Targets
--------------

* normousd
* normouss
* normous
* core (includes normousd, normouss, normous)
* all

Debian/Ubuntu
--------------

To install dependencies on Debian or Ubuntu systems:

    # aptitude install build-essential
    # aptitude install libboost-filesystem-dev libboost-program-options-dev libboost-system-dev libboost-thread-dev

To run tests as well, you will need PyMongo:

    # aptitude install python-pymongo

FreeBSD
--------------

Install the following ports:

  * devel/libexecinfo
  * lang/clang38
  * lang/python

Optional Components if you want to use system libraries instead of the libraries included with Normous

  * archivers/snappy
  * lang/v8
  * devel/boost
  * devel/pcre

Add `CC=clang38 CXX=clang++38` to the `scons` options, when building.

OpenBSD
--------------
Install the following ports:

  * devel/libexecinfo
  * lang/gcc
  * lang/python

Special Build Notes
--------------
  * [open solaris on ec2](building.opensolaris.ec2.md)
