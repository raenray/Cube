#include "Renderer/Renderer2D.h"

#include <array>
#include <stdexcept>
#include <utility>

#include <glad/gl.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/vec3.hpp>

namespace Renderer
{
namespace
{

constexpr const char* kVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in float aLight;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec2 vUV;
out float vLight;

void main()
{
    vUV = aUV;
    vLight = aLight;
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}
)";

constexpr const char* kFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec2 vUV;
in float vLight;

uniform sampler2D u_Texture;

void main()
{
    vec4 albedo = texture(u_Texture, vUV);
    FragColor = vec4(albedo.rgb * vLight, albedo.a);
}
)";

} // namespace

Renderer2D::Renderer2D()
    : m_Shader(kVertexShader, kFragmentShader)
{
    const std::array<float, 24> vertices = {// x, y, z, u, v, light
                                            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                                            1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f};

    const std::array<unsigned int, 6> indices = {0, 1, 2, 2, 3, 0};

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    if (m_VAO == 0 || m_VBO == 0 || m_EBO == 0)
    {
        throw std::runtime_error("Failed to create renderer buffers");
    }

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

Renderer2D::~Renderer2D()
{
    Release();
}

Renderer2D::Renderer2D(Renderer2D&& other) noexcept
    : m_VAO(std::exchange(other.m_VAO, 0))
    , m_VBO(std::exchange(other.m_VBO, 0))
    , m_EBO(std::exchange(other.m_EBO, 0))
    , m_Shader(std::move(other.m_Shader))
{
}

Renderer2D& Renderer2D::operator=(Renderer2D&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Release();

    m_VAO = std::exchange(other.m_VAO, 0);
    m_VBO = std::exchange(other.m_VBO, 0);
    m_EBO = std::exchange(other.m_EBO, 0);
    m_Shader = std::move(other.m_Shader);

    return *this;
}

void Renderer2D::BeginScene(const Camera2D& camera)
{
    m_Shader.Bind();
    m_Shader.SetMat4("u_ViewProjection", camera.GetViewProjection());
    m_Shader.SetInt("u_Texture", 0);
}

void Renderer2D::DrawTexturedQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec2& uvMin, const glm::vec2& uvMax, const Texture2D& texture)
{
    const std::array<float, 24> vertices = {
        // x, y, z, u, v, light
        0.0f, 0.0f, 0.0f, uvMin.x, uvMin.y, 1.0f, 1.0f, 0.0f, 0.0f, uvMax.x, uvMin.y, 1.0f,
        1.0f, 1.0f, 0.0f, uvMax.x, uvMax.y, 1.0f, 0.0f, 1.0f, 0.0f, uvMin.x, uvMax.y, 1.0f};

    const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, 0.0f)) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(size.x, size.y, 1.0f));

    m_Shader.Bind();
    m_Shader.SetMat4("u_Model", model);
    texture.Bind(0);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Renderer2D::DrawMesh(const Texture2D& texture, unsigned int vao, unsigned int indexCount)
{
    if (vao == 0 || indexCount == 0)
    {
        return;
    }

    m_Shader.Bind();
    texture.Bind(0);

    const glm::mat4 identity(1.0f);
    m_Shader.SetMat4("u_Model", identity);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Renderer2D::EndScene() {}

void Renderer2D::Release()
{
    if (m_EBO != 0)
    {
        glDeleteBuffers(1, &m_EBO);
        m_EBO = 0;
    }

    if (m_VBO != 0)
    {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }

    if (m_VAO != 0)
    {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
}

} // namespace Renderer
