# coding=utf-8
from path_number_nodes import NumberNodes
from inkex.tester import ComparisonMixin, TestCase

class NumberNodesTest(ComparisonMixin, TestCase):
    effect_class = NumberNodes
    comparisons = [('--id=p1', '--id=r3')]
