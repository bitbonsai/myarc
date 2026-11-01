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
// search fields, and "circle-arrow-down" for the Library rail's Downloads.
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

// The creation menu's row icons, traced from Arc's. New Space is the Spaces
// rail glyph's two cards with a plus on the front one, on its 28 DIP canvas.
// New Split and New Tab use an 18 DIP canvas at stroke 1.6: a window split a
// third of the way across with a plus replacing its top right corner, and a
// rounded square with a plus.
inline constexpr gfx::PathElement kNewSpacePath[] = {
    gfx::CANVAS_DIMENSIONS, 28, gfx::STROKE, 1.8f, gfx::ROUND_RECT, 11.1f,
    7.05f, 13, 18.4f, 2.6f, gfx::MOVE_TO, 11.1f, 22.35f, gfx::LINE_TO, 10.26f,
    22.59f, gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 7.05f, 20.8f, gfx::LINE_TO, 3.66f,
    8.98f, gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 5.44f, 5.76f, gfx::LINE_TO, 12.94f,
    3.61f, gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 16.15f, 5.4f, gfx::LINE_TO, 16.6f,
    7.05f, gfx::MOVE_TO, 14.1f, 16.25f, gfx::H_LINE_TO, 21.1f, gfx::MOVE_TO,
    17.6f, 12.75f, gfx::V_LINE_TO, 19.75f};
inline constexpr gfx::VectorIconRep kNewSpaceRep[] = {{kNewSpacePath}};
inline constexpr gfx::VectorIcon kNewSpaceIcon(kNewSpaceRep,
                                               std::size(kNewSpaceRep),
                                               "myarc_new_space");

inline constexpr gfx::PathElement kNewSplitPath[] = {
    gfx::CANVAS_DIMENSIONS, 18, gfx::STROKE, 1.6f, gfx::MOVE_TO, 12.5f, 2.5f,
    gfx::H_LINE_TO, 3.5f, gfx::ARC_TO, 2.5f, 2.5f, 0, 0, 0, 1, 5,
    gfx::V_LINE_TO, 14, gfx::ARC_TO, 2.5f, 2.5f, 0, 0, 0, 3.5f, 16.5f,
    gfx::H_LINE_TO, 13.5f, gfx::ARC_TO, 2.5f, 2.5f, 0, 0, 0, 16, 14,
    gfx::V_LINE_TO, 6.5f, gfx::MOVE_TO, 6, 2.5f, gfx::V_LINE_TO, 16.5f,
    gfx::MOVE_TO, 15, 1, gfx::V_LINE_TO, 6, gfx::MOVE_TO, 12.5f, 3.5f,
    gfx::H_LINE_TO, 17.5f};
inline constexpr gfx::VectorIconRep kNewSplitRep[] = {{kNewSplitPath}};
inline constexpr gfx::VectorIcon kNewSplitIcon(kNewSplitRep,
                                               std::size(kNewSplitRep),
                                               "myarc_new_split");

inline constexpr gfx::PathElement kNewTabPath[] = {
    gfx::CANVAS_DIMENSIONS, 18, gfx::STROKE, 1.6f, gfx::ROUND_RECT, 1.5f, 1.5f,
    15, 15, 3, gfx::MOVE_TO, 5.5f, 9, gfx::H_LINE_TO, 12.5f, gfx::MOVE_TO, 9,
    5.5f, gfx::V_LINE_TO, 12.5f};
inline constexpr gfx::VectorIconRep kNewTabRep[] = {{kNewTabPath}};
inline constexpr gfx::VectorIcon kNewTabIcon(kNewTabRep,
                                             std::size(kNewTabRep),
                                             "myarc_new_tab");

