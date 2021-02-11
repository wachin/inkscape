# coding=utf-8
from hpgl_output import HpglOutput
from inkex.tester import ComparisonMixin, TestCase

class HPGLOutputBasicTest(ComparisonMixin, TestCase):
    effect_class = HpglOutput
    compare_file = [
        'svg/shapes.svg',
        'svg/hpgl_multipen.svg'
    ]
    python3_only = True
    comparisons = [()]
