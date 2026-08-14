#include "gui/PanelManager.h"

#include "gui/IPanel.h"
#include "gui/GuiConfig.h"
#include "gui/panels/ChartPanel.h"
#include "gui/panels/MarketPanel.h"
#include "gui/panels/SpreadPanel.h"

#include <algorithm>

PanelManager::PanelManager(CDataBuffer& dataBuffer, const std::vector<std::string>& contracts)
    // 合约列表在这里复制，保证后续创建的行情和价差 Panel 引用始终有效，
    // 不依赖全局 contracts_map 后续是否变化。
    : m_dataBuffer(dataBuffer), m_contracts(contracts)
{
}

PanelManager::~PanelManager() = default;

void PanelManager::Create(PanelType type)
{
    // 所有类型共用一个 ID 序列。除用于显示外，它还会进入 ImGui 的隐藏窗口 ID，
    // 因此即使创建多个同类型面板也不会被 ImGui 合并成同一个窗口。
    const std::uint64_t id = m_nextId++;
    switch (type)
    {
    case PanelType::Market:
        m_panels.push_back(std::make_unique<MarketPanel>(id, m_dataBuffer, m_contracts));
        break;
    case PanelType::Chart:
        // TODO(下一阶段)：ChartPanel 需要行情/指标时，在构造函数中显式注入，
        // 不要从 Panel 内部访问全局 g_pDataBuffer。
        m_panels.push_back(std::make_unique<ChartPanel>(id));
        break;
    case PanelType::Spread:
        m_panels.push_back(std::make_unique<SpreadPanel>(
            id, m_dataBuffer, m_contracts, GuiConfig::kDefaultSpreadStatsWindow));
        break;
    }
}

void PanelManager::RenderAll()
{
    // Render() 可能把 Panel 的 m_open 改为 false，但不会直接改变 m_panels 容器。
    for (const std::unique_ptr<IPanel>& panel : m_panels)
        panel->Render();

    // 所有 Render() 完成后再批量删除关闭的面板，避免循环期间引用/迭代器失效。
    m_panels.erase(
        std::remove_if(m_panels.begin(), m_panels.end(),
                       [](const std::unique_ptr<IPanel>& panel) { return !panel->IsOpen(); }),
        m_panels.end());
}
