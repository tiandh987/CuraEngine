// Copyright (c) 2023 UltiMaker
// CuraEngine is released under the terms of the AGPLv3 or higher

// 定义了一个名为 Progress 的类，用于在 CuraEngine 中处理切片过程中的进度报告。

#ifndef PROGRESS_H
#define PROGRESS_H

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "utils/gettime.h"

namespace cura
{

struct LayerIndex;

// 定义了一个名为 N_PROGRESS_STAGES 的常量，表示进度阶段的数量，其值为 7。
static constexpr size_t N_PROGRESS_STAGES = 7;

/*!
 * Class for handling the progress bar and the progress logging.
 *
 * The progress bar is based on a single slicing of a rather large model which needs some complex support;
 * the relative timing of each stage is currently based on that of the slicing of dragon_65_tilted_large.stl
 */
/*!
 * 用于处理进度条和进度记录的类。
 *
 * 进度条基于相当大的模型的单个切片，需要一些复杂的支持；
 * 目前各阶段的相对时序基于 dragon_65_tilted_large.stl 的切片
*/
class Progress
{
public:
    /*!
     * The stage in the whole slicing process
     *
     * 定义了一个名为 Stage 的枚举类型，表示切片过程的不同阶段。每个枚举值都有与之关联的整数值，从 0 开始递增。
     */
    enum class Stage : unsigned int
    {
        START = 0,
        SLICING = 1,
        PARTS = 2,
        INSET_SKIN = 3,
        SUPPORT = 4,
        EXPORT = 5,
        FINISH = 6
    };

private:
    // 定义了一个名为 times 的静态成员变量，它是一个包含了每个阶段所需时间的数组。
    static constexpr std::array<double, N_PROGRESS_STAGES> times{
        0.0, // START   = 0,
        5.269, // SLICING = 1,
        1.533, // PARTS   = 2,
        71.811, // INSET_SKIN = 3
        51.009, // SUPPORT = 4,
        154.62, // EXPORT  = 5,
        0.1 // FINISH  = 6
    };

    // 定义了一个名为 names 的静态成员变量，它是一个包含了每个阶段名称的字符串视图数组。
    static constexpr std::array<std::string_view, N_PROGRESS_STAGES> names{ "start", "slice", "layerparts", "inset+skin", "support", "export", "process" };
    // 定义了一个名为 accumulated_times 的静态成员变量，它是一个数组，用于累积记录每个阶段的时间。
    static std::array<double, N_PROGRESS_STAGES> accumulated_times; //!< Time past before each stage
    // 定义了一个名为 total_timing 的静态成员变量，表示总计时的估计值。
    static double total_timing; //!< An estimate of the total time
    // 定义了一个名为 first_skipped_layer 的静态成员变量，它是一个可选类型，用于记录跳过时间报告的第一个层索引。
    static std::optional<LayerIndex> first_skipped_layer; //!< The index of the layer for which we skipped time reporting
    /*!
     * Give an estimate between 0 and 1 of how far the process is.
     *
     * \param stage The current stage of processing
     * \param stage_process How far we currently are in the \p stage
     * \return An estimate of the overall progress.
     */
    // 声明了一个名为 calcOverallProgress 的静态成员函数，用于计算整体进度的估计值。
    static double calcOverallProgress(Stage stage, double stage_progress);

public:
    // 声明了一个名为 init 的静态成员函数，用于初始化计算进度所需的一些值。
    static void init(); //!< Initialize some values needed in a fast computation of the progress
    /*!
     * Message progress over the CommandSocket and to the terminal (if the command line arg '-p' is provided).
     *
     * \param stage The current stage of processing
     * \param progress_in_stage Any number giving the progress within the stage
     * \param progress_in_stage_max The maximal value of \p progress_in_stage
     */
    // 声明了一个名为 messageProgress 的静态成员函数，用于消息传递进度信息。
    static void messageProgress(Stage stage, int progress_in_stage, int progress_in_stage_max);

    /*!
     * Message the progress stage over the command socket.
     *
     * \param stage The current stage
     * \param timeKeeper The stapwatch keeping track of the timings for each stage (optional)
     */
    // 声明了一个名为 messageProgressStage 的静态成员函数，用于消息传递进度阶段信息。
    static void messageProgressStage(Stage stage, TimeKeeper* timeKeeper);

    /*!
     * Message the layer progress over the command socket and into logging output.
     *
     * \param layer_nr The processed layer number
     * \param total_layers The total number of layers to be processed
     * \param total_time The total layer processing time, in seconds
     * \param stage The detailed stages time reporting for this layer
     * \param skip_threshold The time threshold under which we consider that the full layer time reporting should be skipped
     *                       because it is not relevant
     */
    // 声明了一个名为 messageProgressLayer 的静态成员函数，用于消息传递层进度信息。
    static void messageProgressLayer(LayerIndex layer_nr, size_t total_layers, double total_time, const TimeKeeper::RegisteredTimes& stages, double skip_threshold = 0.1);
};


} // namespace cura
#endif // PROGRESS_H
