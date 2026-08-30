# Elektor Junior ESP32 Emulator

**[English Version](#english-version) | [Versión en Español](#versión-en-español)**

---

<a id="english-version"></a>

## English Version

More information at [Minibots](https://minibots.wordpress.com/).

This project is a complete emulator of the classic Elektor Junior trainer computer (based on the [KIM-1](https://minibots.wordpress.com/2026/06/10/emulador-de-mos-kim-1-con-arduino-uno-r3/) architecture), specifically designed to run on an ESP32 microcontroller. It stands out for offering a precise simulation of the original hardware through an interactive and asynchronous web interface.

### Key Features

- **Optimized 6502 Core:** Object-oriented C++ emulation of the 6502 CPU, including support for undocumented instructions and BCD arithmetic adjustment.
- **Accurate Peripherals:** Implementation of the RIOT 6532 chip, managing bidirectional ports (PA/PB), data direction registers, and a programmable timer.
- **Realistic Web Interface:** Visual replica of the physical board using HTML/JS, respecting the layout of the hex keypad, command block, and mechanical switches with their corresponding LED.
- **Stable Multiplexed Display:** A system that accumulates the state of the segments activated by the 7442 decoder to send full frames to the browser, completely eliminating flicker.
- **Robust Connection:** Data transmission via WebSockets with local congestion control (`canSend()`) to avoid TCP stack saturation and prevent disconnections.
- **Hardware-accurate Debugging:** Support for step-by-step execution (STEP mode) by freezing the CPU using the Non-Maskable Interrupt (NMI).

### System Architecture

- **Dual-Core (FreeRTOS):** The pure emulation runs on a dedicated task (`machineTask`), dispatching batches of 250 instructions per millisecond, while the main core handles the network.
- **Lock-Free Synchronization:** Extensive use of `std::atomic` variables in the keyboard matrix (3x7) and display to communicate between cores without triggering watchdog resets.
- **Virtual Memory Bus:** Centralized management integrating 1K RAM, 1K ROM (original monitor program), and I/O addresses.

### Quick Start Guide

- **Installation:** Compile and upload the firmware to the ESP32 using your IDE. You must write the filesystem (`LittleFS`) to host the `index.html` file.
- **Web Connection:** Connect your ESP32 to your WiFi network and access the IP shown on the serial monitor from any web browser.
- **Basic Operation:** Use **RST** to boot the monitor program. Use **AD** (Address) and **DA** (Data) to navigate and inject hex code. To run a program, enter its address and press **GO**.

### Requirements

- **Hardware:** Any ESP32-based development board. No wiring or external components are required.
- **Software/Libraries:** PlatformIO or Arduino IDE, `ESPAsyncWebServer`, `AsyncTCP`, `ArduinoJson`, and LittleFS upload tools.

### Requirements

* ### License

This project is licensed under the **GNU General Public License v2.0**. See the `LICENSE` file for more details.

---

<a id="versión-en-español"></a>

## Versión en Español

Más información en [Minibots](https://minibots.wordpress.com/).

Este proyecto es un emulador completo del clásico ordenador de 
entrenamiento Elektor Junior (basado en la arquitectura del [KIM-1](https://minibots.wordpress.com/2026/06/10/emulador-de-mos-kim-1-con-arduino-uno-r3/), diseñado específicamente para ejecutarse en un microcontrolador ESP32. Destaca por ofrecer una simulación precisa del hardware original a través de una interfaz web interactiva y asíncrona.

### Características Principales

- **Núcleo 6502 Optimizado:** Emulación de la CPU 6502 en C++ orientada a objetos, incluyendo soporte para instrucciones no documentadas y ajuste aritmético BCD.

- **Periféricos Precisos:** Implementación del chip RIOT 6532, gestionando puertos bidireccionales (PA/PB), registros de dirección de datos y temporizador programable.

- **Interfaz Web Realista:** Réplica visual del panel físico mediante HTML/JS, respetando la distribución del teclado hexadecimal, el bloque de comandos y los interruptores mecánicos con su LED correspondiente.

- **Display Multiplexado Estable:** Sistema que acumula el estado de los segmentos activados por el decodificador 7442 para enviar fotogramas completos al navegador, eliminando cualquier parpadeo.

- **Conexión Robusta:** Transmisión de datos mediante WebSockets con control de congestión local (`canSend()`) para evitar la saturación de la pila TCP y evitar desconexiones.

- **Depuración Fiel al Hardware:** Soporte para ejecución paso a paso (modo STEP) congelando la CPU mediante la interrupción no evitable (NMI).

### Arquitectura del Sistema

- **Dual-Core (FreeRTOS):** La emulación pura corre en una tarea dedicada (`machineTask`), despachando lotes de 250 instrucciones por milisegundo, mientras el núcleo principal atiende la red.

- **Sincronización Lock-Free:** Empleo extensivo de variables `std::atomic` en la matriz de teclado (3x7) y la pantalla para comunicar los núcleos sin causar bloqueos del *watchdog*.

- **Bus de Memoria Virtual:** Gestión centralizada que integra 1K de RAM, 1K de ROM (programa monitor original) y las direcciones de E/S.

### Guía Rápida de Uso

- **Instalación:** Compila y sube el firmware al ESP32 usando tu entorno de desarrollo. Es obligatorio escribir el sistema de archivos (`LittleFS`) para alojar el `index.html`.

- **Conexión Web:** Conecta tu ESP32 a la red WiFi y entra a la IP mostrada por el puerto serie desde cualquier navegador.

- **Operación Básica:** Utiliza **RST** para arrancar el programa monitor. Emplea **AD** (Address) y **DA** (Data) para navegar e inyectar código hexadecimal. Para arrancar un programa, introduce su dirección y pulsa **GO**.

### Requisitos

- **Hardware:** Cualquier placa de desarrollo basada en ESP32. No se requiere cableado ni componentes externos, todo el hardware del Elektor Junior se emula por software y se controla vía web.
- **Entorno de desarrollo:** PlatformIO (recomendado) o Arduino IDE.
- **Librerías (Dependencias):**
  - `ESPAsyncWebServer` y `AsyncTCP` para el servidor web y los WebSockets.
  - `ArduinoJson` para el parseo de mensajes.
- **Herramientas:** Subida de sistema de archivos `LittleFS` configurada en tu IDE.

* **

### Licencia

Este proyecto está licenciado bajo la **GNU General Public License v2.0**. Puedes consultar el archivo `LICENSE` para más detalles.