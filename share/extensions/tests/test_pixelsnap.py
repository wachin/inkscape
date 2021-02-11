# coding=utf-8
from pixelsnap import PixelSnap
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle

class TestPixelSnapEffectBasic(ComparisonMixin, TestCase):
    effect_class = PixelSnap
    compare_filters = [CompareOrderIndependentStyle()]
    comparisons = [('--id=p1', '--id=r3')]
