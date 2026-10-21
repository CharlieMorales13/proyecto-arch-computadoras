# Discrepancias entre el Segundo Avance y el firmware

Documento de referencia: **Segundo Avance de Proyecto — ECU digital para motocicleta con ESP32**
(18 de octubre de 2026). Es la especificación vigente; reemplaza a la *descripción del proyecto* y al
*Avance de Proyecto* del 15 de octubre.

Este archivo tiene tres partes:

1. Lo que el Segundo Avance ya resolvió.
2. Lo que se cambió en el código para que coincida con el Segundo Avance.
3. Lo que el código define y el Segundo Avance todavía no menciona (conviene agregarlo al documento).

En el código, los valores de la parte 3 están marcados con `[NUEVO]` en `main/comun/config.h`.

---

## 1. Resuelto en el Segundo Avance

| Tema | Cómo quedó (Segundo Avance) | Dónde está en el código |
|---|---|---|
| Plataforma | ESP-IDF v6.1, sin Arduino | todo el proyecto |
| Concurrencia | Una tarea cada 10 ms con estado común, un escritor por campo (Tabla 2) | `main.c`, `estado.h` |
| Orden del ciclo | tiempos, entradas, teclado, fsm_diag, fsm_ecu, km_rpm, persistencia, servos, salidas_leds, display (sección 2.1) | `main.c` |
| Display | Un MAX7219 de 8 dígitos (Tabla 2, Anexo A) | `display.c` |
| LEDs | 74HCT595 a 5 V, resistencias de 470 Ω (Tabla 2, Tabla 4, Anexo A) | hardware; sin cambios de código |
| Tiempos | Marcas de 100, 250 y 500 ms y 1 s; parpadeos de 1, 2 y 5 Hz (Tabla 3) | `tiempos.c` |
| Gasolina | 5 zonas (vacío y 4 niveles) con histéresis (Tabla 1) | `entradas.c` |
| Teclado | Una fila por ciclo, tecla con bandera de validez (Tabla 3) | `teclado.c` |
| Persistencia | Solo marcha, km y servicio; el desbloqueo se pierde al apagar (Tabla 2) | `persistencia.c`, `fsm_diag.c` |
| Reset maestro | Borra la NVS y reinicia la placa, sin escribir campos ajenos (sección 2.3) | `persistencia.c` |
| Entrada a BLOQUEADO | Una tecla en NEUTRAL (Anexo C) | `fsm_ecu.c` |
| Modo de pruebas | Solo desde NEUTRAL (Tabla 2) | `fsm_ecu.c` |
| Arranque | 1 s y pasa a EN_MARCHA (sección 3, Anexo C) | `fsm_ecu.c` |
| RGB en NEUTRAL | Blanco, azul si hay servicio (Anexo C) | `salidas_leds.c` |
| Pines | Anexo B, igual a `pines.h` | `pines.h` |
| Salidas del 595 | Q0–Q3 gasolina, Q4–Q6 RGB, Q7 dir. izquierda; 2.º chip Q0 dir. derecha (Tabla B1) | `salidas_leds.h` |

## 2. Cambios hechos al código para alinearlo

| Tema | El código decía | El Segundo Avance dice | Cambio aplicado |
|---|---|---|---|
| Kilometraje | 1 vuelta = 10 m | **8 vueltas = 1 km** | `METROS_POR_VUELTA = 125` |
| Servicio | a los 5 km | **a los 16 km** | `SERVICIO_CADA_METROS = 16000` |
| Frenos | un freno −50 %/s | **delantero −75 %/s, trasero −25 %/s**; ambos alto en 1 s | `FRENO_DEL_POR_CICLO = 75`, `FRENO_TRA_POR_CICLO = 25`; el ABS (−100 %/s) ya coincidía |
| Escala de velocidad | 0..1000 (por mil) | — | Pasa a **0..10000** (centésimas de %): con por mil, 75 %/s daba 7.5 por ciclo, que no es entero |
| Rutinas | teclas 1 a 3 | **teclas 1 a 4** | La rutina 4 desactiva el aviso de servicio sin borrar los km: `fsm_diag` levanta la petición `quitar_servicio` y `km_rpm`, único escritor de `flag_servicio`, apaga la bandera |
| Quién elige la rutina | `fsm_ecu` | **`fsm_diag`** ("valida la clave 2026# y selecciona la rutina", Tabla 3) | El campo `rutina` pasa a ser de `fsm_diag` |
| Tecla `*` en BLOQUEADO | solo borraba lo escrito | **"* o D regresan a NEUTRAL"** (Anexo C) | `*` borra y regresa a NEUTRAL, igual que `D` |
| Tecla `*` en NEUTRAL | entraba a BLOQUEADO | — | Ya no: solo las teclas distintas de `*` y `D` piden el modo de pruebas |

> El Segundo Avance dice que el firmware compila sin errores ni advertencias (Tabla 1 y Figura D2).
> Con estos cambios hay que **volver a compilar** antes de entregarlo, para que esa afirmación y los
> datos de memoria (230 KB, 8.3 % de DRAM) sigan siendo ciertos.

## 3. Definido en el código, pero no en el Segundo Avance

### 3.1 Comportamiento

