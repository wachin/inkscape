# coding=utf-8
from hpgl_output import HpglOutput
from inkex.tester import ComparisonMixin, TestCase


class HPGLOutputBasicTest(ComparisonMixin, TestCase):
    effect_class = HpglOutput
    compare_file = ["svg/shapes.svg", "svg/hpgl_multipen.svg"]
    comparisons = [("--force=24", "--speed=20", "--orientation=90")]
