#!/usr/bin/env python3
# coding=utf-8
"""
Test the filter elements functionality
"""
from inkex.tester import TestCase
from inkex.tester.svg import svg_file


class GradientTestCase(TestCase):
    source_file = "gradient_with_mixed_offsets.svg"

    def setUp(self):
        super().setUp()
        self.svg = svg_file(self.data_file("svg", self.source_file))

    def test_gradient_offset_order(self):
        _gradient = self.svg.getElementById("MyGradient")
        offsets = [stop.attrib.get("offset") for stop in _gradient.stops]
        assert offsets == ["0%", "50", "100%"]
