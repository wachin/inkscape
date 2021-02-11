# coding=utf-8
from grid_cartesian import GridCartesian
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle

class GridCartesianBasicTest(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    effect_class = GridCartesian
    compare_filters = [CompareOrderIndependentStyle()]
