# coding=utf-8
from text_split import TextSplit
from inkex.tester import ComparisonMixin, TestCase

class TestSplitBasic(ComparisonMixin, TestCase):
    """Test split effect"""
    effect_class = TextSplit
    comparisons = [('--id=t1', '--id=t3')]
