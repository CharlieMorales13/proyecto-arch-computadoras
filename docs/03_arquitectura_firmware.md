# Arquitectura del firmware

## 1. Idea general

Una sola tarea de FreeRTOS (`ciclo_ecu`, en `main.c`) se ejecuta cada **10 ms** con `vTaskDelayUntil()`
y llama a todos los módulos en orden. Todos comparten una estructura `EstadoECU`, que hace el papel del
banco de registros. Al haber una sola tarea no hay condiciones de carrera ni hacen falta mutex ni colas.

```
            ┌───────────────────────── ciclo de 10 ms ─────────────────────────┐
 Entradas   │ tiempos → entradas → teclado                                     │
 Control    │ → fsm_diag → fsm_ecu → km_rpm                                    │
 Memoria    │ → persistencia                                                   │
 Salidas    │ → servos → salidas_leds → display                                │
            └──────────────────────────────────────────────────────────────────┘
                     todos leen y escriben campos de EstadoECU
```

Configuración de la tarea (`main/comun/config.h`): prioridad 5, pila de 6 KB, núcleo 1. El tick de
FreeRTOS está a 1 kHz (`CONFIG_FREERTOS_HZ=1000` en `sdkconfig.defaults`); si no, `config.h` detiene la
compilación con un error.

## 2. Capas y carpetas

Las carpetas siguen las capas del diagrama de bloques de la especificación:

| Capa del diagrama | Carpeta | Archivos |
|---|---|---|
| Lectura de entradas | `main/entradas/` | `entradas.c`, `teclado.c` |
| Control | `main/control/` | `tiempos.c`, `fsm_diag.c`, `fsm_ecu.c`, `km_rpm.c` |
| Estado y memoria | `main/comun/`, `main/memoria/` | `estado.h`, `persistencia.c` |
| Módulos de salida | `main/salidas/` | `servos.c`, `salidas_leds.c`, `display.c` |
| Definiciones | `main/comun/` | `pines.h`, `config.h` |
| Estructural | `main/` | `main.c` |

Todo vive en el componente `main` de ESP-IDF (`main/CMakeLists.txt` lista los archivos y carpetas).

## 3. Convención de cada módulo

```c
esp_err_t modulo_iniciar(void);        // configura su periférico (solo si tiene)
void modulo_actualizar(EstadoECU *e);  // un paso de 10 ms: lee campos, escribe los suyos
```

- `*_iniciar()` se llama una vez desde `app_main()` antes de crear la tarea.
- `*_actualizar()` nunca bloquea ni espera: hace su trabajo y regresa.
- El estado interno que nadie más necesita (antirrebote, fila del teclado, fracción de vuelta) es
  `static` dentro del `.c` del módulo.

## 4. EstadoECU y sus dueños

Cada campo tiene **un solo módulo que lo escribe**. Está anotado en `main/comun/estado.h`.

| Campos | Escribe | Leen |
|---|---|---|
| `ciclos`, `tick_100ms`, `tick_250ms`, `tick_500ms`, `tick_1s`, `fase_1hz`, `fase_2hz`, `fase_5hz` | tiempos | display, salidas_leds, main |
| `ign_on` | entradas | fsm_ecu, persistencia |
| `btn_arranque`, `btn_cambio` (eventos) | entradas | fsm_ecu |
| `freno_del`, `freno_tra` | entradas | fsm_ecu, servos, salidas_leds |
| `sentido_cambio` | entradas | fsm_ecu |
| `dir_izq`, `dir_der`, `intermitentes` | entradas | salidas_leds |
| `adc_gasolina`, `nivel_gasolina` | entradas | fsm_ecu, salidas_leds, main (log) |
| `btn_reset` (evento) | entradas | persistencia |
| `tecla`, `tecla_valida` | teclado | fsm_diag, fsm_ecu |
| `sys_unlocked`, `clave_digitos` | fsm_diag | fsm_ecu, display |
| `rutina` | fsm_diag | servos, salidas_leds, display |
| `quitar_servicio` (petición de un ciclo) | fsm_diag | km_rpm |
| `estado`, `marcha`, `ciclos_en_estado` | fsm_ecu | todos los módulos de control y salida |
| `rpm`, `km_m`, `flag_servicio` | km_rpm | display, salidas_leds, persistencia |
| `velocidad` | servos | km_rpm, fsm_ecu |
| `bits_leds` | salidas_leds | (imagen de los 74HCT595) |
| `digitos` | display | (imagen del MAX7219) |

