#ifndef MYARC_ICONS_H_
#define MYARC_ICONS_H_

#include <iterator>

#include "ui/gfx/vector_icon_types.h"

namespace myarc {

// Filled ringed planet from ionicons 8.1.1 (MIT, dist/svg/planet.svg),
// 512 DIP canvas, non-zero fill: the default Space icon. The Library button
// draws its own art (footer_button.cc).
inline constexpr gfx::PathElement kSaturnPath[] = {
    gfx::CANVAS_DIMENSIONS, 512,
    gfx::FILL_RULE_NONZERO,
    gfx::MOVE_TO, 96.85f, 286.62f,
    gfx::ARC_TO, 8, 8, 0, 0, 0, 84.32f, 294.87f,
    gfx::CUBIC_TO, 102.07f, 373.28f, 172.3f, 432, 256, 432,
    gfx::ARC_TO, 175.3f, 175.3f, 0, 0, 0, 308.41f, 424,
    gfx::ARC_TO, 8, 8, 0, 0, 0, 309.2f, 409,
    gfx::ARC_TO, 1120, 1120, 0, 0, 1, 199.72f, 353.39f,
    gfx::ARC_TO, 1126, 1126, 0, 0, 1, 96.85f, 286.62f,
    gfx::MOVE_TO, 492.72f, 339.51f,
    gfx::CUBIC_TO, 488.53f, 333.93f, 483.61f, 328.07f, 478.02f, 321.98f,
    gfx::ARC_TO, 15.83f, 15.83f, 0, 0, 0, 451.46f, 327.11f,
    gfx::CUBIC_TO, 451.46f, 327.27f, 451.35f, 327.42f, 451.29f, 327.58f,
    gfx::ARC_TO, 15.75f, 15.75f, 0, 0, 0, 454.44f, 343.64f,
    gfx::CUBIC_TO, 477.18f, 368.64f, 480.86f, 382.15f, 479.92f, 385,
    gfx::CUBIC_TO, 477.92f, 387.23f, 462.87f, 391.89f, 421.77f, 381.47f,
    gfx::QUADRATIC_TO, 412.94f, 379.23f, 402.45f, 376.01f,
    gfx::QUADRATIC_TO, 395.69f, 373.93f, 388.66f, 371.52f,
    gfx::ARC_TO, 176.8f, 176.8f, 0, 0, 0, 408.2f, 344.27f,
    gfx::CUBIC_TO, 408.37f, 343.98f, 408.55f, 343.69f, 408.72f, 343.39f,
    gfx::ARC_TO, 175.4f, 175.4f, 0, 0, 0, 432, 256,
    gfx::ARC_TO, 179, 179, 0, 0, 0, 431, 237,
    gfx::CUBIC_TO, 421.43f, 148.83f, 346.6f, 80, 256, 80,
    gfx::ARC_TO, 175.37f, 175.37f, 0, 0, 0, 149.6f, 115.89f,
    gfx::ARC_TO, 177.4f, 177.4f, 0, 0, 0, 103.77f, 167.73f,
    gfx::CUBIC_TO, 103.61f, 168.02f, 103.43f, 168.31f, 103.26f, 168.6f,
    gfx::ARC_TO, 175.5f, 175.5f, 0, 0, 0, 89.43f, 199.12f,
    gfx::QUADRATIC_TO, 83.84f, 194.25f, 78.64f, 189.45f,
    gfx::CUBIC_TO, 73.25f, 184.45f, 68.47f, 179.82f, 64.22f, 175.45f,
    gfx::CUBIC_TO, 34.65f, 145.19f, 31.13f, 129.84f, 32.06f, 127,
    gfx::CUBIC_TO, 34.06f, 124.77f, 47.6f, 121.13f, 80.68f, 128.31f,
    gfx::ARC_TO, 15.82f, 15.82f, 0, 0, 0, 96.22f, 123,
    gfx::LINE_TO, 96.58f, 122.56f,
    gfx::ARC_TO, 15.74f, 15.74f, 0, 0, 0, 87.91f, 97.13f,
    gfx::ARC_TO, 237, 237, 0, 0, 0, 64.13f, 93,
    gfx::CUBIC_TO, 33.41f, 89.47f, 13.3f, 95.52f, 4.35f, 111,
    gfx::CUBIC_TO, 1.11f, 116.58f, -2, 126.09f, 1.63f, 139.6f,
    gfx::CUBIC_TO, 7, 159.66f, 26.14f, 184, 53.23f, 209.5f,
    gfx::CUBIC_TO, 61.86f, 217.63f, 71.29f, 225.87f, 81.35f, 234.14f,
    gfx::CUBIC_TO, 88.67f, 240.14f, 96.35f, 246.2f, 104.25f, 252.22f,
    gfx::QUADRATIC_TO, 112.16f, 258.22f, 120.4f, 264.22f,
    gfx::QUADRATIC_TO, 128.64f, 270.22f, 137.1f, 276,
    gfx::CUBIC_TO, 162.51f, 293.61f, 189.36f, 310.52f, 215.69f, 325.69f,
    gfx::QUADRATIC_TO, 230.03f, 333.95f, 244.33f, 341.69f,
    gfx::QUADRATIC_TO, 258.63f, 349.43f, 272.7f, 356.5f,
    gfx::CUBIC_TO, 294.6f, 367.5f, 316.05f, 377.42f, 336.56f, 385.93f,
    gfx::QUADRATIC_TO, 349.75f, 391.41f, 362.37f, 396.09f,
    gfx::CUBIC_TO, 374.26f, 400.51f, 385.74f, 404.4f, 396.68f, 407.68f,
    gfx::LINE_TO, 397.78f, 408.01f,
    gfx::CUBIC_TO, 423.51f, 415.67f, 445.2f, 419.7f, 462.26f, 420.01f,
    gfx::LINE_TO, 464, 420.01f,
    gfx::CUBIC_TO, 485.64f, 420.01f, 500.3f, 413.63f, 507.58f, 401.01f,
    gfx::CUBIC_TO, 516.67f, 385.39f, 511.66f, 364.69f, 492.72f, 339.51f,
};
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

// "arrow-down" for the Today divider's Clear.
inline constexpr gfx::PathElement kArrowDownPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 12, 5, gfx::V_LINE_TO, 19,
    gfx::MOVE_TO, 19, 12, gfx::LINE_TO, 12, 19, gfx::LINE_TO, 5, 12};
