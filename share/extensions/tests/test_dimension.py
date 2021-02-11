# coding=utf-8
from dimension import Dimension
from inkex.tester import ComparisonMixin, TestCase

class TestDimensionBasic(ComparisonMixin, TestCase):
    effect_class = Dimension
    comparisons = [
        ('--id=p1', '--id=r3'),
        ('--id=p1', '--id=r3', '--type=visual'),
    ]
