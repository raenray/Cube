#include "Renderer/Camera2D.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Renderer
{

Camera2D::Camera2D(float viewportWidth, float viewportHeight)
    : m_ViewportWidth(viewportWidth)
    , m_ViewportHeight(viewportHeight)
    , m_Position(0.0f, 0.0f)
    , m_ViewProjection(1.0f)
{
    Recalculate();
}

void Camera2D::SetViewport(float width, float height)
{
    m_ViewportWidth = width;
    m_ViewportHeight = height;
    Recalculate();
}

void Camera2D::SetPosition(const glm::vec2& position)
{
    m_Position = position;
    Recalculate();
}

void Camera2D::Move(const glm::vec2& delta)
{
    m_Position += delta;
    Recalculate();
}

const glm::vec2& Camera2D::GetPosition() const
{
    return m_Position;
}

float Camera2D::GetViewportWidth() const
{
    return m_ViewportWidth;
}

float Camera2D::GetViewportHeight() const
{
    return m_ViewportHeight;
}

const glm::mat4& Camera2D::GetViewProjection() const
{
    return m_ViewProjection;
}

void Camera2D::Recalculate()
{
    const glm::mat4 projection = glm::ortho(0.0f, m_ViewportWidth, 0.0f, m_ViewportHeight, -1.0f, 1.0f);
    const glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-m_Position.x, -m_Position.y, 0.0f));
    m_ViewProjection = projection * view;
}

} // namespace Renderer
