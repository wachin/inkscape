#!/usr/bin/env python
# coding=utf-8
import inkex
from edge3d import Edge3D
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy, CompareWithPathSpace

class Edge3dBasicTest(ComparisonMixin, TestCase):
    effect_class = Edge3D
    compare_filters = [CompareNumericFuzzy(), CompareWithPathSpace()]
    comparisons = [('--id=p1', '--id=r3'),]

    def test_basic(self):
        args = ['--id=edgeme',
                self.data_file('svg', 'edge3d.svg')]
        self.effect.run(args)
        old_paths = self.effect.original_document.getroot()\
            .xpath('//svg:path[@id="edgeme"]')
        new_paths = self.effect.svg.xpath('//svg:path[@id="edgeme"]')
        self.assertEqual(len(old_paths), 1)
        self.assertEqual(len(new_paths), 1)
        old_paths = self.effect.original_document.getroot().xpath('//svg:path')
        new_paths = self.effect.svg.xpath('//svg:path')
        self.assertEqual(len(old_paths), 1)
        self.assertEqual(len(new_paths), 4)
