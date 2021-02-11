# coding=utf-8
"""
Unit test file for ../inkex.py
"""
# Revision history:
#   * 2012-01-27 (jazzynico): check errormsg function.
#
from __future__ import absolute_import, print_function

from argparse import ArgumentTypeError

import pytest

from inkex.utils import addNS, debug, errormsg, filename_arg, Boolean, to, strargs


class TestInkexBasic(object):
    """Test basic utiltiies of inkex"""

    def test_boolean(self):
        """Inkscape boolean input"""
        assert Boolean('TRUE') is True
        assert Boolean('true') is True
        assert Boolean('True') is True
        assert Boolean('FALSE') is False
        assert Boolean('false') is False
        assert Boolean('False') is False
        assert Boolean('Banana') is None

    def test_debug(self, capsys):
        """Debug messages go to stderr"""
        debug("Hello World")
        assert capsys.readouterr().err == 'Hello World\n'

    def test_to(self):
        """Decorator for generators"""

        @to(list)
        def mylist(a, b, c):
            """Yield as a list"""
            yield a
            yield c
            yield b

        assert isinstance(mylist(1, 2, 3), list)
        assert mylist(1, 2, 3) == [1, 3, 2]

        @to(dict)
        def mydict(a, b, c):
            """Yield as a dictionary"""
            yield ('age', a)
            yield ('name', c)
            yield ('home', b)

        assert isinstance(mydict(1, 2, 3), dict)
        assert mydict(1, 2, 3) == {'age': 1, 'name': 3, 'home': 2}

    def test_filename(self):
        """Filename argument input"""
        assert filename_arg(__file__) == __file__
        with pytest.raises(ArgumentTypeError):
            filename_arg('doesntexist.txt')

    def test_add_ns(self):
        """Test addNS function"""
        assert addNS('inkscape:foo') == '{http://www.inkscape.org/namespaces/inkscape}foo'
        assert addNS('bar', 'inkscape') == '{http://www.inkscape.org/namespaces/inkscape}bar'
        assert addNS('url', 'rdf') == '{http://www.w3.org/1999/02/22-rdf-syntax-ns#}url'
        assert addNS('{http://www.inkscape.org/namespaces/inkscape}bar') == '{http://www.inkscape.org/namespaces/inkscape}bar'
        assert addNS('http://www.inkscape.org/namespaces/inkscape:bar') == '{http://www.inkscape.org/namespaces/inkscape}bar'
        assert addNS('car', 'http://www.inkscape.org/namespaces/inkscape') == '{http://www.inkscape.org/namespaces/inkscape}car'
        assert addNS('{http://www.inkscape.org/namespaces/inkscape}bar', 'rdf') == '{http://www.w3.org/1999/02/22-rdf-syntax-ns#}bar'

    def test_strargs(self):
        """Test strargs function"""
        assert strargs('1.0 2.0 3.0 4.0') == [1.0, 2.0, 3.0, 4.0]
        assert strargs('1 -2 3 -4') == [1.0, -2.0, 3.0, -4.0]
        assert strargs('1,-2,3,-4') == [1.0, -2.0, 3.0, -4.0]
        assert strargs('1-2 3-4') == [1.0, -2.0, 3.0, -4.0]
        assert strargs('1-2,3-4') == [1.0, -2.0, 3.0, -4.0]
        assert strargs('1-2-3-4') == [1.0, -2.0, -3.0, -4.0]

    def test_ascii(self, capsys):
        """Parse ABCabc"""
        errormsg('ABCabc')
        assert capsys.readouterr().err == 'ABCabc\n'

    def test_nonunicode_latin1(self, capsys):
        # Py2 has issues with unicode in docstrings.   *sigh*
        # """Parse Àûïàèé"""
        errormsg('Àûïàèé')
        assert capsys.readouterr().err, 'Àûïàèé\n'

    def test_unicode_latin1(self, capsys):
        # Py2 has issues with unicode in docstrings.   *sigh*
        # """Parse Àûïàèé (unicode)"""
        errormsg(u'Àûïàèé')
        assert capsys.readouterr().err, u'Àûïàèé\n'
