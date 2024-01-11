# coding=utf-8
#
# Copyright (C) 2005 Aaron Spike, aaron@ekips.org
#               2019-2020 Martin Owens
#               2021 Jonathan Neuhauser, jonathan.neuhauser@outlook.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
"""
Functions for handling styles and embedded css
"""

import re
import sys
from collections import OrderedDict
from typing import MutableMapping, Union, Iterable, TYPE_CHECKING
from lxml import etree

from .interfaces.IElement import IBaseElement

from .colors import Color
from .properties import BaseStyleValue, all_properties, ShorthandValue
from .css import CSSCompiler, parser

from .utils import FragmentError, NotifyList, NotifyOrderedDict
from .elements._utils import NSS

if TYPE_CHECKING:
    from .elements._svg import SvgDocumentElement


class Classes(NotifyList):
    """A list of classes applied to an element (used in css and js)"""

    def __init__(self, classes=None, callback=None, element=None):
        if isinstance(classes, str):
            classes = classes.split()
        super().__init__(classes or (), callback=callback)

    def __str__(self):
        return " ".join(self)


class Style(NotifyOrderedDict, MutableMapping[str, Union[str, BaseStyleValue]]):
    """A list of style directives

    .. versionchanged:: 1.2
        The Style API now allows for access to parsed / processed styles via the
        :func:`call` method.

    .. automethod:: __call__
    .. automethod:: __getitem__
    .. automethod:: __setitem__
    """

    color_props = ("stroke", "fill", "stop-color", "flood-color", "lighting-color")
    opacity_props = ("stroke-opacity", "fill-opacity", "opacity", "stop-opacity")
    unit_props = "stroke-width"
    """Dictionary of attributes with units. 
    
    ..versionadded:: 1.2
    """
    associated_props = {
        "fill": "fill-opacity",
        "stroke": "stroke-opacity",
        "stop-color": "stop-opacity",
    }
    """Dictionary of association between color and opacity attributes.

    .. versionadded:: 1.2
    """

    def __init__(self, style=None, callback=None, element=None, **kw):
        self.element = element
        # Either a string style or kwargs (with dashes as underscores).
        style = style or [(k.replace("_", "-"), v) for k, v in kw.items()]
        if isinstance(style, str):
            style = self._parse_str(style, element)
        # Order raw dictionaries so tests can be made reliable
        if isinstance(style, dict) and not isinstance(style, OrderedDict):
            style = [(name, style[name]) for name in sorted(style)]
        # Should accept dict, Style, parsed string, list etc.
        super().__init__(style, callback=callback)

    def _attr_callback(self, key):
        def inner(value):
            self[key] = value

        return inner

    def _parse_str(self, style: str, element=None) -> Iterable[BaseStyleValue]:
        """Create a dictionary from the value of a CSS rule (such as an inline style or
        from an embedded style sheet), including its !important state, parsing the value
        if possible.

        Args:
            style: the content of a CSS rule to parse
            element: the element this style is working on (can be the root SVG, is used
                for parsing gradients etc.)

        Yields:
            :class:`~inkex.properties.BaseStyleValue`: the parsed attribute
        """
        for declaration in style.split(";"):
            if ":" in declaration:
                result = BaseStyleValue.factory_errorhandled(
                    element, declaration=declaration.strip()
                )
                result[1].callback = self._attr_callback(result[0])
                if result is not None:
                    yield result

    @staticmethod
    def parse_str(style: str, element=None):
        """Parse a style passed as string"""
        return Style(style, element=element)

    def __str__(self):
        """Format an inline style attribute from a dictionary"""
        return self.to_str()

    def to_str(self, sep=";"):
        """Convert to string using a custom delimiter"""
        return sep.join([self.get_store(key).declaration for key in self])

    def __add__(self, other):
        """Add two styles together to get a third, composing them"""
        ret = self.copy()
        ret.update(Style(other))
        return ret

    def __iadd__(self, other):
        """Add style to this style, the same as ``style.update(dict)``"""
        self.update(other)
        return self

    def __sub__(self, other):
        """Remove keys and return copy"""
        ret = self.copy()
        ret.__isub__(other)
        return ret

    def __isub__(self, other):
        """Remove keys from this style, list of keys or other style dictionary"""
        for key in other:
            self.pop(key, None)
        return self

    def __ne__(self, other):
        return not self.__eq__(other)

    def copy(self):
        """Create a copy of the style.

        .. versionadded:: 1.2"""
        ret = Style({}, element=self.element)
        for key, value in super().items():
            ret[key] = value
        return ret

    def update(self, other):
        """Update, while respecting ``!important`` declarations."""
        if not isinstance(other, Style):
            other = Style(other)
        # only update
        if isinstance(other, Style):
            for key in other.keys():
                if not (self.get_importance(key) and not other.get_importance(key)):
                    self[key] = other.get_store(key)

    def add_inherited(self, parent):
        """Creates a new Style containing all parent styles with importance "!important"
        and current styles with importance "!important"

        .. versionadded:: 1.2

        Args:
            parent: the parent style that will be merged into this one (will not be
                altered)

        Returns:
            Style: the merged Style object
        """
        ret = self.copy()
        ret.apply_shorthands()  # parent should already have its shortcuts applied

        if not (isinstance(parent, Style)):
            return ret

        for key in parent.keys():
            apply = False
            if key in all_properties and all_properties[key][3]:
                # only set parent value if value is not set or parent importance is
                # higher
                if key not in ret:
                    apply = True
                elif self.get_importance(key) != parent.get_importance(key):
                    apply = parent.get_importance(key)
            if key in ret and ret[key] == "inherit":
                apply = True
            if apply:
                ret[key] = parent[key]
        return ret

    def apply_shorthands(self):
        """Apply all shorthands in this style."""
        for element in list(self.values()):
            if isinstance(element, ShorthandValue):
                element.apply_shorthand(self)

    def __setitem__(self, key, value):
        """Sets a style value.

        .. versionchanged:: 1.2
            ``value`` can now also be non-string objects such as a Gradient.

        Args:
            key (str): the attribute name
            value (Any):

                - a :class:`BaseStyleValue`
                - a string with the value
                - any other object. The :class:`~inkex.properties.BaseStyleValue`
                  subclass of the provided key will attempt to create a string out of
                  the passed value.
        Raises:
            ValueError: when ``value`` is a :class:`~inkex.properties.BaseStyleValue`
                for a different attribute than `key`
            Error: Other exceptions may be raised when converting non-string objects."""
        if not isinstance(value, BaseStyleValue) or value is None:
            # try to convert the value using the factory
            value = BaseStyleValue.factory(
                attr_name=key, value=value, element=self.element
            )
            value.callback = self._attr_callback(key)
            # check if the set attribute is valid
            _ = value.parse_value(self.element)
        elif key != value.attr_name:
            raise ValueError(
                """You're trying to save a value into a style attribute, but the
                provided key is different from the attribute name given in the value"""
            )
        super().__setitem__(key, value)

    def __getitem__(self, key):
        """Returns the unparsed value of the element (minus a possible ``!important``)

        .. versionchanged:: 1.2
            ``!important`` is removed from the value.
        """
        return self.get_store(key).value

    def get(self, key, default=None):
        if key in self:
            return self.__getitem__(key)
        return default

    def get_store(self, key):
        """Gets the :class:`~inkex.properties.BaseStyleValue` of this key, since the
        other interfaces - :func:`__getitem__` and :func:`__call__` - return the
        original and parsed value, respectively.

        .. versionadded:: 1.2

        Args:
            key (str): the attribute name

        Returns:
            BaseStyleValue: the BaseStyleValue struct of this attribute
        """
        return super().__getitem__(key)

    def __call__(self, key, element=None, default=None):
        """Return the parsed value of a style. Optionally, an element can be passed
        that will be used to find gradient definitions etc.

        .. versionadded:: 1.2"""
        # check if there are shorthand properties defined. If so, apply them to a copy
        copy = self
        for value in super().values():
            if isinstance(value, ShorthandValue):
                copy = self.copy()
                copy.apply_shorthands()
        if key in copy:
            return copy.get_store(key).parse_value(element or self.element)
        # style is not set, return the default value
        if key in all_properties or default is not None:
            defvalue = BaseStyleValue.factory(
                attr_name=key, value=default or all_properties[key][1], element=element
            )
            defvalue.callback = self._attr_callback(key)
            return (
                defvalue.parse_value()
            )  # default values are independent of the element
        raise KeyError("Unknown attribute")

    def __eq__(self, other):
        if not isinstance(other, Style):
            other = Style(other)
        if self.keys() != other.keys():
            return False
        for arg in set(self) | set(other):
            if self.get_store(arg) != other.get_store(arg):
                return False
        return True

    def items(self):
        """The styles's parsed items

        .. versionadded:: 1.2"""
        for key, value in super().items():
            yield key, value.value

    def get_importance(self, key, default=False):
        """Returns whether the declaration with ``key`` is marked as ``!important``

        .. versionadded:: 1.2"""
        if key in self:
            return super().__getitem__(key).important
        return default

    def set_importance(self, key, importance):
        """Sets the ``!important`` state of a declaration with key ``key``

        .. versionadded:: 1.2"""
        if key in self:
            super().__getitem__(key).important = importance
        else:
            raise KeyError()
        self._callback()

    def get_color(self, name="fill"):
        """Get the color AND opacity as one Color object"""
        color = Color(self.get(name, "none"))
        return color.to_rgba(self.get(name + "-opacity", 1.0))

    def set_color(self, color, name="fill"):
        """Sets the given color AND opacity as rgba to the fill or stroke style
        properties."""
        color = Color(color)
        if color.space == "rgba" and name in Style.associated_props:
            self[Style.associated_props[name]] = color.alpha
            self[name] = color.to_rgb()
        else:
            self[name] = color

    def update_urls(self, old_id, new_id):
        """Find urls in this style and replace them with the new id"""
        for name, value in self.items():
            if value == f"url(#{old_id})":
                self[name] = f"url(#{new_id})"

    def interpolate(self, other, fraction):
        # type: (Style, Style, float) -> Style
        """Interpolate all properties.

        .. versionadded:: 1.1"""
        from .tween import StyleInterpolator
        from inkex.elements import PathElement

        if self.element is None:
            self.element = PathElement(style=str(self))
        if other.element is None:
            other.element = PathElement(style=str(other))
        return StyleInterpolator(self.element, other.element).interpolate(fraction)

    @classmethod
    def cascaded_style(cls, element):
        """Returns the cascaded style of an element (all rules that apply the element
        itself), based on the stylesheets, the presentation attributes and the inline
        style using the respective specificity of the style

        see https://www.w3.org/TR/CSS22/cascade.html#cascading-order

        .. versionadded:: 1.2

        Args:
            element (BaseElement): the element that the cascaded style will be
                computed for

        Returns:
            Style: the cascaded style
        """
        try:
            styles = list(element.root.stylesheets.lookup_specificity(element))
        except FragmentError:
            styles = []

        # presentation attributes have specificity 0,
        # see https://www.w3.org/TR/SVG/styling.html#PresentationAttributes
        styles.append([element.presentation_style(), (0, 0, 0)])

        # would be (1, 0, 0, 0), but then we'd have to extend every entry
        styles.append([element.style, (float("inf"), 0, 0)])

        # sort styles by specificity (ascending, so when overwriting it's correct)
        styles = sorted(styles, key=lambda item: item[1])

        result = styles[0][0].copy()
        for style, _ in styles[1:]:
            result.update(style)
        result.element = element
        return result

    @classmethod
    def specified_style(cls, element):
        """Returns the specified style of an element, i.e. the cascaded style +
        inheritance, see https://www.w3.org/TR/CSS22/cascade.html#specified-value

        .. versionadded:: 1.2

        Args:
            element (BaseElement): the element that the specified style will be computed
                for

        Returns:
            Style: the specified style
        """

        # We currently dont treat the case where parent=absolute value and
        # element=relative value, i.e. specified = relative * absolute.
        cascaded = Style.cascaded_style(element)

        parent = element.getparent()

        if parent is not None and isinstance(parent, IBaseElement):
            cascaded = Style.add_inherited(cascaded, parent.specified_style())
        cascaded.element = element
        return cascaded  # doesn't have a parent


