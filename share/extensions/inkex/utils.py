# coding=utf-8
#
# Copyright (C) 2010 Nick Drobchenko, nick@cnc-club.ru
# Copyright (C) 2005 Aaron Spike, aaron@ekips.org
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
"""
Basic common utility functions for calculated things
"""
from collections import OrderedDict
import os
import sys
import random
import re
import math

from argparse import ArgumentTypeError
from itertools import tee, cycle

import numpy as np

ABORT_STATUS = -5

(X, Y) = range(2)
PY3 = sys.version_info[0] == 3

# pylint: disable=line-too-long
# Taken from https://www.w3.org/Graphics/SVG/1.1/paths.html#PathDataBNF
DIGIT_REX_PART = r"[0-9]"
DIGIT_SEQUENCE_REX_PART = rf"(?:{DIGIT_REX_PART}+)"
INTEGER_CONSTANT_REX_PART = DIGIT_SEQUENCE_REX_PART
SIGN_REX_PART = r"[+-]"
EXPONENT_REX_PART = rf"(?:[eE]{SIGN_REX_PART}?{DIGIT_SEQUENCE_REX_PART})"
FRACTIONAL_CONSTANT_REX_PART = rf"(?:{DIGIT_SEQUENCE_REX_PART}?\.{DIGIT_SEQUENCE_REX_PART}|{DIGIT_SEQUENCE_REX_PART}\.)"
FLOATING_POINT_CONSTANT_REX_PART = rf"(?:{FRACTIONAL_CONSTANT_REX_PART}{EXPONENT_REX_PART}?|{DIGIT_SEQUENCE_REX_PART}{EXPONENT_REX_PART})"
NUMBER_REX = re.compile(
    rf"(?:{SIGN_REX_PART}?{FLOATING_POINT_CONSTANT_REX_PART}|{SIGN_REX_PART}?{INTEGER_CONSTANT_REX_PART})"
)
# pylint: enable=line-too-long


def _pythonpath():
    for pth in os.environ.get("PYTHONPATH", "").split(":"):
        if os.path.isdir(pth):
            yield pth


def get_user_directory():
    """Return the user directory where extensions are stored.

    .. versionadded:: 1.1"""
    if "INKSCAPE_PROFILE_DIR" in os.environ:
        return os.path.abspath(
            os.path.expanduser(
                os.path.join(os.environ["INKSCAPE_PROFILE_DIR"], "extensions")
            )
        )

    home = os.path.expanduser("~")
    for pth in _pythonpath():
        if pth.startswith(home):
            return pth
    return None


def get_inkscape_directory():
    """Return the system directory where inkscape's core is.

    .. versionadded:: 1.1"""
    for pth in _pythonpath():
        if os.path.isdir(os.path.join(pth, "inkex")):
            return pth
    raise ValueError("Unable to determine the location of Inkscape")


class KeyDict(dict):
    """
    A normal dictionary, except asking for anything not in the dictionary
    always returns the key itself. This is used for translation dictionaries.
    """

    def __getitem__(self, key):
        try:
            return super().__getitem__(key)
        except KeyError:
            return key


def parse_percent(val: str):
    """Parse strings that are either values (i.e., '3.14159') or percentages
    (i.e. '75%') to a float.

    .. versionadded:: 1.2"""
    val = val.strip()
    if val.endswith("%"):
        return float(val[:-1]) / 100
    return float(val)


def Boolean(value):
    """ArgParser function to turn a boolean string into a python boolean"""
    if value.upper() == "TRUE":
        return True
    if value.upper() == "FALSE":
        return False
    return None


def to_bytes(content):
    """Ensures the content is bytes

    .. versionadded:: 1.1"""
    if isinstance(content, bytes):
        return content
    return str(content).encode("utf8")


def debug(what):
    """Print debug message if debugging is switched on"""
    errormsg(what)
    return what


def do_nothing(*args, **kwargs):  # pylint: disable=unused-argument
    """A blank function to do nothing

    .. versionadded:: 1.1"""


def errormsg(msg):
    """Intended for end-user-visible error messages.

    (Currently just writes to stderr with an appended newline, but could do
    something better in future: e.g. could add markup to distinguish error
    messages from status messages or debugging output.)

    Note that this should always be combined with translation::

      import inkex
      ...
      inkex.errormsg(_("This extension requires two selected paths."))
    """
    try:
        sys.stderr.write(msg)
    except TypeError:
        sys.stderr.write(str(msg))
    except UnicodeEncodeError:
        # Python 2:
        # Fallback for cases where sys.stderr.encoding is not Unicode.
        # Python 3:
        # This will not work as write() does not accept byte strings, but AFAIK
        # we should never reach this point as the default error handler is
        # 'backslashreplace'.

        # This will be None by default if stderr is piped, so use ASCII as a
        # last resort.
        encoding = sys.stderr.encoding or "ascii"
        sys.stderr.write(msg.encode(encoding, "backslashreplace"))

    # Write '\n' separately to avoid dealing with different string types.
    sys.stderr.write("\n")


class AbortExtension(Exception):
    """Raised to print a message to the user without backtrace"""


class DependencyError(NotImplementedError):
    """Raised when we need an external python module that isn't available"""


