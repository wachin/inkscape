#!/usr/bin/env python
# coding=utf-8
"""
Test specific elements API from svg xml lxml custom classes.
"""

import pytest
import inkex

from inkex import (
    Group, Layer, Pattern, Guide, Polyline, Use, Defs,
    TextElement, TextPath, Tspan, FlowPara, FlowRoot, FlowRegion, FlowSpan,
    PathElement, Rectangle, Circle, Ellipse, Anchor, Line as LineElement,
    Transform, Style, LinearGradient, RadialGradient, Stop
)
from inkex.colors import Color
from inkex.paths import Move, Line
from inkex.utils import FragmentError

from .test_inkex_elements_base import SvgTestCase

class ElementTestCase(SvgTestCase):
    """Base element testing"""
    tag = 'svg'

    def setUp(self):
        super().setUp()
        self.elem = self.svg.getElement('//svg:{}'.format(self.tag))

    def test_print(self):
        """Print element as string"""
        self.assertEqual(str(self.elem), self.tag)

    def assertElement(self, elem, compare): # pylint: disable=invalid-name
        """Assert an element"""
        self.assertEqual(elem.tostring(), compare)


class PathElementTestCase(ElementTestCase):
    """Test PathElements"""
    source_file = 'with-lpe.svg'
    tag = 'path'

    def test_new_path(self):
        """Test new path element"""
        path = PathElement.new(path=[Move(10, 10), Line(20, 20)])
        self.assertEqual(path.get('d'), 'M 10 10 L 20 20')

    def test_original_path(self):
        """LPE paths can return their original paths"""
        lpe = self.svg.getElementById('lpe')
        nolpe = self.svg.getElementById('nolpe')
        self.assertEqual(str(lpe.path), 'M 30 30 L -10 -10 Z')
        self.assertEqual(str(lpe.original_path), 'M 20 20 L 10 10 Z')
        self.assertEqual(str(nolpe.path), 'M 30 30 L -10 -10 Z')
        self.assertEqual(str(nolpe.original_path), 'M 30 30 L -10 -10 Z')

        lpe.original_path = "M 60 60 L 5 5"
        self.assertEqual(lpe.get('inkscape:original-d'), 'M 60 60 L 5 5')
        self.assertEqual(lpe.get('d'), 'M 30 30 L -10 -10 Z')

        lpe.path = "M 60 60 L 15 15 Z"
        self.assertEqual(lpe.get('d'), 'M 60 60 L 15 15 Z')

        nolpe.original_path = "M 60 60 L 5 5"
        self.assertEqual(nolpe.get('inkscape:original-d', None), None)
        self.assertEqual(nolpe.get('d'), 'M 60 60 L 5 5')

class PolylineElementTestCase(ElementTestCase):
    """Test the polyline elements support"""
    tag = 'polyline'

    def test_type(self):
        """Polyline have their own types"""
        self.assertTrue(isinstance(self.elem, inkex.Polyline))

    def test_polyline_points(self):
        """Basic tests for points attribute as a path"""
        pol = Polyline(points='10,10 50,50 10,15 15,10')
        self.assertEqual(str(pol.path), 'M 10 10 L 50 50 L 10 15 L 15 10')
        pol.path = "M 10 10 L 30 9 L 1 2 C 10 45 3 4 45 60 M 35 35"
        self.assertEqual(pol.get('points'), '10,10 30,9 1,2 45,60 35,35')

class PolygonElementTestCase(ElementTestCase):
    """Test Polygon Elements"""
    tag = 'polygon'

    def test_type(self):
        """Polygons have their own types"""
        self.assertTrue(isinstance(self.elem, inkex.Polygon))

    def test_conversion(self):
        """Polygones are converted to paths"""
        pol = inkex.Polygon(points='10,10 50,50 10,15 15,10')
        self.assertEqual(str(pol.path), 'M 10 10 L 50 50 L 10 15 L 15 10 Z')

class LineElementTestCase(ElementTestCase):
    """Test Line Elements"""
    tag = 'line'

    def test_new_line(self):
        """Line creation"""
        line = LineElement.new((10, 10), (20, 20))
        self.assertElement(line, b'<line x1="10.0" y1="10.0" x2="20.0" y2="20.0"/>')

    def test_type(self):
        """Lines have their own types"""
        self.assertTrue(isinstance(self.elem, inkex.Line))

    def test_conversion(self):
        """Lines are converted to paths"""
        pol = inkex.elements.Line(x1='2', y1='3', x2='4', y2='5')
        self.assertEqual(str(pol.path), 'M 2 3 L 4 5 Z')

