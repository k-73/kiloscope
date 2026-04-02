#include "Airspace.hpp"
#include "Core/Panel/PanelRegistry.hpp"
#include "Render/Draw.hpp"
#include "Render/Model.hpp"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/Geo.hpp"
#include <GeographicLib/Geocentric.hpp>
#include "Ui/Widget.hpp"
#include "Ui/IconsFontAwesome7.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Kilo {

static Render::ModelId sJetModel = Render::kInvalidModel;

Airspace::Airspace() : Panel("Airspace", "Airspace") {
    Render::GetCamera("flight").ResetFollow(12.f);
    double wp1Lat = 52.2297 + 1.0 / 111.32, wp1Lon = 21.0122;
    waypoints_.push_back({wp1Lat, wp1Lon, double(terrain_.Sample(wp1Lat, wp1Lon)), "WP1"});
    if (sJetModel == Render::kInvalidModel)
        sJetModel = Render::LoadModel(std::string(ASSETS_DIR) + "/models/Jet_Lowpoly.obj");
    terrain_ = Render::LoadTerrainDir(std::string(ASSETS_DIR) + "/terrain");
    // Start aircraft above terrain surface
    aircraft_.alt = double(terrain_.Sample(aircraft_.lat, aircraft_.lon)) + 50.0;
    gimbal_.targetAlt = double(terrain_.Sample(gimbal_.targetLat, gimbal_.targetLon));
    RebuildTerrainIfNeeded();
}

void Airspace::RebuildTerrainIfNeeded(bool force) {
    constexpr double kDegPerKm = 1.0 / 111.32;
    double cosLat = std::cos(glm::radians(aircraft_.lat));
    double threshDeg = terrainCfg_.rebuildKm * kDegPerKm;
    double dLat = aircraft_.lat - terrainCenterLat_;
    double dLon = (aircraft_.lon - terrainCenterLon_) * cosLat;  // scale lon by cos(lat)
    if (force || terrainMesh_.indices.empty() || dLat * dLat + dLon * dLon > threshDeg * threshDeg) {
        terrainCenterLat_ = aircraft_.lat;
        terrainCenterLon_ = aircraft_.lon;
        float latRadDeg = float(terrainCfg_.radiusKm * kDegPerKm);
        float lonRadDeg = float(latRadDeg / std::max(cosLat, 0.01));  // wider in lon at high lat
        float stepDeg   = float(terrainCfg_.resolutionM / 111320.0);
        terrainMesh_ = Render::BuildTerrainMesh(terrain_, aircraft_.lat, aircraft_.lon,
                                                latRadDeg, lonRadDeg, stepDeg);
    }
}

static void DrawAircraft(float speed) {
    Render::Group group;

    // OBJ → body frame: scale, offset, coordinate transform
    Render::PushMatrix();
        Render::Scale(0.33f);
        Render::Translate(0, 0, -1.0f);
        // OBJ (X=right, Y=up, +Z=forward) → Body (X=fwd, Y=right, Z=down)
        Render::Transform(glm::mat4(
            glm::vec4(0, -1, 0, 0),
            glm::vec4(0, 0, -1, 0),
            glm::vec4(1, 0, 0, 0),
            glm::vec4(0, 0, 0, 1)));
        Render::Model(sJetModel, Render::Color::Hex("#3a5570"));
    Render::PopMatrix();

    // Engine afterburner — gradient beam, intensity ∝ speed
    if (speed > 0.f) {
        constexpr glm::vec3 kEngineL{-2.2f, -0.21f, 0.f};  // body frame (X=fwd, Y=right, Z=down)
        constexpr glm::vec3 kEngineR{-2.2f,  0.21f, 0.f};
        constexpr float kFlameR = 0.12f;

        float t   = std::min(speed / 120.f, 1.f);   // normalized throttle 0→1
        float len = 0.3f + t * 1.2f;                 // flame length grows with speed
        for (auto& eng : {kEngineL, kEngineR})
            Render::Beam(eng, eng + glm::vec3(-len, 0, 0),
                {1.f, .9f, .7f, .8f * t}, {.9f, .2f, .05f, .1f * t}, kFlameR, 2.5f, 4);
    }
}

