# coding=utf-8
from color_lesssaturation import LessSaturation
from .test_inkex_extensions import ColorBaseCase

class ColorLessSaturationTest(ColorBaseCase):
    effect_class = LessSaturation
    color_tests = [
        ("none", "none"),
        ('hsl(0, 0, 0)', 'hsl(0, 0, 0)'),
        ('hsl(255, 255, 255)', 'hsl(255, 243, 255)'),
        ((0, 0, 0), "#000000"),
        ((255, 255, 255), "#ffffff"),
        ((192, 192, 192), "#c0c0c0"),
        ((128, 128, 128), "#808080"),
        ((128, 0, 0), "#7c0303"),
        ((255, 0, 0), "#f80505"),
        ((128, 128, 0), "#7c7b03"),
        ((255, 255, 0), "#f8f505"),
        ((0, 128, 0), "#037c03"),
        ((0, 255, 0), "#05f805"),
        ((0, 128, 128), "#037c7b"),
        ((0, 255, 255), "#05f8f5"),
        ((0, 0, 128), "#03037c"),
        ((0, 0, 255), "#0505f8"),
        ((128, 0, 128), "#7b037c"),
        ((255, 0, 255), "#f505f8"),
    ]
