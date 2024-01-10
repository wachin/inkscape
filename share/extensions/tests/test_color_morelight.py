# coding=utf-8
from color_morelight import MoreLight
from .test_inkex_extensions import ColorBaseCase


class ColorMoreLightTest(ColorBaseCase):
    effect_class = MoreLight
    color_tests = [
        ("none", "none"),
        ("hsl(0, 0, 0)", "hsl(0, 0, 12)"),
        ("hsl(255, 255, 255)", "hsl(255, 255, 255)"),
        ((0, 0, 0), "#0c0c0c"),
        ((255, 255, 255), "#ffffff"),
        ((192, 192, 192), "#cccccc"),
        ((128, 128, 128), "#8c8c8c"),
        ((128, 0, 0), "#980000"),
        ((255, 0, 0), "#fe1717"),
        ((128, 128, 0), "#989600"),
        ((255, 255, 0), "#fefc17"),
        ((0, 128, 0), "#009800"),
        ((0, 255, 0), "#17fe17"),
        ((0, 128, 128), "#009896"),
        ((0, 255, 255), "#17fefc"),
        ((0, 0, 128), "#000098"),
        ((0, 0, 255), "#1717fe"),
        ((128, 0, 128), "#960098"),
        ((255, 0, 255), "#fc17fe"),
    ]
