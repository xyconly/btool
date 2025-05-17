#pragma once
#include <iostream>
#include <mutex>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <fstream>
#if defined(__linux__)
    #include <unistd.h>
    #include <sys/syscall.h>
#elif defined(__APPLE__)
    #include <pthread.h>
    #include <stdint.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

class Logger {
public:
    enum class Level {
        Debug,
        Info,
        Warning,
        Error
    };

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

#if defined(__linux__)
    inline pid_t get_tid() {
        return static_cast<pid_t>(::syscall(SYS_gettid));
    }
#elif defined(__APPLE__)
    inline uint64_t get_tid() {
        uint64_t tid;
        pthread_threadid_np(NULL, &tid);
        return tid;
    }
#elif defined(_WIN32)
    inline DWORD get_tid() {
        return GetCurrentThreadId();
    }
#else
    inline uint64_t get_tid() {
        // Fallback: not accurate but better than nothing
        return std::hash<std::thread::id>{}(std::this_thread::get_id());
    }
#endif

    void set_log_file(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_file_path_ = path;
        log_file_.open(path, std::ios::app);  // 追加模式
        if (!log_file_.is_open()) {
            std::cerr << "Failed to open log file: " << path << std::endl;
        }
    }

    template<typename... Args>
    void log(Level level, Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "[" << timestamp() << "] "
            << "[" << level_to_string(level) << "] [" << get_tid() << "] ";
    
        (oss << ... << std::forward<Args>(args)) << '\n';
    
        std::string message = oss.str();
        std::cout << message;
    
        if (log_file_.is_open()) {
            log_file_ << message;
            log_file_.flush();  // 可选：确保及时写入
        }
    }

    // 简写接口
    template<typename... Args> void debug(Args&&... args)  { log(Level::Debug,   std::forward<Args>(args)...); }
    template<typename... Args> void info(Args&&... args)   { log(Level::Info,    std::forward<Args>(args)...); }
    template<typename... Args> void warn(Args&&... args)   { log(Level::Warning, std::forward<Args>(args)...); }
    template<typename... Args> void error(Args&&... args)  { log(Level::Error,   std::forward<Args>(args)...); }


    // 析构时关闭后台刷新线程
    ~Logger() {
        stop_ = true;
        cv_.notify_one();
        if (flush_thread_.joinable()) {
            flush_thread_.join();
        }
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
    
private:
    Logger() {
        flush_thread_ = std::thread([this]() {
            std::unique_lock<std::mutex> lock(cv_mutex_);
            while (!stop_) {
                cv_.wait_for(lock, std::chrono::seconds(3));
                if (stop_) break;
                std::lock_guard<std::mutex> lg(mutex_);
                std::cout.flush();  // 强制刷新
            }
        });
    }

    std::string timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        auto t = system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
    
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setw(3) << std::setfill('0') << now_ms.count();
        return oss.str();
    }

    const char* level_to_string(Level level) {
        switch (level) {
            case Level::Debug:   return "DEBUG";
            case Level::Info:    return "INFO";
            case Level::Warning: return "WARN";
            case Level::Error:   return "ERROR";
            default:             return "UNKNOWN";
        }
    }

    std::mutex mutex_;
    std::mutex cv_mutex_;      // 控制线程等待
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
    std::thread flush_thread_;
    std::ofstream log_file_;  // 新增：文件输出流
    std::string log_file_path_;  // 可选：保存路径用于重新打开
};

// 日志流对象
class LogStream {
public:
    LogStream(Logger::Level level) : level_(level) {}

    ~LogStream() {
        Logger::instance().log(level_, oss_.str());
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        oss_ << value;
        return *this;
    }

private:
    Logger::Level level_;
    std::ostringstream oss_;
};

// 宏定义
#define LOG(level) LogStream(Logger::Level::level)

#define CHECK(cond) \
    if (!(cond)) { \
        Logger::instance().log(Logger::Level::Error, "CHECK failed: ", #cond, " at ", __FILE__, ":", __LINE__); \
        std::abort(); \
    }