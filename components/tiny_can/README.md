# tiny_can diagrams

Diagrams of how `tiny_can` works inside. The API itself is described in `include/tiny_can.h`.

Colours: in the sequence diagrams the background says where code runs (blue task context, red interrupt (ISR) context, purple ESP-IDF driver, green hardware); in the class diagram dark blue is the module, light blue its public types, grey a private one and purple the driver; the error states go from green to red.

## Structure

C has no classes, but a module that keeps file-scope `static` state and exposes functions behaves like a single object (a singleton). The class diagram below draws it that way:

- `tiny_can` is the module. Its `static` variables are private attributes (`-`), its public functions are `+` and its `static` functions are `-`.
- The structs (`<<struct>>`) and the enum (`<<enumeration>>`) are drawn as classes with data and no methods.
- A dashed arrow means "uses as a parameter" (a dependency). A filled diamond means "owns" (composition): the module allocates the TX slots and frees nothing, because they live as long as the program. An open diamond means "holds copies of" (aggregation).

```mermaid
classDiagram
    direction LR
    class tiny_can {
        <<module>>
        -twai_node_handle_t s_node
        -tx_slot_t s_slots[n]
        -QueueHandle_t s_free_slots
        -QueueHandle_t s_rx
        -TaskHandle_t s_recover_task
        -uint32_t counters
        +tiny_can_start(cfg) esp_err_t
        +tiny_can_send(id, data, len, timeout_ms) esp_err_t
        +tiny_can_receive(out, timeout_ms) bool
        +tiny_can_get_stats(out) void
        +tiny_can_state_name(state) string
        -on_tx_done() ISR
        -on_rx_done() ISR
        -on_state_change() ISR
        -recover_task() task
    }
    class tiny_can_config_t {
        <<struct>>
        +int tx_gpio
        +int rx_gpio
        +uint32_t bitrate
        +int intr_priority
        +bool self_test
        +uint32_t filter_id
        +uint32_t filter_mask
        +uint32_t rx_queue_len
        +uint32_t tx_queue_len
    }
    class tiny_can_frame_t {
        <<struct>>
        +uint32_t id
        +uint8_t len
        +uint8_t data[8]
    }
    class tiny_can_stats_t {
        <<struct>>
        +tiny_can_state_t state
        +uint16_t tec
        +uint16_t rec
        +uint32_t tx_ok
        +uint32_t tx_failed
        +uint32_t tx_rejected
        +uint32_t rx
        +uint32_t rx_dropped
        +uint32_t bus_errors
        +uint32_t bus_off
    }
    class tiny_can_state_t {
        <<enumeration>>
        ERROR_ACTIVE
        ERROR_WARNING
        ERROR_PASSIVE
        BUS_OFF
    }
    class tx_slot_t {
        <<struct, private>>
        +twai_frame_t frame
        +uint8_t data[8]
    }
    class twai_node {
        <<ESP-IDF driver>>
        +twai_node_transmit()
        +twai_node_receive_from_isr()
        +twai_node_recover()
        +twai_node_get_info()
    }
    tiny_can ..> tiny_can_config_t : start() reads once
    tiny_can ..> tiny_can_stats_t : get_stats() fills
    tiny_can "1" o-- "rx_queue_len" tiny_can_frame_t : s_rx queue
    tiny_can "1" *-- "tx_queue_len + 1" tx_slot_t : TX pool
    tiny_can --> twai_node : s_node
    tiny_can_stats_t --> tiny_can_state_t : state
    twai_node ..> tx_slot_t : holds a pointer until TX done
    style tiny_can fill:#1f6feb,stroke:#0b3d91,color:#ffffff
    style tiny_can_config_t fill:#d0e7ff,stroke:#1f6feb,color:#0b1f33
    style tiny_can_frame_t fill:#d0e7ff,stroke:#1f6feb,color:#0b1f33
    style tiny_can_stats_t fill:#d0e7ff,stroke:#1f6feb,color:#0b1f33
    style tiny_can_state_t fill:#d0e7ff,stroke:#1f6feb,color:#0b1f33
    style tx_slot_t fill:#ececec,stroke:#6e7781,color:#24292f
    style twai_node fill:#e8d5ff,stroke:#8250df,color:#2a1052
```

`tx_slot_t` is private to `tiny_can.c`. It's drawn because it's the reason the module exists: the driver keeps a pointer to each queued frame, so the frame must live in memory the module owns until the driver reports it sent.

## Sending a frame

`tiny_can_send()` returns as soon as the driver has queued the frame. The frame goes on the bus later, and the TX-done interrupt hands its slot back.

