
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
class MathHarness;
extern MathHarness* MathHarness_inst;
class ModeWatcher;
extern ModeWatcher* ModeWatcher_inst;
class LoggerNode;
extern LoggerNode* LoggerNode_inst;

class CommandCenter {
public:
    std::string name = "CommandCenter";
    std::string current_state = "Init";
    Topic<int> hb;
    Topic<bool> ready;
    Topic<bool> gate;
    Topic<int> ping;
    Topic<double> fping;
    Topic<std::string> msg;
    Topic<int> stage;
    bool boot();
    bool toActive();
    bool toDiag();
    bool toSafe();
    bool flipGate(bool on);
    void init();
    void onSystemChange(std::string sys_mode);
    void onLocalChange();
    void set_state(const std::string& st);
    void __rivet_unsub_sys_listeners();
    void __rivet_unsub_local_listeners();
};
CommandCenter* CommandCenter_inst = nullptr;

class MathHarness {
public:
    std::string name = "MathHarness";
    std::string current_state = "Init";
    Topic<bool> done;
    Topic<int> score;
    int __rivet_sub_m2_l0 = -1;
    int __rivet_sub_m3_l0 = -1;
    bool onReady(bool v);
    bool testBooleans();
    bool testArithmetic();
    bool testBuiltins();
    bool onPing(int x);
    bool onFloatPing(double x);
    bool onStage(int s);
    bool onSysMsg(std::string m);
    void init();
    void onSystemChange(std::string sys_mode);
    void onLocalChange();
    void set_state(const std::string& st);
    void __rivet_unsub_sys_listeners();
    void __rivet_unsub_local_listeners();
};
MathHarness* MathHarness_inst = nullptr;

class ModeWatcher {
public:
    std::string name = "ModeWatcher";
    std::string current_state = "Init";
    Topic<int> seen;
    bool onMsg(std::string s);
    bool onGate(bool b);
    bool onDone(bool b);
    bool onScore(int v);
    void init();
    void onSystemChange(std::string sys_mode);
    void onLocalChange();
    void set_state(const std::string& st);
    void __rivet_unsub_sys_listeners();
    void __rivet_unsub_local_listeners();
};
ModeWatcher* ModeWatcher_inst = nullptr;

class LoggerNode {
public:
    std::string name = "LoggerNode";
    std::string current_state = "Init";
    Topic<int> lines;
    bool hbSeen(int v);
    bool readySeen(bool v);
    bool pingSeen(int v);
    bool fpingSeen(double v);
    bool msgSeen(std::string s);
    bool gateSeen(bool b);
    bool stageSeen(int v);
    bool mhDone(bool b);
    bool mhScore(int v);
    bool mwSeen(int v);
    void init();
    void onSystemChange(std::string sys_mode);
    void onLocalChange();
    void set_state(const std::string& st);
    void __rivet_unsub_sys_listeners();
    void __rivet_unsub_local_listeners();
};
LoggerNode* LoggerNode_inst = nullptr;