**Excepción única:** al arrancar, `persistencia_iniciar()` carga `marcha`, `km_m` y `flag_servicio`
desde la NVS antes del primer ciclo. Es el valor de reset de esos registros.

**Lecturas entre ciclos:** un módulo que lee un campo escrito más abajo en la lista ve el valor del
ciclo anterior, igual que leer un registro en un diseño síncrono. Ejemplos: `fsm_diag` lee `estado` del
ciclo anterior; `fsm_ecu` lee la `velocidad` que calculó `servos` en el ciclo anterior.

**Eventos y condiciones:** los eventos (`btn_*`, `tecla_valida`, `tick_*`, `quitar_servicio`) son
verdaderos durante un solo ciclo. Las condiciones (`ign_on`, `freno_*`, interruptores) son niveles sostenidos.

## 5. Módulos

### tiempos.c
Cuenta ciclos y genera las marcas de 100 ms, 250 ms, 500 ms y 1 s (un ciclo cada una) y las fases de
1 Hz, 2 Hz y 5 Hz (mitad encendido, mitad apagado).

### entradas.c
- 10 entradas digitales activas en bajo. Antirrebote: el valor cambia cuando 2 lecturas seguidas
  coinciden (20 ms). Arranque, cambio y reset generan un evento en el flanco de presión.
- El estado inicial se toma del pin real, así que encender la placa con la ignición puesta no genera un
  flanco falso.
- ADC: promedio móvil de 8 lecturas → porcentaje con `ADC_MIN`/`ADC_MAX` → nivel 0..4 con umbrales de
  5/25/50/75 % e histéresis de 3 %.

### teclado.c
Máquina de estados que barre una fila del PCF8574 por ciclo. Al completar las 4 filas evalúa la tecla y
la entrega (`tecla` + `tecla_valida` durante un ciclo) si se repitió en dos barridos y no se había
entregado ya. Busca el chip en 0x20 y, si no responde, en 0x38. Sin chip, el teclado queda deshabilitado
y la ECU sigue funcionando. Cada transferencia I2C tiene un timeout de 2 ms; si una falla, el teclado se
pausa 1 s antes de reintentar (ver sección 7).

### fsm_diag.c
- **Clave:** en NEUTRAL o BLOQUEADO, mientras el sistema esté bloqueado, guarda hasta 4 dígitos; `*` y
  `D` borran; `#` compara con `2026`. Si coincide, `sys_unlocked = true`.
- **Rutina:** con el sistema desbloqueado, las teclas 1 a 4 en NEUTRAL o MODO_PRUEBAS eligen la rutina.
  Fuera de MODO_PRUEBAS la rutina vuelve a "menú".
- **Rutina 4:** `fsm_diag` no toca `flag_servicio`; solo levanta la petición `quitar_servicio` durante
  un ciclo. Quien apaga la bandera es `km_rpm`, su único escritor.
- En APAGADO vuelve a bloquear y borra lo escrito.

### fsm_ecu.c
Máquina de 8 estados (sección 6). También lleva la marcha: con `btn_cambio`, sube si `sentido_cambio`
está activo y baja si no, acotada de 1 a 4 (no cambia en APAGADO ni en MODO_PRUEBAS).

### km_rpm.c
- RPM: rampa de 20 RPM por ciclo (2000 RPM/s) hacia el objetivo del estado (ralentí 1000 en ARRANQUE y
  FRENADO; 3000/5000/7000/9000 en EN_MARCHA según la marcha; 0 en los demás).
