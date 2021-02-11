#!/usr/bin/env python
from webslicer_create_rect import CreateRect
from inkex.tester import ComparisonMixin, TestCase

class TestWebSlicerCreateRectBasic(ComparisonMixin, TestCase):
    effect_class = CreateRect
