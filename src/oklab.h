// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Support for the OKLab/OKLch perceptual color space.
 */
/*
 * Authors:
 *   Rafał Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_OKLAB_H
#define SEEN_OKLAB_H

#include <array>

#include "hsluv.h"

namespace Oklab {

using Triplet = Hsluv::Triplet;
using Hsluv::to_linear, Hsluv::from_linear;

/** @brief Convert an OKLab color to the linear RGB color space.
 *
 * @param oklab_color A triplet containing the L, a, b components.
 * @return a triplet containing linear (de-gamma'd) RGB components, all in [0, 1].
 */
Triplet oklab_to_linear_rgb(Triplet const &oklab_color);

/** @brief Convert a linear RGB color to OKLab coordinates.
 *
 * @param linear_rgb_color A triplet containing degamma'd (linearized) R, G, B components.
 * @return a triplet containing the (L, a, b) coordinates of the color in the OKLab space.
 */
Triplet linear_rgb_to_oklab(Triplet const &linear_rgb_color);

/** Convert an OKLab color to a gamma-compressed sRGB color. */
inline Triplet oklab_to_rgb(Triplet const &oklab_color)
{
    auto rgb = oklab_to_linear_rgb(oklab_color);
    for (auto &component : rgb) {
        component = from_linear(component);
    }
    return rgb;
}

/** Convert a gamma-compressed sRGB color to an OKLab color. */
inline Triplet rgb_to_oklab(Triplet const &rgb_color)
{
    Triplet linear;
    for (size_t i = 0; i < 3; i++) {
        linear[i] = to_linear(rgb_color[i]);
    }
    return linear_rgb_to_oklab(linear);
}

/** @brief Convert an OKLab color to the OKLch coordinates.
 *
 * The OKLch coordinates are more closely aligned with the perceptual properties of a color
 * and therefore are more convenient for the end user. They consist of:
 * L – luminance of the color, in the interval [0, 1] (this is the same as the L in (L, a, b)).
 * c – chroma; how far the color is from grayscale. The range of c-values is [0, cmax] but
 *     cmax depends on L and h; @see Oklab::max_chroma().
 * h – hue. A number in the interval [0, 360), interpreted as a hue angle on the color wheel.
 *
 * @param ok_lab_color A color in the OKLab color space.
 * @return The same color expressed in the Lch coordinates.
 */
Triplet oklab_to_oklch(Triplet const &ok_lab_color);

/** @brief Convert an OKLch color to the OKLab coordinates.
 *
 * For the meaning of the Lch color coordinates, @see oklab_to_oklch().
 */
Triplet oklch_to_oklab(Triplet const &ok_lch_color);
Triplet oklch_radians_to_oklab(Triplet const &ok_lch_with_hue_in_radians);

/** @brief Convert an OKLab color to an OKHSL representation.
 *
 * As of late 2022, OK-HSL (hue, saturation, lightness) is not a fully standardized color
 * space. The version used here stores colors as triples (h, s, L) of doubles, all in
 * the interval [0, 1]. These coordinates have the following meaning:
 *
 * \li \c h is the hue angle, scaled to the interval [0, 1].
 * \li \c s is the linear saturation. For a given OKLch color (L, c, h), linear saturation
 *          is the ratio of c to the maximum possible chroma c_max such that (L, c_max, h)
 *          fits in the sRGB gamut. Therefore, s=1 always corresponds to a maximally saturated
 *          color and s=0 is always a grayscale color.
 * \li \c L is the lightness; it is the same coordinate as the L in OKLab or OKLch.
 *
 * Note that Björn Ottosson proposes a new, non-standard way of compressing the saturation
 * (similar to gamma compression) which results in a different saturation scale. He further
 * suggests varying this compression depending on the hue, although he does not standardize
 * this proposed OKHSL space or the saturation transfer functions. Instead, he uses a somewhat
 * ad-hoc piecewise-linear saturation transfer function in his own picker, which unfortunately
 * breaks differentiability of the parametrization, a great advantage of the OKLch color space.
 * If an OKHSL space is ever standardized, the behaviour of this function may change.
 * See https://bottosson.github.io/posts/colorpicker/ for more details.
 *
 * @param ok_lab_color A color in the OKLab color space.
 * @return The same color expressed in the (h, s, L) coordinates.
 */
Triplet oklab_to_okhsl(Triplet const &ok_lab_color);

/** @brief Convert an OKHSL color to the OKLab coordinates.
 *
 * For a description of the OKHSL color scale used here, see oklab_to_okhsl().
 * If an OKHSL space is ever standardized, the behaviour of this function may change.
 *
 * @param ok_hsl_color A color in the OKHSL coordinates, all normalized to [0, 1].
 * @return The same color expressed in the (L, a, b) coordinates.
 */
Triplet okhsl_to_oklab(Triplet const &ok_hsl_color);

/** @brief Find the maximum OKLch chroma for the given luminosity and hue.
 *
 * @param l OKLab/OKLch luminosity, in the interval [0, 1].
 * @param h OKLch hue angle in degrees (interval [0, 360]).
 * @return The maximum chroma c such that the color oklch(l, c, h) fits in the sRGB gamut.
 */
double max_chroma(double l, double h);

/** @brief Helper functions for rendering color strips used in color sliders.
 *
 * @param h Hue angle in degrees (range 0-360).
 * @param s Saturation (percentage of allowed chroma) in the range 0-1.
 * @param l OKLab/OKLch lightness in the range 0-1.
 * @param[out] map Working store and RGB output showing the range of one of the color coordinates.
 * @return A pointer to the raw data of the map.
 */
uint8_t const *render_hue_scale(double s, double l, std::array<uint8_t, 4 * 1024> *map);
uint8_t const *render_saturation_scale(double h, double l, std::array<uint8_t, 4 * 1024> *map);
uint8_t const *render_lightness_scale(double h, double s, std::array<uint8_t, 4 * 1024> *map);

} // namespace Oklab

#endif // SEEN_OKLAB_H

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