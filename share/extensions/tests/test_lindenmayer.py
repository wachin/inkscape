# coding=utf-8
from lindenmayer import Lindenmayer
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle

class LSystemBasicTest(ComparisonMixin, TestCase):
    effect_class = Lindenmayer
    compare_filters = [CompareOrderIndependentStyle()]
