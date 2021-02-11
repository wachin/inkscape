# coding=utf-8
"""
Test Inkex style parsing functionality.
"""

import pytest

from inkex.styles import Style
from inkex.colors import Color
from inkex.tester import TestCase
from inkex.tester.svg import svg_file

class StyleTest(TestCase):
    """Test path API and calculations"""

    def test_new_style(self):
        """Create a style from a path string"""
        stl = Style("border-color: blue; border-width: 4px;")
        self.assertEqual(str(stl), 'border-color:blue;border-width:4px')

    def test_composite(self):
        """Test chaining styles together"""
        stl = Style("border-color: blue;")
        stl += "border-color: red; border-issues: true;"
        self.assertEqual(str(stl), 'border-color:red;border-issues:true')
        st2 = stl + "border-issues: false;"
        self.assertEqual(str(st2), 'border-color:red;border-issues:false')

    def test_inbuilts(self):
        """Test inbuild style functions"""
        stadd = Style("a: 1") + Style("b: 2")
        self.assertTrue(stadd == Style("b: 2; a: 1"))
        self.assertFalse(stadd == Style("b: 2"))
        self.assertFalse(stadd != Style("b: 2; a: 1"))
        self.assertEqual(stadd - "a: 4", "b: 2")
        stadd -= "b: 3; c: 4"
        self.assertEqual(stadd, Style("a: 1"))

    def test_set_property(self):
        """Set the style attribute directly"""
        stl = Style()
        stl['border-pain'] = 'green'
        self.assertEqual(str(stl), 'border-pain:green')

    def test_color_property(self):
        """Color special handling"""
        stl = Style("fill-opacity:0.7;fill:red;")
        self.assertEqual(stl.get_color('fill').alpha, 0.7)
        self.assertEqual(str(stl.get_color('fill')), 'rgba(255, 0, 0, 0.7)')
        stl.set_color('rgba(0, 127, 0, 0.5)', 'stroke')
        self.assertEqual(str(stl), 'fill-opacity:0.7;fill:red;stroke-opacity:0.5;stroke:#007f00')

    def test_interpolate(self):
        """Test interpolation method."""
        stl1 = Style({'stroke-width':'0px', 'fill-opacity':1.0,'fill':Color((200, 0, 0))})
        stl2 = Style({'stroke-width':'1pc', 'fill-opacity':0.0,'fill':Color((100, 0, 100))})
        stl3 = stl1.interpolate(stl2, 0.5)
        assert stl3['fill-opacity'] == pytest.approx(0.5, 1e-3)
        assert stl3['fill'] == [150, 0, 50]
        assert stl3['stroke-width'] == '8px'

class AttribFallbackTest(TestCase):
    """Test the fallback style for handling attribute based styles"""
    def setUp(self):
        self.svg = svg_file(self.data_file('svg', 'css.svg'))
        self.elem = self.svg.getElementById('rect2')

    def test_fallback_read_style(self):
        """Style comes from style property"""
        self.elem.style['fill'] = 'green'
        self.elem.set('fill', 'red')
        self.assertEqual(self.elem.fallback_style()['fill'], 'green')

    def test_fallback_read_attrib(self):
        """Style comes from attribute"""
        self.elem.style.pop('stroke', None)
        self.assertEqual(self.elem.fallback_style()['stroke'], None)
        self.elem.set('stroke', 'green')
        self.assertEqual(self.elem.fallback_style()['stroke'], 'green')

    def test_fallback_read_css(self):
        """Style from basic css will work"""
        elem = self.svg.getElementById('rect1')
        self.assertEqual(elem.fallback_style()['fill'], 'blue')

    def test_fallback_write_style(self):
        """Styles are set back correctly"""
        self.elem.style['fill'] = 'green'
        self.elem.set('fill', 'red')
        self.elem.fallback_style()['fill'] = 'blue'
        self.assertEqual(self.elem.style['fill'], 'blue')
        self.assertEqual(self.elem.get('fill'), None) # Removed

    def test_fallback_write_attrib(self):
        """Attrib is written back when needed"""
        self.elem.style.pop('stroke', None)
        self.elem.set('stroke', 'green')
        self.elem.fallback_style()['stroke'] = 'blue'
        self.assertEqual(self.elem.style.get('stroke', None), None) # Still empty
        self.assertEqual(self.elem.get('stroke'), 'blue')

    def test_fallback_write_move(self):
        """Style is moved when required"""
        self.elem.style.pop('stroke', None)
        self.elem.set('stroke', 'green')
        self.elem.fallback_style(move=True)['stroke'] = 'blue'
        self.assertEqual(self.elem.style['stroke'], 'blue')
        self.assertEqual(self.elem.get('stroke'), None) # Moved

    def test_fallback_write_css(self):
        """Style can be set into the stylesheet style"""
        elem = self.svg.getElementById('rect1')
        elem.fallback_style()['fill'] = 'green'
        self.assertIn('#rect1 {\n  fill:green;\n}', self.svg.getElementById('style1').text)
        elem.fallback_style()['fill'] = 'red'
        self.assertIn('#rect1 {\n  fill:red;\n}', self.svg.getElementById('style1').text)

    def test_no_attr(self):
        """Given name doesn't exist anywhere"""
        self.elem.style = 'fill:red'
        style = self.elem.fallback_style()
        self.assertEqual(style.get('fill'), 'red')
        self.assertEqual(style.get('jump'), None)

