# coding=utf-8
from color_replace import ReplaceColor
from .test_inkex_extensions import ColorBaseCase

class ColorReplaceTest(ColorBaseCase):
    effect_class = ReplaceColor
    color_tests = [
        ("none", "none"),
        ((0, 0, 0), "#ff0000", []),
        ((128, 0, 0), "#800000", []),
        ((0, 0, 0), "#696969", ['-t1768516095']),
        ((0, 0, 0), "#000000", ["-f1", "-t1768516095"]),
        ((18, 52, 86), "#696969", ["-f305420031", "-t1768516095"]),
        ((18, 52, 86), "#ff0000", ["-f305420031"]),
    ]
