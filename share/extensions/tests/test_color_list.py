# coding=utf-8
from color_list import ListColours
from .test_inkex_extensions import ColorEffectTest

class ColorListTest(ColorEffectTest):
    effect_class = ListColours
    effect_name = 'test_color_list'
    stderr_output = True
    color_tests = []
