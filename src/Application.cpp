// Copyright (c) 2024 UltiMaker
// CuraEngine is released under the terms of the AGPLv3 or higher

#include "Application.h"

#include <chrono>
#include <memory>
#include <string>

#include <boost/uuid/random_generator.hpp> //For generating a UUID.
#include <boost/uuid/uuid_io.hpp> //For generating a UUID.
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/cfg/helpers.h>
#include <spdlog/details/os.h>
#include <spdlog/details/registry.h>
#include <spdlog/sinks/dup_filter_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "FffProcessor.h"
#include "communication/ArcusCommunication.h" //To connect via Arcus to the front-end.
#include "communication/CommandLine.h" //To use the command line to slice stuff.
#include "plugins/slots.h"
#include "progress/Progress.h"
#include "utils/ThreadPool.h"
#include "utils/string.h" //For stringcasecompare.

namespace cura
{

// Application 类的构造函数的实现
// 这段代码主要是在构造函数中初始化了日志输出相关的设置，并配置了日志去重和输出级别的处理
// 
// 这里使用了 Boost 库生成一个随机的 UUID，并将其转换为字符串，并将其赋值给 instance_uuid_。
// 这样就为 instance_uuid_ 成员变量赋了一个随机生成的唯一标识符。
Application::Application()
    : instance_uuid_(boost::uuids::to_string(boost::uuids::random_generator()()))
{
    // 接下来创建了两个日志输出的 sink。
    // 一个是 dup_filter_sink_mt，用于在一定时间范围内去重日志消息，
    // 一个是 stdout_color_sink_mt，用于将日志消息输出到控制台并支持着色。
    auto dup_sink = std::make_shared<spdlog::sinks::dup_filter_sink_mt>(std::chrono::seconds{ 10 });
    auto base_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // 创建的 dup_filter_sink_mt 使用 add_sink() 方法将 stdout_color_sink_mt 添加为其子 sink，
    // 这样可以确保日志消息会输出到控制台，并且在一定时间内不会重复输出相同的消息。
    dup_sink->add_sink(base_sink);

    // 使用 spdlog::default_logger()->sinks() 获取默认日志记录器的 sink 集合，并将其替换为仅包含 dup_filter_sink_mt 的集合。
    // 这样做是为了确保所有的日志消息都经过去重处理，避免日志消息的过度重复。
    spdlog::default_logger()->sinks()
        = std::vector<std::shared_ptr<spdlog::sinks::sink>>{ dup_sink }; // replace default_logger sinks with the duplicating filtering sink to avoid spamming

    // 检查环境变量 "CURAENGINE_LOG_LEVEL" 是否存在，并使用 spdlog::cfg::helpers::load_levels() 方法加载日志级别配置。
    // 这样可以根据环境变量设置日志级别，用于控制日志的输出等级。
    if (auto spdlog_val = spdlog::details::os::getenv("CURAENGINE_LOG_LEVEL"); ! spdlog_val.empty())
    {
        spdlog::cfg::helpers::load_levels(spdlog_val);
    };
}

// Application 类的析构函数实现
// 
// 确保在 Application 对象被销毁时，释放了它所管理的动态分配的资源，防止内存泄漏。
Application::~Application()
{
    // 表示释放 communication_ 指针所指向的对象的内存。
    // 这表明 communication_ 是一个指针，指向某个动态分配的对象，它的释放可能涉及到释放资源或对象的析构。
    delete communication_;

    // 同样释放了 thread_pool_ 指针所指向的对象的内存。
    // 这表明 thread_pool_ 也是一个指针，指向某个动态分配的对象。
    delete thread_pool_;
}

// 实现了单例模式的 getInstance() 方法，返回一个对 Application 类的唯一实例的引用
// 确保了在整个程序中只有一个 Application 实例存在，并提供了一种访问该实例的方法，即调用 getInstance() 方法。
Application& Application::getInstance()
{
    // 声明了一个静态的 Application 对象 instance。
    // 这个对象是在 getInstance() 方法首次被调用时创建的，因为它是静态的，所以它在程序的生命周期内只会被创建一次。
    static Application instance; // Constructs using the default constructor.

    // 返回对 instance 对象的引用。
    // 因为 instance 是静态的，所以它会一直存在于整个程序的生命周期内，并且始终表示唯一的 Application 实例
    return instance;
}

#ifdef ARCUS
void Application::connect()
{
    // 初始化IP地址和端口号的默认值。
    std::string ip = "127.0.0.1";
    int port = 49674;

    // Parse port number from IP address.
    // 从命令行参数中解析端口号。如果参数中包含了端口号（通过冒号分隔），则将IP和端口号分别解析出来。
    std::string ip_port(argv_[2]);
    std::size_t found_pos = ip_port.find(':');
    if (found_pos != std::string::npos)
    {
        ip = ip_port.substr(0, found_pos);
        port = std::stoi(ip_port.substr(found_pos + 1).data());
    }

    // 定义了一个整数变量 n_threads。
    int n_threads;

    // 遍历命令行参数，根据参数指定的选项进行不同的操作。
    // 如果是 -v 选项，则设置日志级别为 debug；
    // 如果是 -m 选项，则解析下一个参数为线程数，并启动线程池；
    // 如果是其他选项，则记录错误并打印帮助信息。
    for (size_t argn = 3; argn < argc_; argn++)
    {
        char* str = argv_[argn];
        if (str[0] == '-')
        {
            for (str++; *str; str++)
            {
                switch (*str)
                {
                case 'v':
                    spdlog::set_level(spdlog::level::debug);
                    break;
                case 'm':
                    str++;
                    n_threads = std::strtol(str, &str, 10);
                    str--;
                    startThreadPool(n_threads);
                    break;
                default:
                    spdlog::error("Unknown option: {}", str);
                    printCall();
                    printHelp();
                    break;
                }
            }
        }
    }

    // 创建 ArcusCommunication 对象，并使用解析得到的 IP 和 端口号 连接。然后将该对象赋值给 communication_ 成员变量。
    ArcusCommunication* arcus_communication = new ArcusCommunication();
    arcus_communication->connect(ip, port);
    communication_ = arcus_communication;
}
#endif // ARCUS

// 这段代码定义了 Application 类的成员函数 printCall()，它用于打印调用命令。
// 
// 它指定了函数的返回类型 (void) 和函数名 (printCall)，并标记为 const，表示该函数不会修改类的成员变量。
void Application::printCall() const
{
    // 使用 spdlog 库打印错误消息
    //
    // spdlog::error() 是一个用于记录错误消息的函数，它接受一个格式化的字符串和参数列表，将格式化后的消息输出到日志中。
    //   "Command called: {}" 是错误消息的格式字符串，其中 {} 是一个占位符，表示后面会填入一个参数。
    //   *argv_ 是解引用 argv_ 指针得到的值，它应该是一个指向字符串的指针，表示调用的命令。这个值被填充到占位符 {} 中，以完成消息的格式化。
    spdlog::error("Command called: {}", *argv_);
}

// 打印帮助信息
void Application::printHelp() const
{
    fmt::print("\n");
    fmt::print("usage:\n");
    fmt::print("CuraEngine help\n");
    fmt::print("\tShow this help message\n");
    fmt::print("\n");
#ifdef ARCUS
    fmt::print("CuraEngine connect <host>[:<port>] [-j <settings.def.json>]\n");
    fmt::print("  --connect <host>[:<port>]\n\tConnect to <host> via a command socket, \n\tinstead of passing information via the command line\n");
    fmt::print("  -v\n\tIncrease the verbose level (show log messages).\n");
    fmt::print("  -m<thread_count>\n\tSet the desired number of threads. Supports only a single digit.\n");
    fmt::print("\n");
#endif // ARCUS
    fmt::print("CuraEngine slice [-v] [-p] [-j <settings.json>] [-s <settingkey>=<value>] [-g] [-e<extruder_nr>] [-o <output.gcode>] [-l <model.stl>] [--next]\n");
    fmt::print("  -v\n\tIncrease the verbose level (show log messages).\n");
    fmt::print("  -m<thread_count>\n\tSet the desired number of threads.\n");
    fmt::print("  -p\n\tLog progress information.\n");
    fmt::print("  -d Add definition search paths seperated by a `:` (Unix) or `;` (Windows)\n");
    fmt::print("  -j\n\tLoad settings.def.json file to register all settings and their defaults.\n");
    fmt::print("  -s <setting>=<value>\n\tSet a setting to a value for the last supplied object, \n\textruder train, or general settings.\n");
    fmt::print("  -l <model_file>\n\tLoad an STL model. \n");
    fmt::print("  -g\n\tSwitch setting focus to the current mesh group only.\n\tUsed for one-at-a-time printing.\n");
    fmt::print("  -e<extruder_nr>\n\tSwitch setting focus to the extruder train with the given number.\n");
    fmt::print("  --next\n\tGenerate gcode for the previously supplied mesh group and append that to \n\tthe gcode of further models for one-at-a-time printing.\n");
    fmt::print("  -o <output_file>\n\tSpecify a file to which to write the generated gcode.\n");
    fmt::print("\n");
    fmt::print("The settings are appended to the last supplied object:\n");
    fmt::print("CuraEngine slice [general settings] \n\t-g [current group settings] \n\t-e0 [extruder train 0 settings] \n\t-l obj_inheriting_from_last_extruder_train.stl [object "
               "settings] \n\t--next [next group settings]\n\t... etc.\n");
    fmt::print("\n");
    fmt::print("In order to load machine definitions from custom locations, you need to create the environment variable CURA_ENGINE_SEARCH_PATH, which should contain all search "
               "paths delimited by a (semi-)colon.\n");
    fmt::print("\n");
}

// 打印 License
void Application::printLicense() const
{
    fmt::print("\n");
    fmt::print("Cura_SteamEngine version {}\n", CURA_ENGINE_VERSION);
    fmt::print("Copyright (C) 2024 Ultimaker\n");
    fmt::print("\n");
    fmt::print("This program is free software: you can redistribute it and/or modify\n");
    fmt::print("it under the terms of the GNU Affero General Public License as published by\n");
    fmt::print("the Free Software Foundation, either version 3 of the License, or\n");
    fmt::print("(at your option) any later version.\n");
    fmt::print("\n");
    fmt::print("This program is distributed in the hope that it will be useful,\n");
    fmt::print("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    fmt::print("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    fmt::print("GNU Affero General Public License for more details.\n");
    fmt::print("\n");
    fmt::print("You should have received a copy of the GNU Affero General Public License\n");
    fmt::print("along with this program.  If not, see <http://www.gnu.org/licenses/>.\n");
}

// 解析命令行参数并创建一个 CommandLine 对象，以便在应用程序中进一步处理这些参数。
void Application::slice()
{
    // 创建了一个字符串向量 arguments，用于存储命令行参数
    std::vector<std::string> arguments;

    // 通过循环遍历命令行参数的数量（argc_），并从 argv_ 数组中获取每个参数的值。
    for (size_t argument_index = 0; argument_index < argc_; argument_index++)
    {
        // 将每个命令行参数添加到 arguments 向量中。
        // emplace_back() 方法用于在向量末尾插入新元素，这里将每个命令行参数作为一个新的字符串插入到向量中。
        arguments.emplace_back(argv_[argument_index]);
    }

    // 创建了一个新的 CommandLine 对象，并将 arguments 向量作为参数传递给 CommandLine 类的构造函数。
    // 然后将指向这个新创建对象的指针赋值给 communication_ 成员变量。
    communication_ = new CommandLine(arguments);
}

// 根据命令行参数执行相应的操作，并在执行过程中处理异常情况，确保程序的正常运行。
void Application::run(const size_t argc, char** argv)
{
    // 将传递给 run() 函数的 命令行参数数量和参数数组 赋值给 Application 类的成员变量 argc_ 和 argv_，以便后续使用。
    argc_ = argc;
    argv_ = argv;

    // 调用了 printLicense() 和 Progress::init() 函数，分别用于 打印许可证信息 和 初始化进度。
    printLicense();
    Progress::init();

    // 检查命令行参数数量是否小于 2。如果小于 2，则打印帮助信息并退出程序。
    if (argc < 2)
    {
        printHelp();
        exit(1);
    }

#ifdef ARCUS
    // 如果定义了 ARCUS 宏，并且命令行参数为 "connect"，则调用 connect() 函数。否则，继续执行后面的条件判断。
    if (stringcasecompare(argv[1], "connect") == 0)
    {
        connect();
    }
    else
#endif // ARCUS
        if (stringcasecompare(argv[1], "slice") == 0)
        {
            slice();
        }
        else if (stringcasecompare(argv[1], "help") == 0)
        {
            printHelp();
        }
        else
        {
            spdlog::error("Unknown command: {}", argv[1]);
            printCall();
            printHelp();
            exit(1);
        }

    // 检查 communication_ 是否为 nullptr，如果是，则表示未打开通信通道，无法进行切片操作，直接退出程序。
    if (! communication_)
    {
        // No communication channel is open any more, so either:
        //- communication failed to connect, or
        //- the engine was called with an unknown command or a command that doesn't connect (like "help").
        // In either case, we don't want to slice.
        exit(0);
    }

    // 调用 startThreadPool() 函数，启动线程池。
    startThreadPool(); // Start the thread pool

    // 使用 while 循环，不断检查通信通道是否还有待处理的切片任务，如果有，则调用 communication_->sliceNext() 函数处理下一个切片任务。
    while (communication_->hasSlice())
    {
        communication_->sliceNext();
    }
}

// 根据传入的参数决定要创建的工作线程数量，并启动相应数量的线程池。
// 它接受一个整数参数 nworkers，用于指定要启动的工作线程数量。
void Application::startThreadPool(int nworkers)
{
    // 声明了一个变量 nthreads，用于存储要创建的线程数量。
    size_t nthreads;

    // 检查传入的 nworkers 是否小于等于 0。
    // 如果是，则表示未指定要启动的工作线程数量，这时会根据硬件并发支持的线程数来决定创建的线程数量。
    // 通常会减去一个线程，以留出主线程。
    if (nworkers <= 0)
    {
        // 如果 nworkers 小于等于 0，并且 thread_pool_ 不为空，则直接返回，保持先前创建的线程池不变。
        if (thread_pool_)
        {
            return; // Keep the previous ThreadPool
        }

        // 获取硬件并发支持的线程数，并将其减去一个作为要创建的线程数量。
        nthreads = std::thread::hardware_concurrency() - 1;
    }
    else
    {
        // 如果传入的 nworkers 大于 0，则将其减去一个作为要创建的线程数量。
        nthreads = nworkers - 1; // Minus one for the main thread
    }

    // 检查当前是否已经存在线程池，并且其线程数是否与要创建的线程数量相同。如果是，则直接返回，保持先前创建的线程池不变。
    if (thread_pool_ && thread_pool_->thread_count() == nthreads)
    {
        return; // Keep the previous ThreadPool
    }

    // 如果不满足保持先前线程池不变的条件，则释放先前创建的线程池对象，并根据计算得到的线程数量创建一个新的线程池对象。
    delete thread_pool_;
    thread_pool_ = new ThreadPool(nthreads);
}

} // namespace cura