class StyleSheetTest(TestCase):
    """Test parsing style sheets"""
    def setUp(self):
        super(StyleSheetTest, self).setUp()
        self.svg = svg_file(self.data_file('svg', 'css.svg'))
        self.css = self.svg.stylesheet

    def test_classes(self):
        """Test element class manipulation"""
        rect = self.svg.getElementById('rect2')
        self.assertEqual(rect.get('class'), 'two')
        self.assertEqual(rect.classes, ['two'])
        rect.classes[0] = 'twa'
        self.assertEqual(rect.get('class'), 'twa')
        rect.classes.append('tri')
        rect.classes.append('four')
        self.assertEqual(rect.get('class'), 'twa tri four')
        rect.classes.remove('twa')
        self.assertEqual(rect.get('class'), 'tri four')
        rect.classes.toggle('toggle')
        self.assertEqual(rect.get('class'), 'tri four toggle')
        rect.classes.toggle('toggle')
        self.assertEqual(rect.get('class'), 'tri four')

    def test_creation(self):
        """Stylesheet is created when needed"""
        self.svg = svg_file(self.data_file('svg', 'empty.svg'))
        self.assertEqual(len(self.svg.stylesheets), 0)
        self.assertEqual(len(self.svg.stylesheet), 0)
        self.assertEqual(len(self.svg.stylesheets), 1)
        self.svg.stylesheet.append('.cls1 { fill: blue; }')
        self.assertIn(b'style><![CDATA[\n.cls1 {\n  fill:blue;\n}\n]]><', self.svg.tostring())

    def test_parsing(self):
        """SVG parsing provides access to stylesheets"""
        sheets = self.svg.stylesheets
        self.assertEqual(len(sheets), 3)
        self.assertEqual(len(sheets[0]), 7)
        self.assertEqual(len(sheets[1]), 0)
        self.assertEqual(len(sheets[2]), 2)

    def test_string(self):
        """Rendered to a string"""
        sheets = self.svg.stylesheets
        self.assertEqual(str(sheets[0][0]), '#layer1 {\n  stroke:yellow;\n}')
        self.assertEqual(str(sheets[2][1]), '.rule {}')

    def test_lookup_by_id(self):
        """ID CSS lookup"""
        self.assertEqual(self.css[0].to_xpath(), "//*[@id='layer1']")
        elem = self.svg.getElement(self.css[0].to_xpath())
        self.assertEqual(elem.get('id'), 'layer1')

    def test_lookup_by_element(self):
        """Element name CSS lookup"""
        self.assertEqual(self.css[1].to_xpath(), "//svg:circle")
        elems = list(self.svg.xpath(self.css[1].to_xpath()))
        self.assertEqual(len(elems), 2)
        self.assertEqual(elems[0].get('id'), 'circle1')
        self.assertEqual(elems[1].get('id'), 'circle2')

    def test_lookup_by_class(self):
        """Class name CSS lookup"""
        self.assertEqual(self.css[2].to_xpath(),\
            "//*[contains(concat(' ', normalize-space(@class), ' '), ' two ')]")
        elem = self.svg.getElement(self.css[2].to_xpath())
        self.assertEqual(elem.get('id'), 'rect2')

    def test_lookup_and(self):
        """Multiple CSS lookups"""
        self.assertEqual(self.css[3].to_xpath(), "//*[@id='rect3']"\
            "[contains(concat(' ', normalize-space(@class), ' '), ' three ')]")
        elem = self.svg.getElement(self.css[3].to_xpath())
        self.assertEqual(elem.get('id'), 'rect3')

    def test_lookup_or(self):
        """SVG rules can look up the right elements"""
        self.assertEqual(self.css[6].to_xpath(), "//*[@id='circle1']|//*[@id='circle2']|"\
             "//*[contains(concat(' ', normalize-space(@class), ' '), ' two ')]")
        elems = self.svg.xpath(self.css[6].to_xpath())
        self.assertEqual(len(elems), 3)
        self.assertEqual(elems[0].get('id'), 'rect2')
        self.assertEqual(elems[1].get('id'), 'circle1')
        self.assertEqual(elems[2].get('id'), 'circle2')

    def test_applied_styles(self):
        """Are styles applied to the svg elements correctly"""
        self.assertEqual(
            str(self.svg.getElementById('rect1').cascaded_style()),
            'fill:blue')
        self.assertEqual(
            str(self.svg.getElementById('rect2').cascaded_style()),
            'fill:green;font:Homie')
        self.assertEqual(
            str(self.svg.getElementById('rect3').cascaded_style()),
            'fill:cyan')
        self.assertEqual(
            str(self.svg.getElementById('rect4').cascaded_style()),
            'fill:grey;stroke:red')
        self.assertEqual(
            str(self.svg.getElementById('circle1').cascaded_style()),
            'fill:red;font:Homie')
        self.assertEqual(
            str(self.svg.getElementById('circle2').cascaded_style()),
            'fill:red;font:Homie')
