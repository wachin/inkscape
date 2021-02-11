# coding=utf-8
from color_HSL_adjust import HslAdjust
from .test_inkex_extensions import ColorBaseCase

class ColorHSLAdjustTest(ColorBaseCase):
    effect_class = HslAdjust
    color_tests = [
        ("none", "none"),
        ((255, 255, 255), "#ffffff"),
        ((0, 0, 0), "#000000"),
        ((0, 128, 0), "#008000"),
        ((91, 166, 176), "#5a74af", ['-x 10']),
        ((91, 166, 176), "#745aaf", ['-x 320']),
        ((91, 166, 176), "#5ba6b0", ['-x 0']),
        ((91, 166, 176), "#af5a6c", ['-x 12345']),
        ((91, 166, 176), "#5aacaf", ['-x -1']),
        ((91, 166, 176), "#4eafbb", ['-s 10']),
        ((91, 166, 176), "#0be5fe", ['-s 90']),
        ((91, 166, 176), "#5ba6b0", ['-s 0']),
        ((91, 166, 176), "#0be5fe", ['-s 100']),
        ((91, 166, 176), "#0be5fe", ['-s 12345']),
        ((91, 166, 176), "#5ba5ae", ['-s -1']),
        ((91, 166, 176), "#7cb8bf", ['-l 10']),
        ((91, 166, 176), "#ffffff", ['-l 90']),
        ((91, 166, 176), "#5ba6b0", ['-l 0']),
        ((91, 166, 176), "#ffffff", ['-l 100']),
        ((91, 166, 176), "#ffffff", ['-l 12345']),
        ((91, 166, 176), "#56a4ad", ['-l -1']),
        ((91, 166, 176), '#5a86af', ['--random_h=true']),
        ((91, 166, 176), '#cde4e6', ['--random_l=true']),
        ((91, 166, 176), '#43b8c6', ['--random_s=true']),
    ]
