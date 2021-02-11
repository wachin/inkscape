# coding=utf-8

from grid_isometric import GridIsometric
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle, CompareWithPathSpace

class TestGridIsometricBasic(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    compare_filters = [CompareOrderIndependentStyle(), CompareWithPathSpace()]
    effect_class = GridIsometric
    comparisons = [()]
