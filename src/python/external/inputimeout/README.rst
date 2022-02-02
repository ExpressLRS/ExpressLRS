inputimeout
===========

.. image:: https://travis-ci.org/johejo/inputimeout.svg?branch=master
    :target: https://travis-ci.org/johejo/inputimeout

.. image:: https://ci.appveyor.com/api/projects/status/2g1fbnrcoj64g8t9?svg=true
    :target: https://ci.appveyor.com/project/johejo/inputimeout

.. image:: https://img.shields.io/pypi/v/inputimeout.svg
    :target: https://pypi.python.org/pypi/inputimeout

.. image:: https://img.shields.io/github/license/mashape/apistatus.svg
    :target: https://raw.githubusercontent.com/johejo/inputimeout/master/LICENSE

.. image:: https://api.codeclimate.com/v1/badges/3d51d0efbd7b86f0b7f1/maintainability
   :target: https://codeclimate.com/github/johejo/inputimeout/maintainability
   :alt: Maintainability

.. image:: https://api.codeclimate.com/v1/badges/3d51d0efbd7b86f0b7f1/test_coverage
   :target: https://codeclimate.com/github/johejo/inputimeout/test_coverage
   :alt: Test Coverage

.. image:: https://codecov.io/gh/johejo/inputimeout/branch/master/graph/badge.svg
  :target: https://codecov.io/gh/johejo/inputimeout

Description
-----------

Multi platform standard input with timeout

Install
-------

.. code:: bash

    $ pip install inputimeout

Usage
-----

.. code:: python

    from inputimeout import inputimeout, TimeoutOccurred
    try:
        something = inputimeout(prompt='>>', timeout=5)
    except TimeoutOccurred:
        something = 'something'
    print(something)

License
-------

MIT