class PatternTestCase(ElementTestCase):
    """Test Pattern elements"""
    tag = 'pattern'

    def test_pattern_transform(self):
        """Patterns have a transformation of their own"""
        pattern = Pattern()
        self.assertEqual(pattern.patternTransform, Transform())
        pattern.patternTransform.add_translate(10, 10)
        self.assertEqual(pattern.get('patternTransform'), 'translate(10, 10)')

class GroupTest(ElementTestCase):
    """Test extra functionality on a group element"""
    tag = 'g'

    def test_new_group(self):
        """Test creating groups"""
        svg = Layer.new('layerA', Group.new('groupA', Rectangle()))
        self.assertElement(svg,\
            b'<g inkscape:groupmode="layer" inkscape:label="layerA">'\
            b'<g inkscape:label="groupA"><rect/></g></g>')

    def test_transform_property(self):
        """Test getting and setting a transform"""
        self.assertEqual(str(self.elem.transform), 'matrix(1.44985 0 0 1.36417 -107.03 -167.362)')
        self.elem.transform = 'translate(12, 14)'
        self.assertEqual(self.elem.transform, Transform('translate(12, 14)'))
        self.assertEqual(str(self.elem.transform), 'translate(12, 14)')

    def test_groupmode(self):
        """Get groupmode is layer"""
        self.assertEqual(self.svg.getElementById('A').groupmode, 'layer')
        self.assertEqual(self.svg.getElementById('C').groupmode, 'group')

    def test_get_path(self):
        """Group path is combined children"""
        print(str(self.svg.getElementById('A').get_path()))
        self.assertEqual(
            str(self.svg.getElementById('A').get_path()),
            'M -108.539 517.61 L -87.6093 496.117 L -98.3066 492.768 L -69.9353 492.301 L -55.5172'
            ' 506.163 L -66.2146 502.814 L -87.1446 524.307 M 60.0914 498.694 L 156.784 439.145 L'
            ' 240.218 491.183 L 143.526 550.731 z M -176.909 458.816 a 64.2385 38.9175 -7.86457 1'
            ' 0 88.3701 -19.0784 a 64.2385 38.9175 -7.86457 0 0 -88.3701 19.0784 z M -300.162'
            ' 513.715 L -282.488 509.9 Z M -214.583 540.504 L -209.001 448.77 Z M -193.189 547.201 '
            'L -238.536 486.266 L -185.049 503.008 L -230.396 442.073 M -193.189 547.201 L -238.536'
            ' 486.266 L -185.049 503.008 L -230.396 442.073 Z M 15 15 L 15.5 20 Z')

    def test_bounding_box(self):
        """A group returns a bounding box"""
        empty = self.svg.add(Group(Group()))
        self.assertEqual(empty.bounding_box(), None)
        self.assertEqual(int(self.svg.getElementById('A').bounding_box().width), 783)
        self.assertEqual(int(self.svg.getElementById('B').bounding_box().height), 114)

class RectTest(ElementTestCase):
    """Test extra functionality on a rectangle element"""
    tag = 'rect'

    def test_parse(self):
        """Test Rectangle parsed from XML"""
        rect = Rectangle(attrib={
            "x": "10px", "y": "20px",
            "width": "100px", "height": "200px",
            "rx": "15px", "ry": "30px" })
        self.assertEqual(rect.left, 10)
        self.assertEqual(rect.top, 20)
        self.assertEqual(rect.right, 10+100)
        self.assertEqual(rect.bottom, 20+200)
        self.assertEqual(rect.width, 100)
        self.assertEqual(rect.height, 200)
        self.assertEqual(rect.rx, 15)
        self.assertEqual(rect.ry, 30)

    def test_compose_transform(self):
        """Composed transformation"""
        self.assertEqual(self.elem.transform, Transform('rotate(16.097889)'))
        self.assertEqual(str(self.elem.composed_transform()),
                         'matrix(1.4019 -0.812338 1.20967 0.709877 -542.221 533.431)')

    def test_effetive_stylesheet(self):
        """Test the non-parent combination of styles"""
        self.assertEqual(str(self.elem.effective_style()),\
            'fill:#0000ff;stroke-width:1px')
        self.assertEqual(str(self.elem.getparent().effective_style()),\
            'fill:#0000ff;stroke-width:1px;stroke:#f00')

    def test_compose_stylesheet(self):
        """Test finding the composed stylesheet for the shape"""
        self.assertEqual(str(self.elem.style), 'fill:#0000ff;stroke-width:1px')
        self.assertEqual(str(self.elem.composed_style()),
                         'fill:#0000ff;stroke:#d88;joker:url(#path1);stroke-width:1px')

    def test_path(self):
        """Rectangle path"""
        self.assertEqual(self.elem.get_path(), 'M 200.0,200.0 h100.0v100.0h-100.0 z')
        self.assertEqual(str(self.elem.path), 'M 200 200 h 100 v 100 h -100 z')

