# coding=utf-8
from color_replace import ReplaceColor
from .test_inkex_extensions import ColorBaseCase
from inkex import Color
from inkex.tester import ComparisonMixin, TestCase


class ColorReplaceTest(ColorBaseCase):
    effect_class = ReplaceColor
    color_tests = [
        ("none", "none"),
        ((0, 0, 0), "#ff0000", []),
        ((128, 0, 0), "#800000", []),
        ((0, 0, 0), "#696969", ["-t1768516095"]),
        ((0, 0, 0), "#000000", ["-f1", "-t1768516095", "-i=False"]),
        ((18, 52, 86), "#696969", ["-f305420031", "-t1768516095"]),
        ((18, 52, 86), "#ff0000", ["-f305420031"]),
        (
            Color([10, 20, 30, 0.2], space="rgba"),
            Color([255, 0, 0, 20 / 255], space="rgba"),
            ["-f169090611", "-t4278190100"],
        ),
        (
            Color([10, 20, 30, 0.5], space="rgba"),
            Color([10, 20, 30, 0.5], space="rgba"),
            ["-f169090611", "-t4278190100", "-i=False"],
        ),
        (
            Color([10, 20, 30, 0.5], space="rgba"),
            Color([255, 0, 0, 20 / 255], space="rgba"),
            ["-f169090611", "-t4278190100"],
        ),
    ]


class ColorReplacePatternTest(ComparisonMixin, TestCase):
    effect_class = ReplaceColor
    compare_file = "svg/simple_patterns.svg"
    comparisons = [("--id=rect184",), ("--id=rect184", "--id=rect184-8")]
