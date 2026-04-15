#pragma once

#include <cstdint>

namespace Renderer
{

class Texture2D
{
public:
    Texture2D() = default;
    Texture2D(int width, int height, const std::uint8_t* rgbaPixels);
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    void Bind(unsigned int slot = 0) const;

    int Width() const { return m_Width; }
    int Height() const { return m_Height; }

private:
    void Release();

private:
    unsigned int m_Handle = 0;
    int m_Width = 0;
    int m_Height = 0;
};

} // namespace Renderer