class PathTest(ElementTestCase):
    """Test path extra functionality"""
    tag = 'path'

    def test_apply_transform(self):
        """Transformation can be applied to path"""
        path = self.svg.getElementById('D')
        path.transform = Transform(translate=(10, 10))
        self.assertEqual(path.get('d'), 'M30,130 L60,130 L60,120 L70,140 L60,160 L60,150 L30,150')
        path.apply_transform()
        self.assertEqual(path.get('d'), 'M 40 140 L 70 140 L 70 130 L 80 150 '
                                        'L 70 170 L 70 160 L 40 160')
        self.assertFalse(path.transform)

class CircleTest(ElementTestCase):
    """Test extra functionality on a circle element"""
    tag = 'circle'

    def test_parse(self):
        """Test Circle parsed from XML"""
        circle = Circle(attrib={"cx": "10px", "cy": "20px", "r": "30px"})
        self.assertEqual(circle.center.x, 10)
        self.assertEqual(circle.center.y, 20)
        self.assertEqual(circle.radius, 30)
        ellipse = Ellipse(attrib={"cx": "10px", "cy": "20px", "rx": "30px", "ry": "40px"})
        self.assertEqual(ellipse.center.x, 10)
        self.assertEqual(ellipse.center.y, 20)
        self.assertEqual(ellipse.radius.x, 30)
        self.assertEqual(ellipse.radius.y, 40)

    def test_new(self):
        """Test new circles"""
        elem = Circle.new((10, 10), 50)
        self.assertElement(elem, b'<circle cx="10.0" cy="10.0" r="50.0"/>')
        elem = Ellipse.new((10, 10), (15, 10))
        self.assertElement(elem, b'<ellipse cx="10.0" cy="10.0" rx="15.0" ry="10.0"/>')

    def test_path(self):
        """Circle path"""
        self.assertEqual(self.elem.get_path(),
                         'M 100.0,50.0 a 50.0,50.0 0 1 0 50.0, '
                         '50.0 a 50.0,50.0 0 0 0 -50.0, -50.0 z')

class AnchorTest(ElementTestCase):
    """Test anchor tags"""
    def test_new(self):
        """Anchor tag creation"""
        link = Anchor.new('https://inkscape.org', Rectangle())
        self.assertElement(link, b'<a xlink:href="https://inkscape.org"><rect/></a>')

class NamedViewTest(ElementTestCase):
    """Test the sodipodi namedview tag"""
    def test_guides(self):
        """Create a guide and see a list of them"""
        self.svg.namedview.add(Guide().move_to(0, 0, 0))
        self.svg.namedview.add(Guide().move_to(0, 0, '90'))
        self.assertEqual(len(self.svg.namedview.get_guides()), 2)

class TextTest(ElementTestCase):
    """Test all text functions"""
    def test_append_superscript(self):
        """Test adding superscript"""
        tap = TextPath()
        tap.append(Tspan.superscript('th'))
        self.assertEqual(len(tap), 1)

    def test_path(self):
        """Test getting paths"""
        self.assertFalse(TextPath().get_path())
        self.assertFalse(TextElement().get_path())
        self.assertFalse(FlowRegion().get_path())
        self.assertFalse(FlowRoot().get_path())
        self.assertFalse(FlowPara().get_path())
        self.assertFalse(FlowSpan().get_path())
        self.assertFalse(Tspan().get_path())


