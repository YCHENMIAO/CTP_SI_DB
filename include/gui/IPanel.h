#ifndef GUI_IPANEL_H
#define GUI_IPANEL_H

#include <cstdint>
#include <string>

// 所有可动态创建的 GUI 面板都实现此接口。
// PanelManager 只依赖该抽象，因此不需要知道行情、图表或价差面板的内部状态。
class IPanel
{
public:
    // 必须使用虚析构：PanelManager 通过 unique_ptr<IPanel> 销毁具体派生对象。
    virtual ~IPanel() = default;

    // 全局唯一实例编号，由 PanelManager 创建面板时分配。
    virtual std::uint64_t Id() const = 0;
    // 用户可见的窗口标题；具体面板会额外拼接 ###隐藏ID 供 ImGui 识别实例。
    virtual const std::string& Title() const = 0;
    // 用户点击 ImGui 窗口右上角关闭按钮后返回 false，供管理器安全删除实例。
    virtual bool IsOpen() const = 0;
    // 在 GUI 主线程、ImGui::NewFrame() 与 ImGui::Render() 之间调用。
    // 每个具体 Panel 自己负责成对调用 ImGui::Begin()/End()。
    virtual void Render() = 0;
};

#endif
