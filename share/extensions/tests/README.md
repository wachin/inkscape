# Inkscape Extension Tests

This folder contains tests for the Inkscape extensions and libraries in this
repo.

Pytest and Pytest-Coverage are required to run tests.   Usually the best way to install it is:

More info here: https://docs.pytest.org/en/latest/getting-started.html

```shell
$ # Python 2
$ pip install pytest pytest-cov

$ # Python 3
$ pip3 install pytest pytest-cov
```

To run all tests:

```shell
# In the top-level directory of the extensions repo:
$ python2 -m pytest
$ python3 -m pytest
```

To run the tests in a specific file (in this case,
`tests/test_color_blackandwhite.py`):

```shell
# In the top-level directory of the extensions repo:
$ python2 -m pytest tests/test_color_blackandwhite.py
$ python3 -m pytest tests/test_color_blackandwhite.py
```
