# coding=utf-8
from pathalongpath import PathAlongPath
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy, CompareWithPathSpace

class TestPathAlongPathBasic(ComparisonMixin, TestCase):
    compare_filters = [CompareNumericFuzzy(), CompareWithPathSpace()]
    comparisons = [('--copymode=Single', '--id=p1', '--id=p2')]
    effect_class = PathAlongPath
