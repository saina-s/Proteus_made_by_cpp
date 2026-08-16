# Proteus Clone

A simple circuit editor and simulator written in C++ using SDL2.

This project was developed for the Object-Oriented Programming course at Sharif University of Technology. The goal was to build a simplified version of Proteus while practicing OOP concepts and separating the simulation logic from the graphical interface.

## Features

The program includes a schematic editor with:

* grid and snap-to-grid
* zoom and canvas panning
* component placement and movement
* rotation and horizontal/vertical mirroring
* single and multiple selection
* editable component properties
* searchable component library
* 90-degree wire routing
* junctions and wire crossings
* undo and redo
* save/load projects
* BMP export

It also includes a basic analog and digital simulation engine. During simulation, wire colors change based on their state, and interactive components such as switches, push buttons and potentiometers can be changed while the circuit is running.

## Components

The current component library includes:

**Sources**

* Ground
* DC Voltage Source
* Battery
* Clock Generator

**Analog / Passive**

* Resistor
* Capacitor
* Inductor
* Potentiometer

**Interactive**

* Switch
* Push Button
* LED
* Seven-Segment Display

**Digital**

* AND
* OR
* NOT
* XOR
* NAND
* D Flip-Flop

**Advanced**

* ADC
* DAC
* Microcontroller
* External Memory

**Peripherals**

* 16x2 LCD
* 4x4 Keypad

**Measurement**

* Voltage Probe
* Voltmeter
* Ammeter
* Two-channel Oscilloscope

## Simulation

The simulator supports both analog and digital circuits.

Digital signals can be `HIGH`, `LOW` or `Undefined`. Logic gates also have configurable propagation delays.

The analog part handles components such as resistors, capacitors, inductors, voltage sources and potentiometers.

Simulation can be run normally, paused, stopped or advanced step by step.

### Simulation controls

| Key  | Action |
| ---- | ------ |
| `F5` | Run    |
| `F6` | Pause  |
| `F7` | Stop   |
| `F8` | Step   |

During simulation:

* red wires represent HIGH
* blue wires represent LOW
* gray wires represent floating or undefined signals

## Microcontroller

The project also contains a small educational microcontroller model with:

* 256 bytes of internal RAM
* accumulator
* program counter
* Port A and Port B
* Intel HEX file loading
* basic instruction execution

Supported instructions include:

* `MOV`
* `ADD`
* `JMP`
* `SETB`
* `CLR`

The microcontroller can interact with components such as the LCD, keypad and external memory through its I/O ports.

## Controls

Most controls are also available from the toolbar or the in-program help menu.

| Shortcut   | Action                     |
| ---------- | -------------------------- |
| `Ctrl + N` | New project                |
| `Ctrl + O` | Open project               |
| `Ctrl + S` | Save project               |
| `Ctrl + E` | Export BMP                 |
| `Ctrl + Z` | Undo                       |
| `Ctrl + Y` | Redo                       |
| `W`        | Wire mode                  |
| `J`        | Place junction             |
| `P`        | Voltage probe              |
| `G`        | Run DRC                    |
| `R`        | Rotate selected component  |
| `H`        | Mirror horizontally        |
| `V`        | Mirror vertically          |
| `Delete`   | Delete selection           |
| `O`        | Open selected oscilloscope |
| `F1`       | Help                       |

For navigating the canvas:

* use the mouse wheel to zoom
* middle-drag to pan
* `Space + Left Drag` can also be used to pan
* dragging on empty space creates a multi-selection box

## Saving Projects

Projects can be saved and opened again from inside the program.

The default project file extension is:

```text
.pcsdl
```

The start menu also keeps a list of recent projects.

## Building

### Requirements

* C++17
* SDL2
* SDL2_ttf

We developed and tested the project using Visual Studio 2022 on Windows.

If you use vcpkg, SDL2 and SDL2_ttf can be installed with:

```powershell
vcpkg install sdl2:x64-windows
vcpkg install sdl2-ttf:x64-windows
```

Then enable vcpkg integration:

```powershell
vcpkg integrate install
```

Open the project in Visual Studio, use an **x64** configuration and make sure the C++ language standard is set to **C++17 or newer**.

Then build and run the project normally.

## Code Structure

The final version is kept in a single C++ source file, but the code itself is divided into separate classes and systems for components, circuit connections, simulation and the UI.

Some of the main classes are:

* `Component`
* `GenericComponent`
* `Pin`
* `Wire`
* `Junction`
* `CircuitDocument`
* `SimulationEngine`
* `Microcontroller`
* `ProteusApp`

Inheritance and polymorphism are used for the different circuit components, while the simulation engine handles the behavior of the circuit independently from most of the UI code.

## Contributors

* **Saina Soltani** — Backend, circuit simulation, component logic, MCU and advanced components
* **Farnoosh Chogani** — Frontend, SDL2 graphics, UI, event handling and user interactions