```mermaid
sequenceDiagram
    autonumber
    box rgba(31,111,235,0.15) task context
        participant App as app task
        participant TC as tiny_can_send()
        participant Pool as s_free_slots
    end
    box rgba(130,80,223,0.15) ESP-IDF driver
        participant Drv as TWAI driver
    end
    box rgba(26,127,55,0.15) hardware
        participant Bus as controller and bus
    end
    box rgba(207,34,46,0.15) ISR context
        participant ISR as on_tx_done (ISR)
    end

    App->>TC: tiny_can_send(id, data, len, timeout_ms)
    TC->>Pool: xQueueReceive(idx, timeout_ms)
    alt a slot is free in time
        Pool-->>TC: idx
        TC->>TC: copy id, len, data into s_slots[idx]
        TC->>Drv: twai_node_transmit(&slot.frame, 0)
        Drv-->>TC: ESP_OK, queued
        TC-->>App: ESP_OK
        Note over App,TC: the frame is queued, not sent yet
        Drv->>Bus: frame out when the bus is idle
        Note over Bus: arbitration, then ACK from another node. Without an ACK the controller retries and the slot stays taken
        Bus->>ISR: TX done
        ISR->>ISR: tx_ok++ (or tx_failed++)
        ISR->>Pool: xQueueSendFromISR(idx)
    else no slot free in time
        TC-->>App: ESP_ERR_TIMEOUT, tx_rejected++
    end
```

## Receiving a frame

The driver gives out a received frame only inside its interrupt callback, so the callback copies it into a FreeRTOS queue and the task reads it from there.

```mermaid
sequenceDiagram
    autonumber
    box rgba(31,111,235,0.15) task context
        participant App as app task
        participant Q as s_rx queue
    end
    box rgba(207,34,46,0.15) ISR context
        participant ISR as on_rx_done (ISR)
    end
    box rgba(130,80,223,0.15) ESP-IDF driver
        participant Drv as TWAI driver
    end
    box rgba(26,127,55,0.15) hardware
        participant Bus as controller and bus
    end

    App->>Q: tiny_can_receive(&f, timeout_ms)
    Note over App: blocked until a frame or the timeout
    Bus->>Drv: frame received, passed the acceptance filter
    Drv->>ISR: on_rx_done
    ISR->>Drv: twai_node_receive_from_isr(&rx)
    Drv-->>ISR: id, dlc, data
    alt the queue has room
        ISR->>Q: xQueueSendFromISR(frame), rx++
        Q-->>App: frame, returns true
    else the queue is full
        ISR->>ISR: rx_dropped++
    end
    Note over ISR,App: the ISR returns "woken", so the app task runs right after the interrupt if it has the higher priority
```

## Error states and bus-off recovery

Every CAN node counts its own errors: the TEC (transmit error counter) goes up by 8 for each failed send, the REC (receive error counter) for each bad frame received, and both go down by 1 for each good frame. The counters decide the state:

```mermaid
stateDiagram-v2
    direction LR
    [*] --> ERROR_ACTIVE : tiny_can_start()
    ERROR_ACTIVE --> ERROR_WARNING : TEC or REC ≥ 96
    ERROR_WARNING --> ERROR_ACTIVE : both < 96
    ERROR_WARNING --> ERROR_PASSIVE : TEC or REC ≥ 128
    ERROR_PASSIVE --> ERROR_WARNING : both < 128
    ERROR_PASSIVE --> BUS_OFF : TEC ≥ 256
    BUS_OFF --> ERROR_ACTIVE : recovery, counters reset to 0
    classDef active fill:#1a7f37,stroke:#0f5323,color:#ffffff
    classDef warning fill:#d4a72c,stroke:#7d4e00,color:#1f1300
    classDef passive fill:#e16f24,stroke:#953800,color:#ffffff
    classDef busoff fill:#cf222e,stroke:#82071e,color:#ffffff
    class ERROR_ACTIVE active
    class ERROR_WARNING warning
    class ERROR_PASSIVE passive
    class BUS_OFF busoff
```

A node that is the only one on the bus gets no ACKs and stops at a TEC of about 127, just short of passive: ACK errors alone don't push a node past passive. Bus-off needs real bus errors, such as a short or a wrong bitrate.

Bus-off is the one state the node doesn't leave by itself. `tiny_can` starts the recovery from a task, because the driver call isn't allowed in an interrupt:

```mermaid
sequenceDiagram
    autonumber
    box rgba(26,127,55,0.15) hardware
        participant Bus as controller and bus
    end
    box rgba(207,34,46,0.15) ISR context
        participant ISR as on_state_change (ISR)
    end
    box rgba(130,80,223,0.15) ESP-IDF driver
        participant Drv as TWAI driver
    end
    box rgba(31,111,235,0.15) task context
        participant R as recover_task
        participant App as app task
    end

    Bus->>ISR: state change to BUS_OFF (TEC ≥ 256)
    ISR->>ISR: bus_off++
    ISR->>R: vTaskNotifyGiveFromISR
    App->>Drv: tiny_can_send() while bus-off
    Drv-->>App: ESP_ERR_INVALID_STATE, tx_rejected++
    R->>R: vTaskDelay(1000 ms)
    R->>Drv: twai_node_recover()
    Note over Drv: also resets the driver's bus error count, so stats.bus_errors restarts at 0
    Drv->>Bus: start recovery
    Note over Bus: waits for 128 × 11 recessive bits, about 2.8 ms on a quiet bus at 500 kbit/s
    Bus->>ISR: state change to ERROR_ACTIVE
    Drv->>Bus: frames queued before bus-off are sent now
    Note over ISR,App: TEC and REC are 0 again and sending works
```
