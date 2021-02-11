# coding=utf-8
from param_curves import ParamCurves
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy, CompareWithPathSpace

class TestParamCurvesBasic(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    effect_class = ParamCurves
    compare_filters = [CompareNumericFuzzy(), CompareWithPathSpace()]
