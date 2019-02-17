# NormousDB

## COMPONENTS

* normousd - The database server.
* normouss - Sharding router.
* normous  - The database shell (uses interactive javascript).

## UTILITIES

  Have moved to [normousdb/normous-tools](https://github.com/normousdb/normous-tools).

## BUILDING

  See [Building](https://github.com/normousdb/normous/docs/building.md).

## RUNNING

  For command line options invoke:

    $ docker run -it normousdb --help

  To run a single server database:

    $ sudo mkdir -p ~/data/db
    $ docker run -it -v $(dirname ~/data/db):/data/db -p 27017-27019:27017-27019 --rm normousdb

    $ # The mongo javascript shell connects to localhost and test database by default:
    $ docker run -it --rm normousdb:mgmt
    > help

## DRIVERS

  At present all of the normal MongoDB drivers are compatible with NormousDB.

  Client drivers for most programming languages are available at
  https://docs.mongodb.com/manual/applications/drivers/. Use the shell
  ("normous") for administrative tasks.

## BUG REPORTS

  [Issues](https://github.com/normousdb/normous/issues/)

## PACKAGING

  Packages are created dynamically by the package.py script located in the
  buildscripts directory. This will generate RPM and Debian packages.

## DOCUMENTATION

  [Documentation](https://github.com/normousdb/normous/docs/)

## LICENSE

  Most NormousDB source files (src/mongo folder and below) are made available
  under the terms of the GNU Affero General Public License (GNU AGPLv3). See
  individual files for details.

## Thanks

  A huge thank you goes out to all the people that made MongoDB as successful as
  it has been. It's a wonderful product/project that has helped a lot of people
  get their jobs done.
