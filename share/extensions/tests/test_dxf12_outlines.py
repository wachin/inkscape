# coding=utf-8
from dxf12_outlines import DxfTwelve
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import WindowsTextCompat


class TestDXF12OutlinesBasic(ComparisonMixin, TestCase):
    compare_file = [
        "svg/shapes.svg",
        "svg/preserved-transforms.svg",
        "svg/dxf_nested_transforms.svg",
        "svg/clips_and_masks.svg",
        "svg/scale_not_unity.svg",
    ]
    comparisons = [()]
    effect_class = DxfTwelve
    compare_filters = [WindowsTextCompat()]
