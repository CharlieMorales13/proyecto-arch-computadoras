# Contexto del proyecto

**Materia:** Arquitectura de Computadoras — Universidad Anáhuac Oaxaca, Facultad de Ingeniería
(Ingeniería en Tecnologías de la Información y Negocios Digitales).
**Docente:** Mtro. Diego Ovadid Acevedo Cruz.
**Integrantes:** Mario Barrita Sánchez Enríquez, Alejandro Cinco Prieto, Héctor Martínez Reyes,
Carlos Antonio Gomez Morales.

## Documentos de origen

| Documento | Fecha | Uso en este repositorio |
|---|---|---|
| *Avance de Proyecto: ECU digital para motocicleta con ESP32* | 15 oct 2026 | Antecedentes: cambio de FPGA a ESP32, primera tabla de estados y riesgos |
| *ECU de motocicleta con ESP32: plan de 3 semanas* | — | Lista de materiales original y plan de trabajo |
| *ECU digital para motocicleta con ESP32: descripción del proyecto* | 19 oct 2026 | Versión simplificada (una tarea, `EstadoECU`); base del firmware |
| ***Segundo Avance de Proyecto: ECU digital para motocicleta con ESP32*** | 18 oct 2026 | **Especificación vigente.** Valores finales, lista de materiales corregida (Anexo A), pines (Anexo B), estados (Anexo C) y responsables (Tabla 5). |

Cuando los documentos no coinciden, manda el Segundo Avance. Lo que el Segundo Avance resolvió, lo que se
cambió en el código para alinearlo y lo que todavía no menciona está en
[04_cambios_al_documento.md](04_cambios_al_documento.md).

## Qué es el sistema

Una computadora de propósito específico que reproduce la Unidad de Control Electrónico (ECU) de una
motocicleta, construida sobre un ESP32 DevKit V1 (ESP-WROOM-32) y programada en C con ESP-IDF 6.1 y
FreeRTOS, sin Arduino.

- **Entradas:** 5 interruptores (ignición, sentido de cambio, direccional izquierda, derecha e
  intermitentes), 5 botones (arranque, freno delantero, freno trasero, cambio y reset maestro), un
  potenciómetro de gasolina y un teclado 4x4.
- **Procesamiento:** antirrebote, máquina de 8 estados con prioridades, clave de desbloqueo `2026#`,
  marcha de 1 a 4, RPM con rampa, kilometraje (8 vueltas = 1 km) con aviso de servicio cada 16 km,
  frenado con ABS y 4 rutinas de prueba (la 4 desactiva el aviso de servicio).
- **Memoria:** marcha, kilometraje y bandera de servicio en la NVS (flash); sobreviven a quitar la
  energía. Solo el reset maestro los borra, y únicamente con la ignición apagada.
- **Salidas:** 2 servos de giro continuo (ruedas), barra de 4 LEDs de gasolina, 2 direccionales, LED
  RGB de estado, display de 8 dígitos y LED de vida.

## Del FPGA al ESP32

El avance anterior era VHDL sobre la Boolean Board (Spartan-7). La arquitectura se conserva y cada
módulo de hardware se vuelve un módulo de software:

| VHDL | ESP32 / C | Archivo |
|---|---|---|
| `top_module` | Ciclo de 10 ms que llama a los módulos | `main/main.c` |
| `divisor_frecuencia` | Contador de ciclos con marcas de 100 ms, 250 ms, 500 ms y 1 s | `main/control/tiempos.c` |
| `debounce_universal` | Antirrebote de 2 lecturas y detección de flancos | `main/entradas/entradas.c` |
| `driver_teclado` | Barrido por I2C con el PCF8574 | `main/entradas/teclado.c` |
| `fsm_principal_ecu` | Máquina de 8 estados | `main/control/fsm_ecu.c` |
| `fsm_diagnostico` | Validación de la clave y selección de la rutina | `main/control/fsm_diag.c` |
| `contador_km_rpm` | RPM y kilometraje | `main/control/km_rpm.c` |
| `banco_registros` | `EstadoECU` en RAM + NVS en flash | `main/comun/estado.h`, `main/memoria/persistencia.c` |
| `ctrl_pwm_servos` | Periférico LEDC | `main/salidas/servos.c` |
| `ctrl_rgb` + `ctrl_leds` | 74HCT595 por GPIO | `main/salidas/salidas_leds.c` |
| `mux_7seg` | MAX7219 por SPI | `main/salidas/display.c` |
| Archivo XDC | Definiciones de pines | `main/comun/pines.h` |
| Reloj de 100 MHz y habilitaciones | Tick de FreeRTOS a 1 kHz y marcas de `tiempos.c` | `sdkconfig.defaults` |

En términos de arquitectura de computadoras: el ESP32 aporta la ALU y el reloj, `EstadoECU` es el banco
de registros, la NVS es la memoria no volátil, `fsm_ecu` es la unidad de control y los periféricos (GPIO,
ADC, LEDC, SPI, I2C) son la entrada/salida.

## Plan de trabajo (Tabla 5 del Segundo Avance)

| Actividad | Responsable | Fecha |
|---|---|---|
| Grabar el firmware en la placa y revisar el monitor serial | Mario Barrita | 19 a 20 oct |
| Probar entradas, teclado y clave 2026# con el monitor serial | Alejandro Cinco | 20 a 22 oct |
| Probar servos, calibrar neutro y fuente de 5 V | Héctor Martínez | 20 a 22 oct |
| Probar 74HCT595, LED RGB y MAX7219 | Carlos Gomez | 20 a 22 oct |
| Integración en protoboard y prueba de cada estado | Todo el equipo | 23 a 25 oct |
| Persistencia: apagar, desconectar y verificar km y marcha | Mario Barrita | 25 oct |
| Maqueta en MDF y versión soldada | Héctor Martínez y Alejandro Cinco | 26 a 29 oct |
| Documentación final, video de respaldo y ensayo de la demostración | Carlos Gomez y todo el equipo | 30 oct a 1 nov |

Módulos de cada integrante, según el plan de 3 semanas y las actividades de la Tabla 5:

| Integrante | Módulos | Parte física |
|---|---|---|
| Mario Barrita | `main.c`, `fsm_ecu`, `persistencia`, integración | Repositorio y control de versiones |
| Alejandro Cinco | `entradas`, `teclado`, `fsm_diag` | Panel de interruptores, botones y teclado; maqueta |
| Héctor Martínez | `servos` | Servos, fuente de 5 V; maqueta |
| Carlos Gomez | `salidas_leds`, `display` | 74HCT595, LED RGB y MAX7219; documentación final |
| *sin asignar* | `tiempos`, `km_rpm` | — |

## Estado del firmware

Primera versión completa de los 11 módulos, alineada con el Segundo Avance. **Aún no se ha grabado en
la placa.** Después de los últimos cambios (ver `docs/04`, parte 2) hay que volver a compilar. Lo que se
tiene que calibrar con las piezas reales está en `main/comun/config.h` (neutro de cada servo, rango del
ADC del potenciómetro).
