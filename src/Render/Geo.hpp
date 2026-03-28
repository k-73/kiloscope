#pragma once
// WGS84 ellipsoid reference and geodetic conversions.
// All internal positions are ENU (X=East, Y=North, Z=Up) relative to a reference point.
//
// Uses GeographicLib for ECEF↔geodetic (7nm precision, correct at poles).
// The ENU rotation matrix and curvature radii are maintained explicitly
// for the Globe shader's double-precision intersection and Cesium delta.

#include <glm/glm.hpp>
#include <GeographicLib/Geocentric.hpp>
#include <cmath>

namespace Kilo::Render {

struct GeoCoord { double lat, lon, alt; };

struct GeoRef {
    // WGS84 constants
    static constexpr double a  = 6378137.0;
    static constexpr double b  = 6356752.314245;
    static constexpr double e2 = 1.0 - (b * b) / (a * a);

    // How far the origin can drift from the last full rotation update
    // before ecefToEnu is recomputed.  0.01° ≈ 1.1km at the equator.
    // At this distance the ENU frame rotation error is < 0.01° — negligible
    // for ellipsoid intersection shape but important to keep stable for the
    // shader (avoids per-frame quantization of cosLat/sinLat/Nrad/Mrad).
    static constexpr double kUpdateThresholdDeg = 0.01;

    // Current origin position (degrees, meters) — updated every frame
    double lat0 = 0.0, lon0 = 0.0, alt0 = 0.0;

    // Precomputed transforms (only update when origin drifts > kUpdateThresholdDeg)
    glm::dvec3 ecefRef{0.0};
    glm::dmat3 ecefToEnu{1.0};

    // Geodetic curvature params — updated every frame for smooth Cesium delta transitions.
    double cosLat = 1.0, sinLat = 0.0, Nrad = a, Mrad = a;

    bool valid = false;

    void Set(double lat, double lon, double alt = 0.0) {
        lon = std::remainder(lon, 360.0);  // normalize to [-180, 180]
        lat0 = lat; lon0 = lon; alt0 = alt;

        // Curvature params update every frame — smooth transitions.
        // Float32 ULP step of cosLat ≈ 6e-8 → sub-mm grid shift — invisible.
        double phi = glm::radians(lat);
        double sp = std::sin(phi), cp = std::cos(phi);
        cosLat = cp; sinLat = sp;
        double w = std::sqrt(1.0 - e2 * sp * sp);
        Nrad = a / w;
        Mrad = a * (1.0 - e2) / (w * w * w);

        // ecefRef + ecefToEnu only update at threshold — stable for intersection.
        if (valid) {
            double dlat = lat - refLat_;
            double dlon = std::remainder(lon - refLon_, 360.0);
            double cosRef = std::cos(glm::radians(refLat_));
            double d2 = dlat * dlat + (dlon * cosRef) * (dlon * cosRef);
            if (d2 < kUpdateThresholdDeg * kUpdateThresholdDeg)
                return;
        }

        refLat_ = lat; refLon_ = lon;
        double lam = glm::radians(lon);
        double sl = std::sin(lam), cl = std::cos(lam);

        ecefRef = ToEcef(lat, lon, alt);
        ecefToEnu = glm::dmat3(
            glm::dvec3(-sl,       -sp * cl,   cp * cl),
            glm::dvec3( cl,       -sp * sl,   cp * sl),
            glm::dvec3( 0.0,       cp,         sp));
        valid = true;
    }

    // Geodetic (deg, deg, m) → ECEF (m)  —  GeographicLib, 7nm precision
    static glm::dvec3 ToEcef(double lat, double lon, double alt) {
        static const auto& earth = GeographicLib::Geocentric::WGS84();
        double X, Y, Z;
        earth.Forward(lat, lon, alt, X, Y, Z);
        return {X, Y, Z};
    }

    // Geodetic → internal ENU (relative to reference)
    glm::dvec3 ToInternal(double lat, double lon, double alt) const {
        return ecefToEnu * (ToEcef(lat, lon, alt) - ecefRef);
    }

    // Internal ENU → geodetic  —  GeographicLib, 7nm precision, correct at poles
    GeoCoord FromInternal(const glm::dvec3& enu) const {
        glm::dvec3 ecef = glm::transpose(ecefToEnu) * enu + ecefRef;
        static const auto& earth = GeographicLib::Geocentric::WGS84();
        double lat, lon, alt;
        earth.Reverse(ecef.x, ecef.y, ecef.z, lat, lon, alt);
        return {lat, lon, alt};
    }

    // Frozen reference — last full-update position (matches ecefRef/ecefToEnu frame).
    // Shader uOriginLL* must use these, not lat0/lon0, because dLon/dLat are deltas from this frame.
    double refLat() const { return refLat_; }
    double refLon() const { return refLon_; }

private:
    double refLat_ = 0.0, refLon_ = 0.0;
};

} // namespace Kilo::Render
