# coding=utf-8
from printing_marks import PrintingMarks
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy, CompareWithPathSpace, \
    CompareOrderIndependentStyle

class PrintingMarksBasicTest(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    effect_class = PrintingMarks
    compare_filters = [
        CompareNumericFuzzy(),
        CompareWithPathSpace(),
        CompareOrderIndependentStyle(),
    ]
