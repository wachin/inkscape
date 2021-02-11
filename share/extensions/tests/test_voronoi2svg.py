# coding=utf-8
from voronoi2svg import Voronoi
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle

class TestVoronoi2svgBasic(ComparisonMixin, TestCase):
    effect_class = Voronoi
    compare_filters = [CompareOrderIndependentStyle()]
    comparisons = [('--id=p1', '--id=r3')]
