#!/usr/bin/en
# coding=utf-8
from ink2canvas import Html5Canvas
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentLines

class Ink2CanvasBasicTest(ComparisonMixin, TestCase):
    effect_class = Html5Canvas
    compare_file = 'svg/shapes-clipboard.svg'
    compare_filters = [CompareOrderIndependentLines()]
    comparisons = [()]