// ── controls ────────────────────────────────────────────────────

void Airspace::DrawControls() {
    ImGui::Begin("Airspace");

    // Aircraft state
    ImGui::Text("Lat %.6f  Lon %.6f  Alt %.0f m", aircraft_.lat, aircraft_.lon, aircraft_.alt);
    ImGui::DragFloat("Speed", &aircraft_.speed, 0.5f, 0.f, 200.f, "%.1f m/s");
    ImGui::SliderFloat("Yaw",   &aircraft_.yaw,  -180.f, 180.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Pitch", &aircraft_.pitch, -45.f,  45.f, "%.1f\xc2\xb0");
    ImGui::Separator();

    // Gimbal target — heading-relative joystick + manual lat/lon/alt
    ImGui::SliderFloat("FOV", &gimbal_.fov, 5.f, 120.f, "%.1f\xc2\xb0");
    ImGui::SliderFloat("Aspect", &gimbal_.aspect, 0.5f, 3.f, "%.2f");
    if (ImGui::InputDouble("Target Lat", &gimbal_.targetLat, 0.01, 0.1, "%.6f") |
        ImGui::InputDouble("Target Lon", &gimbal_.targetLon, 0.01, 0.1, "%.6f"))
        gimbal_.targetAlt = double(terrain_.Sample(gimbal_.targetLat, gimbal_.targetLon));
    ImGui::BeginDisabled();
    ImGui::InputDouble("Target Alt", &gimbal_.targetAlt, 0, 0, "%.0f m");
    ImGui::EndDisabled();

    // Joystick: quadratic response, heading-relative, cos(lat)-corrected longitude
    glm::vec2 joy;
    if (Widget::Joystick("##gimbal", &joy)) {
        float dt   = ImGui::GetIO().DeltaTime;
        float rate = 0.0002f;
        float fx   = joy.x * std::abs(joy.x) * rate * dt;
        float fy   = -joy.y * std::abs(joy.y) * rate * dt;
        float yr   = glm::radians(aircraft_.yaw);
        float cosLat = std::cos(glm::radians(float(aircraft_.lat)));
        gimbal_.targetLat += fy * std::cos(yr) - fx * std::sin(yr);
        gimbal_.targetLon += (fy * std::sin(yr) + fx * std::cos(yr)) / std::max(cosLat, 0.01f);
        gimbal_.targetAlt = double(terrain_.Sample(gimbal_.targetLat, gimbal_.targetLon));
    }
    ImGui::Separator();

    // Waypoints
    if (ImGui::CollapsingHeader("Waypoints")) {
        int removeIdx = -1;
        for (size_t i = 0; i < waypoints_.size(); ++i) {
            ImGui::PushID(int(i));
            auto& wp = waypoints_[i];
            ImGui::Text("%s", wp.label.c_str());
            ImGui::SameLine();
            ImGui::Text("%.6f, %.6f", wp.lat, wp.lon);
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) removeIdx = int(i);
            ImGui::PopID();
        }
        if (removeIdx >= 0)
            waypoints_.erase(waypoints_.begin() + removeIdx);
        ImGui::TextDisabled("Double-click globe to add waypoint");
    }
    ImGui::Separator();

    // Camera info
    auto& cam = Render::GetCamera("flight");
    ImGui::Text("Camera: %s  [C]", cameraFree_ ? "FreeCam" : "Chase");
    ImGui::Text("Eye: %.1f, %.1f, %.1f  Dist: %.1f",
        cam.Position().x, cam.Position().y, cam.Position().z, cam.Distance());

    // Globe appearance
    if (ImGui::CollapsingHeader("Globe")) {
        auto& g = Render::GetGlobe("flight");

        ImGui::Checkbox("Lighting", &g.lighting);
        ImGui::SliderFloat("Ambient", &g.ambient, 0.f, 1.f);
        ImGui::ColorEdit3("Surface", &g.surfaceColor.x);
        ImGui::ColorEdit3("Grid",    &g.gratColor.x);
        ImGui::Separator();

        ImGui::Text("Atmosphere");
        ImGui::ColorEdit3("Atmo Color", &g.atmosphereColor.x);
        ImGui::SliderFloat("Atmo Power", &g.atmospherePow, 1.f, 10.f);
        ImGui::SliderFloat("Atmo Str",   &g.atmosphereStr, 0.f, 2.f);
        ImGui::Separator();

        ImGui::Text("Fog");
        ImGui::Checkbox("Fog", &g.fog);
        ImGui::ColorEdit3("Fog Color", &g.fogColor.x);
        ImGui::DragFloat("Fog Start", &g.fogStart, 100.f, 0.f, 100000.f, "%.0f m");
        ImGui::DragFloat("Fog End",   &g.fogEnd, 1000.f, 1000.f, 500000.f, "%.0f m");
        ImGui::Separator();

        ImGui::Text("Grid Fades (m)");
        ImGui::DragFloat("0.0001\xc2\xb0", &g.gridFades.x, 10.f, 50.f, 5000.f, "%.0f");
        ImGui::DragFloat("0.001\xc2\xb0",  &g.gridFades.y, 100.f, 100.f, 50000.f, "%.0f");
        ImGui::DragFloat("0.01\xc2\xb0",   &g.gridFades.z, 1000.f, 1000.f, 500000.f, "%.0f");
        ImGui::DragFloat("0.1\xc2\xb0",    &g.gridFades.w, 5000.f, 5000.f, 2000000.f, "%.0f");
    }

    if (ImGui::CollapsingHeader("Terrain")) {
        bool changed = false;
        changed |= ImGui::DragFloat("Radius",     &terrainCfg_.radiusKm,   0.5f, 1.f, 50.f, "%.1f km");
        changed |= ImGui::DragFloat("Resolution", &terrainCfg_.resolutionM, 5.f, 10.f, 500.f, "%.0f m");
        changed |= ImGui::DragFloat("Rebuild",    &terrainCfg_.rebuildKm,  0.5f, 1.f, 30.f, "%.1f km");
        if (changed) RebuildTerrainIfNeeded(true);
        int verts = int(terrainMesh_.relPos.size());
        int tris  = terrainMesh_.indexCount / 3;
        ImGui::TextDisabled("%d verts, %d tris", verts, tris);
    }

    ImGui::End();
}