inline constexpr gfx::VectorIconRep kArrowDownRep[] = {{kArrowDownPath}};
inline constexpr gfx::VectorIcon kArrowDownIcon(kArrowDownRep,
                                                std::size(kArrowDownRep),
                                                "myarc_arrow_down");

// Filled robot-head glyph for the agent Space, where external agents open
// their tabs: its footer glyph and title icon until the user picks another.
// Default even-odd fill punches the two circular eyes out of the head.
inline constexpr gfx::PathElement kBotPath[] = {
    gfx::CANVAS_DIMENSIONS, 243,
    gfx::ROUND_RECT, 45, 51, 153, 141, 28,
    gfx::CIRCLE, 91, 112, 17,
    gfx::CIRCLE, 152, 112, 17,
    gfx::MOVE_TO, 38, 95,
    gfx::H_LINE_TO, 27,
    gfx::ARC_TO, 11, 11, 0, 0, 0, 16, 106,
    gfx::V_LINE_TO, 149,
    gfx::ARC_TO, 11, 11, 0, 0, 0, 27, 160,
    gfx::H_LINE_TO, 38,
    gfx::CLOSE,
    gfx::MOVE_TO, 205, 95,
    gfx::H_LINE_TO, 216,
    gfx::ARC_TO, 11, 11, 0, 0, 1, 227, 106,
    gfx::V_LINE_TO, 149,
    gfx::ARC_TO, 11, 11, 0, 0, 1, 216, 160,
    gfx::H_LINE_TO, 205,
    gfx::CLOSE};
inline constexpr gfx::VectorIconRep kBotRep[] = {{kBotPath}};
inline constexpr gfx::VectorIcon kBotIcon(kBotRep,
                                          std::size(kBotRep),
                                          "myarc_bot");

inline constexpr gfx::PathElement kPlusPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 5, 12,
    gfx::H_LINE_TO, 19, gfx::MOVE_TO, 12, 5, gfx::V_LINE_TO, 19};
inline constexpr gfx::VectorIconRep kPlusRep[] = {{kPlusPath}};
inline constexpr gfx::VectorIcon kPlusIcon(kPlusRep,
                                           std::size(kPlusRep),
                                           "myarc_plus");

