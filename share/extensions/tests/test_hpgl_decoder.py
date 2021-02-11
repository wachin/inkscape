# coding=utf-8
from hpgl_decoder import hpglDecoder


class Options(object):
    """
    A dummy class because hpglDecoder expects an object as it's second argument
    TODO: This requirement probably needs to be factored out of the original code.
    """

    def __init__(self):
        self.resolutionX = None
        self.resolutionY = None
        self.docHeight = None


class TesthpglDecoderBasic(object):
    def test_init_values_scale(self):
        x = Options()
        x.resolutionX = 25.4
        x.resolutionY = 25.4
        h = hpglDecoder("", x)

        assert h.scaleX == 1
        assert h.scaleY == 1
