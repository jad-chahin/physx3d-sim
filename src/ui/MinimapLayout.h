#ifndef PHYSICS3D_UI_MINIMAPLAYOUT_H
#define PHYSICS3D_UI_MINIMAPLAYOUT_H

#include <algorithm>

namespace ui::minimap {

struct MarkerLayout {
    float markerX = 0.0f;
    float markerY = 0.0f;
    bool showHeightCue = false;
    bool visualEdgeClamped = false;
};

[[nodiscard]] inline float clampMarkerYForCue(
    const float markerY,
    const float mapInnerY0,
    const float mapInnerY1,
    const float markerRadius,
    const float cueGap,
    const float cueHeight,
    const bool aboveCamera)
{
    const float minMarkerY = aboveCamera
        ? mapInnerY0 + markerRadius + cueGap + cueHeight
        : mapInnerY0 + markerRadius;
    const float maxMarkerY = aboveCamera
        ? mapInnerY1 - markerRadius
        : mapInnerY1 - markerRadius - cueGap - cueHeight;
    if (minMarkerY > maxMarkerY) {
        return std::clamp(markerY, mapInnerY0, mapInnerY1);
    }
    return std::clamp(markerY, minMarkerY, maxMarkerY);
}

[[nodiscard]] inline MarkerLayout computeMarkerLayout(
    const float projectedX,
    const float projectedY,
    const float mapInnerX0,
    const float mapInnerY0,
    const float mapInnerX1,
    const float mapInnerY1,
    const float markerClampPad,
    const float heightNorm,
    const bool edgeClamped)
{
    MarkerLayout layout{};
    const float planarMarkerX = std::clamp(projectedX, mapInnerX0 + markerClampPad, mapInnerX1 - markerClampPad);
    const float planarMarkerY = std::clamp(projectedY, mapInnerY0 + markerClampPad, mapInnerY1 - markerClampPad);
    layout.markerX = planarMarkerX;
    layout.markerY = planarMarkerY;
    layout.showHeightCue = heightNorm > 0.08f;
    layout.visualEdgeClamped = edgeClamped || planarMarkerX != projectedX || planarMarkerY != projectedY;
    return layout;
}

} // namespace ui::minimap

#endif // PHYSICS3D_UI_MINIMAPLAYOUT_H