// The Create a Space panel's Lucide icons, at stroke 2: "square-dashed-plus"
// for the icon picker, "smile" and "info" for the Profile row, "chevron-down"
// for its Profile pill, and "brush" for Choose a Theme.
inline constexpr gfx::PathElement kSquareDashedPlusPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 5, 3, gfx::R_ARC_TO, 2, 2, 0, 0, 0, -2, 2,
    gfx::MOVE_TO, 19, 3, gfx::R_ARC_TO, 2, 2, 0, 0, 1, 2, 2,
    gfx::MOVE_TO, 21, 19, gfx::R_ARC_TO, 2, 2, 0, 0, 1, -2, 2,
    gfx::MOVE_TO, 5, 21, gfx::R_ARC_TO, 2, 2, 0, 0, 1, -2, -2,
    gfx::MOVE_TO, 9, 3, gfx::R_H_LINE_TO, 1,
    gfx::MOVE_TO, 9, 21, gfx::R_H_LINE_TO, 1,
    gfx::MOVE_TO, 14, 3, gfx::R_H_LINE_TO, 1,
    gfx::MOVE_TO, 14, 21, gfx::R_H_LINE_TO, 1,
    gfx::MOVE_TO, 3, 9, gfx::R_V_LINE_TO, 1,
    gfx::MOVE_TO, 21, 9, gfx::R_V_LINE_TO, 1,
    gfx::MOVE_TO, 3, 14, gfx::R_V_LINE_TO, 1,
    gfx::MOVE_TO, 21, 14, gfx::R_V_LINE_TO, 1,
    gfx::MOVE_TO, 8, 12, gfx::R_H_LINE_TO, 8,
    gfx::MOVE_TO, 12, 8, gfx::R_V_LINE_TO, 8};
inline constexpr gfx::VectorIconRep kSquareDashedPlusRep[] = {
    {kSquareDashedPlusPath}};
inline constexpr gfx::VectorIcon kSquareDashedPlusIcon(
    kSquareDashedPlusRep,
    std::size(kSquareDashedPlusRep),
    "myarc_square_dashed_plus");

inline constexpr gfx::PathElement kSmilePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::CIRCLE, 12, 12, 10,
    gfx::NEW_PATH, gfx::STROKE, 2,
    gfx::MOVE_TO, 15, 10, gfx::V_LINE_TO, 9,
    gfx::MOVE_TO, 16.472f, 15, gfx::R_ARC_TO, 6, 6, 0, 0, 1, -8.943f, 0,
    gfx::MOVE_TO, 9, 10, gfx::V_LINE_TO, 9};
inline constexpr gfx::VectorIconRep kSmileRep[] = {{kSmilePath}};
inline constexpr gfx::VectorIcon kSmileIcon(kSmileRep,
                                            std::size(kSmileRep),
                                            "myarc_smile");

inline constexpr gfx::PathElement kInfoPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::CIRCLE, 12, 12, 10,
    gfx::NEW_PATH, gfx::STROKE, 2,
    gfx::MOVE_TO, 12, 16, gfx::R_V_LINE_TO, -4,
    gfx::MOVE_TO, 12, 8, gfx::R_H_LINE_TO, 0.01f};
inline constexpr gfx::VectorIconRep kInfoRep[] = {{kInfoPath}};
inline constexpr gfx::VectorIcon kInfoIcon(kInfoRep,
                                           std::size(kInfoRep),
                                           "myarc_info");

inline constexpr gfx::PathElement kChevronDownPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 6, 9,
    gfx::R_LINE_TO, 6, 6, gfx::R_LINE_TO, 6, -6};
inline constexpr gfx::VectorIconRep kChevronDownRep[] = {{kChevronDownPath}};
inline constexpr gfx::VectorIcon kChevronDownIcon(kChevronDownRep,
                                                  std::size(kChevronDownRep),
                                                  "myarc_chevron_down");