// ── input ───────────────────────────────────────────────────────

void Airspace::HandleInput(float dt, bool focused) {
    // C toggles free camera / chase mode
    if (focused && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        cameraFree_ = !cameraFree_;
        if (!cameraFree_) Render::GetCamera("flight").ResetFollow();
    } 
    if (!focused || cameraFree_) return;

    // WASD: pitch/yaw rate control + bank autopilot input
    constexpr float kPitchRate = 40.f, kYawRate = 50.f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) aircraft_.pitch -= kPitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_S)) aircraft_.pitch += kPitchRate * dt;
    if (ImGui::IsKeyDown(ImGuiKey_A)) { aircraft_.yaw -= kYawRate * dt; bank_ = -1.f; }
    if (ImGui::IsKeyDown(ImGuiKey_D)) { aircraft_.yaw += kYawRate * dt; bank_ =  1.f; }
    if (!ImGui::IsKeyDown(ImGuiKey_A) && !ImGui::IsKeyDown(ImGuiKey_D)) bank_ = 0.f;
}

// ── physics ─────────────────────────────────────────────────────

void Airspace::UpdatePhysics(float dt) {
    aircraft_.pitch = std::clamp(aircraft_.pitch, -80.f, 80.f);

    // Decompose velocity into NED components
    double yr = glm::radians(double(aircraft_.yaw));
    double pr = glm::radians(double(aircraft_.pitch));
    double dN = aircraft_.speed * std::cos(yr) * std::cos(pr) * dt;
    double dE = aircraft_.speed * std::sin(yr) * std::cos(pr) * dt;
    double dU = aircraft_.speed * std::sin(pr) * dt;

    // Integrate in ECEF (correct at all latitudes including poles)
    auto ecef = Render::GeoRef::ToEcef(aircraft_.lat, aircraft_.lon, aircraft_.alt);
    double phi = glm::radians(aircraft_.lat), lam = glm::radians(aircraft_.lon);
    double sp = std::sin(phi), cp = std::cos(phi);
    double sl = std::sin(lam), cl = std::cos(lam);
    glm::dvec3 N{-sp * cl, -sp * sl,  cp};
    glm::dvec3 E{-sl,       cl,        0.0};
    glm::dvec3 U{ cp * cl,  cp * sl,   sp};
    ecef += N * dN + E * dE + U * dU;

    // ECEF → geodetic (GeographicLib, 7nm precision)
    static const auto& earth = GeographicLib::Geocentric::WGS84();
    earth.Reverse(ecef.x, ecef.y, ecef.z, aircraft_.lat, aircraft_.lon, aircraft_.alt);
    aircraft_.alt = std::max(aircraft_.alt, 1.0);

    // Smooth roll toward ±35° based on yaw input (bank_ set by HandleInput)
    aircraft_.roll += (bank_ * 35.f - aircraft_.roll) * std::min(1.f, 5.f * dt);
}

