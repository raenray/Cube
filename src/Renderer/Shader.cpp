#include "Renderer/Shader.h"

#include <stdexcept>
#include <string>
#include <utility>

#include <glad/gl.h>

namespace Renderer
{
namespace
{

unsigned int CompileShader(unsigned int type, std::string_view source)
{
    const unsigned int shader = glCreateShader(type);
    const char* src = source.data();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        int logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string infoLog(static_cast<size_t>(logLength), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, infoLog.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compile failed: " + infoLog);
    }

    return shader;
}

} // namespace

Shader::Shader(std::string_view vertexSource, std::string_view fragmentSource)
{
    const unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
    const unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

    m_Program = glCreateProgram();
    glAttachShader(m_Program, vertexShader);
    glAttachShader(m_Program, fragmentShader);
    glLinkProgram(m_Program);

    int success = 0;
    glGetProgramiv(m_Program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE)
    {
        int logLength = 0;
        glGetProgramiv(m_Program, GL_INFO_LOG_LENGTH, &logLength);
        std::string infoLog(static_cast<size_t>(logLength), '\0');
        glGetProgramInfoLog(m_Program, logLength, nullptr, infoLog.data());
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(m_Program);
        m_Program = 0;
        throw std::runtime_error("Program link failed: " + infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    if (m_Program != 0)
    {
        glDeleteProgram(m_Program);
        m_Program = 0;
    }
}

Shader::Shader(Shader&& other) noexcept
    : m_Program(std::exchange(other.m_Program, 0))
{
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (m_Program != 0)
    {
        glDeleteProgram(m_Program);
    }

    m_Program = std::exchange(other.m_Program, 0);
    return *this;
}

void Shader::Bind() const
{
    glUseProgram(m_Program);
}

void Shader::SetMat4(const char* name, const glm::mat4& value) const
{
    const int location = glGetUniformLocation(m_Program, name);
    glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
}

void Shader::SetVec2(const char* name, const glm::vec2& value) const
{
    const int location = glGetUniformLocation(m_Program, name);
    glUniform2f(location, value.x, value.y);
}

void Shader::SetVec4(const char* name, const glm::vec4& value) const
{
    const int location = glGetUniformLocation(m_Program, name);
    glUniform4f(location, value.x, value.y, value.z, value.w);
}

void Shader::SetInt(const char* name, int value) const
{
    const int location = glGetUniformLocation(m_Program, name);
    glUniform1i(location, value);
}

} // namespace Renderer
