#ifndef GUI_PANEL_MANAGER_H
#define GUI_PANEL_MANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CDataBuffer;
class IPanel;

// 顶部“新建”菜单支持的面板类型。
// 后续新增 Panel 时，需要同步扩展此枚举和 PanelManager::Create()。
enum class PanelType
{
    Market,
    Chart,
    Spread
};

// 动态 Panel 的唯一所有者。
// 负责创建、逐帧调度和删除；不负责具体面板的 ImGui 内容。
class PanelManager
{
public:
    // contracts 在构造时复制一份，MarketPanel/SpreadPanel 引用该稳定容器。
    // dataBuffer 的生命周期由 main/GuiApp 外部保证长于 PanelManager。
    PanelManager(CDataBuffer& dataBuffer, const std::vector<std::string>& contracts);
    ~PanelManager();

    // 根据菜单选择创建一个新 Panel，并为其分配不会重复的实例 ID。
    void Create(PanelType type);
    // 先渲染所有 Panel，再统一删除已关闭对象，避免遍历 vector 时迭代器失效。
    void RenderAll();

private:
    CDataBuffer& m_dataBuffer;                    // 线程安全行情缓存，非拥有引用。
    std::vector<std::string> m_contracts;         // GUI 使用的去重合约快照。
    std::vector<std::unique_ptr<IPanel>> m_panels; // 所有动态面板的唯一所有权。
    std::uint64_t m_nextId = 1;                   // 三种 Panel 共用的递增 ID。
};

#endif
