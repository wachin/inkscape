# coding=utf-8
from funcplot import FuncPlot
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy

class FuncPlotBasicTest(ComparisonMixin, TestCase):
    effect_class = FuncPlot
    compare_filters = [CompareNumericFuzzy()]
    comparisons = [
        ('--id=p1', '--id=r3'),
    ]
