# coding=utf-8
from restack import Restack
from inkex.tester import ComparisonMixin, TestCase

class RestackBasicTest(ComparisonMixin, TestCase):
    effect_class = Restack
    comparisons = [
        ('--tab=positional', '--id=p1', '--id=r3'),
        ('--tab=z_order', '--id=p1', '--id=r3'),
    ]
