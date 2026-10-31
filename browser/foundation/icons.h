#ifndef MYARC_ICONS_H_
#define MYARC_ICONS_H_

#include <iterator>

#include "ui/gfx/vector_icon_types.h"

namespace myarc {

// Filled single-path icons from design/icons/, 24 DIP canvas, even-odd fill:
// library.svg for the footer Library button, saturn.svg as the default Space
// icon.
inline constexpr gfx::PathElement kLibraryPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::MOVE_TO, 15.6f, 3.3f,
    gfx::CUBIC_TO, 16.208f, 3.3f, 16.7f, 3.792f, 16.7f, 4.4f,
    gfx::CUBIC_TO, 16.7f, 5.008f, 16.208f, 5.5f, 15.6f, 5.5f,
    gfx::CUBIC_TO, 14.992f, 5.5f, 14.5f, 5.008f, 14.5f, 4.4f,
    gfx::CUBIC_TO, 14.5f, 3.792f, 14.992f, 3.3f, 15.6f, 3.3f, gfx::CLOSE,
    gfx::MOVE_TO, 7.49f, 6.45f, gfx::LINE_TO, 8.13f, 4.23f,
    gfx::LINE_TO, 9.35f, 6.19f, gfx::LINE_TO, 11.66f, 6.11f,
    gfx::LINE_TO, 10.18f, 7.88f, gfx::LINE_TO, 10.96f, 10.05f,
    gfx::LINE_TO, 8.82f, 9.18f, gfx::LINE_TO, 7, 10.6f,
    gfx::LINE_TO, 7.16f, 8.3f, gfx::LINE_TO, 5.25f, 7.01f,
    gfx::LINE_TO, 7.49f, 6.45f, gfx::CLOSE, gfx::MOVE_TO, 15.3f, 10.6f,
    gfx::LINE_TO, 15.3f, 7.2f, gfx::LINE_TO, 18.8f, 8.9f,
    gfx::LINE_TO, 15.3f, 10.6f, gfx::CLOSE, gfx::MOVE_TO, 5, 13.5f,
    gfx::CUBIC_TO, 5, 12.395f, 5.895f, 11.5f, 7, 11.5f,
    gfx::LINE_TO, 17, 11.5f,
    gfx::CUBIC_TO, 18.105f, 11.5f, 19, 12.395f, 19, 13.5f,
    gfx::LINE_TO, 19, 19,
    gfx::CUBIC_TO, 19, 20.381f, 17.881f, 21.5f, 16.5f, 21.5f,
    gfx::LINE_TO, 7.5f, 21.5f, gfx::CUBIC_TO, 6.119f, 21.5f, 5, 20.381f, 5, 19,
    gfx::LINE_TO, 5, 13.5f, gfx::CLOSE, gfx::MOVE_TO, 10.6f, 13.9f,
    gfx::CUBIC_TO, 10.158f, 13.9f, 9.8f, 14.258f, 9.8f, 14.7f,
    gfx::CUBIC_TO, 9.8f, 15.142f, 10.158f, 15.5f, 10.6f, 15.5f,
    gfx::LINE_TO, 13.4f, 15.5f,
    gfx::CUBIC_TO, 13.842f, 15.5f, 14.2f, 15.142f, 14.2f, 14.7f,
    gfx::CUBIC_TO, 14.2f, 14.258f, 13.842f, 13.9f, 13.4f, 13.9f,
    gfx::LINE_TO, 10.6f, 13.9f, gfx::CLOSE};
inline constexpr gfx::VectorIconRep kLibraryRep[] = {{kLibraryPath}};
inline constexpr gfx::VectorIcon kLibraryIcon(kLibraryRep,
                                              std::size(kLibraryRep),
                                              "myarc_library");

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
// search fields, and the Library rail's "image", "circle-arrow-down",
// "presentation", "copy", "wand" and "arrow-left".
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

inline constexpr gfx::PathElement kPresentationPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 2, 3,
    gfx::R_H_LINE_TO, 20, gfx::MOVE_TO, 21, 3, gfx::R_V_LINE_TO, 11,
    gfx::R_ARC_TO, 2, 2, 0, 0, 1, -2, 2, gfx::H_LINE_TO, 5,
    gfx::R_ARC_TO, 2, 2, 0, 0, 1, -2, -2, gfx::V_LINE_TO, 3,
    gfx::MOVE_TO, 7, 21, gfx::R_LINE_TO, 5, -5, gfx::R_LINE_TO, 5, 5};
inline constexpr gfx::VectorIconRep kPresentationRep[] = {{kPresentationPath}};
inline constexpr gfx::VectorIcon kPresentationIcon(kPresentationRep,
                                                   std::size(kPresentationRep),
                                                   "myarc_presentation");

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

inline constexpr gfx::PathElement kArrowLeftPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 12, 19,
    gfx::R_LINE_TO, -7, -7, gfx::R_LINE_TO, 7, -7, gfx::MOVE_TO, 19, 12,
    gfx::H_LINE_TO, 5};
inline constexpr gfx::VectorIconRep kArrowLeftRep[] = {{kArrowLeftPath}};
inline constexpr gfx::VectorIcon kArrowLeftIcon(kArrowLeftRep,
                                                std::size(kArrowLeftRep),
                                                "myarc_arrow_left");

}  // namespace myarc

#endif  // MYARC_ICONS_H_