// ── scene ───────────────────────────────────────────────────────

void Airspace::DrawWorld(const glm::vec3& pos) {
    // Waypoints (use current scene's GeoRef — works for both flight and gimbal)
    for (auto& wp : waypoints_) {
        auto wpLocal = glm::vec3(Render::GeoToLocal(wp.lat, wp.lon, wp.alt));
        float dist = glm::length(wpLocal - pos) * 0.001f;
        Render::Marker(wpLocal, ICON_FA_LOCATION_DOT, wp.label.c_str(), wp.color,
            "%.2f km\n%.6f, %.6f\n%.0f m", dist, wp.lat, wp.lon, wp.alt);
        // Drag waypoint on terrain surface via pick system
        if (Render::Event().Dragging()) {
            auto& io = ImGui::GetIO();
            double lat, lon, alt;
            // Intersect ellipsoid inflated by current waypoint terrain height
            if (Render::ScreenToGeo(io.MousePos.x, io.MousePos.y, lat, lon, alt, wp.alt)) {
                wp.lat = lat; wp.lon = lon;
                wp.alt = double(terrain_.Sample(lat, lon));
            }
        }
    }

    // Aircraft model (body frame: ZYX Euler rotation from NED)
    Render::PushMatrix();
        Render::Translate(pos);
        Render::RotateZ(aircraft_.yaw);
        Render::RotateY(aircraft_.pitch);
        Render::RotateX(aircraft_.roll);
        DrawAircraft(aircraft_.speed);
    Render::PopMatrix();
}

