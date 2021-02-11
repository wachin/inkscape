# coding=utf-8
from motion import Motion
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy, CompareWithPathSpace

class MotionBasicTest(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    effect_class = Motion
    compare_filters = [CompareNumericFuzzy(), CompareWithPathSpace()]
    comparisons = [
        ('--id=c3', '--id=p2'),
    ]
