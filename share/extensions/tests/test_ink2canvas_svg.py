#!/usr/bin/en
# coding=utf-8
from ink2canvas import Html5Canvas
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentLines
from inkex.tester.filters import WindowsTextCompat


class Ink2CanvasBasicTest(ComparisonMixin, TestCase):
    effect_class = Html5Canvas
    compare_file = "svg/shapes-clipboard.svg"
    compare_filters = [CompareOrderIndependentLines()]
    comparisons = [()]


class Ink2CanvasTestTextPath(ComparisonMixin, TestCase):
    effect_class = Html5Canvas
    compare_file = "svg/multilayered-test.svg"
    # This file contains a textPath
    compare_filters = [CompareOrderIndependentLines()]
    # We don't need a selection for this case, but we need unique filenames for the tester
    comparisons = [("--id=rect3898",)]


class Ink2CanvasTestClosedPath(ComparisonMixin, TestCase):
    effect_class = Html5Canvas
    compare_file = "svg/multiple_closed_subpaths.svg"
    comparisons = [("--id=path31",)]
    compare_filters = [WindowsTextCompat()]


class Ink2CanvasTestCSS(ComparisonMixin, TestCase):
    """Test CSS styles"""

    effect_class = Html5Canvas
    compare_file = "svg/styling-css-04-f.svg"
    comparisons = [("--id=alpha",)]
    compare_filters = [WindowsTextCompat()]
