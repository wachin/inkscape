# coding=utf-8
from inkex.tester import ComparisonMixin, TestCase
from text_extract import Extract

class TestExtractBasic(ComparisonMixin, TestCase):
    effect_class = Extract
    stderr_output = True
    comparisons = [
        ('--direction=tb',),
        ('--direction=bt',),
        ('--direction=lr',),
        ('--direction=rl',),
    ]
