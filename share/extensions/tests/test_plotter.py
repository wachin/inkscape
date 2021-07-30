"""Test Plotter extension"""
from plotter import Plot
from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareReplacement

class TestPlotter(ComparisonMixin, TestCase):
    """Test the plotter extension"""
    stderr_output = True
    effect_class = Plot
    compare_filter_save = True
    compare_filters = [
        CompareReplacement((';', '\n'))
    ]
    comparisons = [
        ('--serialPort=[test]',), # HPGL
        ('--serialPort=[test]', '--commandLanguage=DMPL'),
        ('--serialPort=[test]', '--commandLanguage=KNK'),
    ]