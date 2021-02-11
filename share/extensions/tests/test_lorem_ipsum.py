# coding=utf-8
from lorem_ipsum import LoremIpsum
from inkex.tester import ComparisonMixin, TestCase

class LorumIpsumBasicTest(ComparisonMixin, TestCase):
    effect_class = LoremIpsum
    comparisons = [()]
