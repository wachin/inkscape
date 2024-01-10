# coding=utf-8

from inkex.colors import Color, ColorError, ColorIdError, is_color
from inkex.tester import TestCase


class ColorTest(TestCase):
    """Test for single transformations"""

    def test_empty(self):
        """Empty color (black)"""
        self.assertEqual(Color(), [])
        self.assertEqual(Color().to_rgb(), [0, 0, 0])
        self.assertEqual(Color().to_rgba(), [0, 0, 0, 1.0])
        self.assertEqual(Color().to_hsl(), [0, 0, 0])
        self.assertEqual(str(Color(None)), "none")
        self.assertEqual(str(Color("none")), "none")

    def test_errors(self):
        """Color parsing errors"""
        self.assertRaises(ColorError, Color, {})
        self.assertRaises(ColorError, Color, ["#id"])
        self.assertRaises(ColorError, Color, "#badhex")
        self.assertRaises(ColorIdError, Color, "url(#someid)")
        self.assertRaises(ColorError, Color, [0, 0, 0, 0])
        self.assertRaises(ColorError, Color(None, space="nop").to_rgb)
        self.assertRaises(ColorError, Color(None, space="nop").to_hsl)
        self.assertRaises(ColorError, Color([1], space="nop").__str__)

    def test_namedcolor(self):
        """Named Color"""
        self.assertEqual(Color("red"), [255, 0, 0])
        self.assertEqual(str(Color("red")), "red")
        color = Color("red")
        color[0] = 41
        self.assertEqual(str(color), "#290000")
        color = Color("#ff0000").to_named()
        self.assertEqual(str(color), "red")

    def test_rgb_hex(self):
        """RGB Hex Color"""
        color = Color("#ff0102")
        self.assertEqual(color, [255, 1, 2])
        self.assertEqual(str(color), "#ff0102")
        self.assertEqual(color.red, 255)
        self.assertEqual(color.green, 1)
        self.assertEqual(color.blue, 2)
        color = Color(" #ff0102")
        self.assertEqual(color.to_hsl(), [254, 255, 128])
        self.assertEqual(color.hue, 254)
        self.assertEqual(color.saturation, 255)
        self.assertEqual(color.lightness, 128)
        self.assertEqual(color.alpha, 1.0)

    def test_setter_rgb(self):
        """Color RGB units can be set"""
        color = Color("red")
        color.red = 127
        self.assertEqual(color.red, 127)
        color.green = 5
        self.assertEqual(color.green, 5)
        color.blue = 15
        self.assertEqual(color.blue, 15)
        color.blue = 5.1
        self.assertEqual(color.blue, 5)
        self.assertEqual(str(color), "#7f0505")

    def test_setter_hsl(self):
        """Color HSL units can be set on RGB color"""
        color = Color("#ff0102")
        color.hue = 100
        self.assertEqual(color.space, "rgb")
        self.assertEqual(color.hue, 100)
        color.saturation = 100
        self.assertEqual(color.space, "rgb")
        self.assertEqual(color.saturation, 99)
        color.lightness = 100
        self.assertEqual(color.space, "rgb")
        self.assertEqual(color.lightness, 99)  # No sure, bad conversion?

    def test_setter_alpha(self):
        """Color conversion from rgb to rgba"""
        color = Color("#ff0102")
        self.assertEqual(color.space, "rgb")
        self.assertEqual(color.alpha, 1.0)
        color.alpha = 0.5
        self.assertEqual(color.space, "rgba")
        self.assertEqual(color.alpha, 0.5)

    def test_rgb_to_hsl(self):
        """RGB to HSL Color"""
        self.assertEqual(Color("#ff7c7d").to_hsl(), [254, 255, 189])
        self.assertEqual(Color("#7e7c7d").to_hsl(), [233, 2, 125])
        self.assertEqual(Color("#7e7cff").to_hsl(), [170, 255, 189])
        self.assertEqual(Color("#7eff7d").to_hsl(), [84, 255, 190])

    def test_rgb_short_hex(self):
        """RGB Short Hex Color"""
        self.assertEqual(Color("#fff"), [255, 255, 255])
        self.assertEqual(str(Color("#fff")), "#ffffff")

    def test_rgb_int(self):
        """RGB Integer Color"""
        self.assertEqual(Color("rgb(255,255,255)"), [255, 255, 255])

    def test_rgb_percent(self):
        """RGB Percent Color"""
        self.assertEqual(Color("rgb(100%,100%,100%)"), [255, 255, 255])
        self.assertEqual(Color("rgb(50%,0%,1%)"), [127, 0, 2])
        self.assertEqual(Color("rgb(66.667%,0%,6.667%)"), [170, 0, 17])

    def test_rgba_one(self):
        """Test for https://gitlab.com/inkscape/extensions/-/issues/402"""
        self.assertEqual(Color("rgb(1, 100%,1.0)"), [1, 255, 255])
        self.assertEqual(Color("rgba(1, 100%,1.0, 100%)"), [1, 255, 255, 1])
        self.assertEqual(Color("rgba(1, 100%, 1.0, 1.0)"), [1, 255, 255, 1])
        self.assertEqual(Color("rgba(1, 100%, 1.0, 1)"), [1, 255, 255, 1])
        self.assertEqual(Color("rgba(1, 0, 0, 1)"), [1, 0, 0, 1])
        self.assertEqual(Color([1, 1.0, 1.0, 1], "rgba"), [1, 255, 255, 1])

    def test_rgba_color(self):
        """Parse RGBA colours"""
        self.assertEqual(Color("rgba(45,50,55,1.0)"), [45, 50, 55, 1.0])
        self.assertEqual(Color("rgba(45,50,55,1.5)"), [45, 50, 55, 1.0])
        self.assertEqual(Color("rgba(66.667%,0%,6.667%,0.5)"), [170, 0, 17, 0.5])
        color = Color("rgba(255,127,255,0.5)")
        self.assertEqual(str(color), "rgba(255, 127, 255, 0.5)")
        self.assertEqual(str(color.to_rgb()), "#ff7fff")
        self.assertEqual(color.to_rgb().to_rgba(0.75), [255, 127, 255, 0.75])
        color[3] = 1.0
        self.assertEqual(str(color), "rgb(255, 127, 255)")

    def test_hsl_color(self):
        """Parse HSL colors"""
        color = Color("hsl(4.0, 128, 99)")
        self.assertEqual(color, [4, 128, 99])
        self.assertEqual(str(color), "hsl(4, 128, 99)")
        self.assertEqual(color.hue, 4)
        self.assertEqual(color.saturation, 128)
        self.assertEqual(color.lightness, 99)

    def test_hsl_to_rgb(self):
        """Convert HSL to RGB"""
        color = Color("hsl(172, 131, 128)")
        self.assertEqual(color.to_rgb(), [68, 62, 193])
        self.assertEqual(str(color.to_rgb()), "#443ec1")
        self.assertEqual(color.red, 68)
        self.assertEqual(color.green, 62)
        self.assertEqual(color.blue, 193)
        color = Color("hsl(172, 131, 10)")
        self.assertEqual(color.to_rgb(), [5, 4, 15])
        color = Color("hsl(0, 131, 10)")
        self.assertEqual(color.to_rgb(), [15, 4, 4])
        self.assertEqual(color.to_rgba(), [15, 4, 4, 1.0])

    def test_hsl_grey(self):
        """Parse HSL Grey"""
        color = Color("hsl(172, 0, 128)")
        self.assertEqual(color.to_rgb(), [128, 128, 128])
        color = Color("rgb(128, 128, 128)")
        self.assertEqual(color.to_hsl(), [0, 0, 128])

    def test_is_color(self):
        """Can detect colour format"""
        self.assertFalse(is_color("rgb[t, b, s]"))
        self.assertTrue(is_color("#fff"))
        self.assertTrue(is_color(1364325887))

    def test_int_color(self):
        """Colours from arg parser"""
        color = Color(1364325887)
        self.assertEqual(str(color), "#5151f5")
        color = Color("1364325887")
        self.assertEqual(str(color), "#5151f5")
        color = Color(0xFFFFFFFF)
        self.assertEqual(str(color), "#ffffff")
        color = Color(0xFFFFFF00)
        self.assertEqual(str(color), "rgba(255, 255, 255, 0)")
        self.assertEqual(int(Color("#808080")), 0x808080FF)
        self.assertEqual(int(Color("rgba(128, 128, 128, 0.2)")), 2155905075)

    def test_interpolate(self):
        black = Color("#000000")
        grey50 = Color("#080808")
        white = Color("#111111")
        val = black.interpolate(white, 0.5)
        assert val == grey50
