# coding=utf-8
from render_barcode_qrcode import QrCode
from inkex.tester import ComparisonMixin, TestCase

class TestQRCodeInkscapeBasic(ComparisonMixin, TestCase):
    """Test basic use of QR codes"""
    effect_class = QrCode
    compare_file = 'svg/empty.svg'
    comparisons = [
        ('--text=0123456789', '--typenumber=0'),
        ('--text=BreadRolls', '--typenumber=2', '--encoding=utf8'),
        ('--text=Blue Front Yard', '--typenumber=3', '--correctionlevel=1'),
        ('--text=Waterfall', '--typenumber=1', '--drawtype=circle'),
        ('--text=groupid', '--groupid=testid'),
    ]

class TestQRCodeInkscapeSymbol(ComparisonMixin, TestCase):
    """Test symbols in qr codes"""
    effect_class = QrCode
    compare_file = 'svg/symbol.svg'
    comparisons = [
        ('--text=ThingOne', '--drawtype=symbol', '--correctionlevel=2',
         '--symbolid=AirTransportation_Inv'),
    ]
