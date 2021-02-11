# coding=utf-8
from rubberstretch import RubberStretch
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase

class TestRubberStretchBasic(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    effect_class = RubberStretch
    comparisons = [('--id=p1', '--id=r3')]