// A Library Space card's controls, traced from Arc's on an 18 DIP canvas: the
// header's paintbrush (edit), and the footer's four-way move arrows and "...".
inline constexpr gfx::PathElement kSpaceEditPath[] = {
    gfx::CANVAS_DIMENSIONS, 18,
    // Bristles.
    gfx::MOVE_TO, 6.95f, 9.06f, gfx::LINE_TO, 9.14f, 11.53f,
    gfx::LINE_TO, 8.7f, 12.91f,
    gfx::ARC_TO, 2.8f, 2.8f, 0, 0, 1, 4.44f, 14.6f,
    gfx::LINE_TO, 2.44f, 13.32f,
    gfx::QUADRATIC_TO, 3.12f, 12.12f, 3.21f, 11.85f,
    gfx::ARC_TO, 2.8f, 2.8f, 0, 0, 1, 5.41f, 9.52f, gfx::CLOSE,
    // Ferrule band.
    gfx::MOVE_TO, 7.26f, 8.58f, gfx::LINE_TO, 8.19f, 7.75f,
    gfx::LINE_TO, 10.58f, 10.44f, gfx::LINE_TO, 9.65f, 11.27f, gfx::CLOSE,
    // Handle.
    gfx::MOVE_TO, 8.57f, 7.35f, gfx::LINE_TO, 13.31f, 3.88f,
    gfx::ARC_TO, 1.3f, 1.3f, 0, 0, 1, 15.04f, 5.82f,
    gfx::LINE_TO, 11.03f, 10.12f, gfx::CLOSE};
inline constexpr gfx::VectorIconRep kSpaceEditRep[] = {{kSpaceEditPath}};
inline constexpr gfx::VectorIcon kSpaceEditIcon(kSpaceEditRep,
                                                std::size(kSpaceEditRep),
                                                "myarc_space_edit");

inline constexpr gfx::PathElement kSpaceMovePath[] = {
    gfx::CANVAS_DIMENSIONS, 18,
    gfx::MOVE_TO, 9, 1.6f, gfx::LINE_TO, 11.5f, 5.2f, gfx::H_LINE_TO, 6.5f,
    gfx::CLOSE, gfx::MOVE_TO, 9, 16.4f, gfx::LINE_TO, 6.5f, 12.8f,
    gfx::H_LINE_TO, 11.5f, gfx::CLOSE, gfx::MOVE_TO, 1.6f, 9,
    gfx::LINE_TO, 5.2f, 6.5f, gfx::V_LINE_TO, 11.5f, gfx::CLOSE,
    gfx::MOVE_TO, 16.4f, 9, gfx::LINE_TO, 12.8f, 11.5f, gfx::V_LINE_TO, 6.5f,
    gfx::CLOSE, gfx::NEW_PATH, gfx::STROKE, 1.7f, gfx::MOVE_TO, 9, 4.5f,
    gfx::V_LINE_TO, 13.5f, gfx::MOVE_TO, 4.5f, 9, gfx::H_LINE_TO, 13.5f};
inline constexpr gfx::VectorIconRep kSpaceMoveRep[] = {{kSpaceMovePath}};
inline constexpr gfx::VectorIcon kSpaceMoveIcon(kSpaceMoveRep,
                                                std::size(kSpaceMoveRep),
                                                "myarc_space_move");

inline constexpr gfx::PathElement kSpaceMorePath[] = {
    gfx::CANVAS_DIMENSIONS, 18, gfx::CIRCLE, 4.5f, 9, 1.35f,
    gfx::CIRCLE, 9, 9, 1.35f, gfx::CIRCLE, 13.5f, 9, 1.35f};
inline constexpr gfx::VectorIconRep kSpaceMoreRep[] = {{kSpaceMorePath}};
inline constexpr gfx::VectorIcon kSpaceMoreIcon(kSpaceMoreRep,
                                                std::size(kSpaceMoreRep),
                                                "myarc_space_more");

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

// Media, Spaces and Boosts are traced from Arc's rail glyphs on a 28 DIP canvas
// at stroke 1.8, drawn at 28 DIP. Idle glyphs stroke only the edges a front
// shape leaves visible; the duotone ones paint back to front instead.

