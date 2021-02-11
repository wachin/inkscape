# coding=utf-8
from media_zip import CompressedMedia
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareSize

class CmoBasicTest(ComparisonMixin, TestCase):
    effect_class = CompressedMedia
    compare_filters = [CompareSize()]
    comparisons = [()]
