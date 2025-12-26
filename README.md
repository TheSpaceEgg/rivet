# Rivet Language Reference

Rivet is a domain-specific language (DSL) for designing **Reactive State Machines** for robotics and autonomous systems. It compiles high-level coordination logic into asynchronous, thread-safe C++.

## 1. System Structure
A Rivet program is composed of **System Modes**, **Nodes**, and **Modes**.

### System Modes
System modes define the global states of the entire application.
```rivet
systemMode Startup
systemMode Active
systemMode Shutdown
```

---

## 2. Nodes
Nodes represent isolated processes, hardware drivers, or software modules.

```rivet
node controller FlightCore : Autopilot
  topic altitude = "nav/alt" : float   // Published data stream
  topic state = "sys/state" : string
  
  // Handlers for remote requests (RPC)
  onRequest arm() -> bool
    log info "Arming System"
    transition system "Active"
    return true
    
  // Internal private functions
  func internalCalibrate() -> bool
    return true
```

### Node Keywords
* `controller`: Marks a node authorized to trigger global `system` transitions.
* `ignore system`: Prevents a node from automatically reacting to global system state changes.
* `topic`: Defines an output data stream (`int`, `float`, `string`, `bool`).
* `onRequest`: A public method reachable by other nodes via `request`.
* `func`: A private method for internal node logic.

---

## 3. Communication Architecture
Rivet utilizes a hybrid **Publish-Subscribe** and **Request-Response** model.

### Topics (Pub/Sub)
Nodes subscribe to data from other nodes using `onListen`.
```rivet
node Perception : Lidar
  // Automatically triggers adjust() when FlightCore publishes to 'altitude'
  onListen FlightCore.altitude do adjust()
```

### Requests (RPC)
Nodes can explicitly trigger actions on other nodes.
```rivet
mode FlightCore->Init
  request Perception.scan()         // Standard request
  request silent Motors.calibrate() // Request without automatic logging
```

---

## 4. State Management (Modes)
Modes define logic blocks that execute at specific state intersections.

### Init Mode
The entry point for a node; runs exactly once upon instantiation.
```rivet
mode FlightCore->Init
  altitude.publish(0.0)
  log "FlightCore initialized"
```

### System Reaction Modes
Logic that triggers automatically when the global `SystemManager` transitions.
```rivet
mode Motors->Shutdown
  request Motors.instantStop()
  log warn "Motors reacting to global Shutdown"
```

### Internal Scoped Modes
Logic that is active only when the node itself is in a specific internal state.
```rivet
mode Camera->Scanning
  onListen Sensors.trigger do snap()
```

---

## 5. Built-in Commands

| Command | Description | Example |
| :--- | :--- | :--- |
| `log` | Formatted console output with node context | `log warn "Battery at {val}%"` |
| `print` | Raw standard output | `print "DISK RECORD: {val}"` |
| `transition` | Changes node or global system state | `transition system "Active"` |
| `publish` | Broadens data to a topic | `state.publish("READY")` |
| `return` | Exits a function with a return value | `return true` |

### Log Levels
Supported levels: `info`, `warn`, `error`, `debug`.

---

## 6. Type System
Rivet is statically typed for safety:
* `int`: 32-bit integers (`42`, `-7`)
* `float`: 64-bit floating point (`3.14159`)
* `string`: UTF-8 text strings (`"Hello World"`)
* `bool`: Logic values (`true`, `false`)

---

## 7. Toolchain Workflow

1. **Authoring**: Write your logic in a `.rv` file.
2. **Compilation**: `rivet.exe <script>.rv --cpp` 
3. **C++ Build**: `g++ <script>.rv.cpp -o <app_name> -std=c++17 -pthread`
4. **Deployment**: Run the generated binary on your target hardware.