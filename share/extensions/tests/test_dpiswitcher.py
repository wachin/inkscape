# coding=utf-8
from dpiswitcher import DPISwitcher
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy

class TestDPISwitcherBasic(ComparisonMixin, TestCase):
    effect_class = DPISwitcher
    compare_filters = [CompareNumericFuzzy()]
