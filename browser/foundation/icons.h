#ifndef MYARC_ICONS_H_
#define MYARC_ICONS_H_

#include <iterator>

#include "ui/gfx/vector_icon_types.h"

namespace myarc {

// Filled single-path icon from design/icons/saturn.svg, 24 DIP canvas, even-odd
// fill: the default Space icon. The Library button draws its own art
// (footer_button.cc).
inline constexpr gfx::PathElement kSaturnPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::MOVE_TO, 5.6f, 12,
    gfx::CUBIC_TO, 5.6f, 8.465f, 8.465f, 5.6f, 12, 5.6f,
    gfx::CUBIC_TO, 15.02f, 5.6f, 17.552f, 7.693f, 18.225f, 10.507f,
    gfx::CUBIC_TO, 16.898f, 11.421f, 14.928f, 12.477f, 12.495f, 13.363f,
    gfx::CUBIC_TO, 10.063f, 14.248f, 7.875f, 14.705f, 6.271f, 14.858f,
    gfx::CUBIC_TO, 5.841f, 13.998f, 5.6f, 13.027f, 5.6f, 12, gfx::CLOSE,
    gfx::MOVE_TO, 16.094f, 6.823f,
    gfx::CUBIC_TO, 19.357f, 6.292f, 21.911f, 6.629f, 22.477f, 8.187f,
    gfx::CUBIC_TO, 23.337f, 10.548f, 18.894f, 13.694f, 13.35f, 15.712f,
    gfx::CUBIC_TO, 7.807f, 17.73f, 2.381f, 18.175f, 1.522f, 15.813f,
    gfx::CUBIC_TO, 0.955f, 14.256f, 2.694f, 12.358f, 5.534f, 10.666f,
    gfx::CUBIC_TO, 5.446f, 11.097f, 5.4f, 11.543f, 5.4f, 12,
    gfx::CUBIC_TO, 5.4f, 12.175f, 5.406f, 12.348f, 5.42f, 12.52f,
    gfx::CUBIC_TO, 3.598f, 13.75f, 2.76f, 14.831f, 2.931f, 15.301f,
    gfx::CUBIC_TO, 3.23f, 16.122f, 7.093f, 16.393f, 12.837f, 14.302f,
    gfx::CUBIC_TO, 18.581f, 12.212f, 21.367f, 9.521f, 21.068f, 8.699f,
    gfx::CUBIC_TO, 20.897f, 8.23f, 19.561f, 7.94f, 17.374f, 8.169f,
    gfx::CUBIC_TO, 17.013f, 7.662f, 16.581f, 7.209f, 16.094f, 6.823f,
    gfx::CLOSE, gfx::MOVE_TO, 13.692f, 16.652f,
    gfx::CUBIC_TO, 15.125f, 16.13f, 16.481f, 15.542f, 17.703f, 14.907f,
    gfx::CUBIC_TO, 16.644f, 16.98f, 14.488f, 18.4f, 12, 18.4f,
    gfx::CUBIC_TO, 11.112f, 18.4f, 10.267f, 18.219f, 9.499f, 17.893f,
    gfx::CUBIC_TO, 10.843f, 17.594f, 12.259f, 17.173f, 13.692f, 16.652f,
    gfx::CLOSE};
inline constexpr gfx::VectorIconRep kSaturnRep[] = {{kSaturnPath}};
inline constexpr gfx::VectorIcon kSaturnIcon(kSaturnRep,
                                             std::size(kSaturnRep),
                                             "myarc_saturn");

// Icons from Lucide (ISC, lucide-static 1.61.0): 24 DIP canvas, stroke 2.
// "archive" for Archived Tabs, "plus" for Create, "search" for the Library's
// search fields, and the Library rail's "image", "circle-arrow-down", "copy"
// and "wand".
inline constexpr gfx::PathElement kArchivePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::ROUND_RECT, 2, 3, 20, 5, 1, gfx::MOVE_TO, 4, 8, gfx::V_LINE_TO, 19,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, 2, gfx::H_LINE_TO, 18,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, -2, gfx::V_LINE_TO, 8,
    gfx::MOVE_TO, 10, 12, gfx::R_H_LINE_TO, 4};
