# coding=utf-8
"""Test base inkex module functionality"""
from __future__ import absolute_import, print_function, unicode_literals

import os
import sys

from io import BytesIO

from inkex.base import InkscapeExtension, SvgThroughMixin
from inkex.tester import TestCase

class ModExtension(InkscapeExtension):
    """A non-svg extension that loads, saves and flipples"""

    def effect(self):
        self.document += b'>flipple'

    def load(self, stream):
        return stream.read()

    def save(self, stream):
        stream.write(self.document)


class NoModSvgExtension(SvgThroughMixin, InkscapeExtension):
    """Test the loading and not-saving of non-modified svg files"""

    def effect(self):
        return True


class ModSvgExtension(SvgThroughMixin, InkscapeExtension):
    """Test the loading and saving of svg files"""

    def effect(self):
        self.svg.set('attr', 'foo')


class InkscapeExtensionTest(TestCase):
    """Tests for Inkscape Extensions"""
    effect_class = InkscapeExtension

    def setUp(self):
        self.e = self.effect_class()

    def test_bare_bones(self):
        """What happens when we don't inherit"""
        with self.assertRaises(NotImplementedError):
            self.e.run([])
        with self.assertRaises(NotImplementedError):
            prevarg = sys.argv
            sys.argv = ['pytest']
            try:
                self.e.run()
            finally:
                sys.argv = prevarg
        with self.assertRaises(NotImplementedError):
            self.e.effect()
        with self.assertRaises(NotImplementedError):
            self.e.load(sys.stdin)
        with self.assertRaises(NotImplementedError):
            self.e.save(sys.stdout)
        self.assertEqual(self.e.name, 'InkscapeExtension')

    def test_compat(self):
        """Test a few old functions and how we handle them"""
        with self.assertRaises(AttributeError):
            self.assertEqual(self.e.OptionParser, None)
        with self.assertRaises(AttributeError):
            self.assertEqual(self.e.affect(), None)

    def test_arg_parser_defaults(self):
        """Test arguments for the base class are given defaults"""
        options = self.e.arg_parser.parse_args([])
        self.assertEqual(options.input_file, None)
        self.assertEqual(options.output, None)

    def test_arg_parser_passed(self):
        """Test arguments for the base class are parsed"""
        options = self.e.arg_parser.parse_args(['--output', 'foo.txt', self.empty_svg])
        self.assertEqual(options.input_file, self.empty_svg)
        self.assertEqual(options.output, 'foo.txt')

    def test_svg_path(self):
        """Can get the svg file location"""
        output = os.path.join(self.tempdir, 'output.tmp')
        ext = ModExtension()
        ext.run(['--output', output, self.empty_svg])
        self.assertEqual(ext.svg_path(), os.path.join(self.datadir(), 'svg'))
        self.assertEqual(ext.absolute_href('/foo'), '/foo')
        self.assertEqual(ext.absolute_href('./foo'), os.path.join(self.datadir(), 'svg', 'foo'))
        self.assertEqual(ext.absolute_href('~/foo'), os.path.realpath(os.path.expanduser('~/foo')))
        ext.options.input_file = None
        self.assertEqual(ext.absolute_href('./foo'), os.path.realpath(os.path.expanduser('~/foo')))
        tmp_foo = os.path.realpath('/tmp/foo')
        self.assertEqual(ext.absolute_href('./foo', '/tmp/'), tmp_foo)


class SvgInputOutputTest(TestCase):
    """Test SVG Input Mixin"""

    def test_input_mixin(self):
        """Test svg input gets loaded"""
        obj = NoModSvgExtension()
        obj.run([self.empty_svg])
        self.assertNotEqual(obj.document, None)
        self.assertNotEqual(obj.original_document, None)

    def test_no_output(self):
        """Test svg output isn't saved when not modified"""
        obj = NoModSvgExtension()
        filename = self.temp_file(suffix='.svg')
        obj.run(['--output', filename, self.empty_svg])
        self.assertEqual(type(obj.document).__name__, '_ElementTree')
        self.assertEqual(type(obj.svg).__name__, 'SvgDocumentElement')
        self.assertFalse(os.path.isfile(filename))

    def test_svg_output(self):
        """Test svg output is saved"""
        obj = ModSvgExtension()
        filename = self.temp_file(suffix='.svg')
        obj.run(['--output', filename, self.empty_svg])
        self.assertTrue(os.path.isfile(filename))
        with open(filename, 'r') as fhl:
            self.assertIn('<svg', fhl.read())

    def test_str_document(self):
        """Document is saved even if it's not bytes"""
        obj = ModSvgExtension()
        obj.document = b'foo'
        obj.save(BytesIO())
        obj.document = 'foo'
        ret = BytesIO()
        obj.save(ret)
        self.assertEqual(ret.getvalue(), b'foo')
