# coding=utf-8
from docinfo import DocInfo
from inkex.tester import ComparisonMixin, TestCase

class TestDocInfo(ComparisonMixin, TestCase):
    compare_file = 'svg/guides.svg'
    effect_class = DocInfo
    stderr_output = True
    comparisons = [()]
