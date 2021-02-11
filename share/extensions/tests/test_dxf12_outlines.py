# coding=utf-8
from dxf12_outlines import DxfTwelve
from inkex.tester import InkscapeExtensionTestMixin, TestCase


class TestDXF12OutlinesBasic(InkscapeExtensionTestMixin, TestCase):
    effect_class = DxfTwelve