void Airspace::DrawFlight(float dt) {
    SetupEnv("flight");
    auto nedPos = glm::vec3(Render::GeoToLocal("flight", aircraft_.lat, aircraft_.lon, aircraft_.alt));

    // Camera: chase (follows aircraft heading) or free orbit
    auto& cam = Render::GetCamera("flight");
    if (!cameraFree_)
        cam.Follow(nedPos, aircraft_.yaw);
    else
        cam.Unfollow();

    // Input: checked here because flight viewport is the focus target
    HandleInput(dt, ImGui::IsWindowFocused());

    Render::Begin("flight");
        Render::SetFrame(Render::FrameId::NED);
        Render::Globe();

        RebuildTerrainIfNeeded();
        Render::SetTerrainElevRange(terrain_.elevMin, terrain_.elevMax);
        Render::DrawTerrain(terrainMesh_);

        // World objects (aircraft, waypoints — drag enabled per marker)
        DrawWorld(nedPos);
        auto aircraftEv = Render::Event();
        if (aircraftEv.Hovered())
            Render::Text(nedPos + glm::vec3(0, 0, -0.5f), {1,1,1,.5f},
                "Lat %.6f\nLon %.6f\nAlt %.0f m", aircraft_.lat, aircraft_.lon, aircraft_.alt);

        // Ground projection (cross at surface, vertical line to aircraft)
        Render::Cross({nedPos.x, nedPos.y, 0.f}, 0.5f, {1,1,1,.5f}, 2.f);
        Render::Line(nedPos, {nedPos.x, nedPos.y, 0.f}, {1,1,1,.15f}, 1.0f);

        trail_.Draw({.5f, .5f, .55f, .4f}, 1.5f);

        // Gimbal: sensor frustum + line to target
        auto gimbalNed = nedPos + BodyToNed() * gimbal_.bodyOffset;
        auto targetNed = glm::vec3(Render::GeoToLocal("flight", gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt));
        auto gimbalDir = glm::normalize(targetNed - gimbalNed);
        auto gimbalUp  = BodyToNed() * glm::vec3(0, 0, -1);  // aircraft body up in NED
        Render::Line(nedPos, gimbalNed, Render::Color::Hex("#d4985b50"), 1.f);
        Render::Sensor(gimbalNed, gimbalDir, gimbalUp, gimbal_.fov, gimbal_.aspect, 0.1,
            Render::Color::Hex("#90B0D0"), 1.0f);
        Render::Line(gimbalNed, targetNed, Render::Color::Hex("#00c3ffAA"), 1.f);
        {
            Render::Group tgtGroup;
            Render::Marker(targetNed, ICON_FA_CROSSHAIRS, "Target", Render::Color::Hex("#00ccffff"),
                "Lat %.6f\nLon %.6f\nAlt %.0f m", gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt);
            if (Render::Event().Dragging()) {
                auto& io = ImGui::GetIO();
                double lat, lon, alt;
                if (Render::ScreenToGeo(io.MousePos.x, io.MousePos.y, lat, lon, alt, gimbal_.targetAlt)) {
                    gimbal_.targetLat = lat;
                    gimbal_.targetLon = lon;
                    gimbal_.targetAlt = double(terrain_.Sample(lat, lon));
                }
            }
        }
    Render::End();
    Render::HUD();

    // Double-click on terrain surface → create new waypoint
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        auto& io = ImGui::GetIO();
        double lat, lon, alt;
        // First pass: hit ellipsoid to get approximate lat/lon
        if (Render::ScreenToGeo("flight", io.MousePos.x, io.MousePos.y, lat, lon, alt)) {
            // Refine: re-intersect ellipsoid inflated by terrain height
            double h = double(terrain_.Sample(lat, lon));
            Render::ScreenToGeo("flight", io.MousePos.x, io.MousePos.y, lat, lon, alt, h);
            std::string label = "WP" + std::to_string(waypoints_.size() + 1);
            waypoints_.push_back({lat, lon, double(terrain_.Sample(lat, lon)), std::move(label)});
        }
    }

    // Right-click/hold on terrain → set gimbal target continuously
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        auto& io = ImGui::GetIO();
        double lat, lon, alt;
        // Use current target alt as intersection height — stable during continuous drag
        if (Render::ScreenToGeo("flight", io.MousePos.x, io.MousePos.y, lat, lon, alt, gimbal_.targetAlt)) {
            gimbal_.targetLat = lat;
            gimbal_.targetLon = lon;
            gimbal_.targetAlt = double(terrain_.Sample(lat, lon));
        }
    }

    if (!cameraFree_)
        cam.CaptureFollow();
}

