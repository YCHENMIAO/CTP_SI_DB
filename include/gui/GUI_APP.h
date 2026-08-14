#ifndef GUI_APP_H
#define GUI_APP_H

#include "gui/PanelManager.h"

#include <string>
#include <vector>

class CDataBuffer;
class CIndicator;

// GUI 应用外壳：只管理 GLFW、ImGui、帧循环、顶部菜单和 PanelManager。
// 具体行情或策略视图不得重新放回此类，应实现为新的 IPanel。
class GuiApp
{
public:
    // 当前 indicator 参数为兼容 main.cpp 的既有构造方式而保留。
    // TODO(下一阶段)：ChartPanel 接入指标后，由 PanelManager 明确注入所需依赖。
    GuiApp(CDataBuffer& dataBuffer, CIndicator& indicator);
    ~GuiApp();

    bool Initialize(); // 创建 GLFW/OpenGL/ImGui 资源。
    void Run();        // 主线程逐帧运行，直到主 GLFW 窗口关闭。
    void Shutdown();   // 按初始化逆序释放；允许重复调用。

private:
    void BeginFrame(); // 开启一帧 ImGui。
    void RenderMenu(); // 应用级菜单、账户信息和系统时间。
    void EndFrame();   // 渲染主视口和 Multi-Viewport 并交换缓冲区。

    // 声明顺序很重要：PanelManager 内部 Panel 引用 m_contracts，故合约容器必须先构造、后销毁。
    std::vector<std::string> m_contracts;
    PanelManager m_panelManager;
    struct GLFWwindow* m_window = nullptr;
    bool m_glfwInitialized = false;
    bool m_glfwBackendInitialized = false;
    bool m_openglBackendInitialized = false;
    bool m_initialized = false;
};

#endif
