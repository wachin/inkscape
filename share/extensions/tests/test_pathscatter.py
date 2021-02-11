# coding=utf-8
from pathscatter import PathScatter
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareWithoutIds

class TestPathScatterBasic(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    effect_class = PathScatter
    comparisons = [('--id=p1', '--id=r3'),]
    compare_filters = [CompareWithoutIds()]
