#pragma once

#include <string_view>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace Renderer
{

class Shader
{
public:
    Shader() = default;
    Shader(std::string_view vertexSource, std::string_view fragmentSource);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void Bind() const;

    void SetMat4(const char* name, const glm::mat4& value) const;
    void SetVec2(const char* name, const glm::vec2& value) const;
    void SetVec4(const char* name, const glm::vec4& value) const;
    void SetInt(const char* name, int value) const;

private:
    unsigned int m_Program = 0;
};

} // namespace Renderer