// Sidebar folder rows: "folder" while closed, "folder-open" while open, and
// "chevron-right" beside "chevron-down" as the disclosure arrow.
inline constexpr gfx::PathElement kFolderClosedPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 20, 20, gfx::ARC_TO, 2, 2, 0, 0, 0, 22, 18,
    gfx::LINE_TO, 22, 8, gfx::ARC_TO, 2, 2, 0, 0, 0, 20, 6,
    gfx::LINE_TO, 12.1f, 6, gfx::ARC_TO, 2, 2, 0, 0, 1, 10.41f, 5.1f,
    gfx::LINE_TO, 9.6f, 3.9f, gfx::ARC_TO, 2, 2, 0, 0, 0, 7.93f, 3,
    gfx::LINE_TO, 4, 3, gfx::ARC_TO, 2, 2, 0, 0, 0, 2, 5,
    gfx::LINE_TO, 2, 18, gfx::ARC_TO, 2, 2, 0, 0, 0, 4, 20,
    gfx::CLOSE};
inline constexpr gfx::VectorIconRep kFolderClosedRep[] = {{kFolderClosedPath}};
inline constexpr gfx::VectorIcon kFolderClosedIcon(kFolderClosedRep,
                                                   std::size(kFolderClosedRep),
                                                   "myarc_folder");

inline constexpr gfx::PathElement kFolderOpenPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 6, 14, gfx::LINE_TO, 7.5f, 11.1f,
    gfx::ARC_TO, 2, 2, 0, 0, 1, 9.24f, 10, gfx::LINE_TO, 20, 10,
    gfx::ARC_TO, 2, 2, 0, 0, 1, 21.94f, 12.5f, gfx::LINE_TO, 20.4f, 18.5f,
    gfx::ARC_TO, 2, 2, 0, 0, 1, 18.45f, 20, gfx::LINE_TO, 4, 20,
    gfx::ARC_TO, 2, 2, 0, 0, 1, 2, 18, gfx::LINE_TO, 2, 5,
    gfx::ARC_TO, 2, 2, 0, 0, 1, 4, 3, gfx::LINE_TO, 7.9f, 3,
    gfx::ARC_TO, 2, 2, 0, 0, 1, 9.59f, 3.9f, gfx::LINE_TO, 10.4f, 5.1f,
    gfx::ARC_TO, 2, 2, 0, 0, 0, 12.07f, 6, gfx::LINE_TO, 18, 6,
    gfx::ARC_TO, 2, 2, 0, 0, 1, 20, 8, gfx::LINE_TO, 20, 10};
inline constexpr gfx::VectorIconRep kFolderOpenRep[] = {{kFolderOpenPath}};
inline constexpr gfx::VectorIcon kFolderOpenIcon(kFolderOpenRep,
                                                 std::size(kFolderOpenRep),
                                                 "myarc_folder_open");

inline constexpr gfx::PathElement kChevronRightPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 9, 18,
    gfx::LINE_TO, 15, 12, gfx::LINE_TO, 9, 6};
inline constexpr gfx::VectorIconRep kChevronRightRep[] = {{kChevronRightPath}};
inline constexpr gfx::VectorIcon kChevronRightIcon(kChevronRightRep,
                                                   std::size(kChevronRightRep),
                                                   "myarc_chevron_right");

inline constexpr gfx::PathElement kBrushPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 11, 10, gfx::R_LINE_TO, 3, 3,
    gfx::MOVE_TO, 6.5f, 21, gfx::ARC_TO, 3.5f, 3.5f, 0, 1, 0, 3, 17.5f,
    gfx::R_ARC_TO, 2.62f, 2.62f, 0, 0, 1, -0.708f, 1.792f,
    gfx::ARC_TO, 1, 1, 0, 0, 0, 3, 21, gfx::CLOSE,
    gfx::MOVE_TO, 9.969f, 17.031f, gfx::LINE_TO, 21.378f, 5.624f,
    gfx::R_ARC_TO, 1, 1, 0, 0, 0, -3.002f, -3.002f,
    gfx::LINE_TO, 6.967f, 14.031f};
inline constexpr gfx::VectorIconRep kBrushRep[] = {{kBrushPath}};
inline constexpr gfx::VectorIcon kBrushIcon(kBrushRep,
                                            std::size(kBrushRep),
                                            "myarc_brush");

