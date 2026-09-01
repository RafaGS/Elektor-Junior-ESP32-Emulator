#include "ESP32WebWrapper.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "JuniorMachine.h"
#include "secrets.h"

#if defined(ARDUINO)
ESP32WebWrapper* ESP32WebWrapper::instance_ = nullptr;
#endif

ESP32WebWrapper::ESP32WebWrapper(JuniorMachine* machine)
    : machine_(machine) {
#if defined(ARDUINO)
    instance_ = this;
#endif
}

void ESP32WebWrapper::begin() {
#if defined(ARDUINO)
    if (!LittleFS.begin(true)) {
        Serial.println("[ESP32WebWrapper] LittleFS init failed");
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[ESP32WebWrapper] Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();
    Serial.print("[ESP32WebWrapper] WiFi connected: ");
    Serial.println(WiFi.localIP());

    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    ws_.onEvent([](AsyncWebSocket* server,
                   AsyncWebSocketClient* client,
                   AwsEventType type,
                   void* arg,
                   uint8_t* data,
                   size_t len) {
        ESP32WebWrapper::onWebSocketEvent(server, client, type, arg, data, len);
    });

    server_.addHandler(&ws_);
    server_.begin();

    Serial.println("[ESP32WebWrapper] Web server started");

    // La traza detallada de accesos PA/PB/DDRA/DDRB (con PC/opcode de
    // cada acceso) puede generar muchisimas lineas por segundo durante
    // el bucle de escaneo de teclado + refresco de display del monitor.
    // Se desactiva al arrancar para no saturar el puerto serie (115200
    // baudios) ni ralentizar aun mas el bucle de la CPU; se activa/
    // desactiva en caliente con el boton TRACE del panel web.
    if (machine_ != nullptr) {
        machine_->setTraceEnabled(false);
    }
#endif
}

void ESP32WebWrapper::loop() {
    (void)0;
}

void ESP32WebWrapper::maintainConnections() {
#if defined(ARDUINO)
    // ESPAsyncWebServer requiere llamar a cleanupClients() periodicamente:
    // sin esto, los clientes WebSocket desconectados no liberan su hueco
    // interno (la libreria tiene un limite de clientes concurrentes) y
    // con el tiempo se van agotando o se degrada la memoria - esto
    // provoca exactamente desconexiones/reconexiones intermitentes,
    // independientemente de si TRACE esta activo o no.
    //
    // Deliberadamente NO va dentro de broadcastDisplay(): esa funcion
    // solo se llama desde loop() cuando el display esta activado
    // (isDisplayEnabled()), y si el usuario lo apaga con KEY_DISPLAY,
    // la limpieza de conexiones se pararia con el. Este metodo se
    // llama siempre, independientemente del estado del display.
    ws_.cleanupClients();
#endif
}

void ESP32WebWrapper::broadcastDisplay() {
#if defined(ARDUINO)
    if (machine_ == nullptr) {
        return;
    }

    const auto& display = machine_->display();
    const auto current = display.current();

    // El display real es multiplexado: la CPU va encendiendo un digito
    // a la vez, muy rapido, y el ojo integra el conjunto. Esta funcion
    // solo ve el instante actual (un digito), asi que hay que ACUMULAR
    // el fotograma digito a digito en display_snapshot_ en vez de
    // borrarlo entero en cada llamada. Borrarlo (como hacia el codigo
    // anterior con fill(0)) descarta los otros 5 digitos en cada envio
    // por websocket, por lo que el navegador practicamente nunca recibe
    // un fotograma completo: se ve un digito suelto parpadeando y el
    // resto siempre apagado.
    if (current.valid) {
        display_snapshot_[current.digit_index] = current.segments;
    }

    // JsonDocument + String aqui suponian varias asignaciones dinamicas
    // por llamada (documento, buffer de String, y la copia interna que
    // hace textAll() al encolar para cada cliente), repetidas 30
    // veces/segundo de forma indefinida. Con una demo larga esto puede
    // fragmentar el heap del ESP32 hasta que empiecen a fallar
    // asignaciones dentro de la propia pila AsyncTCP - sin que el ESP32
    // llegue a colgarse ni reiniciarse (por eso no aparecia ningun Guru
    // Meditation), pero sí provocando caidas/reconexiones del
    // WebSocket. El formato del mensaje es fijo y simple, así que se
    // construye en un buffer de pila de tamaño fijo, sin heap alguno
    // por parte de este código (ver nota bajo el bucle sobre la copia
    // interna que aún pueda hacer textAll()).
    // Si no hay clientes conectados, nos ahorramos procesar la cadena JSON
    if (ws_.count() == 0) {
        return;
    }

    char json[48];
    int len = snprintf(json, sizeof(json), "{\"d\":[%u,%u,%u,%u,%u,%u]}",
            display_snapshot_[0], display_snapshot_[1], display_snapshot_[2],
            display_snapshot_[3], display_snapshot_[4], display_snapshot_[5]);

    if (len > 0 && static_cast<size_t>(len) < sizeof(json)) {
        // Usamos auto& (referencia) para no copiar el objeto (evitando el error del mutex).
        // Como sabemos que es el objeto directo y no un puntero, usamos el punto (.).
        for (auto& client : ws_.getClients()) {
            if (client.status() == WS_CONNECTED && client.canSend()) {
                client.text(json, static_cast<size_t>(len));
            }
        }
    }
#endif
}

void ESP32WebWrapper::runCpuBatch(int instruction_count) {
#if defined(ARDUINO)
    if (machine_ == nullptr) {
        return;
    }

    // El teclado (key_matrix_ en JuniorKeyboard) usa std::atomic por
    // celda, con lo que la visibilidad entre nucleos (escritura desde
    // el hilo del WebSocket, lectura desde esta tarea de CPU) queda
    // garantizada SIN necesidad de una seccion critica aqui. A
    // proposito NO se envuelve este bucle en portENTER_CRITICAL: una
    // seccion critica deshabilita interrupciones, y combinada con la
    // traza por Serial (activable con el boton TRACE) eso bloqueaba el
    // vaciado del buffer UART el tiempo suficiente para disparar el
    // watchdog del nucleo (Guru Meditation Error: Interrupt wdt
    // timeout on CPU1). Ver JuniorKeyboard.h para el detalle de los
    // atomics.
    // Si la traza esta activa, cada acceso a PA/PB/DDRA/DDRB dispara un
    // Serial.printf BLOQUEANTE (traceRiotAccess() en JuniorMachine.cpp).
    // En una ventana con mucha actividad de bus (escaneo de teclado +
    // refresco de display) esto puede hacer que este bucle tarde
    // segundos completos en terminar sus 250 instrucciones, sin ceder
    // la CPU ni una vez. Esta tarea comparte nucleo/prioridad con
    // loop() (Arduino) y, segun la libreria, con parte de la pila
    // WiFi/AsyncTCP - acapararla tanto tiempo puede dejar sin CPU al
    // procesamiento de WebSocket el tiempo suficiente para que el
    // navegador de la conexion por muerta ("Desconectado.
    // Reconectando...").
    //
    // OJO: taskYIELD() NO es gratis solo porque "no haya nada mas listo
    // para ejecutar" - si loop() o la pila WiFi/AsyncTCP SI tienen
    // trabajo pendiente en ese instante, taskYIELD() les cede la CPU de
    // verdad, y pueden tardar un rato en devolverla. Ceder cada 16
    // instrucciones (hasta 16 veces por lote de 250) tiene un coste
    // real y constante que antes se pagaba SIEMPRE, incluso con la
    // traza apagada - donde el lote entero tarda microsegundos y este
    // riesgo de bloqueo de UART ni siquiera existe. Por eso solo se
    // cede la CPU periodicamente cuando la traza esta realmente activa
    // (el unico escenario que de verdad lo necesita); con la traza
    // apagada no se cede nada dentro del lote, y vTaskDelay(1) al
    // volver a main.cpp ya cede la CPU de forma natural entre lotes.
    const bool tracing = machine_->traceEnabled();
    static constexpr int kYieldEvery = 16;
    for (int i = 0; i < instruction_count; ++i) {
        const uint16_t pc_before = machine_->cpu().pc();
        const bool was_in_rom = (pc_before >= JuniorMachine::kMonitorStart &&
                                  pc_before <= JuniorMachine::kMonitorEnd);

        machine_->cpu().step();
        machine_->tick();

        if (pending_step_nmi_) {
            // Ya dejamos correr la primera instruccion del programa
            // reanudado (la que aterrizo en RAM tras el RTI de GOEXEC);
            // esta es esa instruccion, ya ejecutada. Toca disparar el
            // NMI ahora, exactamente como si el interruptor STEP
            // mantuviera la linea NMI a nivel bajo en el hardware real.
            machine_->cpu().nmi();
            pending_step_nmi_ = false;
        } else if (step_mode_.load(std::memory_order_relaxed)) {
            const uint16_t pc_after = machine_->cpu().pc();
            const bool now_in_rom = (pc_after >= JuniorMachine::kMonitorStart &&
                                      pc_after <= JuniorMachine::kMonitorEnd);
            if (was_in_rom && !now_in_rom) {
                // Transicion ROM->RAM: la unica forma en que esto ocurre
                // en este monitor es el "RTI" final de GOEXEC, que
                // reanuda el programa del usuario en POINTL/POINTH. Con
                // el interruptor STEP activo, se deja correr esa UNA
                // instruccion (ya se ejecuta en la siguiente vuelta del
                // bucle) y se dispara el NMI justo despues.
                pending_step_nmi_ = true;
            }
        }

        if (tracing && (i % kYieldEvery) == (kYieldEvery - 1)) {
            taskYIELD();
        }
    }
#else
    (void)instruction_count;
#endif
}

#if defined(ARDUINO)
void ESP32WebWrapper::onWebSocketEvent(AsyncWebSocket* server,
                                      AsyncWebSocketClient* client,
                                      AwsEventType type,
                                      void* arg,
                                      uint8_t* data,
                                      size_t len) {
    (void)server;
    (void)client;
    (void)arg;

    if (type == WS_EVT_DATA && instance_ != nullptr) {
        instance_->handleWebSocketMessage(data, len);
    }
}

void ESP32WebWrapper::handleWebSocketMessage(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        Serial.printf("[ESP32WebWrapper] JSON parse failed: %s\n", error.c_str());
        return;
    }

    if (!doc["key"].is<const char*>() || !doc["pressed"].is<bool>()) {
        return;
    }

    const char* key = doc["key"].as<const char*>();
    const bool pressed = doc["pressed"].as<bool>();
    handleKeyInput(key, pressed);
}

