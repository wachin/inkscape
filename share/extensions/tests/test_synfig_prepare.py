# coding=utf-8
from synfig_prepare import SynfigPrep
from inkex.tester import InkscapeExtensionTestMixin, TestCase

class TestSynfigPrepBasic(InkscapeExtensionTestMixin, TestCase):
    effect_class = SynfigPrep
