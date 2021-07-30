# coding=utf-8
from color_randomize import Randomize
from .test_inkex_extensions import ColorBaseCase

class ColorRandomizeTest(ColorBaseCase):
    effect_class = Randomize
    python3_only = True
    color_tests = [
        # "hsl(191, 122, 150)" = rgb(150, 100, 200)
        ("none", "none"),
        # The default ranges are set to 0, and thus the color and opacity should not change (except
        # for rounding errors)
        ("hsl(191, 122, 150)", "hsl(191, 122, 149)"),
        # The user selected 0% values, and thus the color should not change.
        ("hsl(191, 122, 150)", "hsl(191, 122, 149)", ['-y 0', '-t 0', '-m 0']),
        # Random hue only. Saturation and lightness not changed.
        ("hsl(191, 122, 150)", "hsl(201, 122, 149)", ['-y 50', '-t 0', '-m 0']),
        # Random saturation only. Hue and lightness not changed.
        ("hsl(191, 122, 150)", "hsl(191, 165, 149)", ['-y 0', '-t 50', '-m 0']),
        # Random lightness only. Hue and saturation not changed.
        ("hsl(191, 122, 150)", "hsl(190, 120, 194)", ['-y 0', '-t 0', '-m 50']),
        # The maximum hsl values should be between 0 and 100% of their maximum
        ("hsl(191, 122, 150)", "hsl(133, 134, 227)", ['-y 100', '-t 100', '-m 100']),
    ]

    opacity_tests = [
        (5, 5),
        # The user selected 0% opacity range, and thus the opacity should not change.
        (0.15, 0.15, ['-o 0']),
        # The opacity value should be greater than 0
        (0.0, 1.0, ['-o 100']),
        # The opacity value should be lesser than 1
        (1.0, 0.43, ['-o 100']),
        # Other units are available
        ('0.5', 0.654, ['-o 54']),
    ]

    def test_bad_opacity(self):
        """Bad opacity error handled"""
        self.effect.modify_opacity('opacity', 'hello')