| # | Tema | Qué hace el código | Qué agregar al documento |
|---|---|---|---|
| 1 | Aviso de servicio después de la rutina 4 | El aviso se activa cada vez que el km cruza un múltiplo de 16 (16, 32, 48 km). Si se desactiva a los 17 km, el siguiente aviso llega a los 32 km. | "El aviso de servicio se repite cada 16 km." Sin esta regla el aviso volvería a encenderse en el ciclo siguiente, porque el km sigue arriba de 16. |
| 2 | Qué significa "75 %/s" | Se resta el 75 % de la velocidad **máxima** por segundo, no de la velocidad actual | "Los porcentajes de frenado son de la velocidad máxima." |
| 3 | Aceleración y freno motor | Sube a 50 %/s; sin freno (al bajar marcha o quedarse sin gasolina) baja a 30 %/s | Agregar a los valores definidos. |
| 4 | Salida de FRENADO | Al soltar los frenos: EN_MARCHA si la moto aún se mueve; NEUTRAL si ya se detuvo (hay que volver a arrancar) | Agregar al Anexo C. |
| 5 | Frenar detenido | Los frenos solo llevan a FRENADO desde ARRANQUE, EN_MARCHA o FRENADO | Precisar la prioridad "frenado" del Anexo C. |
| 6 | RGB en ARRANQUE | Verde, **azul si hay servicio**, por la regla rojo > azul > blanco > verde | El Anexo C dice solo "Verde"; cambiarlo a "Verde (azul si hay servicio)" o pedir que el código ignore el servicio en ARRANQUE. |
| 7 | RGB en MODO_PRUEBAS | Blanco; en la rutina 1 el RGB entra en el barrido de las 9 salidas | El Anexo C dice "Según la rutina"; precisarlo. |
| 8 | Display en BLOQUEADO y pruebas | BLOQUEADO: `PIn` y un guion por dígito tecleado. Pruebas: `PruEbA -` (menú), `PruEbA 1`, `3` o `4`; rutina 2: `8.8.8.8.8.8.8.8.` | Agregar a la descripción de `display.c`. |
| 9 | Rutina 3 | 2 s en cada marcha (1→2→3→4 y repite); al cambiar de rutina los servos se detienen de inmediato | Agregar a la sección 3. |
| 10 | Interruptor de sentido | Cerrado = subir marcha, abierto = bajar | Agregar a entradas. |
| 11 | Cambio de marcha | Funciona en todos los estados excepto APAGADO y MODO_PRUEBAS | Agregar a "Marchas". |
| 12 | Servos en APAGADO | Sin pulsos (duty 0) en lugar del neutro de 1500 µs, para que no se muevan aunque el neutro no esté calibrado | Agregar al Anexo C (APAGADO). |

### 3.2 Valores numéricos

| Valor | Código | Comentario |
|---|---|---|
| Velocidad de rueda a máxima velocidad | 2 vueltas/s (`RUEDA_RPS_MAX_X100 = 200`) | Un FS90R gira a ~120 rpm. **Resuelve el pendiente de la sección 4** ("confirmar que la demostración llegue a servicio en un tiempo razonable"): las 128 vueltas de 16 km toman ~65 s en marcha 4, ~85 s en 3.ª, ~2.1 min en 2.ª y ~4.3 min en 1.ª. |
| Umbrales de gasolina | 5, 25, 50 y 75 %, histéresis de 3 % | Valores iniciales; se calibran con `ADC_MIN`/`ADC_MAX` (pendiente del propio documento). |
| SPI del MAX7219 | 1 MHz | Lo limita el convertidor de nivel (las resistencias de 10 kΩ hacen lentas las subidas). |
| I2C del PCF8574 | 100 kHz | Máximo que admite el chip. |
| Dirección del PCF8574 | 0x20, o 0x38 si no responde | Algunos módulos traen el PCF8574A. |
| Reconfiguración del MAX7219 | cada 2 s | Si el ruido de los servos altera un registro, el display se recupera solo. |

### 3.3 Riesgos que no están en la Tabla 4

| Riesgo | Impacto | Mitigación |
|---|---|---|
| Corte de energía con la ignición encendida: se pierde lo recorrido desde el último apagado | Medio | Consecuencia de guardar solo al apagar. Opcional: guardar también cada 1 km (no implementado; decisión del equipo). |
| Guardar en la NVS al apagar puede tardar más de 10 ms en ese ciclo | Bajo | Los actuadores ya están detenidos; el log de "ciclo peor" lo muestra. |
| Conectar la fuente de 5 V a VIN con el USB conectado | Medio | Unir solo las tierras mientras se programa por USB. |
| Sin teclado (PCF8574 desconectado) | Bajo | La ECU arranca igual y solo pierde el modo de pruebas. |

### 3.4 Notas al mapa de pines (Anexo B)

- La Tabla 4 dice "no usamos 0, 6 a 12", pero GPIO 2 (LED de vida) también es pin de arranque. Funciona
  porque es el LED de la placa; **no conectar nada más a GPIO 2**.
- GPIO 36 y 39 reciben pulsos falsos de ~80 ns cuando trabaja el ADC (errata 3.11 del ESP32). No afecta
  porque se leen cada 10 ms con antirrebote; no usar interrupciones en esos pines.
- GPIO 5 y 14 emiten pulsos al arrancar el ESP32. No afecta: el LATCH del 595 (27) no se mueve y el
  MAX7219 se configura al iniciar.

## 4. Pendientes que siguen abiertos

- Volver a compilar con los cambios de la parte 2.
- Grabar el firmware y probarlo en la placa (19 al 25 de octubre, Tabla 5).
- Calibrar el neutro de cada servo (`SERVO_NEUTRO_IZQ_US`, `SERVO_NEUTRO_DER_US`) y `ADC_MIN`/`ADC_MAX`.
- Decidir el punto 6 de la parte 3.1 (RGB en ARRANQUE) y si se guarda la NVS también cada 1 km.
