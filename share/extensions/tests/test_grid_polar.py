# coding=utf-8
from grid_polar import GridPolar
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle

class GridPolarBasicTest(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    compare_filters = [CompareOrderIndependentStyle()]
    effect_class = GridPolar
