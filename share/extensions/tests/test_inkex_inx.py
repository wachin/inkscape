#!/usr/bin/env python
# coding=utf-8
"""
Test elements extra logic from svg xml lxml custom classes.
"""

import os
from glob import glob

from inkex.utils import PY3
from inkex.tester import TestCase
from inkex.tester.inx import InxMixin

class InxTestCase(InxMixin, TestCase):
    """Test INX files"""
    def test_inx_files(self):
        """Get all inx files and test each of them"""
        if not PY3:
            self.skipTest("No INX testing in python2")
            return
        for inx_file in glob(os.path.join(self._testdir(), '..', '*.inx')):
            self.assertInxIsGood(inx_file)