void ESP32WebWrapper::handleKeyInput(const char* key, bool pressed) {
    if (machine_ == nullptr || key == nullptr || key[0] == '\0') {
        return;
    }

    std::string key_name = key;
    if (key_name.empty()) {
        return;
    }

    std::transform(key_name.begin(), key_name.end(), key_name.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (key_name.size() == 1 && ((key_name[0] >= '0' && key_name[0] <= '9') ||
                                 (key_name[0] >= 'A' && key_name[0] <= 'F'))) {
        key_name = "KEY_" + key_name;
    } else if (key_name.size() > 1) {
        key_name = "KEY_" + key_name;
    }

    if (pressed) {
        Serial.printf("[KeyTrace] raw=%s normalized=%s pressed=%s\n", key, key_name.c_str(), pressed ? "true" : "false");
    }

    if (key_name == "KEY_RST") {
        if (!pressed) {
            return;
        }
        Serial.println("[KeyTrace] action=RESET");
        machine_->reset();
        return;
    }
    if (key_name == "KEY_NMI") {
        if (!pressed) {
            return;
        }
        Serial.println("[KeyTrace] action=NMI");
        machine_->cpu().nmi();
        return;
    }
    if (key_name == "KEY_STOP") {
        if (!pressed) {
            return;
        }
        paused_ = !paused_;
        Serial.printf("[KeyTrace] action=STOP paused=%s\n", paused_ ? "true" : "false");
        return;
    }
    if (key_name == "KEY_STEPSW") {
        // Interruptor STEP (S24) real del Junior. A diferencia de
        // KEY_STOP, esto NO congela la CPU - el monitor sigue
        // funcionando con normalidad (teclado, display...). Solo arma
        // el modo en el que la PROXIMA vez que GOEXEC arranque un
        // programa (via la tecla GO real, matriz), se ejecute una sola
        // instruccion y se dispare NMI automaticamente para volver al
        // monitor. Ver la deteccion de transicion ROM->RAM en
        // runCpuBatch().
        if (!pressed) {
            return;
        }
        const bool enabled = !step_mode_.load(std::memory_order_relaxed);
        step_mode_.store(enabled, std::memory_order_relaxed);
        Serial.printf("[KeyTrace] action=STEPSW step_mode=%s\n", enabled ? "true" : "false");
        return;
    }
    if (key_name == "KEY_DBGSTEP") {
        // Atajo de depuracion por software (un solo paso de CPU). No es
        // una tecla real del Junior: la tecla real "PL" (+/SKIP en el
        // panel del monitor, fila2-bit2) es KEY_PL y SI llega a la
        // matriz mas abajo. Se renombro este atajo para no colisionar
        // con ese nombre.
        if (!pressed) {
            return;
        }
        Serial.println("[KeyTrace] action=DBGSTEP");
        machine_->cpu().step();
        machine_->tick();
        return;
    }
    if (key_name == "KEY_DISPLAY") {
        if (!pressed) {
            return;
        }
        display_enabled_ = !display_enabled_;
        Serial.printf("[KeyTrace] action=DISPLAY enabled=%s\n", display_enabled_ ? "true" : "false");
        return;
    }
    if (key_name == "KEY_TRACE") {
        if (!pressed) {
            return;
        }
        const bool enabled = !machine_->traceEnabled();
        machine_->setTraceEnabled(enabled);
        Serial.printf("[KeyTrace] action=TRACE enabled=%s\n", enabled ? "true" : "false");
        return;
    }

    if (pressed) {
        const auto [row, col] = machine_->keyboard().defaultCoordsForKey(key_name);
        Serial.printf("[KeyTrace] matrix row=%u col=%u valid=%s\n",
                      static_cast<unsigned>(row),
                      static_cast<unsigned>(col),
                      (row != 0xFFU && col != 0xFFU) ? "true" : "false");
    }

    writeKeyboardMatrixEntry(key_name.c_str(), pressed);
}

void ESP32WebWrapper::writeKeyboardMatrixEntry(const char* key, bool pressed) {
    if (machine_ == nullptr || key == nullptr) {
        return;
    }

    std::string key_name = key;
    if (key_name.empty()) {
        return;
    }

    // setKeyState() escribe con std::atomic (memory_order_release),
    // que ya garantiza visibilidad hacia el otro nucleo sin necesidad
    // de portENTER_CRITICAL aqui. Ver JuniorKeyboard.h.
    machine_->keyboard().setKeyState(key_name, pressed);
}

void ESP32WebWrapper::sendDisplayState() {
    broadcastDisplay();
}
#endif
