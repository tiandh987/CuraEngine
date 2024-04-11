// Copyright (c) 2023 UltiMaker
// CuraEngine is released under the terms of the AGPLv3 or higher

#include "progress/Progress.h"

#include <cassert>
#include <optional>

#include <range/v3/view/enumerate.hpp>
#include <spdlog/spdlog.h>

#include "Application.h" //To get the communication channel to send progress through.
#include "communication/Communication.h" //To send progress through the communication channel.
#include "utils/gettime.h"

namespace cura
{
// 这几行代码初始化了 Progress 类中声明的静态成员变量，分别是累积时间数组、总计时和第一个跳过的层索引。
std::array<double, N_PROGRESS_STAGES> Progress::accumulated_times = { -1 };
double Progress::total_timing = -1;
std::optional<LayerIndex> Progress::first_skipped_layer{};

// 计算整体进度的估计值，根据当前阶段和阶段内的进度百分比来计算。
double Progress::calcOverallProgress(Stage stage, double stage_progress)
{
    assert(stage_progress <= 1.0);
    assert(stage_progress >= 0.0);
    return (accumulated_times.at(static_cast<size_t>(stage)) + stage_progress * times.at(static_cast<size_t>(stage))) / total_timing;
}

// 初始化计算进度所需的值，计算并填充了累积时间数组和总计时。
void Progress::init()
{
    // 声明一个双精度浮点型变量 accumulated_time，用于累积每个阶段的时间
    double accumulated_time = 0;

    // 循环遍历每个进度阶段，N_PROGRESS_STAGES 是之前定义的进度阶段的数量。
    for (size_t stage = 0; stage < N_PROGRESS_STAGES; stage++)
    {
        // 将当前阶段的累积时间设置为 accumulated_time。
        // at() 函数用于访问 std::array 中的元素，static_cast<size_t>(stage) 将 stage 转换为 size_t 类型以索引数组。
        accumulated_times.at(static_cast<size_t>(stage)) = accumulated_time;

        // 累积时间加上当前阶段所需的时间。
        // times 是之前定义的包含每个阶段时间的数组。
        accumulated_time += times.at(static_cast<size_t>(stage));
    }

    // 将总计时设置为累积的最终时间，即所有阶段的时间之和。
    total_timing = accumulated_time;
}

// 传递进度信息，计算整体进度百分比并发送到通信通道。
void Progress::messageProgress(Progress::Stage stage, int progress_in_stage, int progress_in_stage_max)
{
    double percentage = calcOverallProgress(stage, static_cast<double>(progress_in_stage / static_cast<double>(progress_in_stage_max)));
    Application::getInstance().communication_->sendProgress(percentage);
}

// 传递进度阶段信息
void Progress::messageProgressStage(Progress::Stage stage, TimeKeeper* time_keeper)
{
    if (time_keeper != nullptr)
    {
        if (static_cast<int>(stage) > 0)
        {
            spdlog::info("Progress: {} accomplished in {:03.3f}s", names.at(static_cast<size_t>(stage) - 1), time_keeper->restart());
        }
        else
        {
            time_keeper->restart();
        }

        if (static_cast<int>(stage) < static_cast<int>(Stage::FINISH))
        {
            spdlog::info("Starting {}...", names.at(static_cast<size_t>(stage)));
        }
    }
}

// 传递层进度信息
void Progress::messageProgressLayer(LayerIndex layer_nr, size_t total_layers, double total_time, const TimeKeeper::RegisteredTimes& stages, double skip_threshold)
{
    if (total_time < skip_threshold)
    {
        if (! first_skipped_layer)
        {
            first_skipped_layer = layer_nr;
        }
    }
    else
    {
        if (first_skipped_layer)
        {
            spdlog::info("Skipped time reporting for layers [{}...{}]", first_skipped_layer.value(), layer_nr);
            first_skipped_layer.reset();
        }

        messageProgress(Stage::EXPORT, std::max(layer_nr.value, LayerIndex::value_type(0)) + 1, total_layers);

        spdlog::info("┌ Layer export [{}] accomplished in {:03.3f}s", layer_nr, total_time);

        size_t padding = 0;
        auto iterator_max_size = std::max_element(
            stages.begin(),
            stages.end(),
            [](const TimeKeeper::RegisteredTime& time1, const TimeKeeper::RegisteredTime& time2)
            {
                return time1.stage.size() < time2.stage.size();
            });
        if (iterator_max_size != stages.end())
        {
            padding = iterator_max_size->stage.size();

            for (const auto& [index, time] : stages | ranges::views::enumerate)
            {
                spdlog::info("{}── {}:{} {:03.3f}s", index < stages.size() - 1 ? "├" : "└", time.stage, std::string(padding - time.stage.size(), ' '), time.duration);
            }
        }
    }
}

} // namespace cura
