#include "Core/Application.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <iostream>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Core/Input.h"
#include "Core/Time.h"
#include "Game/Game.h"

namespace Core
{

void Application::WindowDeleter::operator()(GLFWwindow* window) const
{
    if (window)
    {
        glfwDestroyWindow(window);
    }
}

Application::Application(int width, int height, std::string title)
    : m_Width(width)
    , m_Height(height)
    , m_Title(std::move(title))
{
    InitWindow();
    InitOpenGL();

    Input::Init(m_Window.get());
    Time::Init();

    m_Game = std::make_unique<GameLayer::Game>(m_Width, m_Height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_Window.get(), true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

Application::~Application()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_Game.reset();
    m_Window.reset();
    glfwTerminate();
}

int Application::Run()
{
    constexpr float kFixedUpdateDelta = 1.0f / 120.0f;
    float updateAccumulator = 0.0f;

    while (!glfwWindowShouldClose(m_Window.get()))
    {
        glfwPollEvents();
        Time::Update();
        const float frameDelta = std::clamp(Time::DeltaTime(), 0.0f, 0.25f);
        updateAccumulator += frameDelta;

        std::cout << "FPS: " << 1 / frameDelta << std::endl;

        if (Input::IsKeyPressed(GLFW_KEY_ESCAPE))
        {
            glfwSetWindowShouldClose(m_Window.get(), GLFW_TRUE);
        }

        int maxSteps = 8;
        while (updateAccumulator >= kFixedUpdateDelta && maxSteps-- > 0)
        {
            m_Game->Update(kFixedUpdateDelta);
            updateAccumulator -= kFixedUpdateDelta;
        }

        if (maxSteps <= 0)
        {
            // 避免螺旋死亡：丢弃过多累积时间
            updateAccumulator = 0.0f;
        }

        glClearColor(0.1f, 0.12f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        m_Game->Render();
        m_Game->RenderUI();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_Window.get());
    }

    return 0;
}

void Application::InitWindow()
{
    if (glfwInit() == GLFW_FALSE)
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* rawWindow = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
    if (!rawWindow)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    m_Window.reset(rawWindow);
    glfwMakeContextCurrent(m_Window.get());
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(m_Window.get(),
                                   [](GLFWwindow*, int width, int height) { glViewport(0, 0, width, height); });
}

void Application::InitOpenGL()
{
    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0)
    {
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // 获取帧缓冲区尺寸而不是窗口尺寸
    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(m_Window.get(), &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

} // namespace Core
