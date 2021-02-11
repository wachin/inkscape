# coding=utf-8
from color_lesshue import LessHue
from .test_inkex_extensions import ColorBaseCase

class ColorLessHueTest(ColorBaseCase):
    effect_class = LessHue
    color_tests = [
        ("none", "none"),
        ('hsl(0, 0, 0)', 'hsl(243, 0, 0)'),
        ('hsl(255, 255, 255)', 'hsl(243, 255, 255)'),
        ((0, 0, 0), "#000000"),
        ((255, 255, 255), "#ffffff"),
        ((192, 192, 192), "#c0c0c0"),
        ((128, 128, 128), "#808080"),
        ((128, 0, 0), "#800024"),
        ((255, 0, 0), "#fe0047"),
        ((128, 128, 0), "#805a00"),
        ((255, 255, 0), "#feb300"),
        ((0, 128, 0), "#248000"),
        ((0, 255, 0), "#47fe00"),
        ((0, 128, 128), "#00805a"),
        ((0, 255, 255), "#00feb3"),
        ((0, 0, 128), "#002480"),
        ((0, 0, 255), "#0047fe"),
        ((128, 0, 128), "#5a0080"),
        ((255, 0, 255), "#b300fe"),
    ]
