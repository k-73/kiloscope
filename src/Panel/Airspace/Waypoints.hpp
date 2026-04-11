#pragma once
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Kilo {

using json = nlohmann::json;

class Gimbal;
class Terrain;

struct Waypoint {
    double      lat = 0.0, lon = 0.0, alt = 0.0;
    std::string label;
    glm::vec4   color = {1.f, .6f, .2f, 1.f};  // orange default
};

// Waypoints — geodetic markers with drag/right-click interaction.
// Right-click sets gimbal target; drag moves marker on terrain surface.
class Waypoints {
public:
    std::vector<Waypoint> list;

    void Add(double lat, double lon, double alt);
    void SnapToTerrain(const Terrain& terrain);

    // Draw markers in current scene; returns true if any waypoint matches the gimbal target.
    // Sets `rightOnMarker` if right-click started on a marker (caller suppresses terrain handler).
    bool Draw(const glm::vec3& aircraftPos, Gimbal& gimbal, const Terrain& terrain,
              bool& rightOnMarker);

    void DrawControls();

    json Save() const;
    void Load(const json& j);  // loads label/lat/lon/color/alt; alt may be re-snapped later
};

} // namespace Kilo
