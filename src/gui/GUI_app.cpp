#include "gui/GUI_APP.h"

#include "DataBuffer.h"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

extern std::map<std::string, std::string> accountConfig_map;
extern std::map<std::string, std::string> contracts_map;

namespace
{
// GLFW 初始化、窗口创建或平台后端发生错误时统一写到标准错误输出。
void GlfwErrorCallback(int error, const char* description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

std::vector<std::string> GetContracts()
{
    // contracts.txt 当前按 key,value 读入 map，但一个 value 可包含多个空格分隔合约。
    // GuiApp 只在构造时解析一次，去重后注入 PanelManager，Panel 不再依赖全局 map。
    std::vector<std::string> contracts;
    for (const auto& [key, value] : contracts_map)
    {
        std::istringstream input(value);
        std::string contract;
        while (input >> contract)
        {
            if (std::find(contracts.begin(), contracts.end(), contract) == contracts.end())
                contracts.push_back(contract);
        }
    }
    return contracts;
}

const char* GetAccountValue(const char* key)
{
    // 只用于当前帧立即传给 ImGui::Text；不缓存 c_str()，避免 map 修改后指针失效。
    const auto found = accountConfig_map.find(key);
    return found == accountConfig_map.end() ? "" : found->second.c_str();
}
}

GuiApp::GuiApp(CDataBuffer& dataBuffer, CIndicator&)
    // 成员按声明顺序初始化：先得到稳定的 m_contracts，再让 PanelManager 复制它。
    // indicator 暂未由应用外壳使用，为保持 main.cpp 现有接口暂时保留参数。
    : m_contracts(GetContracts()), m_panelManager(dataBuffer, m_contracts)
{
}

GuiApp::~GuiApp()
{
    Shutdown();
}

bool GuiApp::Initialize()
{
    // 整体成功后 m_initialized 才为 true；其余分阶段标志用于失败时精确回滚。
    if (m_initialized)
        return true;

    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW. Check DISPLAY/Wayland access.\n";
        return false;
    }
    m_glfwInitialized = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    m_window = glfwCreateWindow(1440, 900, "CTP——交易终端可视化", nullptr, nullptr);
    if (!m_window)
    {
        std::cerr << "Failed to create the GLFW OpenGL window.\n";
        Shutdown();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    // swap interval 依赖当前 OpenGL Context；1 表示开启垂直同步。
    glfwSwapInterval(1);

    // ImGui Context 必须先于 IO、字体、样式和后端初始化。
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // ImPlot 使用当前 ImGui Context，并必须在它之前销毁。
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->AddFontFromFileTTF("/mnt/c/Windows/Fonts/msyh.ttc", 18.0F, nullptr,
                                     io.Fonts->GetGlyphRangesChineseFull()) == nullptr)
        std::cerr << "Failed to load Chinese font: /mnt/c/Windows/Fonts/msyh.ttc\n";
    // 允许 Panel 被拖出主窗口，由 GLFW 后端自动创建独立原生窗口。
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    // 当前阶段不保存面板位置；后续要持久化布局时在这里恢复 IniFilename。
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0F;
    style.Colors[ImGuiCol_WindowBg].w = 1.0F;

    m_glfwBackendInitialized = ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    if (m_glfwBackendInitialized)
        m_openglBackendInitialized = ImGui_ImplOpenGL3_Init("#version 330");
    if (!m_glfwBackendInitialized || !m_openglBackendInitialized)
    {
        std::cerr << "Failed to initialize the ImGui GLFW/OpenGL3 backends.\n";
        Shutdown();
        return false;
    }

    m_initialized = true;
    return true;
}

void GuiApp::Run()
{
    if (!m_initialized)
        return;

    while (!glfwWindowShouldClose(m_window))
    {
        // GUI 与 OpenGL 只在主线程调用。CTP 回调线程只写线程安全的 CDataBuffer。
        glfwPollEvents();
        BeginFrame();
        RenderMenu();
        // GuiApp 不感知具体面板内容，只把本帧渲染调度交给 PanelManager。
        m_panelManager.RenderAll();
        EndFrame();
    }
}

void GuiApp::BeginFrame()
{
    // 两个后端先更新输入/渲染状态，最后由 ImGui 创建新帧。
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GuiApp::RenderMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("新建"))
        {
            // 菜单只描述用户意图；实际构造类型和所有权由 PanelManager 处理。
            if (ImGui::MenuItem("行情面板"))
                m_panelManager.Create(PanelType::Market);
            if (ImGui::MenuItem("图表面板"))
                m_panelManager.Create(PanelType::Chart);
            if (ImGui::MenuItem("价差面板"))
                m_panelManager.Create(PanelType::Spread);
            ImGui::EndMenu();
        }

        ImGui::SameLine();
        ImGui::Text("经纪商：%s    用户：%s",
                    GetAccountValue("brokerId"), GetAccountValue("userId"));

        // 每帧读取本机时间，不保存额外状态或启动定时线程。
        const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm localTime{};
        localtime_r(&now, &localTime);
        std::ostringstream timeText;
        timeText << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
        const std::string time = timeText.str();
        // 使用当前菜单栏宽度和文字宽度，将时间对齐到右侧。
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(time.c_str()).x -
                        ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextUnformatted(time.c_str());
        ImGui::EndMainMenuBar();
    }
}

void GuiApp::EndFrame()
{
    // 生成主 Viewport 的 DrawData，并用主窗口 OpenGL Context 绘制。
    ImGui::Render();

    int displayWidth = 0;
    int displayHeight = 0;
    glfwGetFramebufferSize(m_window, &displayWidth, &displayHeight);
    glViewport(0, 0, displayWidth, displayHeight);
    glClearColor(0.08F, 0.09F, 0.11F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Multi-Viewport 渲染会切换不同平台窗口的 OpenGL Context，完成后必须恢复主 Context。
    GLFWwindow* backupContext = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backupContext);
    glfwSwapBuffers(m_window);
}

void GuiApp::Shutdown()
{
    // 严格按初始化逆序释放；状态标志使初始化中途失败和析构重复调用均保持安全。
    if (m_openglBackendInitialized)
    {
        ImGui_ImplOpenGL3_Shutdown();
        m_openglBackendInitialized = false;
    }
    if (m_glfwBackendInitialized)
    {
        ImGui_ImplGlfw_Shutdown();
        m_glfwBackendInitialized = false;
    }
    if (ImPlot::GetCurrentContext())
        ImPlot::DestroyContext();
    if (ImGui::GetCurrentContext())
        ImGui::DestroyContext();
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_glfwInitialized)
    {
        glfwTerminate();
        m_glfwInitialized = false;
    }
    m_initialized = false;
}