// Media: a photo stacked on a second card, its sun and hills filled.
#define MYARC_MEDIA_BACK gfx::ROUND_RECT, 2.4f, 4.9f, 17.7f, 14.9f, 2.6f
#define MYARC_MEDIA_FRONT gfx::ROUND_RECT, 8.2f, 10.4f, 18.7f, 13.5f, 2.6f
#define MYARC_MEDIA_PHOTO                                                    \
  gfx::CIRCLE, 14.2f, 14.4f, 1.8f, gfx::MOVE_TO, 8.9f, 23.1f, gfx::LINE_TO,  \
      13, 18.6f, gfx::QUADRATIC_TO, 13.6f, 18, 14.2f, 18.6f, gfx::LINE_TO,   \
      16, 20.4f, gfx::LINE_TO, 20, 16.4f, gfx::QUADRATIC_TO, 20.6f, 15.8f,   \
      21.2f, 16.4f, gfx::LINE_TO, 26.6f, 21.8f, gfx::V_LINE_TO, 23.1f,       \
      gfx::CLOSE

inline constexpr gfx::PathElement kMediaPath[] = {
    gfx::CANVAS_DIMENSIONS, 28, gfx::STROKE, 1.8f, gfx::MOVE_TO, 8.2f, 19.8f,
    gfx::H_LINE_TO, 5, gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 2.4f, 17.2f,
    gfx::V_LINE_TO, 7.5f, gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 5, 4.9f,
    gfx::H_LINE_TO, 17.5f, gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 20.1f, 7.5f,
    gfx::V_LINE_TO, 10.4f, MYARC_MEDIA_FRONT, gfx::NEW_PATH, MYARC_MEDIA_PHOTO};
inline constexpr gfx::VectorIconRep kMediaRep[] = {{kMediaPath}};
inline constexpr gfx::VectorIcon kMediaIcon(kMediaRep,
                                            std::size(kMediaRep),
                                            "myarc_media");

// Spaces: an upright card over a second one tilted 16 degrees behind it.
#define MYARC_SPACES_FRONT gfx::ROUND_RECT, 11.1f, 7.05f, 13, 18.4f, 2.6f
#define MYARC_SPACES_BACK                                                      \
  gfx::MOVE_TO, 5.44f, 5.76f, gfx::LINE_TO, 12.94f, 3.61f, gfx::ARC_TO, 2.6f, \
      2.6f, 0, 0, 1, 16.15f, 5.4f, gfx::LINE_TO, 19.54f, 17.22f,              \
      gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 17.76f, 20.44f, gfx::LINE_TO, 10.26f,  \
      22.59f, gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 7.05f, 20.8f, gfx::LINE_TO,    \
      3.66f, 8.98f, gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 5.44f, 5.76f, gfx::CLOSE

inline constexpr gfx::PathElement kSpacesPath[] = {
    gfx::CANVAS_DIMENSIONS, 28, gfx::STROKE, 1.8f, MYARC_SPACES_FRONT,
    gfx::MOVE_TO, 11.1f, 22.35f, gfx::LINE_TO, 10.26f, 22.59f,
    gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 7.05f, 20.8f, gfx::LINE_TO, 3.66f, 8.98f,
    gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 5.44f, 5.76f, gfx::LINE_TO, 12.94f, 3.61f,
    gfx::ARC_TO, 2.6f, 2.6f, 0, 0, 1, 16.15f, 5.4f, gfx::LINE_TO, 16.6f, 7.05f};
inline constexpr gfx::VectorIconRep kSpacesRep[] = {{kSpacesPath}};
inline constexpr gfx::VectorIcon kSpacesIcon(kSpacesRep,
                                             std::size(kSpacesRep),
                                             "myarc_spaces");

