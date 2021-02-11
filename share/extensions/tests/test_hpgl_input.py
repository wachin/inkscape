# coding=utf-8
from hpgl_input import HpglInput
from inkex.tester import ComparisonMixin, TestCase

class TestHpglFileBasic(ComparisonMixin, TestCase):
    effect_class = HpglInput
    compare_file = 'io/test.hpgl'
    comparisons = [()]
