#!/usr/bin/env python3
# coding=utf-8
#
# Copyright (C) 2021 Jonathan Neuhauser, jonathan.neuhauser@outlook.com
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA.
#
"""
Some more complicated styling tests, including inheritance and shorthand attributes
"""
from lxml import etree
from typing import List, Tuple
from inkex.styles import Style
from inkex.colors import Color
from inkex.tester import TestCase
from inkex.tester.svg import svg_file
from inkex import (
    SvgDocumentElement,
    BaseElement,
    ColorError,
    BaseStyleValue,
    RadialGradient,
    Stop,
    PathElement,
)
from inkex import SVG_PARSER


class StyleInheritanceTests(TestCase):
    """Some test cases for css attribute handling"""

    def test_style_sheet_1(self):
        """File from https://commons.wikimedia.org/wiki/File:Test_only.svg, public domain
        note that Inkscape fails the same test: https://gitlab.com/inkscape/inbox/-/issues/1929
        """
        doc: SvgDocumentElement = svg_file(
            self.data_file("svg", "style_inheritance.svg")
        )

        circles: List[BaseElement] = doc.xpath("//svg:circle")
        for circle in circles:
            style = circle.specified_style()
            self.assertEqual(style("fill"), Color("red"), circle.getparent().get_id())

        rects: List[BaseElement] = doc.xpath("//svg:rect")
        for rect in rects:
            style = rect.specified_style()
            self.assertEqual(style("fill"), Color("blue"))

    def test_style_sheet_2(self):
        """This is the unit test styling-css-04-f.svg from
        https://www.w3.org/Graphics/SVG/Test/20061213/htmlObjectHarness/full-styling-css-04-f.html
        Note that the "good" preview image attached on the site is wrong per the explanation
        """
        doc: SvgDocumentElement = svg_file(
            self.data_file("svg", "styling-css-04-f.svg")
        )

        rects: List[BaseElement] = doc.xpath("//svg:rect")

        results = {
            "A": "blue",
            "B": "green",
            "C": "orange",
            "D": "gold",
            "E": "purple",
            "F": "red",
        }
        for rect in rects:
            ident = rect.get_id()
            if len(ident) != 2:
                continue
            result = results[ident[0]]

            style = rect.specified_style()
            self.assertEqual(style("fill"), Color(result))
            self.assertEqual(style["stroke-dasharray"], "none")

    def test_current_color(self):
        """This is the unit test styling-inherit-01-b.svg from
        https://www.w3.org/Graphics/SVG/Test/20061213/htmlObjectHarness/full-styling-inherit-01-b.html
        """

        doc: SvgDocumentElement = svg_file(
            self.data_file("svg", "styling-inherit-01-b.svg")
        )

        objects: List[BaseElement] = doc.xpath("//svg:rect|//svg:ellipse")

        for counter, obj in zip(range(3), objects[:3]):
            fill = obj.specified_style()("fill")
            if counter == 0:
                self.assertEqual(fill, Color("yellow"))
            else:
                if counter == 1:
                    result = "green"
                else:
                    result = "#700"
                self.assertIsInstance(fill, RadialGradient)
                stops = [child for child in fill if isinstance(child, Stop)]
                stop = stops[1]
                self.assertEqual(stop.specified_style()("stop-color"), Color(result))

        stroke = objects[3].specified_style()("stroke")
        self.assertEqual(stroke, Color("red"))

    def test_marker_style(self):
        """Check if markers are read and written correctly"""

        doc: SvgDocumentElement = svg_file(self.data_file("svg", "markers.svg"))
        elem = doc.getElementById("dimension")
        style = elem.specified_style()
        marker = style("marker-start")
        self.assertEqual(marker, doc.getElementById("Arrow1Lstart"))

        # replace marker
        elem.style["marker-start"] = doc.getElementById("Arrow1Lend")
        marker = elem.specified_style()("marker-start")
        self.assertEqual(marker, doc.getElementById("Arrow1Lend"))

        # write invalid attribute
        with self.assertRaisesRegex(ValueError, "Invalid property value"):
            elem.style["marker-start"] = "#url(test)"

        # write invalid attribute, second attempt
        with self.assertRaisesRegex(ValueError, "invalid URL format"):
            elem.style["marker-start"] = "url('test)"

        # write shorthand
        elem.style["marker"] = doc.getElementById("Arrow1Lstart")
        self.assertEqual(elem.style("marker-start"), doc.getElementById("Arrow1Lstart"))
        self.assertEqual(elem.style("marker-mid"), doc.getElementById("Arrow1Lstart"))
        self.assertEqual(elem.style("marker-end"), doc.getElementById("Arrow1Lstart"))

        # write shorthand to empty
        elem.style["marker"] = ""
        self.assertEqual(elem.style("marker-start"), doc.getElementById("Arrow1Lend"))

    def test_get_default(self):
        """Test if the default values are returned for missing attributes"""
        doc: SvgDocumentElement = svg_file(self.data_file("svg", "interp_shapes.svg"))
        elem = doc.getElementById("path6")

        assert elem.style("stroke-dashoffset") == "0"
        assert elem.style("font") == ""

    def parse_style_and_compare(self, tests: List[Tuple[str, dict]]):
        """Parses a style and compares the output to a dictionary of attributes"""
        for shorthand, result in tests:
            style = Style(shorthand)
            for key, value in result.items():
                self.assertEqual(str(style(key)), value)

    def test_font_shorthand(self):
        """Test whether shorthand properties are applied correctly"""
        tests: List[Tuple[str, dict]] = [
            ("font: ", {"font-size": "medium"}),
            (
                r"font: 12px/14px sans-serif",
                {
                    "font-size": "12px",
                    "line-height": "14px",
                    "font-family": "sans-serif",
                },
            ),
            (
                r"font: 80% sans-serif",
                {"font-size": "80%", "font-family": "sans-serif"},
            ),
            (
                r'font: x-large/110% "New Century Schoolbook", serif',
                {
                    "font-size": "x-large",
                    "line-height": "110%",
                    "font-family": '"New Century Schoolbook", serif',
                },
            ),
            (
                r"font: semi-condensed bold italic large Palatino, serif",
                {
                    "font-weight": "bold",
                    "font-style": "italic",
                    "font-size": "large",
                    "font-family": "Palatino, serif",
                    "font-stretch": "semi-condensed",
                },
            ),
            (
                r"font: normal small-caps 120%/120% fantasy",
                {
                    "font-weight": "normal",
                    "font-style": "normal",
                    "font-variant": "small-caps",
                    "font-size": "120%",
                    "line-height": "120%",
                    "font-family": "fantasy",
                },
            ),
        ]
        self.parse_style_and_compare(tests)

    def test_shorthand_overwrites(self):
        """Test whether shorthands correctly follow precedence: only overwrite rules which are
        defined before and not important"""
        tests: List[Tuple[str, dict]] = [
            (
                """font-size: large;
                font-family: Verdana !important;
                font: bold 12px/14px sans-serif;
                font-weight: normal;""",
                {
                    "font-size": "12px",
                    "font-family": "Verdana",
                    "line-height": "14px",
                    "font-weight": "normal",
                },
            )
        ]
        self.parse_style_and_compare(tests)

    def test_gradient_parsing(self):
        """Test if the style correctly outputs Gradient objects"""
        doc: SvgDocumentElement = svg_file(self.data_file("svg", "interp_shapes.svg"))
        elem = doc.getElementById("path6")
        style = elem.style
        grad = style("stroke")
        self.assertEqual(grad, doc.getElementById("linearGradient855"))

    def test_attribute_set(self):
        """Tests if we can set attributes with parsed values"""
        doc: SvgDocumentElement = svg_file(self.data_file("svg", "interp_shapes.svg"))

        elem = doc.getElementById("path6")
        style = elem.style

        tests = [
            (
                "stroke",
                doc.getElementById("linearGradient847"),
                "url(#linearGradient847)",
            ),
            ("fill", Color("red"), "red"),
            ("stroke", None, "none"),
            ("opacity", 0.5, "0.5"),
            ("opacity", 1.2, "1"),
            ("opacity", -2, "0"),
            ("font-variant", "small-caps", "small-caps"),
        ]
        for attr, value, result in tests:
            style[attr] = value
            self.assertEqual(style[attr], result)
            self.assertEqual(elem.specified_style()[attr], result)

    def test_opacity_clip(self):
        """Test if opacity clipping works"""
        style = Style()
        tests = [
            ("opacity", "0.5", 0.5),
            ("opacity", "1.2", 1),
            ("opacity", "-2", 0),
            ("opacity", "50%", 0.5),
        ]
        for attr, value, result in tests:
            style[attr] = value
            self.assertEqual(style(attr), result)

    def test_style_parsing_error(self):
        """Test if bad attribute data raises an exception during parsing"""
        doc: SvgDocumentElement = svg_file(self.data_file("svg", "interp_shapes.svg"))
        tests: List[Tuple[str, Exception]] = [
            (r"opacity: abc", ValueError),
            (r"fill: #GHI", ColorError),
            (r"stroke: url(#missing)", ValueError),
            (r"fill: ", ColorError),
            (r"font-variant: blue", ValueError),
        ]

        for decl, exceptiontype in tests:
            with self.assertRaises(exceptiontype):
                value = BaseStyleValue.factory(declaration=decl)
                _ = value.parse_value(doc)

    def test_attribute_set_invalid(self):
        """Test if bad attribute data raises an exception when setting it on a style"""
        doc: SvgDocumentElement = svg_file(self.data_file("svg", "interp_shapes.svg"))

        elem = doc.getElementById("path6")
        tests = [
            ("fill", "nocolor", "Unknown color format"),
            ("opacity", Style(), "Value must be number"),
            (
                "font-variant",
                "red",
                "Value 'red' is invalid for the property font-variant",
            ),
            ("stroke", "url(#missing)", "Paint server not found"),
        ]
        style = elem.style
        for attr, value, errormsg in tests:
            with self.assertRaisesRegex(Exception, errormsg):
                style[attr] = value

    def test_gradient_id_fallback(self):
        """Test if the gradient fallback (color after nonexistent url) works"""
        doc: SvgDocumentElement = svg_file(self.data_file("svg", "interp_shapes.svg"))
        sty = Style(element=doc)
        sty["stroke"] = "url(#nonexistent) red"
        self.assertEqual(sty("stroke"), Color("red"))

    def test_compare_styles(self):
        """Check style comparison"""
        st1 = Style("fill: blue; stroke: red")
        st2 = Style("fill: orange; stroke: red")
        self.assertNotEqual(st1, st2)
        st2["fill"] = "blue"
        self.assertEqual(st1, st2)
        st1["font-size"] = 1
        self.assertNotEqual(st1, st2)

    def test_basestylevalue(self):
        """Create BaseStyleValue's directly and work on them"""

        val1 = BaseStyleValue.factory("fill: red;")
        self.assertEqual(val1.parse_value(), Color("red"))

        # Compare the style
        self.assertNotEqual(val1, "fill: red;")

        # Create a rule with an invalid declaration
        with self.assertRaises(ValueError):
            _ = BaseStyleValue.parse_declaration("fill=red;")

        # Try to apply a shorthand to the wrong style
        val2 = BaseStyleValue.factory("font: 12pt Verdana")
        style = Style("fill: context-fill;")
        copy = style.copy()

        val2.apply_shorthand(style)

        self.assertEqual(style, copy)

        # Set a value to the wrong key
        with self.assertRaises(ValueError):
            style["stroke"] = BaseStyleValue.factory("font: 12pt Verdana")

    def test_style_bad_interfacing(self):
        """Check a few ways to wrongly interface the Style class"""
        style = Style("fill: red;")

        # call a missing key that is unknown and therefore has no default
        with self.assertRaises(KeyError):
            _ = style("favourite-test")

        # call the add_inherited method with a non-style argument
        style2 = style.add_inherited("fill: blue")
        self.assertEqual(style, style2)

        # set the importance on a value that doesn't exist
        with self.assertRaises(KeyError):
            style.set_importance("stroke", True)

    def test_style_exchange(self):
        doc: SvgDocumentElement = svg_file(self.data_file("svg", "interp_shapes.svg"))

        elem = doc.getElementById("path6")
        style = elem.style

        copystyle = style.copy()
        self.assertIsNone(copystyle.callback)
        copystyle["new-attribute"] = "test"
        self.assertNotIn("new-attribute", elem.style)

        elem.style = copystyle

        copystyle["new-attribute"] = "test"
        self.assertEqual(elem.style("new-attribute"), "test")
        # callback is set after accessing the element
        self.assertIsNotNone(elem.style.callback)
        # copystyle["new-attribute2"] = "test"
        # self.assertEqual(elem.style("new-attribute2"), "test")

    def test_stop_opacity_inheritance(self):
        # subtest of pservers-grad-18b SVG1.1 unit test
        content = """ <svg><g id="test-body-content" font-family="SVGFreeSansASCII,sans-serif" font-size="18">
            <g id="g0" stop-color="#f48" stop-opacity="0.5" color="yellow">
            <linearGradient id="MyGradient1" stop-color="inherit">
                <stop offset="0" stop-color="green" stop-opacity="1"/>
                <stop offset="1" stop-color="inherit" stop-opacity="1"/>
            </linearGradient>
            </g></g></svg>
        """
        doc = etree.fromstring(content, parser=SVG_PARSER)
        grad = doc.getElementById("MyGradient1")
        self.assertEqual(
            grad[0].specified_style()("stop-opacity"), 1
        )  # assert that stop opacity is overwritten
        self.assertEqual(
            grad[1].specified_style()("stop-opacity"), 1
        )  # assert that stop opacity is not inherited by default

    def test_inheritance_second_attribute(self):
        """Check that the second attribute is also correctly inherited"""
        content = """<svg><g fill="red" font-size="18"><g font-size="20" id="test"></g></g></svg>"""
        doc = etree.fromstring(content, parser=SVG_PARSER)
        group = doc.getElementById("test")
        self.assertEqual(group.specified_style()("font-size"), 20)

    def test_inherit_fallback(self):
        content = """<svg><g fill="inherit" style="bla: inherit"><g fill="inherit" id="test" style="bla:inherit"/></g></svg>"""
        doc = etree.fromstring(content, parser=SVG_PARSER)
        group = doc.getElementById("test")
        self.assertEqual(group.specified_style()("fill"), Color("black"))
        self.assertEqual(group.specified_style()("bla"), None)

    def test_direct_child_and_import(self):
        content = """<svg xmlns="http://www.w3.org/2000/svg">
        <style type="text/css"><![CDATA[
        @import url("test.css");
        g > ellipse
        {
            fill: red;
        }
        @import url("test.css");
        ]]></style>
        <g><ellipse id="test"></ellipse></g></svg>"""
        doc = etree.fromstring(content, parser=SVG_PARSER)
        ellipse = doc.getElementById("test")
        self.assertEqual(ellipse.specified_style()("fill"), Color("red"))

    def test_dasharray(self):
        """test parsing of dasharray"""
        elem = PathElement()
        style = elem.style
        tests = [
            ("1 2 3 4", [1, 2, 3, 4]),
            ("1  2,3 4.5", [1, 2, 3, 4.5]),
            ("1;2", []),
            ("1.111", [1.111, 1.111]),
            ("1px, 2px, 3px", [1, 2, 3, 1, 2, 3]),
            ("", []),
            ("1 -2", []),
            (None, []),
            ([1, 2, 3], [1, 2, 3, 1, 2, 3]),
        ]
        for value, result in tests:
            style["stroke-dasharray"] = value
            setvalue = style("stroke-dasharray")
            if result is None:
                self.assertEqual(result, setvalue, f"got {setvalue}, original: {value}")
            else:
                self.assertAlmostTuple(
                    result, setvalue, msg=f"Expected {result}, got {setvalue}"
                )

    def test_dasharray_mutable(self):
        elem = PathElement()
        style = elem.style
        sd = "stroke-dasharray"
        style[sd] = [1, 2, 3, 4]
        style(sd).extend([5, 6])
        self.assertAlmostTuple(style(sd), [1, 2, 3, 4, 5, 6])