// Boosts: a paintbrush across a tile tilted 13 degrees, with two sparkles. The
// tile's outline is cleared around the brush, as Arc leaves a gap there.
#define MYARC_BOOSTS_TILE                                                     \
  gfx::MOVE_TO, 7.56f, 5.84f, gfx::LINE_TO, 16.33f, 3.81f, gfx::ARC_TO, 4.2f, \
      4.2f, 0, 0, 1, 21.36f, 6.96f, gfx::LINE_TO, 23.39f, 15.73f,             \
      gfx::ARC_TO, 4.2f, 4.2f, 0, 0, 1, 20.24f, 20.76f, gfx::LINE_TO, 11.47f,  \
      22.79f, gfx::ARC_TO, 4.2f, 4.2f, 0, 0, 1, 6.44f, 19.64f, gfx::LINE_TO,   \
      4.41f, 10.87f, gfx::ARC_TO, 4.2f, 4.2f, 0, 0, 1, 7.56f, 5.84f, gfx::CLOSE
#define MYARC_BOOSTS_BRUSH                                                    \
  gfx::MOVE_TO, 13.05f, 8.32f, gfx::LINE_TO, 16.36f, 10.55f, gfx::ARC_TO,     \
      1.5f, 1.5f, 0, 0, 1, 16.77f, 12.64f, gfx::LINE_TO, 13.64f, 17.28f,      \
      gfx::ARC_TO, 1.5f, 1.5f, 0, 0, 1, 11.55f, 17.68f, gfx::LINE_TO, 8.24f,  \
      15.45f, gfx::ARC_TO, 1.5f, 1.5f, 0, 0, 1, 7.83f, 13.36f, gfx::LINE_TO,  \
      10.96f, 8.72f, gfx::ARC_TO, 1.5f, 1.5f, 0, 0, 1, 13.05f, 8.32f,         \
      gfx::CLOSE, gfx::MOVE_TO, 8.32f, 15.87f, gfx::LINE_TO, 11.14f, 17.77f,  \
      gfx::ARC_TO, 0.8f, 0.8f, 0, 0, 1, 11.35f, 18.88f, gfx::LINE_TO, 10.79f, \
      19.71f, gfx::ARC_TO, 0.8f, 0.8f, 0, 0, 1, 9.68f, 19.93f, gfx::LINE_TO,  \
      6.86f, 18.03f, gfx::ARC_TO, 0.8f, 0.8f, 0, 0, 1, 6.65f, 16.92f,         \
      gfx::LINE_TO, 7.21f, 16.09f, gfx::ARC_TO, 0.8f, 0.8f, 0, 0, 1, 8.32f,   \
      15.87f, gfx::CLOSE, gfx::MOVE_TO, 8.11f, 19.21f, gfx::ARC_TO, 1.6f,     \
      1.6f, 0, 0, 1, 8.54f, 21.44f, gfx::LINE_TO, 5.85f, 25.41f, gfx::ARC_TO, \
      1.6f, 1.6f, 0, 0, 1, 3.2f, 23.62f, gfx::LINE_TO, 5.89f, 19.65f,         \
      gfx::ARC_TO, 1.6f, 1.6f, 0, 0, 1, 8.11f, 19.21f, gfx::CLOSE
// Clears the brush and a 1 DIP gap around its outline.
#define MYARC_BOOSTS_GAP                                                  \
  gfx::NEW_PATH, gfx::PATH_MODE_CLEAR, gfx::STROKE, 3.8f, MYARC_BOOSTS_BRUSH, \
      gfx::NEW_PATH, gfx::PATH_MODE_CLEAR, MYARC_BOOSTS_BRUSH
#define MYARC_BOOSTS_SPARKLES \
  gfx::CIRCLE, 15.6f, 7.2f, 0.85f, gfx::CIRCLE, 19.3f, 10.6f, 0.85f

inline constexpr gfx::PathElement kBoostsPath[] = {
    gfx::CANVAS_DIMENSIONS, 28, gfx::STROKE, 1.8f, MYARC_BOOSTS_TILE,
    MYARC_BOOSTS_GAP, gfx::NEW_PATH, gfx::STROKE, 1.8f, MYARC_BOOSTS_BRUSH,
    gfx::NEW_PATH, MYARC_BOOSTS_SPARKLES};
inline constexpr gfx::VectorIconRep kBoostsRep[] = {{kBoostsPath}};
inline constexpr gfx::VectorIcon kBoostsIcon(kBoostsRep,
                                             std::size(kBoostsRep),
                                             "myarc_boosts");

