# coding=utf-8
from polyhedron_3d import Poly3D
from inkex.tester import ComparisonMixin, TestCase

class Poly3DBasicTest(ComparisonMixin, TestCase):
    effect_class = Poly3D
    comparisons = [
        ('--show=fce', '--obj=cube', '--r1_ax=x', '--r1_ang=45', '--r2_ax=y', '--r2_ang=45'),
        ('--show=fce', '--obj=cube', '--r1_ax=y', '--r1_ang=45', '--z_sort=cent'),
        ('--show=fce', '--obj=cube', '--r1_ax=z', '--r1_ang=45', '--z_sort=max'),
        ('--show=edg', '--obj=oct', '--r1_ax=z', '--r1_ang=45', '--th=4'),
        ('--show=vtx', '--obj=methane',),
    ]
    compare_file = 'svg/empty.svg'
