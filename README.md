# 🚦 Traffic Intersection Simulator (Operating Systems Project)

A multi-process C simulation of a 4-way traffic intersection, developed for the **Operating Systems** course. 

This project demonstrates the management of concurrent processes, inter-process communication (IPC), and synchronization mechanisms in a Linux environment.

## 📋 Project Overview

The application simulates a scenario where a "Garage" generates cars with specific destinations, and an "Intersection" (Incrocio) manager decides the crossing order based on traffic rules (simulated priority).

### Key Concepts Implemented
*   **Process Management:** Usage of `fork()` to create independent processes for the Intersection, Garage, and individual Cars.
*   **Shared Memory (`shm_open`, `mmap`):** Used to share synchronization primitives (semaphores) between unrelated processes.
*   **POSIX Semaphores (`sem_t`):** Used to synchronize the movement of cars and the intersection manager (Green light/Red light logic and Handshakes).
*   **Pipes:** Unnamed pipes used for communication between the Garage (producer) and the Intersection (consumer) to transmit car directions.
*   **File I/O:** Logging events to `incrocio.txt` and `auto.txt`.

## ⚙️ Architecture

The system is composed of three main entities:

1.  **Garage:** Generates a batch of cars, assigns them random directions, and sends this data to the Intersection via a Pipe. It then forks the car processes.
2.  **Incrocio (Intersection):** Reads incoming car data from the Pipe. It calculates which car has the right of way (using `GetNextCar`) and signals the specific semaphore to let that car pass.
3.  **Automobile (Car):** Waits on its specific semaphore. Once signaled (Green light), it "crosses", logs its passage to a file, and sends an acknowledgment (ACK) back to the Intersection.

## 🛠️ Prerequisites

*   **OS:** Linux (or WSL)
*   **Compiler:** GCC
*   **Libraries:** `pthread` and `rt` (Real-time extensions for shared memory)

## 🚀 Compilation and Execution

To compile the project, use the following command (linking pthread and rt libraries is required):

```bash
gcc main.c -o main -lpthread -lrt
```

To run the simulation:

```bash
./main
```

## 📂 Project Structure

*   `main.c`: Contains the main logic, process creation, and IPC setup.
*   `incrocio.h`: Header file containing helper functions for direction logic (`EstraiDirezione`, `GetNextCar`).
*   `incrocio.txt`: Log file generated during runtime, recording intersection decisions.
*   `auto.txt`: Log file generated during runtime, recording car movements.

## 📝 Logging

The program generates two log files during execution:
*   **incrocio.txt**: Records when the intersection manager allows a car to pass.
*   **auto.txt**: Records the details of the cars (ID and direction) as they cross.

## 👤 Author

**Alberto Mancini**  
University of Perugia  
Course: Operating Systems
