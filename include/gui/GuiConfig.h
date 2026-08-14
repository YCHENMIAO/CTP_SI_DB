#ifndef GUI_CONFIG_H
#define GUI_CONFIG_H

#include <cstddef>

namespace GuiConfig
{
// SpreadPanel 默认使用最近 N 个有效配对价差计算均值和样本标准差。
inline constexpr std::size_t kDefaultSpreadStatsWindow = 100;
}

#endif
