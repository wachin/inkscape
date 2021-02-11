#!/usr/bin/en
# coding=utf-8
from jessyink_effects import JessyinkEffects
from inkex.tester import ComparisonMixin, TestCase

class JessyInkEffectsTest(ComparisonMixin, TestCase):
    effect_class = JessyinkEffects
    comparisons = [
        ('--id=p1', '--id=r3'),
        ('--id=p1', '--effectIn=fade', '--effectOut=pop'),
    ]
    python3_only = True
