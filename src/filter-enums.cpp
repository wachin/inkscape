// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Conversion data for filter and filter primitive enumerations
 *
 * Authors:
 *   Nicholas Bishop
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm.h>
#include <glibmm/i18n.h>

#include "filter-enums.h"

using Inkscape::Util::EnumData;
using Inkscape::Util::EnumDataConverter;

const EnumData<Inkscape::Filters::FilterPrimitiveType> FPData[Inkscape::Filters::NR_FILTER_ENDPRIMITIVETYPE] = {
    // clang-format off
    {Inkscape::Filters::NR_FILTER_BLEND,             _("Blend"),              "svg:feBlend"},
    {Inkscape::Filters::NR_FILTER_COLORMATRIX,       _("Color Matrix"),       "svg:feColorMatrix"},
    {Inkscape::Filters::NR_FILTER_COMPONENTTRANSFER, _("Component Transfer"), "svg:feComponentTransfer"},
    {Inkscape::Filters::NR_FILTER_COMPOSITE,         _("Composite"),          "svg:feComposite"},
    {Inkscape::Filters::NR_FILTER_CONVOLVEMATRIX,    _("Convolve Matrix"),    "svg:feConvolveMatrix"},
    {Inkscape::Filters::NR_FILTER_DIFFUSELIGHTING,   _("Diffuse Lighting"),   "svg:feDiffuseLighting"},
    {Inkscape::Filters::NR_FILTER_DISPLACEMENTMAP,   _("Displacement Map"),   "svg:feDisplacementMap"},
    {Inkscape::Filters::NR_FILTER_FLOOD,             _("Flood"),              "svg:feFlood"},
    {Inkscape::Filters::NR_FILTER_GAUSSIANBLUR,      _("Gaussian Blur"),      "svg:feGaussianBlur"},
    {Inkscape::Filters::NR_FILTER_IMAGE,             _("Image"),              "svg:feImage"},
    {Inkscape::Filters::NR_FILTER_MERGE,             _("Merge"),              "svg:feMerge"},
    {Inkscape::Filters::NR_FILTER_MORPHOLOGY,        _("Morphology"),         "svg:feMorphology"},
    {Inkscape::Filters::NR_FILTER_OFFSET,            _("Offset"),             "svg:feOffset"},
    {Inkscape::Filters::NR_FILTER_SPECULARLIGHTING,  _("Specular Lighting"),  "svg:feSpecularLighting"},
    {Inkscape::Filters::NR_FILTER_TILE,              _("Tile"),               "svg:feTile"},
    {Inkscape::Filters::NR_FILTER_TURBULENCE,        _("Turbulence"),         "svg:feTurbulence"}
    // clang-format on
};
const EnumDataConverter<Inkscape::Filters::FilterPrimitiveType> FPConverter(FPData, Inkscape::Filters::NR_FILTER_ENDPRIMITIVETYPE);

const EnumData<FilterPrimitiveInput> FPInputData[FPINPUT_END] = {
    // clang-format off
    {FPINPUT_SOURCEGRAPHIC,     _("Source Graphic"),     "SourceGraphic"},
    {FPINPUT_SOURCEALPHA,       _("Source Alpha"),       "SourceAlpha"},
    {FPINPUT_BACKGROUNDIMAGE,   _("Background Image"),   "BackgroundImage"},
    {FPINPUT_BACKGROUNDALPHA,   _("Background Alpha"),   "BackgroundAlpha"},
    {FPINPUT_FILLPAINT,         _("Fill Paint"),         "FillPaint"},
    {FPINPUT_STROKEPAINT,       _("Stroke Paint"),       "StrokePaint"},
    // clang-format on
};
const EnumDataConverter<FilterPrimitiveInput> FPInputConverter(FPInputData, FPINPUT_END);

const EnumData<Inkscape::Filters::FilterColorMatrixType> ColorMatrixTypeData[Inkscape::Filters::COLORMATRIX_ENDTYPE] = {
    // clang-format off
    {Inkscape::Filters::COLORMATRIX_MATRIX,           _("Matrix"),             "matrix"},
    {Inkscape::Filters::COLORMATRIX_SATURATE,         _("Saturate"),           "saturate"},
    {Inkscape::Filters::COLORMATRIX_HUEROTATE,        _("Hue Rotate"),         "hueRotate"},
    {Inkscape::Filters::COLORMATRIX_LUMINANCETOALPHA, _("Luminance to Alpha"), "luminanceToAlpha"}
    // clang-format on
};
const EnumDataConverter<Inkscape::Filters::FilterColorMatrixType> ColorMatrixTypeConverter(ColorMatrixTypeData, Inkscape::Filters::COLORMATRIX_ENDTYPE);

// feComposite
const EnumData<FeCompositeOperator> CompositeOperatorData[COMPOSITE_ENDOPERATOR] = {
    // clang-format off
    {COMPOSITE_DEFAULT,          _("Default"),         ""                 },
    {COMPOSITE_OVER,             _("Over"),            "over"             },
    {COMPOSITE_IN,               _("In"),              "in"               },
    {COMPOSITE_OUT,              _("Out"),             "out"              },
    {COMPOSITE_ATOP,             _("Atop"),            "atop"             },
    {COMPOSITE_XOR,              _("XOR"),             "xor"              },
#ifdef WITH_CSSCOMPOSITE
// New CSS
    {COMPOSITE_CLEAR,            _("Clear"),           "clear"            },
    {COMPOSITE_COPY,             _("Copy"),            "copy"             },
    {COMPOSITE_DESTINATION,      _("Destination"),     "destination"      },
    {COMPOSITE_DESTINATION_OVER, _("Destination Over"),"destination-over" },
    {COMPOSITE_DESTINATION_IN,   _("Destination In"),  "destination-in"   },
    {COMPOSITE_DESTINATION_OUT,  _("Destination Out"), "destination-out"  },
    {COMPOSITE_DESTINATION_ATOP, _("Destination Atop"),"destination-atop" },
    {COMPOSITE_LIGHTER,          _("Lighter"),         "lighter"          },
#endif
    {COMPOSITE_ARITHMETIC,       _("Arithmetic"),      "arithmetic"       }
    // clang-format on
};
const EnumDataConverter<FeCompositeOperator> CompositeOperatorConverter(CompositeOperatorData, COMPOSITE_ENDOPERATOR);