// Arc's selected Library rail glyphs are duotone: the enclosed shapes filled
// blue (#2523F6) under strokes in light lavender (#EDEDFD). The colors are
// baked in, so ImageModel's color argument doesn't change them.
#define MYARC_RAIL_FILL gfx::PATH_COLOR_ARGB, 0xFF, 0x25, 0x23, 0xF6
#define MYARC_RAIL_INK \
  gfx::NEW_PATH, gfx::PATH_COLOR_ARGB, 0xFF, 0xED, 0xED, 0xFD
#define MYARC_RAIL_LINE MYARC_RAIL_INK, gfx::STROKE, 2
#define MYARC_RAIL_THIN MYARC_RAIL_INK, gfx::STROKE, 1.8f

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

// The front photo's fill covers the back card's hidden corner.
inline constexpr gfx::PathElement kMediaActivePath[] = {
    gfx::CANVAS_DIMENSIONS, 28, MYARC_RAIL_FILL, MYARC_MEDIA_BACK,
    MYARC_RAIL_THIN, MYARC_MEDIA_BACK, gfx::NEW_PATH, MYARC_RAIL_FILL,
    MYARC_MEDIA_FRONT, MYARC_RAIL_THIN, MYARC_MEDIA_FRONT, MYARC_RAIL_INK,
    MYARC_MEDIA_PHOTO};
inline constexpr gfx::VectorIconRep kMediaActiveRep[] = {{kMediaActivePath}};
inline constexpr gfx::VectorIcon kMediaActiveIcon(kMediaActiveRep,
                                                  std::size(kMediaActiveRep),
                                                  "myarc_media_active");

inline constexpr gfx::PathElement kSpacesActivePath[] = {
    gfx::CANVAS_DIMENSIONS, 28, MYARC_RAIL_FILL, MYARC_SPACES_BACK,
    MYARC_RAIL_THIN, MYARC_SPACES_BACK, gfx::NEW_PATH, MYARC_RAIL_FILL,
    MYARC_SPACES_FRONT, MYARC_RAIL_THIN, MYARC_SPACES_FRONT};
inline constexpr gfx::VectorIconRep kSpacesActiveRep[] = {{kSpacesActivePath}};
inline constexpr gfx::VectorIcon kSpacesActiveIcon(kSpacesActiveRep,
                                                   std::size(kSpacesActiveRep),
                                                   "myarc_spaces_active");

inline constexpr gfx::PathElement kBoostsActivePath[] = {
    gfx::CANVAS_DIMENSIONS, 28, MYARC_RAIL_FILL, MYARC_BOOSTS_TILE,
    MYARC_RAIL_THIN, MYARC_BOOSTS_TILE, MYARC_BOOSTS_GAP, gfx::NEW_PATH,
    MYARC_RAIL_FILL, MYARC_BOOSTS_BRUSH, MYARC_RAIL_THIN, MYARC_BOOSTS_BRUSH,
    MYARC_RAIL_INK, MYARC_BOOSTS_SPARKLES};
inline constexpr gfx::VectorIconRep kBoostsActiveRep[] = {{kBoostsActivePath}};
inline constexpr gfx::VectorIcon kBoostsActiveIcon(kBoostsActiveRep,
                                                   std::size(kBoostsActiveRep),
                                                   "myarc_boosts_active");

#undef MYARC_BOOSTS_SPARKLES
#undef MYARC_BOOSTS_GAP
#undef MYARC_BOOSTS_BRUSH
#undef MYARC_BOOSTS_TILE
#undef MYARC_SPACES_BACK
#undef MYARC_SPACES_FRONT
#undef MYARC_MEDIA_PHOTO
#undef MYARC_MEDIA_FRONT
#undef MYARC_MEDIA_BACK
#undef MYARC_RAIL_THIN
#undef MYARC_RAIL_LINE
#undef MYARC_RAIL_INK
#undef MYARC_RAIL_FILL

}  // namespace myarc

#endif  // MYARC_ICONS_H_
