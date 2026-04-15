#include "Core/Input.h"

#include <GLFW/glfw3.h>

namespace Core
{

GLFWwindow* Input::s_Window = nullptr;

void Input::Init(GLFWwindow* window)
{
    s_Window = window;
}

bool Input::IsKeyPressed(int key)
{
    if (!s_Window)
    {
        return false;
    }
    return glfwGetKey(s_Window, key) == GLFW_PRESS;
}

bool Input::IsMouseButtonPressed(int button)
{
    if (!s_Window)
    {
        return false;
    }
    return glfwGetMouseButton(s_Window, button) == GLFW_PRESS;
}

glm::vec2 Input::GetMousePosition()
{
    if (!s_Window)
    {
        return glm::vec2(0.0f, 0.0f);
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(s_Window, &x, &y);
    return glm::vec2(static_cast<float>(x), static_cast<float>(y));
}

} // namespace Core