// feComponentTransfer
const EnumData<Inkscape::Filters::FilterComponentTransferType> ComponentTransferTypeData[Inkscape::Filters::COMPONENTTRANSFER_TYPE_ERROR] = {
    // clang-format off
    {Inkscape::Filters::COMPONENTTRANSFER_TYPE_IDENTITY, _("Identity"), "identity"},
    {Inkscape::Filters::COMPONENTTRANSFER_TYPE_TABLE,    _("Table"),    "table"},
    {Inkscape::Filters::COMPONENTTRANSFER_TYPE_DISCRETE, _("Discrete"), "discrete"},
    {Inkscape::Filters::COMPONENTTRANSFER_TYPE_LINEAR,   _("Linear"),   "linear"},
    {Inkscape::Filters::COMPONENTTRANSFER_TYPE_GAMMA,    _("Gamma"),    "gamma"},
    // clang-format on
};
const EnumDataConverter<Inkscape::Filters::FilterComponentTransferType> ComponentTransferTypeConverter(ComponentTransferTypeData, Inkscape::Filters::COMPONENTTRANSFER_TYPE_ERROR);

// feConvolveMatrix
const EnumData<Inkscape::Filters::FilterConvolveMatrixEdgeMode> ConvolveMatrixEdgeModeData[Inkscape::Filters::CONVOLVEMATRIX_EDGEMODE_ENDTYPE] = {
    // clang-format off
    {Inkscape::Filters::CONVOLVEMATRIX_EDGEMODE_DUPLICATE, _("Duplicate"), "duplicate"},
    {Inkscape::Filters::CONVOLVEMATRIX_EDGEMODE_WRAP,      _("Wrap"),      "wrap"},
    {Inkscape::Filters::CONVOLVEMATRIX_EDGEMODE_NONE,      C_("Convolve matrix, edge mode", "None"),      "none"}
    // clang-format on
};
const EnumDataConverter<Inkscape::Filters::FilterConvolveMatrixEdgeMode> ConvolveMatrixEdgeModeConverter(ConvolveMatrixEdgeModeData, Inkscape::Filters::CONVOLVEMATRIX_EDGEMODE_ENDTYPE);

// feDisplacementMap
const EnumData<FilterDisplacementMapChannelSelector> DisplacementMapChannelData[DISPLACEMENTMAP_CHANNEL_ENDTYPE] = {
    // clang-format off
    {DISPLACEMENTMAP_CHANNEL_RED, _("Red"),   "R"},
    {DISPLACEMENTMAP_CHANNEL_GREEN, _("Green"), "G"},
    {DISPLACEMENTMAP_CHANNEL_BLUE, _("Blue"),  "B"},
    {DISPLACEMENTMAP_CHANNEL_ALPHA, _("Alpha"), "A"}
    // clang-format on
};
const EnumDataConverter<FilterDisplacementMapChannelSelector> DisplacementMapChannelConverter(DisplacementMapChannelData, DISPLACEMENTMAP_CHANNEL_ENDTYPE);

// feMorphology
const EnumData<Inkscape::Filters::FilterMorphologyOperator> MorphologyOperatorData[Inkscape::Filters::MORPHOLOGY_OPERATOR_END] = {
    // clang-format off
    {Inkscape::Filters::MORPHOLOGY_OPERATOR_ERODE,  _("Erode"),   "erode"},
    {Inkscape::Filters::MORPHOLOGY_OPERATOR_DILATE, _("Dilate"),  "dilate"}
    // clang-format on
};
const EnumDataConverter<Inkscape::Filters::FilterMorphologyOperator> MorphologyOperatorConverter(MorphologyOperatorData, Inkscape::Filters::MORPHOLOGY_OPERATOR_END);

// feTurbulence
const EnumData<Inkscape::Filters::FilterTurbulenceType> TurbulenceTypeData[Inkscape::Filters::TURBULENCE_ENDTYPE] = {
    // clang-format off
    {Inkscape::Filters::TURBULENCE_FRACTALNOISE, _("Fractal Noise"), "fractalNoise"},
    {Inkscape::Filters::TURBULENCE_TURBULENCE,   _("Turbulence"),    "turbulence"}
    // clang-format on
};
const EnumDataConverter<Inkscape::Filters::FilterTurbulenceType> TurbulenceTypeConverter(TurbulenceTypeData, Inkscape::Filters::TURBULENCE_ENDTYPE);

// Light source
const EnumData<LightSource> LightSourceData[LIGHT_ENDSOURCE] = {
    // clang-format off
    {LIGHT_DISTANT, _("Distant Light"), "svg:feDistantLight"},
    {LIGHT_POINT,   _("Point Light"),   "svg:fePointLight"},
    {LIGHT_SPOT,    _("Spot Light"),    "svg:feSpotLight"}
    // clang-format on
};
const EnumDataConverter<LightSource> LightSourceConverter(LightSourceData, LIGHT_ENDSOURCE);

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