class FragmentError(Exception):
    """Raised when trying to do rooty things on an xml fragment"""


def to(kind):  # pylint: disable=invalid-name
    """
    Decorator which will turn a generator into a list, tuple or other object type.
    """

    def _inner(call):
        def _outer(*args, **kw):
            return kind(call(*args, **kw))

        return _outer

    return _inner


def strargs(string, kind=float):
    """Returns a list of floats from a string

    .. versionchanged:: 1.1
        also splits at -(minus) signs by adding a space in front of the - sign

    .. versionchanged:: 1.2
        Full support for the `SVG Path data BNF
        <https://www.w3.org/Graphics/SVG/1.1/paths.html#PathDataBNF>`_
    """
    return [kind(val) for val in NUMBER_REX.findall(string)]


class classproperty:  # pylint: disable=invalid-name, too-few-public-methods
    """Combine classmethod and property decorators"""

    def __init__(self, func):
        self.func = func

    def __get__(self, obj, owner):
        return self.func(owner)


def filename_arg(name):
    """Existing file to read or option used in script arguments"""
    filename = os.path.abspath(os.path.expanduser(name))
    if not os.path.isfile(filename):
        raise ArgumentTypeError(f"File not found: {name}")
    return filename


def pairwise(iterable, start=True):
    "Iterate over a list with overlapping pairs (see itertools recipes)"
    first, then = tee(iterable)
    starter = [(None, next(then, None))]
    if not start:
        starter = []
    return starter + list(zip(first, then))


def circular_pairwise(l):
    """Iterate over a list with overlapping pairs in a periodic way, i.e.
    [1, 2, 3] -> [(1, 2), (2, 3), (3, 1)]

    ..versionadded:: 1.3.1"""
    second = cycle(l)
    next(second)
    return zip(l, second)


EVAL_GLOBALS = {}
EVAL_GLOBALS.update(random.__dict__)
EVAL_GLOBALS.update(math.__dict__)


def math_eval(function, variable="x"):
    """Interpret a function string. All functions from math and random may be used.

    .. versionadded:: 1.1

    Returns:
        a lambda expression if sucessful; otherwise None.
    """
    try:
        if function != "":
            return eval(
                f"lambda {variable}: " + (function.strip('"') or "t"), EVAL_GLOBALS, {}
            )
    # handle incomplete/invalid function gracefully
    except SyntaxError:
        pass
    return None


def is_number(string):
    """Checks if a value is a number

    .. versionadded:: 1.2"""
    try:
        float(string)
        return True
    except ValueError:
        return False


def rational_limit(f: np.poly1d, g: np.poly1d, t0):
    """Computes the limit of the rational function (f/g)(t)
    as t approaches t0.

    .. versionadded:: 1.4"""
    assert g != np.poly1d([0])
    if g(t0) != 0:
        return f(t0) / g(t0)
    elif f(t0) == 0:
        return rational_limit(f.deriv(), g.deriv(), t0)
    else:
        raise ValueError("Limit does not exist.")


def callback_method(func):
    def notify(self, *args, **kwargs):
        result = func(self, *args, **kwargs)
        self._callback()
        return result

    return notify


class NotifyList(list):
    """A list that calls a callback after it is modified
    (to notify a parent about the modification).
    Modified from https://stackoverflow.com/a/13259435/3298143

    .. versionadded:: 1.4"""

    extend = callback_method(list.extend)
    append = callback_method(list.append)
    remove = callback_method(list.remove)
    pop = callback_method(list.pop)
    __delitem__ = callback_method(list.__delitem__)
    __setitem__ = callback_method(list.__setitem__)
    __iadd__ = callback_method(list.__iadd__)
    __imul__ = callback_method(list.__imul__)

    def __getitem__(self, item):
        """Ensure that slicing returns a list of the same datatype"""
        if isinstance(item, slice):
            return self.__class__(list.__getitem__(self, item))
        return list.__getitem__(self, item)

    def __init__(self, *args, callback=None):
        self.callback = None
        list.__init__(self, *args)
        self.callback = callback

    def _callback(self):
        if self.callback is not None:
            self.callback(self)

    def toggle(self, value):
        """If exists, remove it, if not, add it"""
        value = str(value)
        if value in self:
            return self.remove(value)
        return self.append(value)


class NotifyOrderedDict(OrderedDict):
    """An OrderedDict that notifies a callback after a value is changed

    .. versionadded:: 1.4"""

    clear = callback_method(OrderedDict.clear)
    popitem = callback_method(OrderedDict.popitem)
    update = callback_method(OrderedDict.update)
    setdefault = callback_method(OrderedDict.setdefault)
    __setitem__ = callback_method(OrderedDict.__setitem__)
    __delitem__ = callback_method(OrderedDict.__delitem__)

    def __init__(self, *args, callback=None, **kwargs):
        self.callback = None
        super().__init__(*args, **kwargs)
        self.callback = callback

    def _callback(self):
        if self.callback is not None:
            self.callback(self)

    def pop(self, key, default=None):
        super().pop(key, default)
        # On Python < 3.11, pop internally calls __delitem__.
        # This does not happen in 3.11. To avoid
        # calling the callback twice, we need to check the Python version.
        if sys.version_info >= (3, 11):
            if self.callback is not None:
                self.callback(self)