// Painter's palette (Lucide `palette`), used for Boost's Color palette mode.
inline constexpr gfx::PathElement kPalettePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 12, 22,
    gfx::ARC_TO, 1, 1, 0, 0, 1, 12, 2,
    gfx::ARC_TO, 10, 9, 0, 0, 1, 22, 11,
    gfx::ARC_TO, 5, 5, 0, 0, 1, 17, 16,
    gfx::H_LINE_TO, 14.75f,
    gfx::ARC_TO, 1.75f, 1.75f, 0, 0, 0, 13.35f, 18.8f,
    gfx::R_LINE_TO, 0.3f, 0.4f,
    gfx::ARC_TO, 1.75f, 1.75f, 0, 0, 1, 12.25f, 22,
    gfx::CLOSE,
    gfx::NEW_PATH,
    gfx::CIRCLE, 13.5f, 6.5f, 1.0f,
    gfx::CIRCLE, 17.5f, 10.5f, 1.0f,
    gfx::CIRCLE, 6.5f, 12.5f, 1.0f,
    gfx::CIRCLE, 8.5f, 7.5f, 1.0f};
inline constexpr gfx::VectorIconRep kPaletteRep[] = {{kPalettePath}};
inline constexpr gfx::VectorIcon kPaletteIcon(kPaletteRep,
                                              std::size(kPaletteRep),
                                              "myarc_palette");

// Filter bars (Lucide `list-filter`), used for the Archived Tabs Filter button.
inline constexpr gfx::PathElement kFilterPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 3, 6, gfx::H_LINE_TO, 21,
    gfx::MOVE_TO, 7, 12, gfx::H_LINE_TO, 17,
    gfx::MOVE_TO, 10, 18, gfx::H_LINE_TO, 14};
inline constexpr gfx::VectorIconRep kFilterRep[] = {{kFilterPath}};
inline constexpr gfx::VectorIcon kFilterIcon(kFilterRep,
                                             std::size(kFilterRep),
                                             "myarc_filter");

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
// periwinkle (#6E7BFF, softer than Arc's #2523F6) under strokes in light
// lavender (#EDEDFD). The colors are baked in, so ImageModel's color
// argument doesn't change them.
#define MYARC_RAIL_FILL gfx::PATH_COLOR_ARGB, 0xFF, 0x6E, 0x7B, 0xFF
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
// Lucide icons for Peek, at stroke 2: "x", "maximize-2" mirrored to point
// top left and bottom right, and "columns-2" on its round buttons, as Arc's
// X, expand arrows and Split View; "lock" and "triangle-alert" also mark
// Little Arc's address.
inline constexpr gfx::PathElement kPeekClosePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 18, 6,
    gfx::LINE_TO, 6, 18, gfx::MOVE_TO, 6, 6, gfx::R_LINE_TO, 12, 12};
inline constexpr gfx::VectorIconRep kPeekCloseRep[] = {{kPeekClosePath}};
inline constexpr gfx::VectorIcon kPeekCloseIcon(kPeekCloseRep,
                                                std::size(kPeekCloseRep),
                                                "myarc_peek_close");

inline constexpr gfx::PathElement kPeekExpandPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 9, 3, gfx::R_H_LINE_TO, -6, gfx::R_V_LINE_TO, 6,
    gfx::MOVE_TO, 3, 3, gfx::R_LINE_TO, 7, 7,
    gfx::MOVE_TO, 21, 21, gfx::R_LINE_TO, -7, -7,
    gfx::MOVE_TO, 15, 21, gfx::H_LINE_TO, 21, gfx::R_V_LINE_TO, -6};
inline constexpr gfx::VectorIconRep kPeekExpandRep[] = {{kPeekExpandPath}};
inline constexpr gfx::VectorIcon kPeekExpandIcon(kPeekExpandRep,
                                                 std::size(kPeekExpandRep),
                                                 "myarc_peek_expand");

inline constexpr gfx::PathElement kPeekLockPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::ROUND_RECT, 3, 11, 18, 11, 2, gfx::MOVE_TO, 7, 11, gfx::V_LINE_TO, 7,
    gfx::R_ARC_TO, 5, 5, 0, 0, 1, 10, 0, gfx::R_V_LINE_TO, 4};
inline constexpr gfx::VectorIconRep kPeekLockRep[] = {{kPeekLockPath}};
inline constexpr gfx::VectorIcon kPeekLockIcon(kPeekLockRep,
                                               std::size(kPeekLockRep),
                                               "myarc_peek_lock");

