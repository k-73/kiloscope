#pragma once
// WGS84 ellipsoid reference and geodetic conversions.
// All internal positions are ENU (X=East, Y=North, Z=Up) relative to a reference point.

#include <glm/glm.hpp>
#include <cmath>

namespace Kilo::Render {

struct GeoCoord { double lat, lon, alt; };

struct GeoRef {
    // WGS84 constants
    static constexpr double a  = 6378137.0;
    static constexpr double b  = 6356752.314245;
    static constexpr double e2 = 1.0 - (b * b) / (a * a);

    // Reference point (degrees, meters)
    double lat0 = 0.0, lon0 = 0.0, alt0 = 0.0;

    // Precomputed transforms (set by Set())
    glm::dvec3 ecefRef{0.0};
    glm::dmat3 ecefToEnu{1.0};
    // Stable geodetic params (only update at full rotation update, not per-frame)
    double cosLat = 1.0, sinLat = 0.0, Nrad = a, Mrad = a;
    bool   valid = false;

    void Set(double lat, double lon, double alt = 0.0) {
        // Rotation matrix only needs updating every ~1km (intersection shape accuracy)
        // Lat/lon grid uses flat-plane ENU → independent of this matrix
        if (valid) {
            // lat0/lon0 track current position (for uCamLLA in shader).
            // ecefRef/ecefToEnu only update every ~1.1km (for stable uEllCenter).
            double dlat = lat - lat0, dlon = lon - lon0;
            lat0 = lat; lon0 = lon; alt0 = alt;
            if (dlat * dlat + dlon * dlon < 1e-4)  // ~0.01° ≈ 1.1km
                return;  // keep ecefRef and ecefToEnu stable
        }
        lat0 = lat; lon0 = lon; alt0 = alt;
        double phi = glm::radians(lat), lam = glm::radians(lon);
        double sp = std::sin(phi), cp = std::cos(phi);
        double sl = std::sin(lam), cl = std::cos(lam);
        ecefRef = ToEcef(lat, lon, alt);
        ecefToEnu = glm::dmat3(
            glm::dvec3(-sl,      -sp * cl,   cp * cl),
            glm::dvec3( cl,      -sp * sl,   cp * sl),
            glm::dvec3( 0.0,      cp,         sp));
        // Stable geodetic params for shader (constant between full updates → no quantization)
        cosLat = cp; sinLat = sp;
        double w = std::sqrt(1.0 - e2 * sp * sp);
        Nrad = a / w;
        Mrad = a * (1.0 - e2) / (w * w * w);
        valid = true;
    }

    // Geodetic (deg, deg, m) → ECEF (m)
    static glm::dvec3 ToEcef(double lat, double lon, double alt) {
        double phi = glm::radians(lat), lam = glm::radians(lon);
        double sp = std::sin(phi), cp = std::cos(phi);
        double n = a / std::sqrt(1.0 - e2 * sp * sp);
        return {(n + alt) * cp * std::cos(lam),
                (n + alt) * cp * std::sin(lam),
                (n * (1.0 - e2) + alt) * sp};
    }

    // Geodetic → internal ENU (relative to reference)
    glm::dvec3 ToInternal(double lat, double lon, double alt) const {
        return ecefToEnu * (ToEcef(lat, lon, alt) - ecefRef);
    }

    // Internal ENU → geodetic (iterative, ~2 iterations)
    GeoCoord FromInternal(const glm::dvec3& enu) const {
        glm::dvec3 ecef = glm::transpose(ecefToEnu) * enu + ecefRef;
        double p = std::sqrt(ecef.x * ecef.x + ecef.y * ecef.y);
        double lon = glm::degrees(std::atan2(ecef.y, ecef.x));
        double lat = std::atan2(ecef.z, p * (1.0 - e2));
        for (int i = 0; i < 3; ++i) {
            double sp = std::sin(lat);
            double n = a / std::sqrt(1.0 - e2 * sp * sp);
            lat = std::atan2(ecef.z + e2 * n * sp, p);
        }
        double sp = std::sin(lat);
        double n = a / std::sqrt(1.0 - e2 * sp * sp);
        double alt = p / std::cos(lat) - n;
        return {glm::degrees(lat), lon, alt};
    }
};

} // namespace Kilo::Render
