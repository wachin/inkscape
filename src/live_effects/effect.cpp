// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *   Abhishek Sharma
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

//#define LPE_ENABLE_TEST_EFFECTS //uncomment for toy effects

// include effects:
#include "live_effects/lpe-angle_bisector.h"
#include "live_effects/lpe-attach-path.h"
#include "live_effects/lpe-bendpath.h"
#include "live_effects/lpe-bool.h"
#include "live_effects/lpe-bounding-box.h"
#include "live_effects/lpe-bspline.h"
#include "live_effects/lpe-circle_3pts.h"
#include "live_effects/lpe-circle_with_radius.h"
#include "live_effects/lpe-clone-original.h"
#include "live_effects/lpe-constructgrid.h"
#include "live_effects/lpe-copy_rotate.h"
#include "live_effects/lpe-curvestitch.h"
#include "live_effects/lpe-dashed-stroke.h"
#include "live_effects/lpe-dynastroke.h"
#include "live_effects/lpe-ellipse_5pts.h"
#include "live_effects/lpe-embrodery-stitch.h"
#include "live_effects/lpe-envelope.h"
#include "live_effects/lpe-extrude.h"
#include "live_effects/lpe-fill-between-many.h"
#include "live_effects/lpe-fill-between-strokes.h"
#include "live_effects/lpe-fillet-chamfer.h"
#include "live_effects/lpe-gears.h"
#include "live_effects/lpe-interpolate.h"
#include "live_effects/lpe-interpolate_points.h"
#include "live_effects/lpe-jointype.h"
#include "live_effects/lpe-knot.h"
#include "live_effects/lpe-lattice.h"
#include "live_effects/lpe-lattice2.h"
#include "live_effects/lpe-line_segment.h"
#include "live_effects/lpe-measure-segments.h"
#include "live_effects/lpe-mirror_symmetry.h"
#include "live_effects/lpe-offset.h"
#include "live_effects/lpe-parallel.h"
#include "live_effects/lpe-path_length.h"
#include "live_effects/lpe-patternalongpath.h"
#include "live_effects/lpe-perp_bisector.h"
#include "live_effects/lpe-perspective-envelope.h"
#include "live_effects/lpe-powerclip.h"
#include "live_effects/lpe-powermask.h"
#include "live_effects/lpe-powerstroke.h"
#include "live_effects/lpe-pts2ellipse.h"
#include "live_effects/lpe-recursiveskeleton.h"
#include "live_effects/lpe-rough-hatches.h"
#include "live_effects/lpe-roughen.h"
#include "live_effects/lpe-ruler.h"
#include "live_effects/lpe-show_handles.h"
#include "live_effects/lpe-simplify.h"
#include "live_effects/lpe-sketch.h"
#include "live_effects/lpe-slice.h"
#include "live_effects/lpe-spiro.h"
#include "live_effects/lpe-tangent_to_curve.h"
#include "live_effects/lpe-taperstroke.h"
#include "live_effects/lpe-test-doEffect-stack.h"
#include "live_effects/lpe-text_label.h"
#include "live_effects/lpe-transform_2pts.h"
#include "live_effects/lpe-vonkoch.h"

#include "live_effects/lpeobject.h"

#include "xml/node-event-vector.h"
#include "xml/sp-css-attr.h"

#include "display/curve.h"
#include "message-stack.h"
#include "path-chemistry.h"
#include "ui/icon-loader.h"
#include "ui/tools/node-tool.h"
#include "ui/tools/pen-tool.h"

#include "object/sp-defs.h"
#include "object/sp-root.h"
#include "object/sp-shape.h"

#include <cstdio>
#include <cstring>
#include <pangomm/layout.h>
#include <gtkmm/expander.h>

