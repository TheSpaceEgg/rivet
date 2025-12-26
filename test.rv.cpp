
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <utility>

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
    struct Sub {
        int id;
        std::function<void(T)> cb;
    };
    std::vector<Sub> subscribers;
    int next_id = 1;
public:
    void publish(T val) {
        for (auto& s : subscribers) {
            if (s.cb) s.cb(val);
        }
    }

    // Returns a subscription handle that can be used to unsubscribe.
    int subscribe(std::function<void(T)> cb) {
        int id = next_id++;
        subscribers.push_back(Sub{id, std::move(cb)});
        return id;
    }

    void unsubscribe(int id) {
        subscribers.erase(
            std::remove_if(subscribers.begin(), subscribers.end(),
                           [&](const Sub& s) { return s.id == id; }),
            subscribers.end());
    }
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
    Topic<bool> system_ready;
    bool activate() {
        { std::stringstream _ss; _ss << "CommandCenter.activate()"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        SystemManager::set_mode("Startup");
        return true;
    }
    void __rivet_unsub_sys_listeners() {
    }
    void __rivet_unsub_local_listeners() {
    }
    void init() {
        this->__rivet_unsub_sys_listeners();
        this->__rivet_unsub_local_listeners();
        { std::stringstream _ss; _ss << "Init: kicking off test"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->system_ready.publish(true);
        CommandCenter_inst->activate();
    }
    void onSystemChange(std::string sys_mode) {
        this->__rivet_unsub_sys_listeners();
        if (sys_mode == "Startup") {
            { std::stringstream _ss; _ss << "Startup mode entered"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
            SystemManager::set_mode("Active");
        }
    }
    void onLocalChange() {
        this->__rivet_unsub_local_listeners();
    }
};
CommandCenter* CommandCenter_inst = nullptr;

class SensorArray {
public:
    std::string name = "SensorArray";
    std::string current_state = "Init";
    Topic<double> reading;
    Topic<bool> ok;
    bool beginSampling(bool ready) {
        { std::stringstream _ss; _ss << "SensorArray.beginSampling() ready=" << ready; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->reading.publish(47.0);
        if (true) {
            { std::stringstream _ss; _ss << "if true: PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "if true: FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((!false)) {
            { std::stringstream _ss; _ss << "if not false: PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "if not false: FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if (ready) {
            { std::stringstream _ss; _ss << "ready is true"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "ready is false (unexpected in this script)"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
        }
        if ((!ready)) {
            { std::stringstream _ss; _ss << "not ready branch should not run"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "not ready correctly skipped"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        }
        if ((1 < 2)) {
            { std::stringstream _ss; _ss << "1 < 2 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "1 < 2 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((2 <= 2)) {
            { std::stringstream _ss; _ss << "2 <= 2 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "2 <= 2 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((3 > 4)) {
            { std::stringstream _ss; _ss << "3 > 4 should be false"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "3 > 4 correctly false"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        }
        if ((5 >= 5)) {
            { std::stringstream _ss; _ss << "5 >= 5 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "5 >= 5 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((10 == 10)) {
            { std::stringstream _ss; _ss << "10 == 10 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "10 == 10 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((10 != 11)) {
            { std::stringstream _ss; _ss << "10 != 11 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "10 != 11 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((true || (false && false))) {
            { std::stringstream _ss; _ss << "true or (false and false) => true PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "precedence test 1 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if (((true || false) && false)) {
            { std::stringstream _ss; _ss << "(true or false) and false should be FALSE (this branch should not run)"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "(true or false) and false correctly evaluated to false"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        }
        if (((!true) || true)) {
            { std::stringstream _ss; _ss << "(not true) or true => true PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "not precedence test FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((((2 + 3) * 4) == 20)) {
            { std::stringstream _ss; _ss << "(2+3)*4 == 20 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "(2+3)*4 == 20 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if (((21 / 2) > 10)) {
            { std::stringstream _ss; _ss << "21/2 > 10 PASS (integer or float division depends on your typing)"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "21/2 > 10 was false (check division semantics)"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
        }
        if (((7 % 3) == 1)) {
            { std::stringstream _ss; _ss << "7 % 3 == 1 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "7 % 3 == 1 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((ready && (47.0 > 40.0))) {
            { std::stringstream _ss; _ss << "ready and reading high -> entering nested test"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
            if ((1 == 2)) {
                { std::stringstream _ss; _ss << "nested: impossible branch"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
            } else {
                { std::stringstream _ss; _ss << "nested: else branch PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
            }
        } else if ((ready && (47.0 <= 40.0))) {
            { std::stringstream _ss; _ss << "elif: shouldn't hit (47 <= 40 false)"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "else: shouldn't hit (ready true and 47>40 true)"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        if ((3 < 0)) {
            { std::stringstream _ss; _ss << "elif-chain: branch 1 should not run"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        } else if ((3 < 2)) {
            { std::stringstream _ss; _ss << "elif-chain: branch 2 should not run"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        } else if ((3 == 3)) {
            { std::stringstream _ss; _ss << "elif-chain: branch 3 PASS (3 == 3)"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        } else {
            { std::stringstream _ss; _ss << "elif-chain: else should not run"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
        }
        this->ok.publish(true);
        this->current_state = "Done";
        return true;
    }
    void __rivet_unsub_sys_listeners() {
    }
    void __rivet_unsub_local_listeners() {
    }
    void init() {
        this->__rivet_unsub_sys_listeners();
        this->__rivet_unsub_local_listeners();
    }
    void onSystemChange(std::string sys_mode) {
        this->__rivet_unsub_sys_listeners();
        if (sys_mode == "Active") {
            { std::stringstream _ss; _ss << "SensorArray sees system Active"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        }
        if (sys_mode == "Done") {
            { std::stringstream _ss; _ss << "SensorArray finished conditional tests"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
        }
    }
    void onLocalChange() {
        this->__rivet_unsub_local_listeners();
    }
};
SensorArray* SensorArray_inst = nullptr;

class LoggerNode {
public:
    std::string name = "LoggerNode";
    std::string current_state = "Init";
    bool recordData(double val) {
        std::cout << "Logger saw reading: " << val << std::endl;
        return true;
    }
    bool recordOk(bool v) {
        { std::stringstream _ss; _ss << "Logger saw ok=" << v; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        return true;
    }
    void __rivet_unsub_sys_listeners() {
    }
    void __rivet_unsub_local_listeners() {
    }
    void init() {
        this->__rivet_unsub_sys_listeners();
        this->__rivet_unsub_local_listeners();
        { std::stringstream _ss; _ss << "LoggerNode init"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    }
    void onSystemChange(std::string sys_mode) {
        (void)sys_mode;
        return;
    }
    void onLocalChange() {
        this->__rivet_unsub_local_listeners();
    }
};
LoggerNode* LoggerNode_inst = nullptr;

int main() {
    CommandCenter_inst = new CommandCenter();
    SensorArray_inst = new SensorArray();
    LoggerNode_inst = new LoggerNode();
    SystemManager::on_transition.push_back([](std::string m) { CommandCenter_inst->onSystemChange(m); });
    SystemManager::on_transition.push_back([](std::string m) { SensorArray_inst->onSystemChange(m); });
    CommandCenter_inst->system_ready.subscribe([=](auto val) {
        SensorArray_inst->beginSampling(val);
    });
    SensorArray_inst->reading.subscribe([=](auto val) {
        LoggerNode_inst->recordData(val);
    });
    SensorArray_inst->ok.subscribe([=](auto val) {
        LoggerNode_inst->recordOk(val);
    });
    CommandCenter_inst->init();
    SensorArray_inst->init();
    LoggerNode_inst->init();
    std::cout << "--- Rivet System Started ---" << std::endl;
    while(true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
    return 0;
}
