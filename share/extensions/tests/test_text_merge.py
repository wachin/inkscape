#!/usr/bin/env python
from text_merge import Merge
from inkex.tester import ComparisonMixin, TestCase

class TestMergeBasic(ComparisonMixin, TestCase):
    effect_class = Merge
    comparisons = [()]
