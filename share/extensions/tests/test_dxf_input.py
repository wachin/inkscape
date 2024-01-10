# coding=utf-8

from dxf_input import DxfInput

from inkex.tester import ComparisonMixin, TestCase
from inkex.tester.filters import CompareNumericFuzzy


class TestDxfInputBasic(ComparisonMixin, TestCase):
    compare_file = [
        "io/test_r12.dxf",
        "io/test_r14.dxf",
        # Unit test for https://gitlab.com/inkscape/extensions/-/issues/355
        "io/dxf_with_arc.dxf",
        # test polylines
        "io/dxf_polylines.dxf",
        # File missing a BLOCKS session
        "io/no_block_section.dxf",
        # test placement of graphical objects from BLOCKS section
        "io/dxf_multiple_inserts.dxf",
        # test correct colors generated
        # currently BYLAYER and BYBLOCK colors in inserted block are wrong
        "io/color.dxf",
        "io/test_input_rotated_ellipse_r14.dxf",
        "io/test_one_blankline_at_the_end.dxf",
    ]
    compare_filters = [CompareNumericFuzzy()]
    comparisons = [()]
    effect_class = DxfInput

    def _apply_compare_filters(self, data, is_saving=None):
        """Remove the full pathnames"""
        if is_saving is True:
            return data
        data = super()._apply_compare_filters(data)
        return data.replace((self.datadir() + "/").encode("utf-8"), b"")


class TestDxfInputBasicError(ComparisonMixin, TestCase):
    TestCase.stderr_protect = False
    # sample uses POLYLINE,TEXT (R12), LWPOLYLINE,MTEXT (R13, R14)
    # however has warnings when handling points with a display mode
    compare_file = [
        "io/test2_r12.dxf",
        "io/test2_r13.dxf",
        "io/test2_r14.dxf",
        "io/test_extrude.dxf",
    ]
    compare_filters = [CompareNumericFuzzy()]
    comparisons = [()]
    effect_class = DxfInput

    def _apply_compare_filters(self, data, is_saving=None):
        """Remove the full pathnames"""
        if is_saving is True:
            return data
        data = super()._apply_compare_filters(data)
        return data.replace((self.datadir() + "/").encode("utf-8"), b"")


class TestDxfInputTextHeight(ComparisonMixin, TestCase):
    compare_file = ["io/CADTextHeight.dxf"]
    compare_filters = [CompareNumericFuzzy()]
    comparisons = [(), ("--textscale=1.411",)]
    effect_class = DxfInput

    def _apply_compare_filters(self, data, is_saving=None):
        """Remove the full pathnames"""
        if is_saving is True:
            return data
        data = super()._apply_compare_filters(data)
        return data.replace((self.datadir() + "/").encode("utf-8"), b"")
