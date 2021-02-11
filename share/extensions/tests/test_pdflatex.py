# coding=utf-8
"""
Test calling pdflatex to convert formule to svg.

This test uses cached output from the `pdflatex` command because this
test is not a test of pdflatex, but of the extension only. The mocked
output also allows testing in the CI builder without dependancies.

To re-generate the cached files, run the pytest command:

NO_MOCK_COMMANDS=1 pytest tests/test_pdflatex.py

This will use pdflatex, but will also store the output of the call
to `tests/data/cmd/pdflatex/[key].msg.output (and also to `cmd/inkscape/...`)

The key depends on the comparison arguments, so changing them will invalidate
the file and you must regenerate them.

Remove the `.output` extension from the above file and commit it to the
repository only AFTER all the tests pass and you are happy with them.

Clean up any old `.msg` files with invalid or old keys.

(use EXPORT_COMPARE to generate the output svgs, see inkex.tester docs)
"""
from pdflatex import PdfLatex
from inkex.tester import ComparisonMixin, TestCase

class PdfLatexTest(ComparisonMixin, TestCase):
    compare_file = 'svg/empty.svg'
    effect_class = PdfLatex
    comparisons = [
        ('--formule=\\(\\displaystyle\\frac{\\pi^2}{6}=\\lim_{n \\to \\infty}\\sum_{k=1}^n \\frac{1}{k^2}\\)', '--packages='),
    ]
