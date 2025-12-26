
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <sstream>

enum class LogLevel { INFO, WARN, ERROR, DEBUG };
struct Logger {
    static void log(const std::string& node, LogLevel level, const std::string& msg) {
        std::cout << "[" << node << "] ";
        switch(level) {
            case LogLevel::INFO:  std::cout << "[INFO] "; break;
            case LogLevel::WARN:  std::cout << "\033[33m[WARN]\033[0m "; break;
            case LogLevel::ERROR: std::cout << "\033[31m[ERROR]\033[0m "; break;
            case LogLevel::DEBUG: std::cout << "\033[36m[DEBUG]\033[0m "; break;
        }
        std::cout << msg << std::endl;
    }
};

template <typename T>
class Topic {
    std::vector<std::function<void(T)>> subscribers;
public:
    void publish(T val) { for (auto& cb : subscribers) cb(val); }
    void subscribe(std::function<void(T)> cb) { subscribers.push_back(cb); }
};

class SystemManager {
public:
    static std::string current_mode;
    static std::vector<std::function<void(std::string)>> on_transition;
    static void set_mode(const std::string& m) {
        if (current_mode != m) {
            std::cout << "[SYS] Transitioning to: " << m << std::endl;
            current_mode = m;
            for (auto& cb : on_transition) cb(m);
        }
    }
};
std::string SystemManager::current_mode = "Init";
std::vector<std::function<void(std::string)>> SystemManager::on_transition;

class CommandCenter;
extern CommandCenter* CommandCenter_inst;
class SensorArray;
extern SensorArray* SensorArray_inst;
class LoggerNode;
extern LoggerNode* LoggerNode_inst;

class CommandCenter {
public:
    std::string name = "CommandCenter";
    std::string current_state = "Init";
    Topic<int> heartbeat;
    Topic<bool> system_ready;
    bool activate() {
        { std::stringstream _ss; _ss << "Activating System Phase: Startup"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        SystemManager::set_mode("Startup");
        return true;
        return true;
    }
    bool goLive() {
        { std::stringstream _ss; _ss << "System GO LIVE"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
        SystemManager::set_mode("Active");
        return true;
        return true;
    }
    void init() {
        { std::stringstream _ss; _ss << "Initializing CommandCenter..."; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        CommandCenter_inst->activate();
    }
    void onSystemChange(std::string sys_mode) {
        if (sys_mode == "Startup") {
            { std::stringstream _ss; _ss << "CommandCenter in Startup. Signaling sensors..."; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
            this->system_ready.publish(true);
            CommandCenter_inst->goLive();
        }
    }
};
CommandCenter* CommandCenter_inst = nullptr;

class SensorArray {
public:
    std::string name = "SensorArray";
    std::string current_state = "Init";
    Topic<double> reading;
    bool beginSampling(bool ready) {
        { std::stringstream _ss; _ss << "Sensors online. Starting sampling..."; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->reading.publish(42.0);
        return true;
        return true;
    }
    void init() {
    }
    void onSystemChange(std::string sys_mode) {
        if (sys_mode == "Active") {
            { std::stringstream _ss; _ss << "This is a test"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
            { std::stringstream _ss; _ss << "This is a test"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
            { std::stringstream _ss; _ss << "This is a test"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
            { std::stringstream _ss; _ss << "This is a test"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
            { std::stringstream _ss; _ss << "SensorArray detected Active system state."; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
            std::cout << "Sampling at high frequency..." << std::endl;
        }
    }
};
SensorArray* SensorArray_inst = nullptr;

class LoggerNode {
public:
    std::string name = "LoggerNode";
    std::string current_state = "Init";
    Topic<std::string> disk_status;
    bool recordData(double val) {
        std::cout << "DISK RECORD: " << val << std::endl;
        return true;
        return true;
    }
    void init() {
        this->disk_status.publish("READY");
    }
    void onSystemChange(std::string sys_mode) {
    }
};
LoggerNode* LoggerNode_inst = nullptr;

int main() {
    CommandCenter_inst = new CommandCenter();
    SensorArray_inst = new SensorArray();
    LoggerNode_inst = new LoggerNode();
    SystemManager::on_transition.push_back([](std::string m) { CommandCenter_inst->onSystemChange(m); });
    SystemManager::on_transition.push_back([](std::string m) { SensorArray_inst->onSystemChange(m); });
    SystemManager::on_transition.push_back([](std::string m) { LoggerNode_inst->onSystemChange(m); });
    CommandCenter_inst->system_ready.subscribe([=](auto val) {
        SensorArray_inst->beginSampling(val);
    });
    SensorArray_inst->reading.subscribe([=](auto val) {
        LoggerNode_inst->recordData(val);
    });
    CommandCenter_inst->init();
    SensorArray_inst->init();
    LoggerNode_inst->init();
    std::cout << "--- Rivet System Started ---" << std::endl;
    while(true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
    return 0;
}
