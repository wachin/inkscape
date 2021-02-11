#!/usr/bin/env python
# coding=utf-8
from extrude import Extrude
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyleAndPath, CompareWithPathSpace

class ExtrudeBasicTest(ComparisonMixin, TestCase):
    effect_class = Extrude
    comparisons = [('--id=p1', '--id=p2')]
    compare_filters = [CompareWithPathSpace(), CompareOrderIndependentStyleAndPath()]