- Km: suma millonésimas de vuelta según la velocidad (`velocidad × RUEDA_RPS_MAX_X100 / 100` por ciclo).
  Cada vuelta completa suma 125 m a `km_m` (8 vueltas = 1 km).
- Servicio: `flag_servicio` se activa cada vez que `km_m` cruza un múltiplo de 16 km (16, 32, 48...).
  Cuando llega la petición `quitar_servicio` (rutina 4), apaga la bandera sin borrar los km. A velocidad
  máxima, 16 km (128 vueltas) toman ~65 s.

### persistencia.c
Único módulo que usa la NVS. Al arrancar carga los 3 valores; en el flanco de apagado de la ignición
guarda los que cambiaron; con `btn_reset` y la ignición apagada borra la NVS y reinicia el ESP32.

### servos.c
LEDC a 50 Hz y 14 bits. La velocidad va de 0 a 10000 (centésimas de %, para que todos los porcentajes
den pasos enteros). Sube 50 por ciclo (50 %/s) hasta `marcha × 2500` y baja según el freno: 75 por ciclo
con el delantero (−75 %/s), 25 con el trasero (−25 %/s), 100 con ambos (ABS, −100 %/s) y 30 sin freno
(−30 %/s). El pulso es `neutro ± velocidad × 400 / 10000` µs, reflejado en el servo derecho. En APAGADO,
sin pulsos. En MODO_PRUEBAS solo se mueven con la rutina 3 (2 s en cada marcha). El PWM solo se escribe
cuando el pulso cambia y con al menos 30 ms entre escrituras (ver sección 7).

### Rutinas del modo de pruebas

| Tecla | Rutina | Quién la ejecuta |
|---|---|---|
| 1 | Barrido de las 9 salidas de LEDs (incluye el RGB) | salidas_leds |
| 2 | Display con todos los segmentos (`8.8.8.8.8.8.8.8.`) | display |
| 3 | Servos en cada marcha, 2 s por marcha | servos |
| 4 | Desactiva el aviso de servicio sin borrar los km | fsm_diag lo pide, km_rpm lo apaga |
| D | Sale a NEUTRAL | fsm_ecu |

### salidas_leds.c
Calcula 16 bits (barra de gasolina, RGB con prioridad, direccionales) y los envía a los 74HCT595 solo si
cambiaron. Hace parpadear el LED de vida a 1 Hz. En la rutina 1 hace un barrido de las 9 salidas.

### display.c
Cada 100 ms compone un texto de 8 caracteres según el estado, lo convierte a segmentos y lo envía al
MAX7219 si cambió. Cada 2 s reconfigura el chip.

## 6. Máquina de estados principal

Prioridad de evaluación en cada ciclo (el orden de los `if` en `siguiente_estado()`):

1. Ignición apagada → **APAGADO**
2. Gasolina en nivel 0 → **SIN_COMBUSTIBLE**
3. Algún freno con la moto en ARRANQUE, EN_MARCHA o FRENADO → **FRENADO**
4. Modo de pruebas (teclas)
5. Operación normal

| Estado actual | Condición | Siguiente |
|---|---|---|
| APAGADO | ignición encendida y gasolina > 0 | NEUTRAL |
| SIN_COMBUSTIBLE | gasolina > 0 | NEUTRAL |
| NEUTRAL | tecla (ni `*` ni D), sistema bloqueado | BLOQUEADO |
| NEUTRAL | tecla 1–4, sistema desbloqueado | MODO_PRUEBAS |
| NEUTRAL | botón de arranque | ARRANQUE |
| BLOQUEADO | clave `2026#` correcta | MODO_PRUEBAS (rutina menú) |
| BLOQUEADO | tecla `*` o D | NEUTRAL |
| MODO_PRUEBAS | tecla D | NEUTRAL |
| ARRANQUE | 1 s en el estado | EN_MARCHA |
| FRENADO | frenos sueltos y velocidad > 0 | EN_MARCHA |
| FRENADO | frenos sueltos y velocidad = 0 | NEUTRAL |