inline constexpr gfx::VectorIconRep kArchiveRep[] = {{kArchivePath}};
inline constexpr gfx::VectorIcon kArchiveIcon(kArchiveRep,
                                              std::size(kArchiveRep),
                                              "myarc_archive");

inline constexpr gfx::PathElement kPlusPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 5, 12,
    gfx::H_LINE_TO, 19, gfx::MOVE_TO, 12, 5, gfx::V_LINE_TO, 19};
inline constexpr gfx::VectorIconRep kPlusRep[] = {{kPlusPath}};
inline constexpr gfx::VectorIcon kPlusIcon(kPlusRep,
                                           std::size(kPlusRep),
                                           "myarc_plus");

// Lucide "arrow-left" at stroke 1.5: the Library's Exit Library tile, as thin
// as Arc's.
inline constexpr gfx::PathElement kArrowLeftPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 1.5f, gfx::MOVE_TO, 12, 19,
    gfx::R_LINE_TO, -7, -7, gfx::R_LINE_TO, 7, -7, gfx::MOVE_TO, 19, 12,
    gfx::H_LINE_TO, 5};
inline constexpr gfx::VectorIconRep kArrowLeftRep[] = {{kArrowLeftPath}};
inline constexpr gfx::VectorIcon kArrowLeftIcon(kArrowLeftRep,
                                                std::size(kArrowLeftRep),
                                                "myarc_arrow_left");

inline constexpr gfx::PathElement kSearchPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 21, 21,
    gfx::R_LINE_TO, -4.34f, -4.34f, gfx::NEW_PATH, gfx::STROKE, 2,
    gfx::CIRCLE, 11, 11, 8};
inline constexpr gfx::VectorIconRep kSearchRep[] = {{kSearchPath}};
inline constexpr gfx::VectorIcon kSearchIcon(kSearchRep,
                                             std::size(kSearchRep),
                                             "myarc_search");

inline constexpr gfx::PathElement kImagePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::ROUND_RECT, 3, 3, 18, 18, 2, gfx::NEW_PATH, gfx::STROKE, 2,
    gfx::CIRCLE, 9, 9, 2, gfx::NEW_PATH, gfx::STROKE, 2, gfx::MOVE_TO, 21, 15,
    gfx::R_LINE_TO, -3.086f, -3.086f, gfx::R_ARC_TO, 2, 2, 0, 0, 0, -2.828f, 0,
    gfx::LINE_TO, 6, 21};
inline constexpr gfx::VectorIconRep kImageRep[] = {{kImagePath}};
inline constexpr gfx::VectorIcon kImageIcon(kImageRep,
                                            std::size(kImageRep),
                                            "myarc_image");

inline constexpr gfx::PathElement kCircleArrowDownPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::CIRCLE, 12, 12, 10,
    gfx::NEW_PATH, gfx::STROKE, 2, gfx::MOVE_TO, 12, 8, gfx::R_V_LINE_TO, 8,
    gfx::MOVE_TO, 8, 12, gfx::R_LINE_TO, 4, 4, gfx::R_LINE_TO, 4, -4};
inline constexpr gfx::VectorIconRep kCircleArrowDownRep[] = {
    {kCircleArrowDownPath}};
inline constexpr gfx::VectorIcon kCircleArrowDownIcon(
    kCircleArrowDownRep,
    std::size(kCircleArrowDownRep),
    "myarc_circle_arrow_down");

inline constexpr gfx::PathElement kCopyPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::ROUND_RECT, 8, 8, 14, 14, 2, gfx::NEW_PATH, gfx::STROKE, 2,
    gfx::MOVE_TO, 4, 16, gfx::R_CUBIC_TO, -1.1f, 0, -2, -0.9f, -2, -2,
    gfx::V_LINE_TO, 4, gfx::R_CUBIC_TO, 0, -1.1f, 0.9f, -2, 2, -2,
    gfx::R_H_LINE_TO, 10, gfx::R_CUBIC_TO, 1.1f, 0, 2, 0.9f, 2, 2};
