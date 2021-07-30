# coding=utf-8
from restack import Restack
from inkex.tester import ComparisonMixin, TestCase

class RestackBasicTest(ComparisonMixin, TestCase):
    effect_class = Restack
    comparisons = [
        ('--tab=positional', '--id=p1', '--id=r3'),
        ('--tab=z_order', '--id=p1', '--id=r3'),
        ('--tab=z_order', '--id=r3', '--id=p1', '--id=t5', '--id=r2'),
        ('--tab=z_order', '--id=r2', '--id=t5', '--id=p1', '--id=r3'),
        ('--nb_direction=custom', '--angle=50.0', '--id=s1', '--id=p1', '--id=c3', '--id=slicerect1'),
    ]
