#pragma once
#include <glm/glm.hpp>
#include <string>
#include <cstdint>

namespace Kilo::Render {

using ModelId = uint32_t;
inline constexpr ModelId kInvalidModel = 0;

// Load model from file (cached by path). Supported: .obj
ModelId LoadModel(const std::string& path);
bool    IsModelLoaded(ModelId id);

// Draw loaded model at current transform stack position.
void Model(ModelId id, const glm::vec4& color);
void Model(ModelId id);  // per-submesh default colors

// Cleanup (called from Render::Shutdown)
void ShutdownModels();

} // namespace Kilo::Render
