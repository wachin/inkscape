# coding=utf-8

from color_morehue import MoreHue
from .test_inkex_extensions import ColorBaseCase

class ColorMoreHueTest(ColorBaseCase):
    effect_class = MoreHue
    color_tests = [
        ("none", "none"),
        ('hsl(0, 0, 0)', 'hsl(12, 0, 0)'),
        ('hsl(255, 255, 255)', 'hsl(12, 255, 255)'),
        ((0, 0, 0), "#000000"),
        ((255, 255, 255), "#ffffff"),
        ((192, 192, 192), "#c0c0c0"),
        ((128, 128, 128), "#808080"),
        ((128, 0, 0), "#802400"),
        ((255, 0, 0), "#fe4700"),
        ((128, 128, 0), "#5d8000"),
        ((255, 255, 0), "#b9fe00"),
        ((0, 128, 0), "#008024"),
        ((0, 255, 0), "#00fe47"),
        ((0, 128, 128), "#005d80"),
        ((0, 255, 255), "#00b9fe"),
        ((0, 0, 128), "#240080"),
        ((0, 0, 255), "#4700fe"),
        ((128, 0, 128), "#80005d"),
        ((255, 0, 255), "#fe00b9"),
    ]
