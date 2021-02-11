# coding=utf-8
from ungroup_deep import UngroupDeep
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle

class TestUngroupBasic(ComparisonMixin, TestCase):
    effect_class = UngroupDeep
    compare_filters = [CompareOrderIndependentStyle()]
    comparisons = [
        (),
        ('--id=layer2',)
    ]
