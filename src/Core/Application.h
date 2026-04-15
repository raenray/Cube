#pragma once

#include <memory>
#include <string>

struct GLFWwindow;

namespace GameLayer
{
class Game;
}

namespace Core
{

class Application
{
public:
    Application(int width, int height, std::string title);
    ~Application();

    int Run();

private:
    struct WindowDeleter
    {
        void operator()(GLFWwindow* window) const;
    };

    void InitWindow();
    void InitOpenGL();

private:
    int m_Width;
    int m_Height;
    std::string m_Title;

    std::unique_ptr<GLFWwindow, WindowDeleter> m_Window;
    std::unique_ptr<GameLayer::Game> m_Game;
};

} // namespace Core
