#include "Renderer/Texture2D.h"

#include <stdexcept>
#include <utility>

#include <glad/gl.h>

namespace Renderer
{

Texture2D::Texture2D(int width, int height, const std::uint8_t* rgbaPixels)
    : m_Width(width)
    , m_Height(height)
{
    glGenTextures(1, &m_Handle);
    if (m_Handle == 0)
    {
        throw std::runtime_error("Failed to create texture");
    }

    glBindTexture(GL_TEXTURE_2D, m_Handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);
}

Texture2D::~Texture2D()
{
    Release();
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : m_Handle(std::exchange(other.m_Handle, 0))
    , m_Width(std::exchange(other.m_Width, 0))
    , m_Height(std::exchange(other.m_Height, 0))
{
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Release();
    m_Handle = std::exchange(other.m_Handle, 0);
    m_Width = std::exchange(other.m_Width, 0);
    m_Height = std::exchange(other.m_Height, 0);

    return *this;
}

void Texture2D::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_Handle);
}

void Texture2D::Release()
{
    if (m_Handle != 0)
    {
        glDeleteTextures(1, &m_Handle);
        m_Handle = 0;
    }
}

} // namespace Renderer
