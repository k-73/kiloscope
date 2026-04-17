#pragma once
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Kilo {

using json = nlohmann::json;

class Terrain;

struct Waypoint {
    double      lat = 0.0, lon = 0.0, alt = 0.0;
    std::string label;
    glm::vec4   color = {1.f, .6f, .2f, 1.f};
};

// Waypoints — geodetic marker collection. No deps: terrain is passed where used.
// Interaction result is returned from Draw; the caller decides what to do with it.
class Waypoints {
public:
    struct DrawResult {
        int  rightClickedIdx = -1;  // one-shot right-click on a marker
        int  draggedIdx      = -1;  // marker being dragged this frame
        bool targetMatched   = false;  // some wp matches currentTarget (lat/lon)
        bool targetDragged   = false;  // the matched wp is the one being dragged
    };

    std::vector<Waypoint> list;

    void Add(double lat, double lon, double alt);
    void SnapToTerrain(const Terrain& terrain);

    // Draws markers; highlights wp matching currentTarget. Handles drag/right-click.
    // `currentTarget` = (lat, lon, alt) of whatever the caller considers the active target.
    DrawResult Draw(const glm::vec3& aircraftPos,
                    const glm::dvec3& currentTarget,
                    const Terrain& terrain);

    void DrawControls();

    json Save() const;
    void Load(const json& j);
};

} // namespace Kilo
