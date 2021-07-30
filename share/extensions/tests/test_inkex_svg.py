#!/usr/bin/env python
# coding=utf-8
#
# Copyright (C) 2018 Martin Owens
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
Test the svg interface for inkscape extensions.
"""
from inkex.transforms import Vector2d
from inkex import Guide
from inkex.tester import TestCase
from inkex.tester.svg import svg, svg_file, uu_svg
from inkex import addNS

class BasicSvgTest(TestCase):
    """Basic svg tests"""

    def test_svg_load(self):
        """Test loading an svg with the right parser"""
        self.assertEqual(type(svg()).__name__, 'SvgDocumentElement')

    def test_add_ns(self):
        """Test adding a namespace to a tag"""
        self.assertEqual(addNS('g', 'svg'), '{http://www.w3.org/2000/svg}g')
        self.assertEqual(addNS('h', 'inkscape'), '{http://www.inkscape.org/namespaces/inkscape}h')
        self.assertEqual(addNS('i', 'sodipodi'),
                         '{http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd}i')
        self.assertEqual(addNS('{p}j'), '{p}j')

    def test_svg_ids(self):
        """Test a list of ids from an svg document"""
        self.assertEqual(svg('id="apples"').get_ids(), {'apples'})

    def test_svg_new_id(self):
        """Test generatign a new id for a given tag"""
        doc = svg('id="apples"')
        usedids = set(['apples'])
        for prefix in ['apples'] * 3:
            newid = doc.get_unique_id(prefix)
            self.assertTrue(newid.startswith(prefix))
            self.assertTrue(newid not in usedids)
            usedids.add(newid)

    def test_svg_select_id(self):
        """Select an id from the document"""
        doc = svg('id="bananas"')
        doc.selection.set('bananas')
        self.assertEqual(doc.selection['bananas'], doc)
        self.assertEqual(doc.selection.first(), doc)
        doc = svg('id="apples"')
        doc.selected.set(doc.getElementById('apples'))
        self.assertEqual(doc.selection['apples'], doc)
        self.assertEqual(doc.selection.first(), doc)

    def test_svg_by_class(self):
        """Select elements by class"""
        doc = svg_file(self.data_file('svg', 'multilayered-test.svg'))
        elems = doc.getElementsByClass('frog')
        self.assertEqual([elem.get_id() for elem in elems],
                         ['path3902', 'text3926', 'path3900', 'rect3898'])
        elems = doc.getElementsByClass('apple')
        self.assertEqual([elem.get_id() for elem in elems], ['text3926', 'rect3898'])

    def test_svg_by_href(self):
        """Select element by xlink href"""
        doc = svg_file(self.data_file('svg', 'multilayered-test.svg'))
        elem = doc.getElementsByHref('path3900')[0]
        self.assertEqual(elem.TAG, 'textPath')
        self.assertEqual(elem.get_id(), 'textPath3923')
        elem = doc.getElementsByHref('path3904')[0]
        self.assertEqual(elem.TAG, 'textPath')
        self.assertEqual(elem.get_id(), 'textPath3906')
        self.assertEqual(doc.getElementsByHref('not-an-id'), [])

    def test_svg_by_url_link(self):
        """Select element by urls in styles"""
        doc = svg_file(self.data_file('svg', 'markers.svg'))
        elem = doc.getElementsByStyleUrl('Arrow1Lend')[0]
        self.assertEqual(elem.get_id(), 'dimension')
        elem = doc.getElementsByStyleUrl('Arrow1Lstart')[0]
        self.assertEqual(elem.get_id(), 'dimension')
        self.assertEqual(doc.getElementsByStyleUrl('not-an-id'), [])

    def test_selected_bbox(self):
        """Can we get a bounding box from the selected items"""
        doc = svg_file(self.data_file('svg', 'multilayered-test.svg'))
        doc.selected.set('path3904', 'path3902')
        from inkex.transforms import BoundingBox
        x, y, w, h = 199.544, 156.412, 377.489, 199.972  # from inkscape --query-all
        expected_3904 = BoundingBox((x, x + w), (y, y + h))
        x, y, w, h = 145.358, 478.373, 439.135, 419.142  # from inkscape --query-all
        expected_3902 = BoundingBox((x, x + w), (y, y + h))
        expected = list(expected_3902 + expected_3904)

        for x, y in zip(expected, doc.selection.bounding_box()):
            self.assertDeepAlmostEqual(tuple(x), tuple(y), delta=1e-3)


    def test_svg_name(self):
        """Can get the sodipodi name attribute"""
        doc = svg_file(self.data_file('svg', 'multilayered-test.svg'))
        self.assertEqual(doc.name, 'Nouveau document 1')

    def test_svg_nameview(self):
        """Can get the sodipodi nameview element"""
        doc = svg()
        self.assertEqual(doc.namedview.center.x, 0)
        self.assertEqual(type(doc.namedview).__name__, 'NamedView')

    def test_svg_layers(self):
        """Selected layer is selected"""
        doc = svg_file(self.data_file('svg', 'multilayered-test.svg'))
        self.assertEqual(doc.get_current_layer().get('id'), 'layer3')
        doc = svg('id="empty"')
        self.assertEqual(doc.get_current_layer(), doc)

    def test_svg_center_position(self):
        """SVG with namedview has a center position"""
        doc = svg_file(self.data_file('svg', 'multilayered-test.svg'))
        self.assertTrue(doc.namedview.center.is_close((30.714286, 520.0)))
        self.assertTrue(svg().namedview.center.is_close(Vector2d()))

    def test_defs(self):
        """Can get the defs from an svg file"""
        doc = svg_file(self.data_file('svg', 'markers.svg'))
        self.assertEqual(len(doc.defs), 2)
        doc = svg('id="empty"')
        self.assertEqual(len(doc.defs), 0)

    def test_scale(self):
        """Scale of a document"""
        doc = svg('id="empty" viewBox="0 0 100 100" width="200" height="200"')
        self.assertEqual(doc.width, 200.0)
        self.assertEqual(doc.get_viewbox()[2], 100.0)
        self.assertEqual(doc.scale, 2.0)
        doc = svg('id="empty" viewBox="0 0 0 0" width="200" height="200"')
        self.assertEqual(doc.scale, 1.0)

class NamedViewTest(TestCase):
    """Tests for the named view functionality"""

    def test_create_guide(self):
        """Test creating guides"""
        doc = svg_file(self.data_file('svg', 'multilayered-test.svg'))
        namedview = doc.namedview
        self.assertEqual(len(namedview.get_guides()), 0)

        namedview.add(Guide().move_to(50, 50, 45))
        self.assertEqual(len(namedview.get_guides()), 1)
        guide, = namedview.get_guides()
        self.assertEqual(guide.get('position'), '50,50')
        self.assertEqual(guide.get('orientation'), '0.707107,-0.707107')


class GetDocumentWidthTest(TestCase):
    """Tests for Effect.width."""

    def test_no_dimensions(self):
        """An empty width value should be default zero width"""
        self.assertEqual(svg().width, 0)

    def test_empty_width(self):
        """An empty width value should be the same as a missing width."""
        self.assertEqual(svg('width=""').width, 0)

    def test_empty_viewbox(self):
        """An empty viewBox value should be the same as a missing viewBox."""
        self.assertEqual(svg('viewBox=""').width, 0)

    def test_empty_width_and_viewbox(self):
        """Empty values for both should be the same as both missing."""
        self.assertEqual(svg('width="" viewBox=""').width, 0)

    def test_width_only(self):
        """Test a fixed width"""
        self.assertAlmostEqual(svg('width="120mm"').width, 453.5433071)

    def test_width_and_viewbox(self):
        """If both are present, width overrides viewBox."""
        self.assertAlmostEqual(svg('width="120mm" viewBox="0 0 22 99"').width, 453.5433071)

    def test_viewbox_only(self):
        """IF only the viewBox is present"""
        self.assertEqual(svg('viewBox="0 0 22 99"').width, 22.0)

    def test_only_valid_viewbox(self):
        """An empty width value should be the same as a missing width."""
        self.assertEqual(svg('width="" viewBox="0 0 22 99"').width, 22.0)

    def test_non_zero_viewbox_x(self):
        """Demonstrate that a non-zero x value (viewbox[0]) does not affect the width value."""
        self.assertEqual(svg('width="" viewBox="5 7 22 99"').width, 22.0)


class GetDocumentHeightTest(TestCase):
    """Tests for Effect.height."""

    def test_no_dimensions(self):
        """Test height from blank svg"""
        self.assertEqual(svg().height, 0)

    def test_empty_height(self):
        """An empty height value should be the same as a missing height."""
        self.assertEqual(svg('height=""').height, 0)

    def test_empty_viewbox(self):
        """An empty viewBox value should be the same as a missing viewBox."""
        self.assertEqual(svg('viewBox=""').height, 0)

    def test_empty_height_viewbox(self):
        """Empty values for both should be the same as both missing."""
        self.assertEqual(svg('height="" viewBox=""').height, 0)

    def test_height_only(self):
        """A simple height only in px"""
        self.assertEqual(svg('height="330px"').height, 330)

    def test_height_and_viewbox(self):
        """If both are present, height overrides viewBox."""
        self.assertEqual(svg('height="330px" viewBox="0 0 22 99"').height, 330)

    def test_viewbox_only(self):
        """Height from viewBox only"""
        self.assertEqual(svg('viewBox="0 0 22 99"').height, 99.0)

    def test_no_height_valid_viewbox(self):
        """An empty height value should be the same as a missing height."""
        self.assertEqual(svg('height="" viewBox="0 0 22 99"').height, 99.0)

    def test_non_zero_viewbox_y(self):
        """Demonstrate that a non-zero y value (viewbox[1]) does not affect the height value."""
        self.assertEqual(svg('height="" viewBox="5 7 22 99"').height, 99.0)


class GetDocumentUnitTest(TestCase):
    """Tests for Effect.unit."""

    def test_no_dimensions(self):
        """Default units with no arguments"""
        self.assertEqual(svg().unit, 'px')

    def test_width_only(self):
        """"Units from document width only"""
        # TODO: Determine whether returning 'px' in this case is the
        #     intended behavior.
        self.assertEqual(svg('width="100m"').unit, 'px')

    def test_height_only(self):
        """Units from document height only"""
        # TODO: Determine whether returning 'px' in this case is the
        #     intended behavior.
        self.assertEqual(svg('height="100m"').unit, 'px')

    def test_viewbox_only(self):
        """Test viewbox only document units"""
        self.assertEqual(svg('viewBox="0 0 377 565"').unit, 'px')

    # Unit-ratio tests. Don't exhaustively test every unit conversion, just
    # demonstrate that the logic works.

    def test_width_and_viewbox_px(self):
        """100mm is ~377px, so unit should be 'px'."""
        self.assertEqual(svg('width="100mm" viewBox="0 0 377 565"').unit, 'px')

    def test_width_and_viewbox_in(self):
        """100mm is ~3.94in, so unit should be 'in'."""
        self.assertEqual(svg('width="100mm" viewBox="0 0 3.94 5.90"').unit, 'in')

    def test_unitless_width_and_viewbox(self):
        """Unitless width should be treated as 'px'."""
        # 3779px is ~1m, so unit should be 'm'.
        self.assertEqual(svg('width="3779" viewBox="0 0 1 1.5"').unit, 'm')

    def test_height_with_viewbox(self):
        """150mm is ~5.90in, so unit should be 'in', but height is ignored"""
        # TODO: Determine whether returning 'px' in this case is the intended
        #     behavior.
        self.assertEqual(svg('height="150mm" viewBox="0 0 3.94 5.90"').unit, 'px')

    def test_height_width_and_viewbox(self):
        """100mm is ~23.6pc, so unit should be 'pc'."""
        doc = svg('width="100mm" height="150mm" viewBox="0 0 23.6 35.4"')
        self.assertEqual(doc.unit, 'pc')

    def test_large_error_reverts_to_px(self):
        """'px' instead of using the closest match 'pc'."""
        # 100mm is ~23.6pc; 24.1 is ~2% off from that, so unit should fall back
        self.assertEqual(svg('width="100mm" viewBox="0 0 24.1 35.4"').unit, 'px')

    # TODO: Demonstrate that unknown width units are treated as px while
    #     determining the ratio.

    # 100px fallback tests. Although that value is an arbitrary default, it's
    # possible for users of inkex.Effect to depend on this behavior.
    # NOTE: Do not treat the existence of these tests as a reason to preserve
    # the 100px fallback logic.

    def test_bad_width_number(self):
        """Fallback test: Bad numbers default to 100"""
        # First, demonstrate that 1in is 2.54cm, so unit should be 'cm'.
        self.assertEqual(svg('width="1in" viewBox="0 0 2.54 1"').unit, 'cm')

        # Corrupt the width to contain an invalid number component; note that
        # the units change to 'px'. This is because the corrupt number part is
        # replaced with 100px, producing a width of "100px";
        self.assertEqual(svg('width="ABCDin" viewBox="0 0 2.54 1"').unit, 'px')

    def test_bad_viewbox_entry(self):
        """Fallback test: Bad viewBox default to 100"""
        # First, demonstrate that 3779px is 1m, so unit should be 'm'.
        self.assertEqual(svg('width="3779px" viewBox="0 0 1 1"').unit, 'm')

        # Corrupt the viewBox to include a non-float value; will default to 'px'
        self.assertEqual(svg('width="3779px" viewBox="x 0 1 1"').unit, 'px')


