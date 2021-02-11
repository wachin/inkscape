# coding=utf-8
#
# Unit test file for ../markers_strokepaint.py
# Revision history:
#  * 2012-01-27 (jazzynico): checks defaulf parameters and file handling.
#
from markers_strokepaint import MarkersStrokePaint
from inkex.tester import ComparisonMixin, TestCase

class MarkerStrokePaintBasicTest(ComparisonMixin, TestCase):
    effect_class = MarkersStrokePaint
    compare_file = 'svg/markers.svg'
    comparisons = [
        ('--tab="object"', '--id=dimension'),
        ('--tab="custom"', '--id=dimension'),
    ]

    def test_basic(self):
        args = ['--id=dimension',
                self.data_file('svg', 'markers.svg')]
        eff = MarkersStrokePaint()
        eff.run(args)
        old_markers = eff.original_document.getroot().xpath('//svg:defs//svg:marker')
        new_markers = eff.svg.xpath('//svg:defs//svg:marker')
        self.assertEqual(len(old_markers), 2)
        self.assertEqual(len(new_markers), 4)
