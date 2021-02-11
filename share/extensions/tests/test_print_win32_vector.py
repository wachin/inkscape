# coding=utf-8
import sys

import pytest

from print_win32_vector import PrintWin32Vector
from inkex.tester import InkscapeExtensionTestMixin, TestCase

@pytest.mark.skipif(sys.platform != 'win32', reason="Only runs on windows")
class TestPrintWin32VectorBasic(InkscapeExtensionTestMixin, TestCase):
    effect_class = PrintWin32Vector