void Airspace::DrawGimbal() {
    ImGui::Begin("Gimbal");
        SetupEnv("gimbal");

        // Gimbal position: aircraft NED + body-rotated offset
        auto aircraftNed = glm::vec3(Render::GeoToLocal("gimbal", aircraft_.lat, aircraft_.lon, aircraft_.alt));
        auto gimbalNed   = aircraftNed + BodyToNed() * gimbal_.bodyOffset;
        auto targetNed   = glm::vec3(Render::GeoToLocal("gimbal", gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt));
        float dist = glm::length(targetNed - gimbalNed);

        // Camera: look from gimbal toward target
        auto& cam = Render::GetCamera("gimbal");
        cam.LookAt(gimbalNed, targetNed);
        cam.Fov() = gimbal_.fov;
        cam.NearPlane() = 0.05f;

        Render::Begin("gimbal");
            Render::SetFrame(Render::FrameId::NED);
            Render::Globe();
            RebuildTerrainIfNeeded();
            Render::SetTerrainElevRange(terrain_.elevMin, terrain_.elevMax);
            Render::DrawTerrain(terrainMesh_);
            DrawWorld(aircraftNed);
        Render::End();
        Render::Crosshair();

        // HUD overlay: FOV, distance, target coords
        if (Render::HudBegin()) {
            ImGui::TextColored({1,1,1,.4f}, "FOV %.0f\xc2\xb0  D %.0fm", gimbal_.fov, dist);
            ImGui::TextColored({1,1,1,.3f}, "%.6f  %.6f  %.0fm",
                gimbal_.targetLat, gimbal_.targetLon, gimbal_.targetAlt);
            Render::HudEnd();
        }
    ImGui::End();
}

void Airspace::SetupEnv(const char* scene) {
    Render::SetOrigin(scene, aircraft_.lat, aircraft_.lon, 0.0);
    auto& env    = Render::GetEnvironment(scene);
    env.bgColor  = {0.015f, 0.02f, 0.04f};
    env.showSun  = true;
    env.lightDir = Render::ToInternal<Render::NED>({0.3f, 0.2f, -0.9f});
}

// ── worker thread (1kHz fixed timestep) ────────────────────────

void Airspace::OnLoop() {
    constexpr float kDt = 0.001f;  // 1ms fixed timestep
    UpdatePhysics(kDt);
    trail_.Record(aircraft_.lat, aircraft_.lon, aircraft_.alt);
}

// ── main thread (rendering) ────────────────────────────────────

void Airspace::OnDraw() {
    DrawControls();
    DrawFlight(ImGui::GetIO().DeltaTime);
    DrawGimbal();
}

// ── persistence ────────────────────────────────────────────────

json Airspace::SaveSettings() const {
    json j;
    json wps = json::array();
    for (auto& wp : waypoints_) {
        json w = {{"lat", wp.lat}, {"lon", wp.lon}, {"alt", wp.alt}, {"label", wp.label}};
        w["color"] = {wp.color.r, wp.color.g, wp.color.b, wp.color.a};
        wps.push_back(w);
    }
    j["waypoints"] = wps;
    j["terrain"] = {
        {"radiusKm",    terrainCfg_.radiusKm},
        {"resolutionM", terrainCfg_.resolutionM},
        {"rebuildKm",   terrainCfg_.rebuildKm},
    };
    return j;
}

void Airspace::LoadSettings(const json& j) {
    if (j.contains("terrain")) {
        auto& t = j["terrain"];
        terrainCfg_.radiusKm    = t.value("radiusKm",    terrainCfg_.radiusKm);
        terrainCfg_.resolutionM = t.value("resolutionM", terrainCfg_.resolutionM);
        terrainCfg_.rebuildKm   = t.value("rebuildKm",   terrainCfg_.rebuildKm);
    }
    if (j.contains("waypoints")) {
        waypoints_.clear();
        for (auto& w : j["waypoints"]) {
            Waypoint wp;
            wp.lat   = w.value("lat", 0.0);
            wp.lon   = w.value("lon", 0.0);
            wp.alt   = double(terrain_.Sample(w.value("lat", 0.0), w.value("lon", 0.0)));
            wp.label = w.value("label", std::string("WP"));
            if (w.contains("color") && w["color"].is_array() && w["color"].size() == 4)
                wp.color = {w["color"][0], w["color"][1], w["color"][2], w["color"][3]};
            waypoints_.push_back(std::move(wp));
        }
    }
}

static const bool reg_ = RegisterPanel<Airspace>("Airspace", "Airspace");

} // namespace Kilo
