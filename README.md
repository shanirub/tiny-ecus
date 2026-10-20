# tiny-ecus

A 4WD skid-steer RC car built as a small distributed system. Each wheel has its own ESP32-C3 node, and all nodes talk over a CAN bus, the way ECUs do in a real car. An ESP32-CAM acts as the gateway: it takes remote commands and later streams video over WiFi. It is a hobby project that puts learning ahead of usefulness.

## Goals

- Four independent wheel nodes that drive together smoothly, using closed-loop speed control with encoder feedback.
- Corner lights that stay in sync across nodes, through clock synchronization over CAN.
- Fail-safe behavior: the car stops on its own if a node or the remote link goes silent, and before the battery runs too low.
- Control traffic (CAN) kept separate from video traffic (WiFi).

## Documentation

- [Project overview](docs/can-rc-car-project-overview.md): goals, learning goals, principles and non-goals.
- [Parts list](docs/can-rc-car-parts-list-final.md): hardware bill of materials.
- [Development](docs/dev-setup.md): install the toolchain, build, flash, run the host tests and the bench tools.
- [Roadmap and current status](.planning/can-rc-car/task_plan.md): phases, decisions and next step. This is the source of truth for project status.
