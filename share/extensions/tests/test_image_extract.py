# coding=utf-8

import os
from image_extract import ExtractImage
from inkex.tester import ComparisonMixin, InkscapeExtensionTestMixin, TestCase

class ExtractImageBasicTest(ComparisonMixin, InkscapeExtensionTestMixin, TestCase):
    stderr_protect = False
    effect_class = ExtractImage
    compare_file = 'svg/images.svg'
    comparisons = [
        ('--selectedonly=False',),
        ('--selectedonly=True', '--id=embeded_image01'),
    ]

    def test_all_comparisons(self):
        """Images are extracted to a file directory"""
        for args in self.comparisons:
            outdir = os.path.join(self.tempdir, 'img')
            args += ('--filepath={}/'.format(outdir),)
            self.assertEffect(self.compare_file, args=args)

            outfile = os.path.join(outdir, 'embeded_image01.png')
            self.assertTrue(os.path.isfile(outfile), "No output file created! {}".format(outfile))

            with open(outfile, 'rb') as fhl:
                data_a = fhl.read()

            self.assertTrue(data_a, "No data produced with {}".format(args))

            outfile = self.get_compare_outfile(args)
            if os.environ.get('EXPORT_COMPARE', False):
                with open(outfile + '.export', 'wb') as fhl:
                    fhl.write(data_a)
                    print("Written output: {}.export".format(outfile))

            with open(outfile, 'rb') as fhl:
                data_b = fhl.read()

            self.assertEqual(data_a, data_b)
