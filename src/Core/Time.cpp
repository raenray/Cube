#include "Core/Time.h"

#include <GLFW/glfw3.h>

namespace Core
{

float Time::s_DeltaTime = 0.0f;
float Time::s_ElapsedTime = 0.0f;
float Time::s_LastTime = 0.0f;

void Time::Init()
{
    s_LastTime = static_cast<float>(glfwGetTime());
    s_DeltaTime = 0.0f;
    s_ElapsedTime = 0.0f;
}

void Time::Update()
{
    const float now = static_cast<float>(glfwGetTime());
    s_DeltaTime = now - s_LastTime;
    s_LastTime = now;
    s_ElapsedTime = now;
}

float Time::DeltaTime()
{
    return s_DeltaTime;
}

float Time::ElapsedTime()
{
    return s_ElapsedTime;
}

} // namespace Core
