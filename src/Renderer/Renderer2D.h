#pragma once

#include <glm/vec2.hpp>

#include "Renderer/Camera2D.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture2D.h"

namespace Renderer
{

class Renderer2D
{
public:
    Renderer2D();
    ~Renderer2D();

    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    Renderer2D(Renderer2D&& other) noexcept;
    Renderer2D& operator=(Renderer2D&& other) noexcept;

    void BeginScene(const Camera2D& camera);
    void DrawTexturedQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec2& uvMin, const glm::vec2& uvMax, const Texture2D& texture);
    void DrawMesh(const Texture2D& texture, unsigned int vao, unsigned int indexCount);
    void EndScene();

private:
    void Release();

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;
    Shader m_Shader;
};

} // namespace Renderer