class StyleSheets(list):
    """
    Special mechanism which contains all the stylesheets for an svg document
    while also caching lookups for specific elements.

    This caching is needed because data can't be attached to elements as they are
    re-created on the fly by lxml so lookups have to be centralised.
    """

    def lookup(self, element):
        """
        Find all styles for this element.
        """
        for sheet in self:
            for style in sheet.lookup(element):
                yield style

    def lookup_specificity(self, element):
        """
        Find all styles for this element and return the specificity of the match.

        .. versionadded:: 1.2
        """
        for sheet in self:
            for style in sheet.lookup_specificity(element):
                yield style


class StyleSheet(list):
    """
    A style sheet, usually the CDATA contents of a style tag, but also
    a css file used with a css. Will yield multiple Style() classes.
    """

    comment_strip = re.compile(r"(\/\/.*?\n)|(\/\*.*?\*\/|@import .*;)")

    def __init__(self, content=None, callback=None):
        super().__init__()
        self.callback = None
        # Remove comments
        content = self.comment_strip.sub("", (content or ""))
        # Parse rules
        for block in content.split("}"):
            if block:
                self.append(block)
        self.callback = callback

    def __str__(self):
        return "\n" + "\n".join([str(style) for style in self]) + "\n"

    def _callback(self, style=None):  # pylint: disable=unused-argument
        if self.callback is not None:
            self.callback(self)

    def add(self, rule, style):
        """Append a rule and style combo to this stylesheet"""
        self.append(
            ConditionalStyle(rules=rule, style=str(style), callback=self._callback)
        )

    def append(self, other):
        """Make sure callback is called when updating"""
        if isinstance(other, str):
            if "{" not in other:
                return  # Warning?
            rules, style = other.strip("}").split("{", 1)
            if rules.strip().startswith("@"):  # ignore @font-face and @import
                return
            other = ConditionalStyle(
                rules=rules, style=style.strip(), callback=self._callback
            )
        super().append(other)
        self._callback()

    def lookup(self, element):
        """Lookup the element against all the styles in this sheet"""
        for style in self:
            if any(style.checks(element)):
                yield style

    def lookup_specificity(self, element):
        """Lookup the element_id against all the styles in this sheet
        and return the specificity of the match

        Args:
            element: the element of the element that styles are being queried for

        Yields:
            Tuple[ConditionalStyle, Tuple[int, int, int]]: all matched styles and the
            specificity of the match
        """
        for style in self:
            for rule, check in zip(style.rules, style.checks):
                if check(element):
                    yield (style, rule.specificity)


