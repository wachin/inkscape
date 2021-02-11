# coding=utf-8
"""Test units inkex module functionality"""
from inkex.tester import TestCase

import inkex
import inkex.tween as tween
import pytest

class TweenTest(TestCase):
    """Unit tests for the Inkscape inkex tween library"""
    black = inkex.Color('#000000')
    grey50 = inkex.Color('#080808')
    white = inkex.Color('#111111')

    def test_interpcoord(self):
        val = tween.interpcoord(0, 1, 0.5)
        assert val == pytest.approx(0.5, 1e-3)

    def test_interppoints(self):
        val = tween.interppoints((0,0), (1,1), 0.5)
        assert val == pytest.approx((0.5, 0.5), (1e-3, 1e-3))