namespace Inkscape {

namespace LivePathEffect {

const EnumEffectData<EffectType> LPETypeData[] = {
    // {constant defined in effect-enum.h, N_("name of your effect"), "name of your effect in SVG"}
    // please sync order with effect-enum.h
/* 0.46 */
    {
        BEND_PATH,
        NC_("path effect", "Bend") ,//label
        "bend_path" ,//key
        "bend-path" ,//icon
        N_("Bend an object along the curvature of another path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        GEARS,
        NC_("path effect", "Gears") ,//label
        "gears" ,//key
        "gears" ,//icon
        N_("Create interlocking, configurable gears based on the nodes of a path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        PATTERN_ALONG_PATH,
        NC_("path effect", "Pattern Along Path") ,//label
        "skeletal" ,//key
        "skeletal" ,//icon
        N_("Place one or more copies of another path along the path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    }, // for historic reasons, this effect is called skeletal(strokes) in Inkscape:SVG
    {
        CURVE_STITCH,
        NC_("path effect", "Stitch Sub-Paths") ,//label
        "curvestitching" ,//key
        "curvestitching" ,//icon
        N_("Draw perpendicular lines between subpaths of a path, like rungs of a ladder") ,//description
        true  ,//on_path
        false ,//on_shape
        true ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
/* 0.47 */
    {
        VONKOCH,
        NC_("path effect", "VonKoch") ,//label
        "vonkoch" ,//key
        "vonkoch" ,//icon
        N_("Create VonKoch fractal") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        KNOT,
        NC_("path effect", "Knot") ,//label
        "knot" ,//key
        "knot" ,//icon
        N_("Create gaps in self-intersections, as in Celtic knots") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        CONSTRUCT_GRID,
        NC_("path effect", "Construct grid") ,//label
        "construct_grid" ,//key
        "construct-grid" ,//icon
        N_("Create a (perspective) grid from a 3-node path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        SPIRO,
        NC_("path effect", "Spiro spline") ,//label
        "spiro" ,//key
        "spiro" ,//icon
        N_("Make the path curl like wire, using Spiro B-Splines. This effect is usually used directly on the canvas with the Spiro mode of the drawing tools.") ,//description
        true  ,//on_path
        false ,//on_shape
        false ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        ENVELOPE,
        NC_("path effect", "Envelope Deformation") ,//label
        "envelope" ,//key
        "envelope" ,//icon
        N_("Adjust the shape of an object by transforming paths on its four sides") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        INTERPOLATE,
        NC_("path effect", "Interpolate Sub-Paths") ,//label
        "interpolate" ,//key
        "interpolate" ,//icon
        N_("Create a stepwise transition between the 2 subpaths of a path") ,//description
        true  ,//on_path
        false ,//on_shape
        false ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        ROUGH_HATCHES,
        NC_("path effect", "Hatches (rough)") ,//label
        "rough_hatches" ,//key
        "rough-hatches" ,//icon
        N_("Fill the object with adjustable hatching") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        SKETCH,
        NC_("path effect", "Sketch") ,//label
        "sketch" ,//key
        "sketch" ,//icon
        N_("Draw multiple short strokes along the path, as in a pencil sketch") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        RULER,
        NC_("path effect", "Ruler") ,//label
        "ruler" ,//key
        "ruler" ,//icon
        N_("Add ruler marks to the object in adjustable intervals, using the object's stroke style.") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
/* 0.91 */
    {
        POWERSTROKE,
        NC_("path effect", "Power stroke") ,//label
        "powerstroke" ,//key
        "powerstroke" ,//icon
        N_("Create calligraphic strokes and control their variable width and curvature. This effect can also be used directly on the canvas with a pressure sensitive stylus and the Pencil tool.") ,//description
        true  ,//on_path
        true  ,//on_shape
        false ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        CLONE_ORIGINAL,
        NC_("path effect", "Clone original") ,//label
        "clone_original" ,//key
        "clone-original" ,//icon
        N_("Let an object take on the shape, fill, stroke and/or other attributes of another object.") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
/* 0.92 */
    {
        SIMPLIFY,
        NC_("path effect", "Simplify") ,//label
        "simplify" ,//key
        "simplify" ,//icon
        N_("Smoothen and simplify a object. This effect is also available in the Pencil tool's tool controls.") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        LATTICE2,
        NC_("path effect", "Lattice Deformation 2") ,//label
        "lattice2" ,//key
        "lattice2" ,//icon
        N_("Warp an object's shape based on a 5x5 grid") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        PERSPECTIVE_ENVELOPE,
        NC_("path effect", "Perspective/Envelope") ,//label
        "perspective-envelope" ,//key wrong key with "-" retain because historic
        "perspective-envelope" ,//icon
        N_("Transform the object to fit into a shape with four corners, either by stretching it or creating the illusion of a 3D-perspective") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        INTERPOLATE_POINTS,
        NC_("path effect", "Interpolate points") ,//label
        "interpolate_points" ,//key
        "interpolate-points" ,//icon
        N_("Connect the nodes of the object (e.g. corresponding to data points) by different types of lines.") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        TRANSFORM_2PTS,
        NC_("path effect", "Transform by 2 points") ,//label
        "transform_2pts" ,//key
        "transform-2pts" ,//icon
        N_("Scale, stretch and rotate an object by two handles") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        SHOW_HANDLES,
        NC_("path effect", "Show handles") ,//label
        "show_handles" ,//key
        "show-handles" ,//icon
        N_("Draw the handles and nodes of objects (replaces the original styling with a black stroke)") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        ROUGHEN,
        NC_("path effect", "Roughen") ,//label
        "roughen" ,//key
        "roughen" ,//icon
        N_("Roughen an object by adding and randomly shifting new nodes") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        BSPLINE,
        NC_("path effect", "BSpline") ,//label
        "bspline" ,//key
        "bspline" ,//icon
        N_("Create a BSpline that molds into the path's corners. This effect is usually used directly on the canvas with the BSpline mode of the drawing tools.") ,//description
        true  ,//on_path
        false ,//on_shape
        false ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        JOIN_TYPE,
        NC_("path effect", "Join type") ,//label
        "join_type" ,//key
        "join-type" ,//icon
        N_("Select among various join types for a object's corner nodes (mitre, rounded, extrapolated arc, ...)") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        TAPER_STROKE,
        NC_("path effect", "Taper stroke") ,//label
        "taper_stroke" ,//key
        "taper-stroke" ,//icon
        N_("Let the path's ends narrow down to a tip") ,//description
        true  ,//on_path
        true  ,//on_shape
        false ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        MIRROR_SYMMETRY,
        NC_("path effect", "Mirror symmetry") ,//label
        "mirror_symmetry" ,//key
        "mirror-symmetry" ,//icon
        N_("Mirror an object along a movable axis, or around the page center. The mirrored copy can be styled independently.") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        COPY_ROTATE,
        NC_("path effect", "Rotate copies") ,//label
        "copy_rotate" ,//key
        "copy-rotate" ,//icon
        N_("Create multiple rotated copies of an object, as in a kaleidoscope. The copies can be styled independently.") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
/* Ponyscape -> Inkscape 0.92*/
    {
        ATTACH_PATH,
        NC_("path effect", "Attach path") ,//label
        "attach_path" ,//key
        "attach-path" ,//icon
        N_("Glue the current path's ends to a specific position on one or two other paths") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        FILL_BETWEEN_STROKES,
        NC_("path effect", "Fill between strokes") ,//label
        "fill_between_strokes" ,//key
        "fill-between-strokes" ,//icon
        N_("Turn the path into a fill between two other open paths (e.g. between two paths with PowerStroke applied to them)") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        FILL_BETWEEN_MANY,
        NC_("path effect", "Fill between many") ,//label
        "fill_between_many" ,//key
        "fill-between-many" ,//icon
        N_("Turn the path into a fill between multiple other open paths (e.g. between paths with PowerStroke applied to them)") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        ELLIPSE_5PTS,
        NC_("path effect", "Ellipse by 5 points") ,//label
        "ellipse_5pts" ,//key
        "ellipse-5pts" ,//icon
        N_("Create an ellipse from 5 nodes on its circumference") ,//description
        true  ,//on_path
        true  ,//on_shape
        false ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        BOUNDING_BOX,
        NC_("path effect", "Bounding Box") ,//label
        "bounding_box" ,//key
        "bounding-box" ,//icon
        N_("Turn the path into a bounding box that entirely encompasses another path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
/* 1.0 */
    {
        MEASURE_SEGMENTS,
        NC_("path effect", "Measure Segments") ,//label
        "measure_segments" ,//key
        "measure-segments" ,//icon
        N_("Add dimensioning for distances between nodes, optionally with projection and many other configuration options") ,//description
        true  ,//on_path
        true  ,//on_shape
        false ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        FILLET_CHAMFER,
        NC_("path effect", "Corners (Fillet/Chamfer)") ,//label
        "fillet_chamfer" ,//key
        "fillet-chamfer" ,//icon
        N_("Adjust the shape of a path's corners, rounding them to a specified radius, or cutting them off") ,//description
        true  ,//on_path
        true  ,//on_shape
        false ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        POWERCLIP,
        NC_("path effect", "Power clip") ,//label
        "powerclip" ,//key
        "powerclip" ,//icon
        N_("Invert, hide or flatten a clip (apply like a Boolean operation)") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        POWERMASK,
        NC_("path effect", "Power mask") ,//label
        "powermask" ,//key
        "powermask" ,//icon
        N_("Invert or hide a mask, or use its negative") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        PTS2ELLIPSE,
        NC_("path effect", "Ellipse from points") ,//label
        "pts2ellipse" ,//key
        "pts2ellipse" ,//icon
        N_("Draw a circle, ellipse, arc or slice based on the nodes of a path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        OFFSET,
        NC_("path effect", "Offset") ,//label
        "offset" ,//key
        "offset" ,//icon
        N_("Offset the path, optionally keeping cusp corners cusp") ,//description
        true  ,//on_path
        true  ,//on_shape
        true ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        DASHED_STROKE,
        NC_("path effect", "Dashed Stroke") ,//label
        "dashed_stroke" ,//key
        "dashed-stroke" ,//icon
        N_("Add a dashed stroke whose dashes end exactly on a node, optionally with the same number of dashes per path segment") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    /* 1.1 */
    {
        BOOL_OP,
        NC_("path effect", "Boolean operation") ,//label
        "bool_op" ,//key
        "bool-op" ,//icon
        N_("Cut, union, subtract, intersect and divide a path non-destructively with another path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    {
        SLICE,
        NC_("path effect", "Slice") ,//label
        "slice" ,//key
        "slice" ,//icon
        N_("Slices the item into parts. It can also be applied multiple times.") ,//description
        true  ,//on_path
        true  ,//on_shape
        true ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
    // VISIBLE experimental LPE
    {
        ANGLE_BISECTOR,
        NC_("path effect", "Angle bisector") ,//label
        "angle_bisector" ,//key
        "experimental" ,//icon
        N_("Draw a line that halves the angle between the first three nodes of the path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        CIRCLE_WITH_RADIUS,
        NC_("path effect", "Circle (by center and radius)") ,//label
        "circle_with_radius" ,//key
        "experimental" ,//icon
        N_("Draw a circle, where the first node of the path is the center, and the last determines its radius") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        CIRCLE_3PTS,
        NC_("path effect", "Circle by 3 points") ,//label
        "circle_3pts" ,//key
        "experimental" ,//icon
        N_("Draw a circle whose circumference passes through the first three nodes of the path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        EXTRUDE,
        NC_("path effect", "Extrude") ,//label
        "extrude" ,//key
        "experimental" ,//icon
        N_("Extrude the path, creating a face for each path segment") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        LINE_SEGMENT,
        NC_("path effect", "Line Segment") ,//label
        "line_segment" ,//key
        "experimental" ,//icon
        N_("Draw a straight line that connects the first and last node of a path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        PARALLEL,
        NC_("path effect", "Parallel") ,//label
        "parallel" ,//key
        "experimental" ,//icon
        N_("Create a draggable line that will always be parallel to a two-node path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        PERP_BISECTOR,
        NC_("path effect", "Perpendicular bisector") ,//label
        "perp_bisector" ,//key
        "experimental" ,//icon
        N_("Draw a perpendicular line in the middle of the (imaginary) line that connects the start and end nodes") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        TANGENT_TO_CURVE,
        NC_("path effect", "Tangent to curve") ,//label
        "tangent_to_curve" ,//key
        "experimental" ,//icon
        N_("Draw a tangent with variable length and additional angle that can be moved along the path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
#ifdef LPE_ENABLE_TEST_EFFECTS
    {
        DOEFFECTSTACK_TEST,
        NC_("path effect", "doEffect stack test") ,//label
        "doeffectstacktest" ,//key
        "experimental" ,//icon
        N_("Test LPE") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        DYNASTROKE,
        NC_("path effect", "Dynamic stroke") ,//label
        "dynastroke" ,//key
        "experimental" ,//icon
        N_("Create calligraphic strokes with variably shaped ends, making use of a parameter for the brush angle") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        LATTICE,
        NC_("path effect", "Lattice Deformation") ,//label
        "lattice" ,//key
        "experimental" ,//icon
        N_("Deform an object using a 4x4 grid") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        PATH_LENGTH,
        NC_("path effect", "Path length") ,//label
        "path_length" ,//key
        "experimental" ,//icon
        N_("Display the total length of a (curved) path") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        RECURSIVE_SKELETON,
        NC_("path effect", "Recursive skeleton") ,//label
        "recursive_skeleton" ,//key
        "experimental" ,//icon
        N_("Draw a path recursively") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        TEXT_LABEL,
        NC_("path effect", "Text label") ,//label
        "text_label" ,//key
        "experimental" ,//icon
        N_("Add a label for the object") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        true ,//experimental
    },
    {
        EMBRODERY_STITCH,
        NC_("path effect", "Embroidery stitch") ,//label
        "embrodery_stitch" ,//key
        "embrodery-stitch" ,//icon
        N_("Embroidery stitch") ,//description
        true  ,//on_path
        true  ,//on_shape
        true  ,//on_group
        false ,//on_image
        false ,//on_text
        false ,//experimental
    },
#endif

};

const EnumEffectDataConverter<EffectType> LPETypeConverter(LPETypeData, sizeof(LPETypeData) / sizeof(*LPETypeData));

int
Effect::acceptsNumClicks(EffectType type) {
    switch (type) {
        case INVALID_LPE: return -1; // in case we want to distinguish between invalid LPE and valid ones that expect zero clicks
        case ANGLE_BISECTOR: return 3;
        case CIRCLE_3PTS: return 3;
        case CIRCLE_WITH_RADIUS: return 2;
        case LINE_SEGMENT: return 2;
        case PERP_BISECTOR: return 2;
        default: return 0;
    }
}

Effect*
Effect::New(EffectType lpenr, LivePathEffectObject *lpeobj)
{
    Effect* neweffect = nullptr;
    switch (lpenr) {
        case EMBRODERY_STITCH:
            neweffect = static_cast<Effect*> ( new LPEEmbroderyStitch(lpeobj) );
            break;
        case BOOL_OP:
            neweffect = static_cast<Effect*> ( new LPEBool(lpeobj) );
            break;
        case PATTERN_ALONG_PATH:
            neweffect = static_cast<Effect*> ( new LPEPatternAlongPath(lpeobj) );
            break;
        case BEND_PATH:
            neweffect = static_cast<Effect*> ( new LPEBendPath(lpeobj) );
            break;
        case SKETCH:
            neweffect = static_cast<Effect*> ( new LPESketch(lpeobj) );
            break;
        case ROUGH_HATCHES:
            neweffect = static_cast<Effect*> ( new LPERoughHatches(lpeobj) );
            break;
        case VONKOCH:
            neweffect = static_cast<Effect*> ( new LPEVonKoch(lpeobj) );
            break;
        case KNOT:
            neweffect = static_cast<Effect*> ( new LPEKnot(lpeobj) );
            break;
        case GEARS:
            neweffect = static_cast<Effect*> ( new LPEGears(lpeobj) );
            break;
        case CURVE_STITCH:
            neweffect = static_cast<Effect*> ( new LPECurveStitch(lpeobj) );
            break;
        case LATTICE:
            neweffect = static_cast<Effect*> ( new LPELattice(lpeobj) );
            break;
        case ENVELOPE:
            neweffect = static_cast<Effect*> ( new LPEEnvelope(lpeobj) );
            break;
        case CIRCLE_WITH_RADIUS:
            neweffect = static_cast<Effect*> ( new LPECircleWithRadius(lpeobj) );
            break;
        case SPIRO:
            neweffect = static_cast<Effect*> ( new LPESpiro(lpeobj) );
            break;
        case CONSTRUCT_GRID:
            neweffect = static_cast<Effect*> ( new LPEConstructGrid(lpeobj) );
            break;
        case PERP_BISECTOR:
            neweffect = static_cast<Effect*> ( new LPEPerpBisector(lpeobj) );
            break;
        case TANGENT_TO_CURVE:
            neweffect = static_cast<Effect*> ( new LPETangentToCurve(lpeobj) );
            break;
        case MIRROR_SYMMETRY:
            neweffect = static_cast<Effect*> ( new LPEMirrorSymmetry(lpeobj) );
            break;
        case CIRCLE_3PTS:
            neweffect = static_cast<Effect*> ( new LPECircle3Pts(lpeobj) );
            break;
        case ANGLE_BISECTOR:
            neweffect = static_cast<Effect*> ( new LPEAngleBisector(lpeobj) );
            break;
        case PARALLEL:
            neweffect = static_cast<Effect*> ( new LPEParallel(lpeobj) );
            break;
        case COPY_ROTATE:
            neweffect = static_cast<Effect*> ( new LPECopyRotate(lpeobj) );
            break;
        case OFFSET:
            neweffect = static_cast<Effect*> ( new LPEOffset(lpeobj) );
            break;
        case RULER:
            neweffect = static_cast<Effect*> ( new LPERuler(lpeobj) );
            break;
        case INTERPOLATE:
            neweffect = static_cast<Effect*> ( new LPEInterpolate(lpeobj) );
            break;
        case INTERPOLATE_POINTS:
            neweffect = static_cast<Effect*> ( new LPEInterpolatePoints(lpeobj) );
            break;
        case TEXT_LABEL:
            neweffect = static_cast<Effect*> ( new LPETextLabel(lpeobj) );
            break;
        case PATH_LENGTH:
            neweffect = static_cast<Effect*> ( new LPEPathLength(lpeobj) );
            break;
        case LINE_SEGMENT:
            neweffect = static_cast<Effect*> ( new LPELineSegment(lpeobj) );
            break;
        case DOEFFECTSTACK_TEST:
            neweffect = static_cast<Effect*> ( new LPEdoEffectStackTest(lpeobj) );
            break;
        case BSPLINE:
            neweffect = static_cast<Effect*> ( new LPEBSpline(lpeobj) );
            break;
        case DYNASTROKE:
            neweffect = static_cast<Effect*> ( new LPEDynastroke(lpeobj) );
            break;
        case RECURSIVE_SKELETON:
            neweffect = static_cast<Effect*> ( new LPERecursiveSkeleton(lpeobj) );
            break;
        case EXTRUDE:
            neweffect = static_cast<Effect*> ( new LPEExtrude(lpeobj) );
            break;
        case POWERSTROKE:
            neweffect = static_cast<Effect*> ( new LPEPowerStroke(lpeobj) );
            break;
        case CLONE_ORIGINAL:
            neweffect = static_cast<Effect*> ( new LPECloneOriginal(lpeobj) );
            break;
        case ATTACH_PATH:
            neweffect = static_cast<Effect*> ( new LPEAttachPath(lpeobj) );
            break;
        case FILL_BETWEEN_STROKES:
            neweffect = static_cast<Effect*> ( new LPEFillBetweenStrokes(lpeobj) );
            break;
        case FILL_BETWEEN_MANY:
            neweffect = static_cast<Effect*> ( new LPEFillBetweenMany(lpeobj) );
            break;
        case ELLIPSE_5PTS:
            neweffect = static_cast<Effect*> ( new LPEEllipse5Pts(lpeobj) );
            break;
        case BOUNDING_BOX:
            neweffect = static_cast<Effect*> ( new LPEBoundingBox(lpeobj) );
            break;
        case JOIN_TYPE:
            neweffect = static_cast<Effect*> ( new LPEJoinType(lpeobj) );
            break;
        case TAPER_STROKE:
            neweffect = static_cast<Effect*> ( new LPETaperStroke(lpeobj) );
            break;
        case SIMPLIFY:
            neweffect = static_cast<Effect*> ( new LPESimplify(lpeobj) );
            break;
        case LATTICE2:
            neweffect = static_cast<Effect*> ( new LPELattice2(lpeobj) );
            break;
        case PERSPECTIVE_ENVELOPE:
            neweffect = static_cast<Effect*> ( new LPEPerspectiveEnvelope(lpeobj) );
            break;
        case FILLET_CHAMFER:
            neweffect = static_cast<Effect*> ( new LPEFilletChamfer(lpeobj) );
            break;
        case POWERCLIP:
            neweffect = static_cast<Effect*> ( new LPEPowerClip(lpeobj) );
            break;
        case POWERMASK:
            neweffect = static_cast<Effect*> ( new LPEPowerMask(lpeobj) );
            break;
        case ROUGHEN:
            neweffect = static_cast<Effect*> ( new LPERoughen(lpeobj) );
            break;
        case SHOW_HANDLES:
            neweffect = static_cast<Effect*> ( new LPEShowHandles(lpeobj) );
            break;
        case TRANSFORM_2PTS:
            neweffect = static_cast<Effect*> ( new LPETransform2Pts(lpeobj) );
            break;
        case MEASURE_SEGMENTS:
            neweffect = static_cast<Effect*> ( new LPEMeasureSegments(lpeobj) );
            break;
        case PTS2ELLIPSE:
            neweffect = static_cast<Effect*> ( new LPEPts2Ellipse(lpeobj) );
            break;
        case DASHED_STROKE:
            neweffect = static_cast<Effect *>(new LPEDashedStroke(lpeobj));
            break;
        case SLICE:
            neweffect = static_cast<Effect *>(new LPESlice(lpeobj));
            break;
        default:
            g_warning("LivePathEffect::Effect::New called with invalid patheffect type (%d)", lpenr);
            neweffect = nullptr;
            break;
    }

    if (neweffect) {
        neweffect->readallParameters(lpeobj->getRepr());
    }

    return neweffect;
}

void Effect::createAndApply(const char* name, SPDocument *doc, SPItem *item)
{
    // Path effect definition
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();
    Inkscape::XML::Node *repr = xml_doc->createElement("inkscape:path-effect");
    repr->setAttribute("effect", name);

    doc->getDefs()->getRepr()->addChild(repr, nullptr); // adds to <defs> and assigns the 'id' attribute
    const gchar * repr_id = repr->attribute("id");
    Inkscape::GC::release(repr);

    gchar *href = g_strdup_printf("#%s", repr_id);
    SP_LPE_ITEM(item)->addPathEffect(href, true);
    g_free(href);
}

void
Effect::createAndApply(EffectType type, SPDocument *doc, SPItem *item)
{
    createAndApply(LPETypeConverter.get_key(type).c_str(), doc, item);
}

Effect::Effect(LivePathEffectObject *lpeobject)
    : apply_to_clippath_and_mask(false),
      _provides_knotholder_entities(false),
      oncanvasedit_it(0),
      is_visible(_("Is visible?"), _("If unchecked, the effect remains applied to the object but is temporarily disabled on canvas"), "is_visible", &wr, this, true),
      lpeversion(_("Version"), _("LPE version"), "lpeversion", &wr, this, "0", true),
      show_orig_path(false),
      keep_paths(false),
      is_load(true),
      on_remove_all(false),
      lpeobj(lpeobject),
      concatenate_before_pwd2(false),
      sp_lpe_item(nullptr),
      current_zoom(0),
      refresh_widgets(false),
      current_shape(nullptr),
      provides_own_flash_paths(true), // is automatically set to false if providesOwnFlashPaths() is not overridden
      defaultsopen(false),
      is_ready(false),
      is_applied(false)
{
    registerParameter( dynamic_cast<Parameter *>(&is_visible) );
    registerParameter( dynamic_cast<Parameter *>(&lpeversion) );
    is_visible.widget_is_visible = false;
}

Effect::~Effect() = default;

Glib::ustring
Effect::getName() const
{
    if (lpeobj->effecttype_set && LPETypeConverter.is_valid_id(lpeobj->effecttype) )
        return Glib::ustring( _(LPETypeConverter.get_label(lpeobj->effecttype).c_str()) );
    else
        return Glib::ustring( _("No effect") );
}

EffectType
Effect::effectType() const {
    return lpeobj->effecttype;
}

std::vector<SPLPEItem *> 
Effect::getCurrrentLPEItems() const {
    std::vector<SPLPEItem *> result;
    auto hreflist = getLPEObj()->hrefList;
    for (auto item : hreflist) {
        SPLPEItem * lpeitem = dynamic_cast<SPLPEItem *>(item);
        if (lpeitem) {
            result.push_back(lpeitem);
        }
    }
    return result;
}

/**
 * Is performed a single time when the effect is freshly applied to a path
 */
void
Effect::doOnApply (SPLPEItem const*/*lpeitem*/)
{
}

void
Effect::setCurrentZoom(double cZ)
{
    current_zoom = cZ;
}

/**
 * Overridden function to apply transforms for example to powerstroke, jointtype or tapperstroke
 */
void Effect::transform_multiply(Geom::Affine const &postmul, bool /*set*/) {}

/**
 * @param lpeitem The item being transformed
 *
 * @pre effect is referenced by lpeitem
 *
 * FIXME Probably only makes sense if this effect is referenced by exactly one
 * item (`this->lpeobj->hrefList` contains exactly one element)?
 */
void Effect::transform_multiply(Geom::Affine const &postmul, SPLPEItem *lpeitem)
{
    assert("pre: effect is referenced by lpeitem" &&
           std::any_of(lpeobj->hrefList.begin(), lpeobj->hrefList.end(),
                       [lpeitem](SPObject *obj) { return lpeitem == dynamic_cast<SPLPEItem *>(obj); }));

    // FIXME Is there a way to eliminate the raw Effect::sp_lpe_item pointer?
    sp_lpe_item = lpeitem;

    transform_multiply(postmul, false);
}

void
Effect::setSelectedNodePoints(std::vector<Geom::Point> sNP)
{
    selectedNodesPoints = sNP;
}

/**
 * The lpe is on clipboard
 */
bool Effect::isOnClipboard()
{
    SPDocument *document = getSPDoc();
    if (!document) {
        return false;
    }
    Inkscape::XML::Node *root = document->getReprRoot();
    Inkscape::XML::Node *clipnode = sp_repr_lookup_name(root, "inkscape:clipboard", 1);
    return clipnode != nullptr;
}

bool
Effect::isNodePointSelected(Geom::Point const &nodePoint) const
{
    if (selectedNodesPoints.size() > 0) {
        using Geom::X;
        using Geom::Y;
        for (auto p : selectedNodesPoints) {
            Geom::Affine transformCoordinate = sp_lpe_item->i2dt_affine();
            Geom::Point p2(nodePoint[X],nodePoint[Y]);
            p2 *= transformCoordinate;
            if (Geom::are_near(p, p2, 0.01)) {
                return true;
            }
        }
    }
    return false;
}

void
Effect::processObjects(LPEAction lpe_action)
{
    SPDocument *document = getSPDoc();
    if (!document) {
        return;
    }
    sp_lpe_item = dynamic_cast<SPLPEItem *>(*getLPEObj()->hrefList.begin());
    if (!document || !sp_lpe_item) {
        return;
    }
    sp_lpe_item_enable_path_effects(sp_lpe_item, false);
    for (auto id : items) {
        SPObject *elemref = nullptr;
        if ((elemref = document->getObjectById(id.c_str()))) {
            Inkscape::XML::Node * elemnode = elemref->getRepr();
            std::vector<SPItem*> item_list;
            auto item = dynamic_cast<SPItem *>(elemref);
            item_list.push_back(item);
            std::vector<Inkscape::XML::Node*> item_to_select;
            std::vector<SPItem*> item_selected;
            SPCSSAttr *css;
            Glib::ustring css_str;
            switch (lpe_action){
            case LPE_TO_OBJECTS:
                if (item->isHidden()) {
                    item->deleteObject(true);
                } else {
                    elemnode->removeAttribute("sodipodi:insensitive");
                    if (!SP_IS_DEFS(item->parent)) {
                        item->moveTo(sp_lpe_item, false);
                    }
                }
                break;

            case LPE_ERASE:
                item->deleteObject(true);
                break;

            case LPE_VISIBILITY:
                css = sp_repr_css_attr_new();
                sp_repr_css_attr_add_from_string(css, elemref->getRepr()->attribute("style"));
                if (!this->isVisible()/* && std::strcmp(elemref->getId(),sp_lpe_item->getId()) != 0*/) {
                    css->setAttribute("display", "none");
                } else {
                    css->removeAttribute("display");
                }
                sp_repr_css_write_string(css,css_str);
                elemnode->setAttributeOrRemoveIfEmpty("style", css_str);
                break;

            default:
                break;
            }
        }
    }
    if (lpe_action == LPE_ERASE || lpe_action == LPE_TO_OBJECTS) {
        items.clear();
    }
    sp_lpe_item_enable_path_effects(sp_lpe_item, true);
}

/**
 * Is performed each time before the effect is updated.
 */
void
Effect::doBeforeEffect (SPLPEItem const*/*lpeitem*/)
{
    //Do nothing for simple effects
}
/**
 * Is performed at the end of the LPE only one time per "lpeitem"
 * in paths/shapes is called in middle of the effect so we add the
 * "curve" param to allow updates in the LPE results at this stage
 * for groups dont need to send "curve" parameter because is applied 
 * when the LPE process finish, we can update LPE results without "curve" parameter
 * @param lpeitem the element that has this LPE
 * @param curve the curve to pass when in mode path or shape
 */
void Effect::doAfterEffect (SPLPEItem const* /*lpeitem*/, SPCurve *curve)
{
    //Do nothing for simple effects
}

void Effect::doOnException(SPLPEItem const * /*lpeitem*/)
{
    has_exception = true;
    pathvector_after_effect = pathvector_before_effect;
}


void Effect::doOnRemove (SPLPEItem const* /*lpeitem*/)
{
}
void Effect::doOnVisibilityToggled(SPLPEItem const* /*lpeitem*/)
{
}
//secret impl methods (shhhh!)
void Effect::doAfterEffect_impl(SPLPEItem const *lpeitem, SPCurve *curve)
{
    doAfterEffect(lpeitem, curve);
    is_load = false;
    is_applied = false;
}
void Effect::doOnApply_impl(SPLPEItem const* lpeitem)
{
    sp_lpe_item = const_cast<SPLPEItem *>(lpeitem);
    is_applied = true;
    // we can override "lpeversion" value in each LPE using doOnApply
    // this allow to handle legacy LPE and some times update to newest definitions
    // In BBB Martin, Mc and Jabiertxof make decision 
    // to only update this value per each LPE when changes.
    // and use the Inkscape release version that has this new LPE change
    // LPE without lpeversion are created in an inkscape lower than 1.0
    lpeversion.param_setValue("1", true);
    doOnApply(lpeitem);
    setReady();
    has_exception = false;
}

void Effect::doBeforeEffect_impl(SPLPEItem const* lpeitem)
{
    sp_lpe_item = const_cast<SPLPEItem *>(lpeitem);
    doBeforeEffect(lpeitem);
    update_helperpath();
}

void
Effect::writeParamsToSVG() {
    std::vector<Inkscape::LivePathEffect::Parameter *>::iterator p;
    for (p = param_vector.begin(); p != param_vector.end(); ++p) {
        (*p)->write_to_SVG();
    }
}

/**
 * If the effect expects a path parameter (specified by a number of mouse clicks) before it is
 * applied, this is the method that processes the resulting path. Override it to customize it for
 * your LPE. But don't forget to call the parent method so that is_ready is set to true!
 */
void
Effect::acceptParamPath (SPPath const*/*param_path*/) {
    setReady();
}

/*
 *  Here be the doEffect function chain:
 */
void
Effect::doEffect (SPCurve * curve)
{
    Geom::PathVector orig_pathv = curve->get_pathvector();

    Geom::PathVector result_pathv = doEffect_path(orig_pathv);

    curve->set_pathvector(result_pathv);
}

Geom::PathVector
Effect::doEffect_path (Geom::PathVector const & path_in)
{
    Geom::PathVector path_out;

    if ( !concatenate_before_pwd2 ) {
        // default behavior
        for (const auto & i : path_in) {
            Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_in = i.toPwSb();
            Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_out = doEffect_pwd2(pwd2_in);
            Geom::PathVector path = Geom::path_from_piecewise( pwd2_out, LPE_CONVERSION_TOLERANCE);
            // add the output path vector to the already accumulated vector:
            for (const auto & j : path) {
                path_out.push_back(j);
            }
        }
    } else {
        // concatenate the path into possibly discontinuous pwd2
        Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_in;
        for (const auto & i : path_in) {
            pwd2_in.concat( i.toPwSb() );
        }
        Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_out = doEffect_pwd2(pwd2_in);
        path_out = Geom::path_from_piecewise( pwd2_out, LPE_CONVERSION_TOLERANCE);
    }

    return path_out;
}

Geom::Piecewise<Geom::D2<Geom::SBasis> >
Effect::doEffect_pwd2 (Geom::Piecewise<Geom::D2<Geom::SBasis> > const & pwd2_in)
{
    g_warning("Effect has no doEffect implementation");
    return pwd2_in;
}

void
Effect::readallParameters(Inkscape::XML::Node const* repr)
{
    std::vector<Parameter *>::iterator it = param_vector.begin();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    while (it != param_vector.end()) {
        Parameter * param = *it;
        const gchar * key = param->param_key.c_str();
        const gchar * value = repr->attribute(key);
        if (value) {
            bool accepted = param->param_readSVGValue(value);
            if (!accepted) {
                g_warning("Effect::readallParameters - '%s' not accepted for %s", value, key);
            }
        } else {
            Glib::ustring pref_path = (Glib::ustring)"/live_effects/" +
                                       (Glib::ustring)LPETypeConverter.get_key(effectType()).c_str() +
                                       (Glib::ustring)"/" +
                                       (Glib::ustring)key;
            bool valid = prefs->getEntry(pref_path).isValid();
            if(valid){
                param->param_update_default(prefs->getString(pref_path).c_str());
            } else {
                param->param_set_default();
            }
        }
        ++it;
    }
}

/* This function does not and SHOULD NOT write to XML */
void
Effect::setParameter(const gchar * key, const gchar * new_value)
{
    Parameter * param = getParameter(key);
    if (param) {
        if (new_value) {
            bool accepted = param->param_readSVGValue(new_value);
            if (!accepted) {
                g_warning("Effect::setParameter - '%s' not accepted for %s", new_value, key);
            }
        } else {
            // set default value
            param->param_set_default();
        }
    }
}

void
Effect::registerParameter(Parameter * param)
{
    param_vector.push_back(param);
}


/**
 * Add all registered LPE knotholder handles to the knotholder
 */
void
Effect::addHandles(KnotHolder *knotholder, SPItem *item) {
    using namespace Inkscape::LivePathEffect;

    // add handles provided by the effect itself
    addKnotHolderEntities(knotholder, item);

    // add handles provided by the effect's parameters (if any)
    for (auto & p : param_vector) {
        p->addKnotHolderEntities(knotholder, item);
    }    
    if (is_load) {
        SPLPEItem *lpeitem = dynamic_cast<SPLPEItem *>(item);
        if (lpeitem) {
            sp_lpe_item_update_patheffect(lpeitem, false, false);
        }
    }
}

/**
 * Return a vector of PathVectors which contain all canvas indicators for this effect.
 * This is the function called by external code to get all canvas indicators (effect and its parameters)
 * lpeitem = the item onto which this effect is applied
 * @todo change return type to one pathvector, add all paths to one pathvector instead of maintaining a vector of pathvectors
 */
std::vector<Geom::PathVector>
Effect::getCanvasIndicators(SPLPEItem const* lpeitem)
{
    std::vector<Geom::PathVector> hp_vec;

    // add indicators provided by the effect itself
    addCanvasIndicators(lpeitem, hp_vec);

    // add indicators provided by the effect's parameters
    for (auto & p : param_vector) {
        p->addCanvasIndicators(lpeitem, hp_vec);
    }
    Geom::Affine scale = lpeitem->i2doc_affine();
    for (auto &path : hp_vec) {
        path *= scale;
    }
    return hp_vec;
}

/**
 * Add possible canvas indicators (i.e., helperpaths other than the original path) to \a hp_vec
 * This function should be overwritten by derived effects if they want to provide their own helperpaths.
 */
void
Effect::addCanvasIndicators(SPLPEItem const*/*lpeitem*/, std::vector<Geom::PathVector> &/*hp_vec*/)
{
}

/**
 * Call to a method on nodetool to update the helper path from the effect
 */
void
Effect::update_helperpath() {
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    if (desktop) {
        Inkscape::UI::Tools::NodeTool *nt = dynamic_cast<Inkscape::UI::Tools::NodeTool*>(desktop->event_context);
        if (nt) {
            Inkscape::UI::Tools::sp_update_helperpath(desktop);
        }
    }
}

/**
 * This *creates* a new widget, management of deletion should be done by the caller
 */
Gtk::Widget *
Effect::newWidget()
{
    // use manage here, because after deletion of Effect object, others might still be pointing to this widget.
    Gtk::Box * vbox = Gtk::manage( new Gtk::Box(Gtk::ORIENTATION_VERTICAL) );

    vbox->set_border_width(5);

    std::vector<Parameter *>::iterator it = param_vector.begin();
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            Parameter * param = *it;
            Gtk::Widget * widg = param->param_newWidget();
            Glib::ustring * tip = param->param_getTooltip();
            if (widg) {
                if (param->widget_is_enabled) {
                    widg->set_sensitive(true);
                } else {
                    widg->set_sensitive(false);
                }
                vbox->pack_start(*widg, true, true, 2);
                if (tip) {
                    widg->set_tooltip_text(*tip);
                } else {
                    widg->set_tooltip_text("");
                    widg->set_has_tooltip(false);
                }
            }
        }

        ++it;
    }
    if(Gtk::Widget* widg = defaultParamSet()) {
        vbox->pack_start(*widg, true, true, 2);
    }
    return dynamic_cast<Gtk::Widget *>(vbox);
}

bool sp_enter_tooltip(GdkEventCrossing *evt, Gtk::Widget *widg)
{
    widg->trigger_tooltip_query();
    return true;
}

/**
 * This *creates* a new widget, with default values setter
 */
Gtk::Widget *
Effect::defaultParamSet()
{
    // use manage here, because after deletion of Effect object, others might still be pointing to this widget.
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    Gtk::Box * vbox_expander = Gtk::manage( new Gtk::Box(Gtk::ORIENTATION_VERTICAL) );
    Glib::ustring effectname = (Glib::ustring)Inkscape::LivePathEffect::LPETypeConverter.get_label(effectType());
    Glib::ustring effectkey = (Glib::ustring)Inkscape::LivePathEffect::LPETypeConverter.get_key(effectType());
    std::vector<Parameter *>::iterator it = param_vector.begin();
    bool has_params = false;
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            has_params = true;
            Parameter * param = *it;
            const gchar * key   = param->param_key.c_str();
            if (g_strcmp0(key, "lpeversion") == 0) {
                ++it;
                continue;
            }
            const gchar * label = param->param_label.c_str();
            Glib::ustring value = param->param_getSVGValue();
            Glib::ustring defvalue  = param->param_getDefaultSVGValue();
            Glib::ustring pref_path = "/live_effects/";
            pref_path += effectkey;
            pref_path +="/";
            pref_path += key;
            bool valid = prefs->getEntry(pref_path).isValid();
            const gchar * set_or_upd;
            Glib::ustring def = Glib::ustring(_("<b>Default value:</b> ")) + defvalue;
            Glib::ustring ove = Glib::ustring(_("<b>Default value overridden:</b> "));
            if (valid) {
                set_or_upd = _("Update");
                def = "";
            } else {
                set_or_upd = _("Set");
                ove = "";
            }
            Gtk::Box * vbox_param = Gtk::manage( new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) );
            Gtk::Box *namedicon = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
            Gtk::Label *parameter_label = Gtk::manage(new Gtk::Label(label, Gtk::ALIGN_START));
            parameter_label->set_use_markup(true);
            parameter_label->set_use_underline(true);
            parameter_label->set_ellipsize(Pango::ELLIPSIZE_END);
            Glib::ustring tooltip = Glib::ustring("<b>") + parameter_label->get_text() + Glib::ustring("</b>\n") +
                                    param->param_tooltip + Glib::ustring("\n");
            Gtk::Image *info = sp_get_icon_image("info", 20);
            Gtk::EventBox *infoeventbox = Gtk::manage(new Gtk::EventBox());
            infoeventbox->add(*info);
            infoeventbox->set_tooltip_markup((tooltip + def + ove).c_str());
            namedicon->pack_start(*infoeventbox, false, false, 2);
            namedicon->pack_start(*parameter_label, true, true, 2);
            namedicon->set_homogeneous(false);
            vbox_param->pack_start(*namedicon, true, true, 2);
            Gtk::Button *set = Gtk::manage(new Gtk::Button((Glib::ustring)set_or_upd));
            Gtk::Button *unset = Gtk::manage(new Gtk::Button(Glib::ustring(_("Unset"))));
            unset->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &Effect::unsetDefaultParam), pref_path,
                                                       tooltip, param, info, set, unset));
            set->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &Effect::setDefaultParam), pref_path, tooltip,
                                                     param, info, set, unset));
            if (!valid) {
                unset->set_sensitive(false);
            }
            unset->set_size_request (90, -1);
            set->set_size_request (90, -1);
            vbox_param->pack_end(*unset, false, true, 2);
            vbox_param->pack_end(*set, false, true, 2);

            vbox_expander->pack_start(*vbox_param, true, true, 2);
        }
        ++it;
    }
    Glib::ustring tip = "<b>" + effectname + (Glib::ustring)_("</b>: Set default parameters");
    Gtk::Expander * expander = Gtk::manage(new Gtk::Expander(tip));
    expander->set_use_markup(true);
    expander->add(*vbox_expander);
    expander->set_expanded(defaultsopen);
    expander->property_expanded().signal_changed().connect(sigc::bind<0>(sigc::mem_fun(*this, &Effect::onDefaultsExpanderChanged), expander ));
    if (has_params) {
        Gtk::Widget *vboxwidg = dynamic_cast<Gtk::Widget *>(expander);
        vboxwidg->set_margin_bottom(5);
        vboxwidg->set_margin_top(5);
        return vboxwidg;
    } else {
        return nullptr;
    }
}

void
Effect::onDefaultsExpanderChanged(Gtk::Expander * expander)
{
    defaultsopen = expander->get_expanded();
}

void Effect::setDefaultParam(Glib::ustring pref_path, Glib::ustring tooltip, Parameter *param, Gtk::Image *info,
                             Gtk::Button *set, Gtk::Button *unset)
{
    Glib::ustring value = param->param_getSVGValue();
    Glib::ustring defvalue  = param->param_getDefaultSVGValue();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setString(pref_path, value);
    gchar * label = _("Update");
    set->set_label((Glib::ustring)label);
    unset->set_sensitive(true);
    Glib::ustring ove = Glib::ustring(_("<b>Default value overridden:</b> ")) + value;
    info->set_tooltip_markup((tooltip + ove).c_str());
}

void Effect::unsetDefaultParam(Glib::ustring pref_path, Glib::ustring tooltip, Parameter *param, Gtk::Image *info,
                               Gtk::Button *set, Gtk::Button *unset)
{
    Glib::ustring value = param->param_getSVGValue();
    Glib::ustring defvalue  = param->param_getDefaultSVGValue();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->remove(pref_path);
    gchar * label = _("Set");
    set->set_label((Glib::ustring)label);
    unset->set_sensitive(false);
    Glib::ustring def = Glib::ustring(_("<b>Default value:</b> Default"));
    info->set_tooltip_markup((tooltip + def).c_str());
}

Inkscape::XML::Node *Effect::getRepr()
{
    return lpeobj->getRepr();
}

SPDocument *Effect::getSPDoc()
{
    if (lpeobj->document == nullptr) {
        g_message("Effect::getSPDoc() returns NULL");
    }
    return lpeobj->document;
}

Parameter *
Effect::getParameter(const char * key)
{
    Glib::ustring stringkey(key);

    if (param_vector.empty()) return nullptr;
    std::vector<Parameter *>::iterator it = param_vector.begin();
    while (it != param_vector.end()) {
        Parameter * param = *it;
        if ( param->param_key == key) {
            return param;
        }

        ++it;
    }

    return nullptr;
}

Parameter *
Effect::getNextOncanvasEditableParam()
{
    if (param_vector.size() == 0) // no parameters
        return nullptr;

    oncanvasedit_it++;
    if (oncanvasedit_it >= static_cast<int>(param_vector.size())) {
        oncanvasedit_it = 0;
    }
    int old_it = oncanvasedit_it;

    do {
        Parameter * param = param_vector[oncanvasedit_it];
        if(param && param->oncanvas_editable) {
            return param;
        } else {
            oncanvasedit_it++;
            if (oncanvasedit_it == static_cast<int>(param_vector.size())) {  // loop round the map
                oncanvasedit_it = 0;
            }
        }
    } while (oncanvasedit_it != old_it); // iterate until complete loop through map has been made

    return nullptr;
}

void
Effect::editNextParamOncanvas(SPItem * item, SPDesktop * desktop)
{
    if (!desktop) return;

    Parameter * param = getNextOncanvasEditableParam();
    if (param) {
        param->param_editOncanvas(item, desktop);
        gchar *message = g_strdup_printf(_("Editing parameter <b>%s</b>."), param->param_label.c_str());
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free(message);
    } else {
        desktop->messageStack()->flash( Inkscape::WARNING_MESSAGE,
                                        _("None of the applied path effect's parameters can be edited on-canvas.") );
    }
}

/* This function should reset the defaults and is used for example to initialize an effect right after it has been applied to a path
* The nice thing about this is that this function can use knowledge of the original path and set things accordingly for example to the size or origin of the original path!
*/
void
Effect::resetDefaults(SPItem const* /*item*/)
{
    std::vector<Inkscape::LivePathEffect::Parameter *>::iterator p;
    for (p = param_vector.begin(); p != param_vector.end(); ++p) {
        (*p)->param_set_default();
        (*p)->write_to_SVG();
    }
}

bool
Effect::providesKnotholder() const
{
    // does the effect actively provide any knotholder entities of its own?
    if (_provides_knotholder_entities) {
        return true;
    }

    // otherwise: are there any parameters that have knotholderentities?
    for (auto p : param_vector) {
        if (p->providesKnotHolderEntities()) {
            return true;
        }
    }

    return false;
}

} /* namespace LivePathEffect */

} /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
