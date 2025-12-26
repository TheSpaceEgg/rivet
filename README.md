# Rivet Language Reference

Rivet is a domain-specific language (DSL) for defining robust, state-machine-driven robotics architectures. It enforces strict separation of concerns, type safety, and authority hierarchies.

## 1. System Definition
Define the high-level lifecycle states of the robot. `Init`, `Normal`, and `Shutdown` are reserved and implicitly exist.

```rivet
systemMode Launch
systemMode Emergency
```

## 2. Nodes
Nodes are the primary components (sensors, actuators, logic).

**Standard Node**
```rivet
node Sensor : Lidar
  // ...
```

**Controller Node**
Only controllers can trigger `transition system`.
```rivet
node controller Brain : MainCPU
```

**Isolated Node**
Ignores system mode changes (e.g., recorders).
```rivet
node BlackBox : Recorder ignore system
```

## 3. Data & Communication
**Topics** (Typed data streams)
```rivet
topic altitude = "env/alt" : float
topic status   = "sys/stat" : string
```

**Functions**
* `onRequest`: Public API (callable by others).
* `func`: Private helper (internal only).

```rivet
onRequest calibrate() -> bool
  return true

func internalCheck(val: int) -> bool
  return true
```

**Actions**
```rivet
altitude.publish(10.5)
request OtherNode.calibrate()
request silent OtherNode.save() // Ignore return value
```

## 4. Event Handling (Listeners)
Nodes react to topics published by others.

**Global Listener** (Active in all modes)
```rivet
// Inline
onListen Brain.status handleState(s: string)
  internalCheck(1)

// Delegated (Routes to function)
onListen Brain.abort do stopProcess()
```

**Scoped Listener** (Active only in specific mode)
```rivet
mode Sensor->Calibrating
  onListen Brain.abort do stopProcess()
```

## 5. State Management (Modes)
Logic is grouped into Modes.

**Explicit Modes**
Define behavior for a specific node in a specific state.
```rivet
// Standard Mode
mode Sensor->Calibrating
  transition "Idle"

// System-Bound Mode (Runs when system is in 'Emergency')
mode Thrusters->Emergency
  request Thrusters.stop()
```

**Isolation**
A specific mode can ignore system state changes.
```rivet
mode Thrusters->Firing ignore system
```

## 6. Transitions
**Local Transition**: Switches the *current node* to a new local state.
```rivet
transition "Idle"
```

**System Transition**: Switches the *entire robot* to a new system mode. (Controllers only).
```rivet
transition system "Emergency"
```

## 7. Example Script
```rivet
systemMode Mission

node controller Brain : CPU
  topic state = "sys/state" : string

  onRequest start() -> bool
    transition system "Mission"
    return true

node Sensor : Lidar
  topic data = "env/data" : float

  // 1. Reserved Init Mode
  mode Sensor->Init
    data.publish(0.0)

  // 2. System-Specific Behavior
  mode Sensor->Mission
    onListen Brain.state do processState()

  func processState(s: string) -> bool
    data.publish(1.0)
    return true
```