#pragma once
// Globe extension — WGS84 ellipsoid rendering + geodetic conversions.
// Internal header for Draw.cpp End() integration. User API is in Draw.hpp.

#include <string>
#include <glm/glm.hpp>

namespace Kilo::Render {
struct GlobeConfig;
void InitGlobe(const std::string& shaderDir);
void ShutdownGlobe();
void DrawGlobe(const GlobeConfig& cfg);
void RenderTerrain();
} // namespace Kilo::Render
