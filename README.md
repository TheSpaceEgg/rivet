# Rivet Language — Syntax & Design Notes (WIP)

Rivet is a domain-specific language for lightweight robotics systems, intended as a simpler alternative to ROS for tightly-coupled robotic deployments.

---

## 1. File Structure

A Rivet source file consists only of top-level declarations.

Valid top-level declarations:

- `systemMode`
- `node`
- `mode`

Anything else at the top level is a syntax error.

---

## 2. Indentation Rules

Rivet uses indentation to define scope.

- Indentation is significant
- Blocks are introduced by a newline followed by indentation
- There are no braces for blocks
- Indentation must be consistent within a block

---

## 3. Comments

### Line comments

```rivet
// This is a comment
```

- Runs from `//` to end of line
- Ignored by the lexer

### Block comments

```rivet
/*
  Multi-line comment
*/
```

- Not nestable
- Newlines count for line/column tracking
- Unterminated block comments are an error

---

## 4. `systemMode` Declarations

System modes are global mode names that may be implemented by multiple nodes.

```rivet
systemMode survey
systemMode diagnostics
```

Rules:

- Must be declared at top level
- Names must be identifiers
- Reserved mode names must not be declared as system modes

Reserved modes:

- `boot`
- `normal`
- `idle`
- `fault`
- `shutdown`

---

## 5. `node` Declarations

Nodes represent hardware or logical components.

```rivet
node imu : I2C{bus:3, addr:0x49}
node motor : PWM{pin:12, freq:20000}
```

Syntax:

```rivet
node <nodeName> : <nodeType>{ optional raw config }
```

Rules:

- Node names must be unique
- Node types are identifiers
- Config blocks are currently treated as raw text blobs (parsing deferred)

---

## 6. `mode` Declarations

Modes define behaviour for a node under a named operating condition.

```rivet
mode imu->normal
  readSensor()
  publishData()
```

---

## 7. Mode Name Types

There are two kinds of mode names.

### 7.1 Identifier mode names

```rivet
mode imu->normal
```

- Must be a reserved mode OR a declared `systemMode`

### 7.2 Node-local string mode names

```rivet
mode imu->"calibrate"
```

- Exist only for that node
- Do not require global declaration
- Must be referenced exactly

---

## 8. Mode Bodies

Mode bodies are indented blocks of statements.

Currently supported statements:

- function calls only

```rivet
mode imu->boot
  initIMU()
  setRange(2000)
```

Rules:

- One statement per line
- Must be a function call
- No control flow yet

---

## 9. Mode Delegation (`do`)

Modes may delegate to another mode.

```rivet
mode imu->fault do normal
```

Rules:

- `do` is single-line only
- A `mode ... do ...` declaration cannot have an indented body
- Delegation targets must resolve to:
  - a reserved mode
  - a declared `systemMode`
  - or a node-local string mode on the same node

Invalid:

```rivet
mode imu->idle do calibrate
  sleepMs(100)
```

---

## 10. Delegation Resolution Rules

When resolving `do <target>`:

1. If `<target>` is quoted, it must exist as a node-local string mode
2. If `<target>` is an identifier, it must be:
   - a reserved mode
   - a systemMode
   - OR a node-local string mode with matching text

Example:

```rivet
mode imu->"calibrate"
  startCal()

mode imu->idle do calibrate
```

---

## 11. Error Reporting

Diagnostics include:

- filename
- line and column
- coloured severity (`error`, `warning`, `note`)
- source line
- caret position

---

## 12. Validation Rules

The validator enforces:

- Duplicate node declarations
- Unknown nodes in mode declarations
- Duplicate mode bindings per node
- Unknown identifier mode names
- Invalid delegation targets
- Circular delegation chains per node

---

## 13. Current Limitations

Rivet does not yet support:

- variables
- expressions
- conditionals
- loops
- returns
- message passing
- runtime execution

This is deliberate; the front-end is being built first.

---

## 14. Planned Next Steps

### Near-term

- `publish` statements
- request/response semantics
- runtime execution skeleton
- system-wide mode switching

### Mid-term

- typed messages
- transport abstraction (I2C, SPI, CAN, UART, PWM)
- code generation

### Long-term

- static scheduling
- distributed execution
- tooling (syntax highlighting, LSP)

---

## 15. Design Philosophy

- Explicit over implicit
- Compile-time errors preferred over runtime ambiguity
- Minimal runtime complexity
- Clear mapping to embedded systems
- No magic behaviour
