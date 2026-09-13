## Modifications and Contributions

The original task switcher was extended with operating system mechanisms for
inter-process synchronization and communication.

### Modified Components

- **Task Control Block (TCB)**: extended with process states required to support
  waiting and synchronization mechanisms.
- **UART communication**: substantially redesigned, replacing the original
  polling-based implementation with an interrupt-driven approach managed by
  the operating system.

### New Components

- **Semaphores**: implemented from scratch to provide synchronization between tasks.
- **Message queues**: implemented from scratch to enable communication between tasks.

### Main Programs

- **`main.c`**: runs on the Arduino Mega 2560. It creates 20 tasks, starts the
  operating system, and orchestrates 10 producer tasks and 10 consumer tasks.
  Semaphores are used to synchronize task execution and ensure ordered output.
- **`main_pc.c`**: runs on the host PC and configures the `termios` interface
  for serial communication with the Arduino, providing mechanisms to read from
  and write to the board.

### Original Components

The following components of the original implementation were left unchanged:

- Timer management
- Scheduler
- TCB list management
- Context-switching mechanism (AVR Assembly)