inline constexpr gfx::VectorIconRep kCopyRep[] = {{kCopyPath}};
inline constexpr gfx::VectorIcon kCopyIcon(kCopyRep,
                                           std::size(kCopyRep),
                                           "myarc_copy");

inline constexpr gfx::PathElement kWandPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 15, 4,
    gfx::V_LINE_TO, 2, gfx::MOVE_TO, 15, 16, gfx::R_V_LINE_TO, -2,
    gfx::MOVE_TO, 8, 9, gfx::R_H_LINE_TO, 2, gfx::MOVE_TO, 20, 9,
    gfx::R_H_LINE_TO, 2, gfx::MOVE_TO, 17.8f, 11.8f, gfx::LINE_TO, 19, 13,
    gfx::MOVE_TO, 15, 9, gfx::R_H_LINE_TO, 0.01f, gfx::MOVE_TO, 17.8f, 6.2f,
    gfx::LINE_TO, 19, 5, gfx::MOVE_TO, 3, 21, gfx::R_LINE_TO, 9, -9,
    gfx::MOVE_TO, 12.2f, 6.2f, gfx::LINE_TO, 11, 5};
inline constexpr gfx::VectorIconRep kWandRep[] = {{kWandPath}};
inline constexpr gfx::VectorIcon kWandIcon(kWandRep,
                                           std::size(kWandRep),
                                           "myarc_wand");

// Arc's selected Library rail glyphs are duotone: the enclosed shapes filled
// blue (#2523F6) under the Lucide strokes in light lavender (#EDEDFD). The
// colors are baked in, so ImageModel's color argument doesn't change them.
#define MYARC_RAIL_FILL gfx::PATH_COLOR_ARGB, 0xFF, 0x25, 0x23, 0xF6
#define MYARC_RAIL_LINE                                          \
  gfx::NEW_PATH, gfx::PATH_COLOR_ARGB, 0xFF, 0xED, 0xED, 0xFD, \
      gfx::STROKE, 2

inline constexpr gfx::PathElement kArchiveActivePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, MYARC_RAIL_FILL,
    gfx::ROUND_RECT, 2, 3, 20, 5, 1, gfx::MOVE_TO, 4, 8, gfx::V_LINE_TO, 19,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, 2, gfx::H_LINE_TO, 18,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, -2, gfx::V_LINE_TO, 8, gfx::CLOSE,
    MYARC_RAIL_LINE,
    gfx::ROUND_RECT, 2, 3, 20, 5, 1, gfx::MOVE_TO, 4, 8, gfx::V_LINE_TO, 19,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, 2, gfx::H_LINE_TO, 18,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, -2, gfx::V_LINE_TO, 8,
    gfx::MOVE_TO, 10, 12, gfx::R_H_LINE_TO, 4};
inline constexpr gfx::VectorIconRep kArchiveActiveRep[] = {
    {kArchiveActivePath}};
inline constexpr gfx::VectorIcon kArchiveActiveIcon(kArchiveActiveRep,
                                                    std::size(kArchiveActiveRep),
                                                    "myarc_archive_active");

inline constexpr gfx::PathElement kImageActivePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, MYARC_RAIL_FILL,
    gfx::ROUND_RECT, 3, 3, 18, 18, 2, MYARC_RAIL_LINE,
    gfx::ROUND_RECT, 3, 3, 18, 18, 2, MYARC_RAIL_LINE,
    gfx::CIRCLE, 9, 9, 2, MYARC_RAIL_LINE, gfx::MOVE_TO, 21, 15,
    gfx::R_LINE_TO, -3.086f, -3.086f, gfx::R_ARC_TO, 2, 2, 0, 0, 0, -2.828f, 0,
    gfx::LINE_TO, 6, 21};