Comportamiento de las salidas por estado:

| Estado | Servos | RPM | LED RGB | Display |
|---|---|---|---|---|
| APAGADO | sin pulsos | 0 | apagado | apagado |
| BLOQUEADO | neutro | → 0 | rojo | `PIn --` |
| NEUTRAL | neutro | → 0 | blanco (azul con servicio) | km |
| ARRANQUE | acelera hacia la marcha | → 1000 | verde (azul con servicio) | RPM y marcha |
| EN_MARCHA | velocidad de la marcha | → 3000…9000 | verde (azul con servicio) | RPM y marcha |
| FRENADO | delantero −75 %/s, trasero −25 %/s, ambos −100 %/s (ABS) | → 1000 | rojo; a 5 Hz con ABS | RPM y marcha |
| MODO_PRUEBAS | solo rutina 3 | → 0 | blanco | `PruEbA n` / `88888888` |
| SIN_COMBUSTIBLE | desacelera hasta parar | → 0 | rojo | km |

"→ 0" significa que las RPM bajan con la rampa de 2000 RPM/s; solo en APAGADO pasan a 0 de inmediato.

## 7. Tiempo de ejecución del ciclo y watchdog

### Medición

`main.c` mide con `esp_timer_get_time()` cuánto tarda **cada módulo** y el ciclo completo. Cada 5 s
imprime en el monitor serial:

```
I (...) main: estado=EN_MARCHA marcha=3 rpm=7000 km=4125 m gas=3 (adc 2650)
I (...) main: peor us: tiempos=2 entradas=40 teclado=600 fsm_diag=3 fsm_ecu=5 km_rpm=4 ...
I (...) main: ciclo peor=950 us, ciclos de mas de 10 ms=0
```

Los módulos se ejecutan desde una tabla (`MODULOS[]` en `main.c`) que define el orden del ciclo.

### Por qué se disparaba el task watchdog

En la primera prueba en la placa, el watchdog se disparaba cada 5 s en `ciclo_ecu` (CPU 1) y el peor
ciclo medía 52,603 µs. El watchdog vigila la tarea IDLE de cada núcleo. `vTaskDelayUntil()` solo bloquea
si la hora de despertar todavía no llega: cuando un ciclo se pasa de 10 ms, regresa sin bloquear. Si eso
se repite, `ciclo_ecu` (prioridad 5) nunca suelta la CPU 1, IDLE1 no corre en 5 s y el watchdog se
dispara.

**Causa confirmada con la medición por módulo** (segunda prueba en la placa):

```
peor us: ... fsm_ecu=8508 ... servos=39917 ...
ciclo peor=40051 us, ciclos de mas de 10 ms=499
```

`servos.c` tardaba ~40 ms en **cada** ciclo. En el ESP32, `ledc_update_duty()` espera activamente
(`while (duty_start)`, dentro de una sección crítica) a que el hardware aplique el duty anterior, lo que
ocurre hasta el inicio del siguiente periodo del PWM: hasta 20 ms a 50 Hz. Como `servos.c` escribía los
dos canales en cada ciclo de 10 ms, aunque el valor no cambiara, cada ciclo esperaba hasta 20 ms por
canal.

**Corrección:** la velocidad se sigue calculando cada 10 ms, pero el PWM solo se escribe cuando el pulso
cambia y con al menos 3 ciclos (30 ms) entre escrituras (`SERVO_CICLOS_ENTRE_ESCRITURAS`). Para entonces
ya pasó un periodo completo y `ledc_update_duty()` no espera. El servo lee el pulso a 50 Hz, así que
actualizarlo cada 30 ms no cambia su movimiento.

Además se corrigieron otras dos operaciones que pueden alargar el ciclo:

| Causa | Detalle | Corrección |
|---|---|---|
| `ESP_LOG` dentro del ciclo | Sin el driver de UART, cada carácter espera a que haya lugar en la FIFO de 128 bytes a 115200 baudios (~87 µs por carácter): un reporte de 250 caracteres son ~20 ms de espera activa | `main.c` instala el driver de UART con un buffer de 4 KB; `ESP_LOG` copia el texto y regresa |
| I2C del teclado | Si el PCF8574 no contesta (NACK), el driver de ESP-IDF espera activamente hasta el timeout en cada transferencia, y se repetía en cada ciclo | Timeout de 2 ms (una transferencia dura ~0.2 ms) y, tras un error, pausa de 1 s antes de reintentar, con un solo aviso en el log |

Además, el ciclo ahora **siempre bloquea**: si `xTaskDelayUntil()` regresa sin haber esperado (ciclo de
más de 10 ms), la tarea cede un tick con `vTaskDelay(1)` y toma la hora actual como nueva referencia.
Así IDLE1 corre en todos los ciclos aunque un módulo se pase. Cada caso se cuenta en "ciclos de mas de
10 ms" del reporte. **El watchdog sigue activo** (`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=y`).

### Costo esperado por módulo

| Módulo | Costo aprox. por ciclo |
|---|---|
| entradas (10 GPIO + 1 ADC) | < 0.05 ms |
| teclado (1 escritura + 1 lectura I2C a 100 kHz) | ~0.6 ms |
| lógica de control | < 0.05 ms |
| servos (4 llamadas LEDC, solo si el pulso cambia y cada 30 ms como mínimo) | < 0.05 ms |
| salidas_leds (16 bits por GPIO, solo si cambian) | < 0.05 ms |
| display (8 palabras SPI a 1 MHz, cada 100 ms y solo si cambian) | < 0.3 ms |
| **Total típico** | **~1 ms de 10 ms** |
| persistencia al apagar la ignición (una vez) | algunos ms; si la flash tiene que borrar una página puede pasar de 10 ms en ese único ciclo |

Esta tabla es una estimación; el reporte de cada 5 s da los valores reales en la placa.

### Resultado en la placa (después de la corrección de servos)

```
peor us: tiempos=8 entradas=98 teclado=1 fsm_diag=8 fsm_ecu=2 km_rpm=9 persistencia=1 servos=5 salidas_leds=33 display=481
ciclo peor=599 us, ciclos de mas de 10 ms=0
```

El ciclo usa ~0.6 ms de los 10 ms y el watchdog ya no se dispara. En los primeros 5 s después de
encender hay un ciclo de ~9.5 ms, por la inicialización (primera lectura del ADC y primer mensaje de
`fsm_ecu`); no se repite. Falta medir con la ignición apagándose (escritura en la NVS) y con el teclado
conectado.

## 8. Cómo compilar y grabar

Desde VS Code con la extensión ESP-IDF: **Build**, **Flash** y **Monitor**. Desde PowerShell:

```powershell
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
idf.py set-target esp32      # solo la primera vez
idf.py build
idf.py -p COM5 flash monitor # cambia COM5 por el puerto de la placa
```

Si se cambia `sdkconfig.defaults` después de haber compilado, hay que borrar `sdkconfig` (o ejecutar
`idf.py fullclean`) para que los nuevos valores se apliquen.

## 9. Probar módulos por separado (semana 1)

Todos los módulos se llaman desde el ciclo de `main.c`. Para probar uno solo basta comentar en
`ciclo_ecu()` las llamadas que no se necesitan y, si hace falta, fijar a mano los campos que leería:

```c
// Probar servos sin entradas ni máquina de estados:
tiempos_actualizar(e);
e->estado = ESTADO_EN_MARCHA;
e->marcha = 2;
servos_actualizar(e);
```

El log de cada 5 s y los mensajes de cada módulo (`ESP_LOGI`) se ven en el monitor serial.