class UseTest(ElementTestCase):
    """Test extra functionality on a use element"""
    tag = 'use'

    def test_path(self):
        """Use path follows ref"""
        self.assertEqual(str(self.elem.path), 'M 0 0 L 10 10 Z')

    def test_empty_ref(self):
        """An empty ref or None ref doesn't cause an error"""
        use = Use()
        use.set('xlink:href', 'something')
        self.assertRaises(FragmentError, getattr, use, 'href')
        elem = self.svg.add(Use())
        self.assertEqual(elem.href, None)
        elem.set('xlink:href', '')
        self.assertEqual(elem.href, None)
        elem.set('xlink:href', '#badref')
        self.assertEqual(elem.href, None)
        elem.set('xlink:href', self.elem.get('xlink:href'))
        self.assertEqual(elem.href.get('id'), 'path1')

    def test_unlink(self):
        """Test use tag unlinking"""
        elem = self.elem.unlink()
        self.assertEqual(str(elem.path), 'M 0 0 L 10 10 Z')
        self.assertEqual(elem.tag_name, 'path')
        self.assertEqual(elem.getparent().get('id'), 'C')

class StopTests(ElementTestCase):
    """Color stop tests"""
    black = Color('#000000')
    grey50 = Color('#080808')
    white = Color('#111111')

    def test_interpolate(self):
        """Interpolate colours"""
        stl1 = Style({'stop-color': self.black, 'stop-opacity': 0.0})
        stop1 = Stop(offset='0.0', style=str(stl1))
        stl2 = Style({'stop-color': self.white, 'stop-opacity': 1.0})
        stop2 = Stop(offset='1.0', style=str(stl2))
        stop3 = stop1.interpolate(stop2, 0.5)
        assert stop3.style['stop-color'] == str(self.grey50)
        assert float(stop3.style['stop-opacity']) == pytest.approx(0.5, 1e-3)