inline constexpr gfx::VectorIconRep kImageActiveRep[] = {{kImageActivePath}};
inline constexpr gfx::VectorIcon kImageActiveIcon(kImageActiveRep,
                                                  std::size(kImageActiveRep),
                                                  "myarc_image_active");

inline constexpr gfx::PathElement kCircleArrowDownActivePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, MYARC_RAIL_FILL, gfx::CIRCLE, 12, 12, 10,
    MYARC_RAIL_LINE, gfx::CIRCLE, 12, 12, 10,
    MYARC_RAIL_LINE, gfx::MOVE_TO, 12, 8, gfx::R_V_LINE_TO, 8,
    gfx::MOVE_TO, 8, 12, gfx::R_LINE_TO, 4, 4, gfx::R_LINE_TO, 4, -4};
inline constexpr gfx::VectorIconRep kCircleArrowDownActiveRep[] = {
    {kCircleArrowDownActivePath}};
inline constexpr gfx::VectorIcon kCircleArrowDownActiveIcon(
    kCircleArrowDownActiveRep,
    std::size(kCircleArrowDownActiveRep),
    "myarc_circle_arrow_down_active");

// The back card is filled whole; the front card's fill covers the overlap.
inline constexpr gfx::PathElement kCopyActivePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, MYARC_RAIL_FILL,
    gfx::ROUND_RECT, 2, 2, 14, 14, 2, gfx::NEW_PATH, MYARC_RAIL_FILL,
    gfx::ROUND_RECT, 8, 8, 14, 14, 2, MYARC_RAIL_LINE,
    gfx::ROUND_RECT, 8, 8, 14, 14, 2, MYARC_RAIL_LINE,
    gfx::MOVE_TO, 4, 16, gfx::R_CUBIC_TO, -1.1f, 0, -2, -0.9f, -2, -2,
    gfx::V_LINE_TO, 4, gfx::R_CUBIC_TO, 0, -1.1f, 0.9f, -2, 2, -2,
    gfx::R_H_LINE_TO, 10, gfx::R_CUBIC_TO, 1.1f, 0, 2, 0.9f, 2, 2};
inline constexpr gfx::VectorIconRep kCopyActiveRep[] = {{kCopyActivePath}};
inline constexpr gfx::VectorIcon kCopyActiveIcon(kCopyActiveRep,
                                                 std::size(kCopyActiveRep),
                                                 "myarc_copy_active");

// Arc's Boosts glyph sits on a filled tile; here the wand's sparkles do.
inline constexpr gfx::PathElement kWandActivePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, MYARC_RAIL_FILL,
    gfx::ROUND_RECT, 7, 1, 16, 16, 4, MYARC_RAIL_LINE,
    gfx::MOVE_TO, 15, 4, gfx::V_LINE_TO, 2, gfx::MOVE_TO, 15, 16,
    gfx::R_V_LINE_TO, -2, gfx::MOVE_TO, 8, 9, gfx::R_H_LINE_TO, 2,
    gfx::MOVE_TO, 20, 9, gfx::R_H_LINE_TO, 2, gfx::MOVE_TO, 17.8f, 11.8f,
    gfx::LINE_TO, 19, 13, gfx::MOVE_TO, 15, 9, gfx::R_H_LINE_TO, 0.01f,
    gfx::MOVE_TO, 17.8f, 6.2f, gfx::LINE_TO, 19, 5, gfx::MOVE_TO, 3, 21,
    gfx::R_LINE_TO, 9, -9, gfx::MOVE_TO, 12.2f, 6.2f, gfx::LINE_TO, 11, 5};
inline constexpr gfx::VectorIconRep kWandActiveRep[] = {{kWandActivePath}};
inline constexpr gfx::VectorIcon kWandActiveIcon(kWandActiveRep,
                                                 std::size(kWandActiveRep),
                                                 "myarc_wand_active");

#undef MYARC_RAIL_LINE
#undef MYARC_RAIL_FILL

}  // namespace myarc

#endif  // MYARC_ICONS_H_
