// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_COLOR_H
#define SEEN_SP_COLOR_H

/*
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <string>
typedef unsigned int guint32; // uint is guaranteed to hold up to 2^32 − 1

/* Useful composition macros */

#define SP_RGBA32_R_U(v) (((v) >> 24) & 0xff)
#define SP_RGBA32_G_U(v) (((v) >> 16) & 0xff)
#define SP_RGBA32_B_U(v) (((v) >> 8) & 0xff)
#define SP_RGBA32_A_U(v) ((v) & 0xff)
#define SP_COLOR_U_TO_F(v) ((v) / 255.0)
#define SP_COLOR_F_TO_U(v) ((unsigned int) ((v) * 255. + .5))
#define SP_RGBA32_R_F(v) SP_COLOR_U_TO_F (SP_RGBA32_R_U (v))
#define SP_RGBA32_G_F(v) SP_COLOR_U_TO_F (SP_RGBA32_G_U (v))
#define SP_RGBA32_B_F(v) SP_COLOR_U_TO_F (SP_RGBA32_B_U (v))
#define SP_RGBA32_A_F(v) SP_COLOR_U_TO_F (SP_RGBA32_A_U (v))
#define SP_RGBA32_U_COMPOSE(r,g,b,a) ((((r) & 0xff) << 24) | (((g) & 0xff) << 16) | (((b) & 0xff) << 8) | ((a) & 0xff))
#define SP_RGBA32_F_COMPOSE(r,g,b,a) SP_RGBA32_U_COMPOSE (SP_COLOR_F_TO_U (r), SP_COLOR_F_TO_U (g), SP_COLOR_F_TO_U (b), SP_COLOR_F_TO_U (a))
#define SP_RGBA32_C_COMPOSE(c,o) SP_RGBA32_U_COMPOSE(SP_RGBA32_R_U(c),SP_RGBA32_G_U(c),SP_RGBA32_B_U(c),SP_COLOR_F_TO_U(o))

struct SVGICCColor;

/**
 * An RGB color with optional icc-color part
 */
struct SPColor {
    SPColor();
    SPColor( SPColor const& other );
    SPColor( float r, float g, float b );
    SPColor( guint32 value );
    virtual ~SPColor();

    SPColor& operator= (SPColor const& other);

    bool operator == ( SPColor const& other ) const;
    bool operator != ( SPColor const& other ) const { return !(*this == other); };
    bool isClose( SPColor const& other, float epsilon ) const;

    void set( float r, float g, float b );
    void set( guint32 value );

    guint32 toRGBA32( int alpha ) const;
    guint32 toRGBA32( double alpha ) const;

    std::string toString() const;

    SVGICCColor* icc;
    union {
        float c[3];
    } v;

    guint32 get_rgba32_ualpha (guint32 alpha) const;
    guint32 get_rgba32_falpha (float alpha) const;

    void get_rgb_floatv (float *rgb) const;
    void get_cmyk_floatv (float *cmyk) const;

    /* Plain mode helpers */

    static void rgb_to_hsv_floatv (float *hsv, float r, float g, float b);
    static void hsv_to_rgb_floatv (float *rgb, float h, float s, float v);

    static void rgb_to_hsl_floatv (float *hsl, float r, float g, float b);
    static void hsl_to_rgb_floatv (float *rgb, float h, float s, float l);

    static void rgb_to_cmyk_floatv (float *cmyk, float r, float g, float b);
    static void cmyk_to_rgb_floatv (float *rgb, float c, float m, float y, float k);
};


#endif // SEEN_SP_COLOR_H
