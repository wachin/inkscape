# coding=utf-8
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy, CompareWithPathSpace

from whirl import Whirl

class WhirlBasicTest(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    effect_class = Whirl
    compare_filters = [CompareNumericFuzzy(), CompareWithPathSpace()]
    comparisons = [('--id=p1', '--id=r3')]