class UserUnitTest(TestCase):
    """Tests for methods that are based on the value of unit."""

    def assertToUserUnit(self, user_unit, test_value, expected):  # pylint: disable=invalid-name
        """Checks a user unit and a test_value against the expected result"""
        doc = uu_svg(user_unit)
        self.assertEqual(doc.unit, user_unit, msg=svg)
        self.assertAlmostEqual(doc.unittouu(test_value), expected)

    def assertFromUserUnit(self, user_unit, value, unit, expected):  # pylint: disable=invalid-name
        """Check converting from a user unity for the test_value"""
        self.assertAlmostEqual(uu_svg(user_unit).uutounit(value, unit), expected)

    # Unit-ratio tests. Don't exhaustively test every unit conversion, just
    # demonstrate that the logic works.

    def test_unittouu_in_to_cm(self):
        """1in is ~2.54cm"""
        self.assertToUserUnit('cm', '1in', 2.54)

    def test_unittouu_yd_to_m(self):
        """1yd is ~0.9144m"""
        self.assertToUserUnit('m', '1yd', 0.9144)

    def test_unittouu_identity(self):
        """If the input and output units are the same, the input and output
           values should exactly be the same, too."""
        self.assertToUserUnit('pc', '9.87654321pc', 9.87654321)

    def test_unittouu_unitless_input(self):
        """Passing a unitless value to unittouu() should treat the units as 'px'."""
        self.assertToUserUnit('in', '96', 1)  # 1in == 96px

    def test_unittouu_empty_input(self):
        """Passing an empty string to unittouu() should treat the value as zero."""
        self.assertToUserUnit('in', '', 0)

    def test_unittouu_parsing(self):
        """Test user unit parsing forms"""
        for value in (
                '100pc',
                '100  pc',
                '   100pc',
                '100pc  ',
                '+100pc',
                '100.0pc',
                '100.0e0pc',
                '10.0e1pc',
                '10.0e+1pc',
                '1000.0e-1pc',
                '.1e+3pc',
                '+.1e+3pc',
        ):
            # 100pc is ~3.937in
            self.assertToUserUnit('px', value, 1600)

    def test_unittouu_bad_input_number(self):
        """Bad input number"""
        self.assertToUserUnit('cm', '1in', 2.54)
        # Demonstrate that 1in is ~2.54cm.

        # Corrupt the input to contain an invalid number component; note that
        # the result changes to zero.
        self.assertToUserUnit('cm', 'ABCDin', 0)

    def test_unittouu_bad_input_unit(self):
        """Bad input unit"""
        # Demonstrate that 1.0in passes through without change.
        self.assertToUserUnit('in', '1.0in', 1.0)

        # Corrupt the input to contain an invalid unit component; note that the
        # result changes to 0.0, because corrupt parsing is zero px.
        # it used to be the ratio between inches and pixels. This was
        # because unittouu() treats unknown units as 'px'.
        self.assertToUserUnit('in', '1.0ABCD', 0)

    # Unit-ratio tests. Don't exhaustively test every unit conversion, just
    # demonstrate that the logic works.

    def test_uutounit_cm_to_in(self):
        """Convert 1 user unit ('in') to 'cm'."""
        self.assertFromUserUnit('in', 1, 'cm', 2.54)  # 1in is ~2.54cm

    def test_uutounit_m_to_yd(self):
        """Convert 1 user unit ('yd') to 'm'."""
        self.assertFromUserUnit('yd', 1, 'm', 0.9144)  # 1yd is ~0.9144m

    def test_uutounit_identity(self):
        """If the input and output units are the same, the input and output
           values should exactly be the same, too."""
        self.assertFromUserUnit('pc', 9.87654321, 'pc', 9.87654321)

    def test_uutounit_unknown_unit(self):
        """Demonstrate that passing an unknown unit string to uutounit()"""
        self.assertEqual(uu_svg('in').uutounit(1, 'px'), 96.0)

    def test_adddocumentunit_common(self):
        """Test common add_unit results"""
        # For valid float inputs, the output should be the input with the user unit appended.
        doc = uu_svg('pt')
        cases = (
            # Input, expected output
            (100, '100pt'),
            ('100', '100pt'),
            ('+100', '100pt'),
            ('-100', '-100pt'),
            ('100.0', '100pt'),
            ('100.0e0', '100pt'),
            ('10.0e1', '100pt'),
            ('10.0e+1', '100pt'),
            ('1000.0e-1', '100pt'),
            ('.1e+3', '100pt'),
            ('+.1e+3', '100pt'),
            ('   100', '100pt'),
            ('100   ', '100pt'),
            ('  100   ', '100pt'),
        )
        for input_value, expected in cases:
            self.assertEqual(doc.add_unit(input_value), expected)

    def test_adddocumentunit_non_float(self):
        """Strings that are invalid floats should pass through unchanged."""
        doc = uu_svg('pt')
        inputs = (
            '',
            'ABCD',
            '.',
            '   ',
        )
        for value in inputs:
            self.assertEqual(doc.add_unit(value), '')
