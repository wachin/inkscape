#!/usr/bin/env python
# coding=utf-8
from interp_att_g import InterpAttG
from inkex.tester import ComparisonMixin, TestCase

class InterpAttGBasicTest(ComparisonMixin, TestCase):
    effect_class = InterpAttG
    comparisons = [('--id=layer1',)]

class InterpAttGColorRoundingTest(ComparisonMixin, TestCase):
    effect_class = InterpAttG
    compare_file = 'svg/group_interpolate.svg'
    comparisons = {
        # test for truncating/rounding bug inbox#1892
        ('--id=g53', '--att=fill', '--start-val=#181818', '--end-val=#000000'),
        # test for clipping of values <= 1
        ('--id=g53', '--att=fill', '--start-val=#050505', '--end-val=#000000')}