class GradientTests(ElementTestCase):
    """Gradient testing"""
    black = Color('#000000')
    grey50 = Color('#080808')
    white = Color('#111111')

    whiteop1 = Style({'stop-color': white, 'stop-opacity': 1.0})
    blackop1 = Style({'stop-color': black, 'stop-opacity': 1.0})
    whiteop0 = Style({'stop-color': white, 'stop-opacity': 0.0})
    blackop0 = Style({'stop-color': black, 'stop-opacity': 0.0})

    translate11 = Transform('translate(1.0, 1.0)')
    translate22 = Transform('translate(2.0, 2.0)')

    def test_parse(self):
        """Gradients parsed from XML"""
        values = [
            (LinearGradient,
             {'x1': '0px', 'y1': '1px', 'x2': '2px', 'y2': '3px'},
             {'x1': 0.0,   'y1': 1.0,   'x2': 2.0,   'y2': 3.0},
             ),
            (RadialGradient,
             {'cx': '0px', 'cy': '1px', 'fx': '2px', 'fy': '3px', 'r': '4px'},
             {'cx': 0.0,   'cy': 1.0,   'fx': 2.0,   'fy': 3.0}
            )]
        for classname, attributes, expected in values:
            grad = classname(attrib=attributes)
            grad.apply_transform()  # identity transform
            for key, value in expected.items():
                assert float(grad.get(key)) == pytest.approx(value, 1e-3)
            grad = classname(attrib=attributes)
            grad = grad.interpolate(grad, 0.0)
            for key, value in expected.items():
                assert float(grad.get(key)) == pytest.approx(value, 1e-3)

    def test_apply_transform(self):
        """Transform gradients"""
        values = [
            (LinearGradient,
             {'x1': 0.0, 'y1': 0.0, 'x2': 1.0, 'y2': 1.0},
             {'x1': 1.0, 'y1': 1.0, 'x2': 2.0, 'y2': 2.0}),
            (RadialGradient,
             {'cx': 0.0, 'cy': 0.0, 'fx': 1.0, 'fy': 1.0, 'r': 1.0},
             {'cx': 1.0, 'cy': 1.0, 'fx': 2.0, 'fy': 2.0, 'r': 1.0}
            )]
        for classname, orientation, expected in values:
            grad = classname().update(**orientation)
            grad.gradientTransform = self.translate11
            grad.apply_transform()
            val = grad.get('gradientTransform')
            assert val is None
            for key, value in expected.items():
                assert float(grad.get(key)) == pytest.approx(value, 1e-3)

    def test_stops(self):
        """Gradients have stops"""
        for classname in [LinearGradient, RadialGradient]:
            grad = classname()
            stops = [
                Stop().update(offset=0.0, style=self.whiteop0),
                Stop().update(offset=1.0, style=self.blackop1)]
            grad.add(*stops)
            assert [s1.tostring() == s2.tostring() for s1, s2 in zip(grad.stops, stops)]

    def test_stop_styles(self):
        """Gradients have styles"""
        for classname in [LinearGradient, RadialGradient]:
            grad = classname()
            stops = [
                Stop().update(offset=0.0, style=self.whiteop0),
                Stop().update(offset=1.0, style=self.blackop1)]
            grad.add(*stops)
            assert [str(s1) == str(s2.style) for s1, s2 in zip(grad.stop_styles, stops)]

    def test_get_stop_offsets(self):
        """Gradients stop offsets"""
        for classname in [LinearGradient, RadialGradient]:
            grad = classname()
            stops = [
                Stop().update(offset=0.0, style=self.whiteop0),
                Stop().update(offset=1.0, style=self.blackop1)]
            grad.add(*stops)
            for stop1, stop2 in zip(grad.stop_offsets, stops):
                self.assertEqual(float(stop1), pytest.approx(float(stop2.offset), 1e-3))

    def test_interpolate(self):
        """Gradients can be interpolated"""
        values = [
            (LinearGradient,
             {'x1': 0, 'y1': 0, 'x2': 1, 'y2': 1},
             {'x1': 2, 'y1': 2, 'x2': 1, 'y2': 1},
             {'x1': 1.0, 'y1': 1.0, 'x2': 1.0, 'y2': 1.0}),
            (RadialGradient,
             {'cx': 0, 'cy': 0, 'fx': 1, 'fy': 1, 'r': 0},
             {'cx': 2, 'cy': 2, 'fx': 1, 'fy': 1, 'r': 1},
             {'cx': 1.0, 'cy': 1.0, 'fx': 1.0, 'fy': 1.0, 'r': 0.5})
            ]
        for classname, orientation1, orientation2, expected in values:
            # gradient 1
            grad1 = classname()
            stops1 = [
                Stop().update(offset=0.0, style=self.whiteop0),
                Stop().update(offset=1.0, style=self.blackop1)]
            grad1.add(*stops1)
            grad1.update(gradientTransform=self.translate11)
            grad1.update(**orientation1)

            # gradient 2
            grad2 = classname()
            stops2 = [
                Stop().update(offset=0.0, style=self.blackop1),
                Stop().update(offset=1.0, style=self.whiteop0)]
            grad2.add(*stops2)
            grad2.update(gradientTransform=self.translate22)
            grad2.update(**orientation2)
            grad = grad1.interpolate(grad2, 0.5)
            comp = Style({'stop-color': self.grey50, 'stop-opacity': 0.5})
            self.assertEqual(str(grad.stops[0].style), str(Style(comp)))
            self.assertEqual(str(grad.stops[1].style), str(Style(comp)))
            self.assertEqual(str(grad.gradientTransform), 'translate(1.5, 1.5)')
            for key, value in expected.items():
                self.assertEqual(float(grad.get(key)), pytest.approx(value, 1e-3))


class SymbolTest(ElementTestCase):
    """Test Symbol elements"""
    source_file = 'symbol.svg'
    tag = 'symbol'

    def test_unlink_symbol(self):
        """Test unlink symbols"""
        use = self.svg.getElementById('plane01')
        self.assertEqual(use.tag_name, 'use')
        self.assertEqual(use.href.tag_name, 'symbol')
        # Unlinking should replace symbol with group
        elem = use.unlink()
        self.assertEqual(elem.tag_name, 'g')
        self.assertEqual(str(elem.transform), 'translate(18, 16)')
        self.assertEqual(elem[0].tag_name, 'title')
        self.assertEqual(elem[1].tag_name, 'rect')

class DefsTest(ElementTestCase):
    """Test the definitions tag"""
    source_file = 'shapes.svg'
    tag = 'defs'

    def test_defs(self):
        """Make sure defs can be seen in the nodes of an svg"""
        self.assertTrue(isinstance(self.svg.defs, Defs))
        defs = self.svg.getElementById('defs33')
        self.assertTrue(isinstance(defs, Defs))

class StyleTest(ElementTestCase):
    """Test a style tag"""
    source_file = 'css.svg'
    tag = 'style'

    def test_style(self):
        """Make sure style tags can be loaded and saved"""
        css = self.svg.stylesheet
        self.assertTrue(css)
