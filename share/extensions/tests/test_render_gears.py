# coding=utf-8
from render_gears import Gears
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentStyle

class GearsBasicTest(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    effect_class = Gears
    compare_filters = [CompareOrderIndependentStyle()]