inline constexpr gfx::PathElement kPeekNotSecurePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 21.73f, 18,
    gfx::R_LINE_TO, -8, -14, gfx::R_ARC_TO, 2, 2, 0, 0, 0, -3.48f, 0,
    gfx::R_LINE_TO, -8, 14, gfx::ARC_TO, 2, 2, 0, 0, 0, 4, 21,
    gfx::R_H_LINE_TO, 16, gfx::R_ARC_TO, 2, 2, 0, 0, 0, 1.73f, -3,
    gfx::MOVE_TO, 12, 9, gfx::R_V_LINE_TO, 4, gfx::MOVE_TO, 12, 17,
    gfx::R_H_LINE_TO, 0.01f};
inline constexpr gfx::VectorIconRep kPeekNotSecureRep[] = {
    {kPeekNotSecurePath}};
inline constexpr gfx::VectorIcon kPeekNotSecureIcon(
    kPeekNotSecureRep,
    std::size(kPeekNotSecureRep),
    "myarc_peek_not_secure");

// Lucide "columns-2" for Peek's Open in Split View button.
inline constexpr gfx::PathElement kPeekSplitPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::ROUND_RECT, 3, 3, 18, 18,
    2, gfx::MOVE_TO, 12, 3, gfx::V_LINE_TO, 21};
inline constexpr gfx::VectorIconRep kPeekSplitRep[] = {{kPeekSplitPath}};
inline constexpr gfx::VectorIcon kPeekSplitIcon(kPeekSplitRep,
                                                std::size(kPeekSplitRep),
                                                "myarc_peek_split");

// Lucide "code-xml" for the address field's local page chip.
inline constexpr gfx::PathElement kCodeXmlPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 18, 16, gfx::R_LINE_TO, 4, -4, gfx::R_LINE_TO, -4, -4,
    gfx::MOVE_TO, 6, 8, gfx::R_LINE_TO, -4, 4, gfx::R_LINE_TO, 4, 4,
    gfx::MOVE_TO, 14.5f, 4, gfx::R_LINE_TO, -5, 16};
inline constexpr gfx::VectorIconRep kCodeXmlRep[] = {{kCodeXmlPath}};
inline constexpr gfx::VectorIcon kCodeXmlIcon(kCodeXmlRep,
                                              std::size(kCodeXmlRep),
                                              "myarc_code_xml");

// Lucide "sliders-horizontal" for Boost Editor Contrast/Brightness/Saturation.
inline constexpr gfx::PathElement kSlidersHorizontalPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 21, 4, gfx::H_LINE_TO, 14,
    gfx::MOVE_TO, 10, 4, gfx::H_LINE_TO, 3,
    gfx::MOVE_TO, 21, 12, gfx::H_LINE_TO, 12,
    gfx::MOVE_TO, 8, 12, gfx::H_LINE_TO, 3,
    gfx::MOVE_TO, 21, 20, gfx::H_LINE_TO, 16,
    gfx::MOVE_TO, 12, 20, gfx::H_LINE_TO, 3,
    gfx::MOVE_TO, 14, 2, gfx::V_LINE_TO, 6,
    gfx::MOVE_TO, 8, 10, gfx::V_LINE_TO, 14,
    gfx::MOVE_TO, 16, 18, gfx::V_LINE_TO, 22};
inline constexpr gfx::VectorIconRep kSlidersHorizontalRep[] = {
    {kSlidersHorizontalPath}};
inline constexpr gfx::VectorIcon kSlidersHorizontalIcon(
    kSlidersHorizontalRep,
    std::size(kSlidersHorizontalRep),
    "myarc_sliders_horizontal");

// Lucide "square-mouse-pointer" (Chrome DevTools inspect-element icon) for the
// Boost Code editor element picker.
inline constexpr gfx::PathElement kInspectElementPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 21, 11, gfx::V_LINE_TO, 5,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, -2, -2,
    gfx::H_LINE_TO, 5,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, -2, 2,
    gfx::V_LINE_TO, 19,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, 2,
    gfx::H_LINE_TO, 6,
    gfx::MOVE_TO, 12, 12, gfx::LINE_TO, 15, 21,
    gfx::LINE_TO, 17, 17, gfx::LINE_TO, 21, 15, gfx::CLOSE};
inline constexpr gfx::VectorIconRep kInspectElementRep[] = {
    {kInspectElementPath}};
inline constexpr gfx::VectorIcon kInspectElementIcon(
    kInspectElementRep,
    std::size(kInspectElementRep),
    "myarc_inspect_element");

#undef MYARC_RAIL_FILL

}  // namespace myarc

#endif  // MYARC_ICONS_H_
