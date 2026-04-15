#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace Renderer
{

class Camera2D
{
public:
    Camera2D(float viewportWidth, float viewportHeight);

    void SetViewport(float width, float height);
    void SetPosition(const glm::vec2& position);
    void Move(const glm::vec2& delta);
    const glm::vec2& GetPosition() const;

    float GetViewportWidth() const;
    float GetViewportHeight() const;

    const glm::mat4& GetViewProjection() const;

private:
    void Recalculate();

private:
    float m_ViewportWidth;
    float m_ViewportHeight;
    glm::vec2 m_Position;
    glm::mat4 m_ViewProjection;
};

} // namespace Renderer
