# coding=utf-8
from color_moresaturation import MoreSaturation
from .test_inkex_extensions import ColorBaseCase


class ColorMoreSaturationTest(ColorBaseCase):
    effect_class = MoreSaturation
    color_tests = [
        ("none", "none"),
        ("hsl(0, 0, 0)", "hsl(0, 12, 0)"),
        ("hsl(255, 255, 255)", "hsl(255, 255, 255)"),
        ((0, 0, 0), "#000000"),
        ((255, 255, 255), "#ffffff"),
        ((192, 192, 192), "#c2bdbd"),
        ((128, 128, 128), "#857a7a"),
        ((128, 0, 0), "#800000"),
        ((255, 0, 0), "#fe0000"),
        ((128, 128, 0), "#807e00"),
        ((255, 255, 0), "#fefb00"),
        ((0, 128, 0), "#008000"),
        ((0, 255, 0), "#00fe00"),
        ((0, 128, 128), "#00807e"),
        ((0, 255, 255), "#00fefb"),
        ((0, 0, 128), "#000080"),
        ((0, 0, 255), "#0000fe"),
        ((128, 0, 128), "#7e0080"),
        ((255, 0, 255), "#fb00fe"),
    ]
