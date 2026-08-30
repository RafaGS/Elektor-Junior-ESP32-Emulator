#if defined(ARDUINO)
#include <Arduino.h>

#include "ESP32WebWrapper.h"
#include "JuniorMachine.h"

static JuniorMachine machine;
static ESP32WebWrapper wrapper(&machine);

// El Junior real corre su 6502 a un reloj del orden de 1 MHz, y una
// instruccion 6502 tipica consume ~3-4 ciclos, es decir, unas 250-300
// instrucciones reales por milisegundo. El bucle anterior ejecutaba UNA
// sola instruccion por vuelta (cpu().step() una vez) y luego esperaba
// 1 ms completo -> la CPU emulada iba varios cientos de veces mas lenta
// que el hardware real. El bucle de monitor que escanea teclado y
// refresca el display (SCAND/AK/CONVD en monitor.asm) necesita muchas
// decenas de instrucciones solo para completar un ciclo, y se ejecuta
// en un bucle de espera activa (START: JSR SCAND; BNE START) - a una
// instruccion/ms, una sola pasada visible del monitor podia tardar
// segundos, lo que hace que no se perciba ninguna correspondencia
// entre pulsar una tecla y verla reflejada en el display.
//
// kInstructionsPerTick aproxima ese ritmo real. Es una aproximacion
// (no es cycle-accurate: step() ejecuta una instruccion completa, no
// cuenta ciclos), pensada para que el monitor responda a un ritmo
// perceptible como "en tiempo real". Ajustar este valor si el Junior
// concreto usa un cristal de reloj distinto o si se nota
// demasiado rapido/lento.
static constexpr int kInstructionsPerTick = 250;

static void machineTask(void* parameter) {
    (void)parameter;

    for (;;) {
        if (!wrapper.isPaused()) {
            // runCpuBatch() ejecuta el lote entero. La visibilidad del
            // teclado entre nucleos la garantizan los atomics de
            // JuniorKeyboard (ver ese fichero), no una seccion critica
            // aqui: envolver muchas instrucciones de CPU en un lock que
            // deshabilita interrupciones es peligroso en cuanto hay
            // Serial de por medio (traza), porque bloquea el vaciado
            // del UART el tiempo suficiente para disparar el watchdog.
            wrapper.runCpuBatch(kInstructionsPerTick);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup() {
    Serial.begin(115200);
    delay(250);

    machine.reset();
    wrapper.begin();

    xTaskCreatePinnedToCore(machineTask,
                            "junior_machine_task",
                            4096,
                            nullptr,
                            1,
                            nullptr,
                            1);
}

void loop() {
    wrapper.maintainConnections();
    if (wrapper.isDisplayEnabled()) {
        wrapper.broadcastDisplay();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

#else
int main() {
    return 0;
}
#endif
