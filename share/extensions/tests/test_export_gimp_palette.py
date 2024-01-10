# coding=utf-8
from export_gimp_palette import ExportGimpPalette
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import WindowsTextCompat


class TestExportGplBasic(ComparisonMixin, TestCase):
    effect_class = ExportGimpPalette
    compare_file = "svg/colors.svg"
    compare_filters = [WindowsTextCompat()]