class ConditionalStyle(Style):
    """
    Just like a Style object, but includes one or more
    conditional rules which places this style in a stylesheet
    rather than being an attribute style.
    """

    def __init__(self, rules="*", style=None, callback=None, **kwargs):
        super().__init__(style=style, callback=callback, **kwargs)
        self._rules: str = rules
        self.rules = list(parser.parse(rules, namespaces=NSS))
        self.checks = [
            CSSCompiler.compile_node(selector.parsed_tree) for selector in self.rules
        ]

    def matches(self, element: etree.Element):
        """Checks if an individual element matches this selector.

        .. versionadded:: 1.4"""
        if isinstance(element, etree._Comment):
            return False
        if any(check(element) for check in self.checks):
            return True
        return False

    def all_matches(self, document: etree.Element):
        """Get all matches of this selector in document as iterator.

        .. versionadded:: 1.4"""
        for el in document.iter():
            if self.matches(el):
                yield el

    def __str__(self):
        """Return this style as a css entry with class"""
        content = self.to_str(";\n  ")
        rules = ",\n".join(str(rule) for rule in self.rules)
        if content:
            return f"{rules} {{\n  {content};\n}}"
        return f"{rules} {{}}"

    def get_specificities(self):
        """Gets an iterator of the specificity of all rules in this ConditionalStyle

        .. versionadded:: 1.2"""
        for rule in self.rules:
            yield rule.specificity