bool CommandCenter::boot() {
    { std::stringstream _ss; _ss << "CommandCenter.boot()"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    SystemManager::set_mode("Startup");
    return true;
}

bool CommandCenter::toActive() {
    { std::stringstream _ss; _ss << "CommandCenter.toActive()"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
    SystemManager::set_mode("Active");
    return true;
}

bool CommandCenter::toDiag() {
    { std::stringstream _ss; _ss << "CommandCenter.toDiag()"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    SystemManager::set_mode("Diagnostics");
    return true;
}

bool CommandCenter::toSafe() {
    { std::stringstream _ss; _ss << "CommandCenter.toSafe()"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    SystemManager::set_mode("Safe");
    return true;
}

bool CommandCenter::flipGate(bool on) {
    { std::stringstream _ss; _ss << "CommandCenter.flipGate(on=" << on << ")"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    this->gate.publish(on);
    return true;
}

void CommandCenter::__rivet_unsub_sys_listeners() {
}

void CommandCenter::__rivet_unsub_local_listeners() {
}

void CommandCenter::init() {
    this->__rivet_unsub_sys_listeners();
    this->__rivet_unsub_local_listeners();
        { std::stringstream _ss; _ss << "Init: kick off"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        CommandCenter_inst->boot();
}

void CommandCenter::onSystemChange(std::string sys_mode) {
    this->__rivet_unsub_sys_listeners();
    if (sys_mode == "Startup") {
        { std::stringstream _ss; _ss << "SYS Startup entered"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->hb.publish(1);
        this->msg.publish("sys: Startup");
        this->gate.publish(false);
        this->stage.publish(0);
        this->ready.publish(true);
        this->ping.publish(0);
        this->ping.publish(1);
        this->ping.publish(2);
        this->fping.publish(0.25);
        this->fping.publish(1.25);
        this->stage.publish(1);
        CommandCenter_inst->flipGate(true);
        CommandCenter_inst->toActive();
    }
    if (sys_mode == "Active") {
        { std::stringstream _ss; _ss << "SYS Active entered"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->hb.publish(2);
        this->msg.publish("sys: Active");
        this->ping.publish(3);
        this->ping.publish(4);
        this->ping.publish(-1);
        this->fping.publish(-0.5);
        this->fping.publish(0.75);
        this->stage.publish(2);
        CommandCenter_inst->flipGate(false);
        CommandCenter_inst->toDiag();
    }
    if (sys_mode == "Diagnostics") {
        { std::stringstream _ss; _ss << "SYS Diagnostics entered"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->hb.publish(9);
        this->msg.publish("sys: Diagnostics");
        this->stage.publish(0);
        this->ping.publish(8);
        this->ping.publish(9);
        this->fping.publish(0.10);
        this->fping.publish(0.90);
        MathHarness_inst->set_state("LocalA");
        MathHarness_inst->set_state("LocalB");
        CommandCenter_inst->toSafe();
    }
    if (sys_mode == "Safe") {
        { std::stringstream _ss; _ss << "SYS Safe entered (end)"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
        this->hb.publish(3);
        this->msg.publish("sys: Safe");
        this->gate.publish(false);
        this->stage.publish(0);
    }
}

void CommandCenter::onLocalChange() {
    this->__rivet_unsub_local_listeners();
}

void CommandCenter::set_state(const std::string& st) {
    this->current_state = st;
    this->onLocalChange();
}

bool MathHarness::onReady(bool v) {
    { std::stringstream _ss; _ss << "MathHarness.onReady(v=" << v << ")"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    if (v) {
        { std::stringstream _ss; _ss << "READY true -> running full test battery"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        { std::stringstream _ss; _ss << "MathHarness: tests complete"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->done.publish(true);
    } else {
        { std::stringstream _ss; _ss << "READY false (unexpected)"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    return true;
}

bool MathHarness::testBooleans() {
    { std::stringstream _ss; _ss << "TEST: booleans + precedence"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    if (true) {
        { std::stringstream _ss; _ss << "if true PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "if true FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((!false)) {
        { std::stringstream _ss; _ss << "not false PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "not false FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((true || (false && false))) {
        { std::stringstream _ss; _ss << "true or (false and false) PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "precedence FAIL (case 1)"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((false || (true && false))) {
        { std::stringstream _ss; _ss << "false or true and false => false PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "precedence FAIL (case 2)"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if (((!true) || true)) {
        { std::stringstream _ss; _ss << "(not true) or true PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "(not true) or true FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((!(true && false))) {
        { std::stringstream _ss; _ss << "not (true and false) PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "not (true and false) FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if (((true && true) && (true || false))) {
        { std::stringstream _ss; _ss << "compound boolean PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "compound boolean FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    return true;
}

bool MathHarness::testArithmetic() {
    { std::stringstream _ss; _ss << "TEST: arithmetic + comparisons"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    if ((((2 + 3) * 4) == 20)) {
        { std::stringstream _ss; _ss << "(2+3)*4 == 20 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "(2+3)*4 == 20 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if (((2 + (3 * 4)) == 14)) {
        { std::stringstream _ss; _ss << "2+3*4 precedence PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "2+3*4 precedence FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if (((7 % 3) == 1)) {
        { std::stringstream _ss; _ss << "7%3 == 1 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "7%3 == 1 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((10 == 10)) {
        { std::stringstream _ss; _ss << "10==10 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "10==10 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((10 != 11)) {
        { std::stringstream _ss; _ss << "10!=11 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "10!=11 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((-5 < 0)) {
        { std::stringstream _ss; _ss << "-5 < 0 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "-5 < 0 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((1.5 < 2.0)) {
        { std::stringstream _ss; _ss << "1.5 < 2.0 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "1.5 < 2.0 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((2.5 >= 2.5)) {
        { std::stringstream _ss; _ss << "2.5 >= 2.5 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "2.5 >= 2.5 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((3.14 == 3.14)) {
        { std::stringstream _ss; _ss << "3.14 == 3.14 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "3.14 == 3.14 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((3.14 != 3.15)) {
        { std::stringstream _ss; _ss << "3.14 != 3.15 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "3.14 != 3.15 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    return true;
}

bool MathHarness::testBuiltins() {
    { std::stringstream _ss; _ss << "TEST: builtins (min/max/clamp) via asserts"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    if ((std::min<double>((double)(10), (double)(20)) == 10)) {
        { std::stringstream _ss; _ss << "min(10,20)==10 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "min(10,20)==10 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((std::max<double>((double)(10), (double)(20)) == 20)) {
        { std::stringstream _ss; _ss << "max(10,20)==20 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "max(10,20)==20 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((std::min<double>((double)(1.5), (double)(2.0)) == 1.5)) {
        { std::stringstream _ss; _ss << "min(1.5,2.0)==1.5 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "min(1.5,2.0)==1.5 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((std::max<double>((double)(1.5), (double)(2.0)) == 2.0)) {
        { std::stringstream _ss; _ss << "max(1.5,2.0)==2.0 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "max(1.5,2.0)==2.0 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((std::clamp<double>((double)(5), (double)(0), (double)(10)) == 5)) {
        { std::stringstream _ss; _ss << "clamp(5,0,10)==5 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "clamp(5,0,10)==5 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((std::clamp<double>((double)(-5), (double)(0), (double)(10)) == 0)) {
        { std::stringstream _ss; _ss << "clamp(-5,0,10)==0 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "clamp(-5,0,10)==0 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((std::clamp<double>((double)(50), (double)(0), (double)(10)) == 10)) {
        { std::stringstream _ss; _ss << "clamp(50,0,10)==10 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "clamp(50,0,10)==10 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    if ((std::clamp<double>((double)(2.5), (double)(0.0), (double)(10.0)) == 2.5)) {
        { std::stringstream _ss; _ss << "clamp(2.5,0.0,10.0)==2.5 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "clamp(2.5,0.0,10.0)==2.5 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    return true;
}

bool MathHarness::onPing(int x) {
    { std::stringstream _ss; _ss << "MathHarness.onPing(x=" << x << ")"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    if ((((x % 2) == 0) && (x >= 0))) {
        { std::stringstream _ss; _ss << "ping even and non-negative"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "ping odd or negative"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
    }
    if (((x < 0) || (x == 0))) {
        { std::stringstream _ss; _ss << "ping <= 0 branch"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "ping > 0 branch"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    }
    return true;
}

bool MathHarness::onFloatPing(double x) {
    { std::stringstream _ss; _ss << "MathHarness.onFloatPing(x=" << x << ")"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    if ((x < 0.0)) {
        { std::stringstream _ss; _ss << "fping negative"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "fping non-negative"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    }
    if ((std::clamp<double>((double)(x), (double)(0.0), (double)(1.0)) >= 0.0)) {
        { std::stringstream _ss; _ss << "clamp(x,0,1) >= 0 PASS"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    } else {
        { std::stringstream _ss; _ss << "clamp(x,0,1) >= 0 FAIL"; Logger::log(this->name, LogLevel::ERROR, _ss.str()); }
    }
    return true;
}

bool MathHarness::onStage(int s) {
    { std::stringstream _ss; _ss << "MathHarness.onStage(s=" << s << ")"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    if ((s == 0)) {
        this->set_state("Idle");
    } else if ((s == 1)) {
        this->set_state("LocalA");
    } else if ((s == 2)) {
        this->set_state("LocalB");
    } else {
        this->set_state("Idle");
    }
    return true;
}

bool MathHarness::onSysMsg(std::string m) {
    std::cout << "MathHarness saw sys msg: " << m << std::endl;
    return true;
}

void MathHarness::__rivet_unsub_sys_listeners() {
}

void MathHarness::__rivet_unsub_local_listeners() {
    if (__rivet_sub_m2_l0 != -1) { CommandCenter_inst->ping.unsubscribe(__rivet_sub_m2_l0); __rivet_sub_m2_l0 = -1; }
    if (__rivet_sub_m3_l0 != -1) { CommandCenter_inst->fping.unsubscribe(__rivet_sub_m3_l0); __rivet_sub_m3_l0 = -1; }
}

void MathHarness::init() {
    this->__rivet_unsub_sys_listeners();
    this->__rivet_unsub_local_listeners();
        { std::stringstream _ss; _ss << "MathHarness Init"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
}

void MathHarness::onSystemChange(std::string sys_mode) {
    this->__rivet_unsub_sys_listeners();
}

void MathHarness::onLocalChange() {
    this->__rivet_unsub_local_listeners();
    if (this->current_state == "Idle") {
        { std::stringstream _ss; _ss << "MathHarness local Idle"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
        this->score.publish(0);
    }
    if (this->current_state == "LocalA") {
        if (__rivet_sub_m2_l0 == -1) __rivet_sub_m2_l0 = CommandCenter_inst->ping.subscribe([this](auto val) {
            this->onPing(val);
        });
        { std::stringstream _ss; _ss << "MathHarness local LocalA"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->score.publish(10);
    }
    if (this->current_state == "LocalB") {
        if (__rivet_sub_m3_l0 == -1) __rivet_sub_m3_l0 = CommandCenter_inst->fping.subscribe([this](auto val) {
            this->onFloatPing(val);
        });
        { std::stringstream _ss; _ss << "MathHarness local LocalB"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
        this->score.publish(20);
    }
}

void MathHarness::set_state(const std::string& st) {
    this->current_state = st;
    this->onLocalChange();
}

bool ModeWatcher::onMsg(std::string s) {
    { std::stringstream _ss; _ss << "ModeWatcher.onMsg(s=" << s << ")"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    return true;
}

bool ModeWatcher::onGate(bool b) {
    { std::stringstream _ss; _ss << "ModeWatcher.onGate(b=" << b << ")"; Logger::log(this->name, LogLevel::WARN, _ss.str()); }
    return true;
}

bool ModeWatcher::onDone(bool b) {
    { std::stringstream _ss; _ss << "ModeWatcher.onDone(b=" << b << ")"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    if (b) {
        this->seen.publish(1);
    } else {
        this->seen.publish(0);
    }
    return true;
}

bool ModeWatcher::onScore(int v) {
    { std::stringstream _ss; _ss << "ModeWatcher.onScore(v=" << v << ")"; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    return true;
}

void ModeWatcher::__rivet_unsub_sys_listeners() {
}

void ModeWatcher::__rivet_unsub_local_listeners() {
}

void ModeWatcher::init() {
    this->__rivet_unsub_sys_listeners();
    this->__rivet_unsub_local_listeners();
}

void ModeWatcher::onSystemChange(std::string sys_mode) {
    this->__rivet_unsub_sys_listeners();
    if (sys_mode == "Active") {
        { std::stringstream _ss; _ss << "ModeWatcher sees system Active"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    }
    if (sys_mode == "Safe") {
        { std::stringstream _ss; _ss << "ModeWatcher sees system Safe"; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    }
}

void ModeWatcher::onLocalChange() {
    this->__rivet_unsub_local_listeners();
}

void ModeWatcher::set_state(const std::string& st) {
    this->current_state = st;
    this->onLocalChange();
}

bool LoggerNode::hbSeen(int v) {
    { std::stringstream _ss; _ss << "LOG hb=" << v; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    return true;
}

bool LoggerNode::readySeen(bool v) {
    { std::stringstream _ss; _ss << "LOG ready=" << v; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    return true;
}

bool LoggerNode::pingSeen(int v) {
    { std::stringstream _ss; _ss << "LOG ping=" << v; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    return true;
}

bool LoggerNode::fpingSeen(double v) {
    { std::stringstream _ss; _ss << "LOG fping=" << v; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    return true;
}

bool LoggerNode::msgSeen(std::string s) {
    std::cout << "LOG msg: " << s << std::endl;
    return true;
}

bool LoggerNode::gateSeen(bool b) {
    { std::stringstream _ss; _ss << "LOG gate=" << b; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    return true;
}

bool LoggerNode::stageSeen(int v) {
    { std::stringstream _ss; _ss << "LOG stage=" << v; Logger::log(this->name, LogLevel::DEBUG, _ss.str()); }
    return true;
}

bool LoggerNode::mhDone(bool b) {
    { std::stringstream _ss; _ss << "LOG math.done=" << b; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    return true;
}

bool LoggerNode::mhScore(int v) {
    { std::stringstream _ss; _ss << "LOG math.score=" << v; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    return true;
}

bool LoggerNode::mwSeen(int v) {
    { std::stringstream _ss; _ss << "LOG watch.seen=" << v; Logger::log(this->name, LogLevel::INFO, _ss.str()); }
    return true;
}

void LoggerNode::__rivet_unsub_sys_listeners() {
}

void LoggerNode::__rivet_unsub_local_listeners() {
}

void LoggerNode::init() {
    this->__rivet_unsub_sys_listeners();
    this->__rivet_unsub_local_listeners();
}

void LoggerNode::onSystemChange(std::string sys_mode) {
    (void)sys_mode;
    return;
}

void LoggerNode::onLocalChange() {
    this->__rivet_unsub_local_listeners();
}

void LoggerNode::set_state(const std::string& st) {
    this->current_state = st;
    this->onLocalChange();
}

int main() {
    CommandCenter_inst = new CommandCenter();
    MathHarness_inst = new MathHarness();
    ModeWatcher_inst = new ModeWatcher();
    LoggerNode_inst = new LoggerNode();
    SystemManager::on_transition.push_back([](std::string m) { CommandCenter_inst->onSystemChange(m); });
    SystemManager::on_transition.push_back([](std::string m) { MathHarness_inst->onSystemChange(m); });
    SystemManager::on_transition.push_back([](std::string m) { ModeWatcher_inst->onSystemChange(m); });
    CommandCenter_inst->ready.subscribe([=](auto val) {
        MathHarness_inst->onReady(val);
    });
    CommandCenter_inst->ping.subscribe([=](auto val) {
        MathHarness_inst->onPing(val);
    });
    CommandCenter_inst->fping.subscribe([=](auto val) {
        MathHarness_inst->onFloatPing(val);
    });
    CommandCenter_inst->stage.subscribe([=](auto val) {
        MathHarness_inst->onStage(val);
    });
    CommandCenter_inst->msg.subscribe([=](auto val) {
        MathHarness_inst->onSysMsg(val);
    });
    CommandCenter_inst->msg.subscribe([=](auto val) {
        ModeWatcher_inst->onMsg(val);
    });
    CommandCenter_inst->gate.subscribe([=](auto val) {
        ModeWatcher_inst->onGate(val);
    });
    MathHarness_inst->done.subscribe([=](auto val) {
        ModeWatcher_inst->onDone(val);
    });
    MathHarness_inst->score.subscribe([=](auto val) {
        ModeWatcher_inst->onScore(val);
    });
    CommandCenter_inst->hb.subscribe([=](auto val) {
        LoggerNode_inst->hbSeen(val);
    });
    CommandCenter_inst->ready.subscribe([=](auto val) {
        LoggerNode_inst->readySeen(val);
    });
    CommandCenter_inst->ping.subscribe([=](auto val) {
        LoggerNode_inst->pingSeen(val);
    });
    CommandCenter_inst->fping.subscribe([=](auto val) {
        LoggerNode_inst->fpingSeen(val);
    });
    CommandCenter_inst->msg.subscribe([=](auto val) {
        LoggerNode_inst->msgSeen(val);
    });
    CommandCenter_inst->gate.subscribe([=](auto val) {
        LoggerNode_inst->gateSeen(val);
    });
    CommandCenter_inst->stage.subscribe([=](auto val) {
        LoggerNode_inst->stageSeen(val);
    });
    MathHarness_inst->done.subscribe([=](auto val) {
        LoggerNode_inst->mhDone(val);
    });
    MathHarness_inst->score.subscribe([=](auto val) {
        LoggerNode_inst->mhScore(val);
    });
    ModeWatcher_inst->seen.subscribe([=](auto val) {
        LoggerNode_inst->mwSeen(val);
    });
    CommandCenter_inst->init();
    MathHarness_inst->init();
    ModeWatcher_inst->init();
    LoggerNode_inst->init();
    std::cout << "--- Rivet System Started ---" << std::endl;
    while(true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
    return 0;
}
