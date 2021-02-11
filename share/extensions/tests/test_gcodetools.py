# coding=utf-8

import sys
import os

from gcodetools import Gcodetools
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase
from inkex.tester.filters import CompareOrderIndependentBytes

SETTINGS = (
    '--id=p1', '--max-area-curves=100',
    '--area-inkscape-radius=-10', '--area-tool-overlap=0',
    '--area-fill-angle=0', '--area-fill-shift=0', '--area-fill-method=0',
    '--area-fill-method=0', '--area-find-artefacts-diameter=5',
    '--area-find-artefacts-action=mark with an arrow',
    '--biarc-tolerance=1', '--biarc-max-split-depth=4',
    '--path-to-gcode-order=subpath by subpath',
    '--path-to-gcode-depth-function=d',
    '--path-to-gcode-sort-paths=false', '--Zscale=1', '--Zoffset=0',
    '--auto_select_paths=true', '--min-arc-radius=0.05000000074505806',
    '--comment-gcode-from-properties=false', '--create-log=false',
    '--add-numeric-suffix-to-filename=false', '--Zsafe=5',
    '--unit=G21 (All units in mm)', '--postprocessor= ',
)
FILESET = SETTINGS + ('--directory=/home', '--filename=output.ngc',)

class TestGcodetoolsBasic(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    stderr_protect = False
    effect_class = Gcodetools
    comparisons = [
        FILESET + ('--active-tab="area_fill"',),
        FILESET + ('--active-tab="area"',),
        FILESET + ('--active-tab="area_artefacts"',),
        FILESET + ('--active-tab="dxfpoints"',),
        FILESET + ('--active-tab="orientation"',),
        FILESET + ('--active-tab="tools_library"',),
        FILESET + ('--active-tab="lathe_modify_path"',),
        FILESET + ('--active-tab="offset"',),
    ]
    compare_filters = [CompareOrderIndependentBytes()]

    def test_all_comparisons(self):
        """
        gcodetools tries to write to a folder and filename specified
        on the command line, this needs to be handled carefully.
        """
        for tab in (
                ('--active-tab="path-to-gcode"',),
                #('--active-tab="engraving"',),
                #('--active-tab="graffiti"',),
                ('--active-tab="lathe"',),
            ):
            args = SETTINGS + tab + (
                '--directory={}'.format(self.tempdir),
                '--filename=output.ngc',
            )
            self.assertEffect(self.compare_file, args=args)

            outfile = os.path.join(self.tempdir, 'output.ngc')
            self.assertTrue(os.path.isfile(outfile), "No output file created! {}".format(outfile))

            with open(outfile, 'rb') as fhl:
                data_a = fhl.read()

            self.assertTrue(data_a, "No data produced with {}".format(tab))

            outfile = self.get_compare_outfile(args)
            if os.environ.get('EXPORT_COMPARE', False):
                with open(outfile + '.export', 'wb') as fhl:
                    fhl.write(data_a)
                    print("Written output: {}.export".format(outfile))

            with open(outfile, 'rb') as fhl:
                data_b = fhl.read()

            self.assertEqual(data_a, data_b)

if sys.version_info[0] == 3:
    # This changes output between python2 and python3, we don't know
    # why and don't have the gcodetool developers to help us understand.
    TestGcodetoolsBasic.comparisons.append(
        FILESET + ('--active-tab="plasma-prepare-path"',),
    )
