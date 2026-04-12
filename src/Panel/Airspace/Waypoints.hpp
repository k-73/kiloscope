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
    glm::vec4   color = {1.f, .6f, .2f, 1.f};
};

// Waypoints — geodetic markers with drag/right-click interaction.
// Holds references to gimbal (for target tracking) and terrain (for surface snap).
class Waypoints {
public:
    Waypoints(Gimbal& gimbal, const Terrain& terrain)
        : gimbal_(gimbal), terrain_(terrain) {}

    std::vector<Waypoint> list;
    bool rightOnMarker = false;  // set by Draw when right-click starts on a marker

    void Add(double lat, double lon, double alt);
    void SnapToTerrain();

    // Draw markers; returns true if any waypoint matches the gimbal target.
    bool Draw(const glm::vec3& aircraftPos);

    void DrawControls();

    json Save() const;
    void Load(const json& j);

private:
    Gimbal&        gimbal_;
    const Terrain& terrain_;
};

} // namespace Kilo
