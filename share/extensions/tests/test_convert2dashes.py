# coding=utf-8
from convert2dashes import Dashit
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase


class DashitBasicTest(ComparisonMixin, TestCase):
    comparisons = ([],)
    effect_class = Dashit

    def test_basic(self):
        args = ["--id=dashme", self.data_file("svg", "dash.svg")]
        self.effect.run(args)
        old_dashes = (
            self.effect.original_document.getroot().getElement("//svg:path").path
        )
        new_dashes = self.effect.svg.getElement("//svg:path").path
        assert len(new_dashes) > len(old_dashes)


class DashitCommaTest(ComparisonMixin, TestCase):
    comparisons = (["--id=dashme2"],)
    effect_class = Dashit
    compare_file = ["svg/dash.svg", "svg/dash_zerolength.svg"]
