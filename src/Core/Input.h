#pragma once

#include <glm/vec2.hpp>

struct GLFWwindow;

namespace Core
{

class Input
{
public:
    static void Init(GLFWwindow* window);
    static bool IsKeyPressed(int key);
    static bool IsMouseButtonPressed(int button);
    static glm::vec2 GetMousePosition();

private:
    static GLFWwindow* s_Window; // non-owning
};

} // namespace Core
