# coding=utf-8
from generate_voronoi import GenerateVoronoi
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle

class TestPatternBasic(ComparisonMixin, TestCase):
    effect_class = GenerateVoronoi
    comparisons = [('--id=r3', '--id=p1'),]
    compare_filters = [CompareOrderIndependentStyle()]
