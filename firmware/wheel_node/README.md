# Wheel node firmware

The firmware for one ESP32-C3 wheel node: one motor through a DRV8833, one Hall encoder, and text commands over the USB port. Build and flash: `docs/dev-setup.md`. Hardware-free logic lives in `components/` (`components/README.md`); this directory holds the code that touches pins, interrupts and FreeRTOS.

| File | What it does |
|---|---|
| `main/main.c` | `app_main`: starts the modules below, the 1 Hz LED blink, and a placeholder 100 Hz telemetry sampler |
| `main/pins.h` | every GPIO the firmware uses, and nowhere else |
| `main/console.c` | the serial commands (`HELP`, `STATUS`, `ENC`, `DUTY`, `STOP`, …), one `OK`/`ERR` reply per line |
| `main/telemetry.c` | `T,` lines on the USB port through a queue, so a slow port never blocks the sampler |
| `main/encoder.c` | encoder pins and interrupt handler; decoding in `tiny_qdec` |
| `main/drive.c` | motor PWM, driver pins and the 100 Hz drive loop; run logic in `tiny_motor` |

## Motor drive

Two parts: `tiny_motor` (component) decides, `drive.c` (here) acts. Two tasks share the motor state: the console task changes it on `DUTY` and `STOP`, and the drive task advances it every 10 ms. They only meet inside the `s_lock` critical section in `drive.c`. Only the drive task writes the driver pins.

### States

```mermaid
stateDiagram-v2
    [*] --> IDLE: boot, tiny_motor_init()
    IDLE --> RUN: DUTY, tiny_motor_run()
    RUN --> RUN: DUTY again (new target, ramps from the current duty)
    RUN --> IDLE: run time over and the ramp down reached 0
    RUN --> IDLE: STOP, tiny_motor_stop() (no ramp)
    RUN --> FAULT: stall (duty at least 30 %, at most 1 count per period, for 300 ms)
    FAULT --> FAULT: DUTY refused
    FAULT --> IDLE: STOP
```

| State | Driver | Pins |
|---|---|---|
| `IDLE` | asleep (STBY low) | both inputs low: coast |
| `RUN` | awake (STBY high) | slow-decay PWM from `tiny_motor_pins()` |
| `FAULT` | asleep (STBY low) | both inputs low: coast; latched until `STOP` (or a reset) |

### Boot

```mermaid
sequenceDiagram
    participant main as main.c
    participant drive as drive.c
    participant motor as tiny_motor
    participant hw as GPIO, LEDC, DRV8833
    participant task as drive task

    Note over hw: until configured, the DRV8833's pull-downs hold STBY, AIN1, AIN2 low
    main->>main: encoder_start()
    main->>drive: drive_start()
    drive->>hw: STBY level low, then STBY as output (never drives high)
    drive->>hw: LEDC timer: 20 kHz, 10 bit
    drive->>hw: AIN1, AIN2 channels at duty 0 (low: coast)
    drive->>motor: tiny_motor_init(), state IDLE
    drive->>task: xTaskCreate(drive_task, priority 6)
```

### A `DUTY` run

```mermaid
sequenceDiagram
    actor you
    participant console as console task
    participant drive as drive.c
    participant motor as tiny_motor
    participant task as drive task
    participant hw as LEDC, DRV8833

    you->>console: DUTY 50 5
    console->>console: parse, range check (-100..100 %, 1..30 s)
    console->>drive: drive_run(50, 5, &run)
    drive->>motor: tiny_motor_run(500 permille, 500 steps), under s_lock
    Note over motor: RUN, target 500
    motor-->>drive: true
    drive-->>console: run = 50 (the target after the cap)
    console-->>you: OK duty=50 for 5 s
    Note over you,console: DUTY 100 would reply OK duty=80 for 5 s (capped from 100)
    loop every 10 ms (vTaskDelayUntil)
        task->>task: encoder_read(), counts = change since last period
        task->>motor: tiny_motor_step(counts), under s_lock
        motor-->>task: duty (moves at most 50 permille toward target)
        task->>hw: apply(RUN, duty): tiny_motor_pins() to both channels, then STBY high
    end
    Note over motor: duty 50, 100 ... 500 in 100 ms, hold, after 500 steps target 0, ramp down to 0, then IDLE
    task->>hw: apply(IDLE, 0): STBY low, then both inputs low
```

### `STOP`

```mermaid
sequenceDiagram
    actor you
    participant console as console task
    participant drive as drive.c
    participant motor as tiny_motor
    participant task as drive task
    participant hw as LEDC, DRV8833

    you->>console: STOP
    console->>drive: drive_stop()
    drive->>motor: tiny_motor_stop(), under s_lock
    Note over motor: IDLE, duty 0, no ramp, a FAULT is cleared
    console-->>you: OK stopped
    Note over task: next period, within 10 ms
    task->>motor: tiny_motor_step(counts)
    motor-->>task: 0 (not RUN)
    task->>hw: apply(IDLE, 0): STBY low, then both inputs low (coast)
```

### Stall trip

```mermaid
sequenceDiagram
    actor you
    participant console as console task
    participant motor as tiny_motor
    participant task as drive task
    participant hw as LEDC, DRV8833

    Note over hw: running at 30 % or more, the wheel is blocked
    loop every 10 ms while counts stay within -1..1
        task->>motor: tiny_motor_step(counts)
        Note over motor: stall_steps + 1, reset by any real movement
    end
    Note over motor: 30th stalled period: FAULT, duty 0
    task->>hw: apply(FAULT, 0): STBY low, then both inputs low
    you->>console: DUTY 50
    console-->>you: ERR stall fault latched, STOP clears it
    you->>console: STATUS
    console-->>you: OK ... drive=stall duty=0
    you->>console: STOP
    console-->>you: OK stopped
    Note over motor: IDLE, DUTY accepted again
```
