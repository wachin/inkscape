# coding=utf-8
import pytest

from prepare_file_save_as import PreProcess
from inkex.tester import ComparisonMixin, TestCase

class TestPrepareFileSaveBasic(ComparisonMixin, TestCase):
    effect_class = PreProcess
    comparisons = [()]
