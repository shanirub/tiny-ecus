# CAN RC Car — Project Overview

Sep 27, 2026 · @Shani Rubovitch

## What it is

A 4WD skid-steer RC car built as a small distributed system: each wheel has its own microcontroller, and all nodes talk over a CAN bus, the way ECUs do in a real car. It is a hobby project that puts learning and fun ahead of usefulness.

## Goals

The finished car is a calm indoor explorer, driven remotely, whose wheels and lights behave as one car even though they are separate nodes.

- Four wheel nodes drive together smoothly, with no twitching or veering from timing mismatches.
- Each wheel knows its actual speed, not just the commanded one.
- Blinkers and lights at all four corners stay in sync, like a real car.
- The car fails safe: if a node or the remote link goes silent, it stops on its own.
- The car protects its own battery and stops before it runs too low.
- A camera streams video over WiFi, separate from the control traffic. The ESP32-CAM's exact role is still open to ideas.

## Learning goals

Learning is the main point; the car is the vehicle for it.

- **Distributed systems:** clock synchronization between independent nodes (the step after NTP), coordinated actions across nodes, failure detection and fail-safe behavior.
- **CAN bus:** how the protocol works, message priority through IDs, bus topology and termination.
- **Protocol design:** designing our own application layer on top of raw CAN, rather than a full standard stack.
- **Embedded control:** motor control, encoder feedback and closed-loop speed control on ESP32-C3 nodes.
- **Architecture:** separating the control plane (CAN) from the data plane (WiFi video), and gateway patterns.
- **Power and hardware:** Li-ion battery safety, protection, fusing and power distribution for a mobile device.

## Principles and non-goals

- **Learning over importance:** silly is fine, as long as it teaches something.
- **Simple mechanics:** skid-steer only; no Ackermann steering or mecanum wheels.
- **Safety in hardware first:** limits like voltage and current are enforced by parts, not only by firmware.
- **Phased:** each stage works on its own before the next one starts.
- **Not a race car:** speed and performance are not goals.

## Possible future directions

These are ideas for later, not commitments.

- Image processing, for example help when reversing.
- An Android app to control the car.
- A Raspberry Pi as a bench tool for sniffing and logging the CAN bus.
- Custom PCBs for the wheel nodes, replacing loose modules.
