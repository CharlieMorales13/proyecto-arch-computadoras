# Investigación de hardware

Para cada componente de la lista de materiales: qué es, sus datos eléctricos relevantes, cómo se conecta
al ESP32, cómo lo usa el firmware y qué problemas encontramos. Los datos vienen de las hojas de datos
citadas al final. Los valores de corriente marcados como *aprox.* dependen del fabricante del módulo y
hay que confirmarlos con el multímetro.

---

## 1. ESP32 DevKit V1 (ESP-WROOM-32, 38 pines)

| Dato | Valor |
|---|---|
| CPU | Xtensa LX6 de 2 núcleos, hasta 240 MHz |
| Memoria | 520 KB de SRAM, 4 MB de flash SPI (en el módulo) |
| Lógica | 3.3 V. **Las entradas no toleran 5 V.** |
| Alimentación | USB (5 V) o pin VIN (5 V) → regulador AMS1117 a 3.3 V |
| Corriente por GPIO | Recomendado ≤ 20 mA; máximo absoluto 40 mA |
| Salida en alto | ≥ 2.64 V (0.8 × VDD) |

**Periféricos que usa la ECU:**

| Periférico | Uso | API de ESP-IDF 6.1 |
|---|---|---|
| GPIO | 10 entradas, 74HCT595, LED de vida | `driver/gpio.h` |
| ADC1 (SAR de 12 bits) | Potenciómetro en GPIO 32 (canal 4) | `esp_adc/adc_oneshot.h` |
| LEDC | 2 PWM de 50 Hz para los servos | `driver/ledc.h` |
| SPI3 (VSPI) | MAX7219 | `driver/spi_master.h` |
| I2C | PCF8574 | `driver/i2c_master.h` |
| esp_timer | Medir la duración del ciclo | `esp_timer.h` |
| NVS (flash) | Marcha, km y servicio | `nvs_flash.h`, `nvs.h` |

**Restricciones de pines** (base del mapa de `main/comun/pines.h`):

- **6 a 11:** conectados a la flash interna. No se tocan.
- **0, 2, 5, 12, 15:** pines de arranque (*strapping*). El 12 en alto al encender cambia el voltaje
  de la flash y la placa no arranca; el 0 en bajo entra en modo de programación. Usamos 2 (LED de la
  placa, ya previsto por el fabricante), 5 (CS del MAX7219) y 15 (reset maestro con pull-up), que son
  seguros con ese uso.
- **1 y 3:** UART del USB (monitor serial).
- **34, 35, 36, 39:** solo entrada y **sin pull-up interna**. Llevan 10 kΩ externas.
- **36 y 39:** errata 3.11 del ESP32: cuando se enciende el ADC reciben un pulso falso a bajo de ~80 ns.
  Con lectura por sondeo cada 10 ms y antirrebote no afecta; con interrupciones sí.
- **5 y 14:** emiten pulsos durante el arranque del ESP32 (ver sección 4 y 5).
- **ADC2** no funciona mientras el Wi-Fi está activo; por eso el potenciómetro va en ADC1 (GPIO 32).

**ADC:** con atenuación de 12 dB (`ADC_ATTEN_DB_12`) mide de 0 a ~3.1 V. Es poco lineal en los
extremos: el último tramo del potenciómetro satura en 4095. Para un indicador de gasolina de 5 niveles
no importa; se calibra `ADC_MIN`/`ADC_MAX` y se promedian 8 lecturas para quitar ruido.

---

## 2. Servomotores de giro continuo (FS90R o SG90 360°)

| Dato | Valor |
|---|---|
| Alimentación | 4.8 a 6 V (usamos la fuente de 5 V) |
| Velocidad sin carga | ~110 rpm a 4.8 V, ~130 rpm a 6 V (≈ 2 vueltas/s) |
| Señal | PWM de 50 Hz (periodo 20 ms), nivel lógico de 3.3 V aceptado |
| Corriente | Unos cientos de mA en marcha; picos al arrancar o atorarse (*aprox.* 0.5–0.8 A) |

En un servo de giro continuo el ancho de pulso controla la **velocidad**, no el ángulo:

| Pulso | Efecto |
|---|---|
| ~1500 µs | Quieto (punto neutro; se ajusta con el tornillo del FS90R) |
| 1500 + Δ | Gira en un sentido; más Δ, más rápido |
| 1500 − Δ | Gira en el otro sentido |

La velocidad deja de aumentar alrededor de ±400 µs, por eso las marchas usan ±100, ±200, ±300 y
±400 µs. Los dos servos van en espejo en la maqueta, así que el derecho recibe el pulso reflejado.

**Cálculo de duty** (LEDC a 50 Hz con 14 bits = 16 384 cuentas en 20 000 µs, 1.22 µs por cuenta):

| Pulso | duty = t / 20 000 × 16 384 |
|---|---|
| 1500 µs (neutro) | 1229 |
| 1900 µs (marcha 4, izquierdo) | 1556 |
| 1100 µs (marcha 4, derecho) | 901 |

**Problemas y decisiones:**
- Los SG90 360° baratos no siempre traen tornillo de ajuste: su neutro real puede ser 1480 o 1520 µs.
  Por eso hay un neutro por servo en `config.h`.
- Con la ignición apagada se cortan los pulsos (duty 0): el servo se detiene aunque el neutro esté mal.
- Los picos de corriente hacen caer el voltaje y pueden reiniciar el ESP32 si comparten alimentación.
  Fuente de 5 V aparte, tierra común y 1000 µF junto a los servos.

---

## 3. MAX7219 con display de 8 dígitos

| Dato | Valor |
|---|---|
| Alimentación | 4.0 a 5.5 V (usamos 5 V) |
| Entrada en alto (VIH) | **mínimo 3.5 V** → el ESP32 (3.3 V) no llega: requiere convertidor de nivel |
| Reloj serie | hasta 10 MHz |
| Interfaz | Palabras de 16 bits: 4 bits sin uso, 4 de dirección, 8 de dato; MSB primero |
| Multiplexado | Interno: el chip refresca los 8 dígitos solo |
| Corriente | La fija la resistencia RSET del módulo. A brillo bajo (4/15) *aprox.* < 100 mA; a brillo máximo con todo encendido puede pasar de 300 mA |

**Funcionamiento:** el dato entra por DIN en cada flanco de subida de CLK con LOAD (CS) en bajo; al
subir LOAD se copia al registro indicado. Eso es exactamente SPI modo 0 con CS por hardware.

| Registro | Dirección | Valor que usamos |
|---|---|---|
| Dígitos 0–7 | 0x01–0x08 | Segmentos (0x01 = dígito de la derecha) |
| Decode mode | 0x09 | 0x00 (segmentos directos, para poder mostrar letras) |
| Intensidad | 0x0A | 4 |
| Scan limit | 0x0B | 7 (8 dígitos) |
| Shutdown | 0x0C | 1 (encendido) |
| Display test | 0x0F | 0 |

En modo sin decodificar cada bit es un segmento: `DP A B C D E F G` (bit 7 a 0). La tabla de caracteres
está en `main/salidas/display.c`.

**Problemas y decisiones:**
- El ruido de los servos en la alimentación de 5 V puede cambiar un registro de control (por ejemplo,
  apagar el display o cambiar el scan limit). El firmware reenvía la configuración cada 2 s.
- GPIO 5 (CS) emite pulsos al arrancar el ESP32; la configuración inicial limpia lo que haya llegado.
- Se usa un solo módulo, más uno de respaldo (Anexo A del Segundo Avance).

---

## 4. Convertidor de nivel lógico 3.3 V ↔ 5 V (4 canales, BSS138)

Cada canal es un MOSFET BSS138 con resistencias de 10 kΩ hacia 3.3 V (lado LV) y 5 V (lado HV).

| Conexión | Lado LV | Lado HV |
|---|---|---|
| Alimentación | LV = 3.3 V del ESP32 | HV = 5 V de la fuente |
| Canal 1 | GPIO 23 (DIN) | DIN del MAX7219 |
| Canal 2 | GPIO 18 (CLK) | CLK del MAX7219 |
| Canal 3 | GPIO 5 (CS) | LOAD del MAX7219 |
| Canal 4 | libre | libre |
| GND | común | común |

**Limitación:** la subida de la señal en el lado de 5 V la hace la resistencia de 10 kΩ, que es lenta.
Funciona bien hasta unos cientos de kHz o pocos MHz según el cableado; por eso el SPI va a **1 MHz**. A
esa velocidad, actualizar los 8 dígitos tarda menos de 0.3 ms.

---

## 5. Registros de corrimiento 74HCT595 (2 en cadena)

| Dato | 74HC595 | 74HCT595 |
|---|---|---|
| Alimentación | 2 a 6 V | 4.5 a 5.5 V |
| Entrada en alto (VIH) | 0.7 × VCC (2.3 V a 3.3 V; **3.5 V a 5 V**) | **2.0 V** |
| Corriente por salida | ±6 mA garantizados; máximo absoluto ±35 mA | igual |
| Corriente total por chip | máximo **70 mA** por VCC o GND | igual |

**Funcionamiento:** cada flanco de subida de SH_CP (CLK) mete un bit de DS (DATA) y corre los demás;
el bit que sale por Q7' entra al segundo chip. Un flanco de subida de ST_CP (LATCH) copia los 16 bits a
las salidas a la vez, así que los LEDs nunca muestran un valor a medias. OE va a GND y MR a VCC.

Asignación de salidas: Q0–Q3 barra de gasolina, Q4–Q6 RGB, Q7 direccional izquierda, Q8 (primera salida
del segundo chip) direccional derecha. Quedan 7 salidas libres.

**Problema encontrado: voltaje para el LED RGB.** Si los 595 se alimentan a 3.3 V, su salida en alto
es ~3.1 V con carga. El canal rojo (Vf ≈ 2.0 V) enciende bien, pero el verde y el azul (Vf ≈ 3.0–3.2 V)
casi no encienden. Sin verde ni azul no se puede mostrar blanco (NEUTRAL), verde (EN_MARCHA) ni azul
(servicio).

| Opción | Ventaja | Desventaja |
|---|---|---|
| Opción | Ventaja | Desventaja |
|---|---|---|
| **74HCT595 a 5 V** (elegida en el Segundo Avance) | Acepta los 3.3 V del ESP32 según la hoja de datos; el RGB enciende completo | Cambiar la pieza (mismo encapsulado y patas, sin cambios de código) |
| 74HC595 a 5 V | Sin cambiar la pieza | La hoja pide 3.5 V en alto y el ESP32 da 3.3 V: suele funcionar, pero fuera de especificación |
| 74HC595 a 3.3 V | Sin cambios | RGB incompleto |

**Resistencias de 470 Ω a 5 V** (Anexo A del Segundo Avance), cuidando el límite de 70 mA por chip (el
primer chip puede tener encendidos 4 LEDs de gasolina, 3 del RGB y 1 direccional a la vez):

| LED | Vf aprox. | Resistencia | Corriente |
|---|---|---|---|
| Rojo, amarillo, ámbar | 2.0 V | 470 Ω | ~6.4 mA |
| Verde estándar | 2.2 V | 470 Ω | ~6 mA |
| RGB rojo | 2.0 V | 470 Ω | ~6.4 mA |
| RGB verde y azul | 3.1 V | 470 Ω | ~4 mA |

Peor caso del primer chip ≈ 4 × 6.4 + 6.4 + 2 × 4 + 6.4 ≈ 46 mA, dentro del límite. Con 220 Ω a 5 V el
mismo caso pasaría de 90 mA; por eso se cambiaron las resistencias. Si el verde o el azul del RGB se ven
tenues junto al rojo, se puede bajar solo esos dos canales a 330 Ω (~5.8 mA) sin pasar el límite.

**Arranque:** GPIO 14 (CLK) emite pulsos al encender el ESP32 y mete bits basura al registro, pero no
pasan a las salidas porque GPIO 27 (LATCH) no se mueve. El primer `escribir_595(0)` los limpia.

---

## 6. Expansor PCF8574 y teclado 4x4

| Dato | Valor |
|---|---|
| Alimentación | 2.5 a 6 V (a 3.3 V, igual que el ESP32: no necesita convertidor) |
| Bus | I2C, **solo modo estándar de 100 kHz** |
| Dirección | PCF8574: 0x20–0x27; PCF8574A: 0x38–0x3F (pines A0–A2; los módulos suelen venir en 0x20) |
| Pines P0–P7 | Cuasi-bidireccionales: escribir 1 deja el pin como entrada con pull-up débil (~100 µA); escribir 0 lo pone en bajo (soporta hasta 25 mA) |

**Teclado:** 4 filas en P0–P3, 4 columnas en P4–P7. Para leer una fila se escribe 0 en esa fila y 1 en
todo lo demás, y se leen las columnas: la que esté en 0 es la tecla presionada.

```
escribir 1111 1110  -> fila 1 en bajo
leer     1101 1110  -> columna 2 en bajo -> tecla '2'
```

**Limitación:** a 100 kHz cada escritura o lectura de un byte tarda ~0.25 ms con el encabezado de
dirección. Barrer las 4 filas son 8 transferencias (~2.5 ms), un cuarto del ciclo de 10 ms. El firmware
barre **una fila por ciclo** (~0.6 ms) y completa el teclado cada 40 ms.

El módulo trae resistencias de pull-up de 10 kΩ en SDA y SCL, suficientes a 100 kHz.

---

## 7. Potenciómetro de 10 kΩ (gasolina)

Extremos a **3.3 V** y GND (nunca a 5 V: dañaría el GPIO 32), cursor a GPIO 32. 10 kΩ toma 0.33 mA y la
impedancia es suficientemente baja para el ADC. Un capacitor de 100 nF entre el cursor y GND reduce el
ruido, sobre todo con los servos cerca.

---

## 8. Interruptores, botones y antirrebote

Todos van **a GND** cuando se activan; la resistencia de pull-up mantiene el pin en 3.3 V en reposo.

```
3.3 V ── 10 kΩ ──┬── GPIO          (o pull-up interna del ESP32)
                 └── interruptor ── GND
```

Un contacto mecánico rebota durante 1 a 10 ms típicamente (Ganssle, 2008). Con lecturas cada 10 ms y
exigiendo 2 lecturas iguales, cualquier cambio de nivel tiene que durar al menos 10–20 ms para
aceptarse; los rebotes no pasan. La pulsación de un botón genera un evento de un solo ciclo (flanco).

Interruptores SPDT: común al GPIO, un extremo a GND y el otro sin conectar.

---

## 9. Alimentación

```
Fuente 5 V / 3 A ──┬── servos (con 1000 µF junto a ellos)
                   ├── MAX7219 (VCC)
                   ├── lado HV del convertidor de nivel
                   ├── 2 × 74HCT595
                   └── GND común con el ESP32
USB de la PC ───────── ESP32 DevKit (3.3 V para PCF8574, potenciómetro y lado LV)
```

| Consumidor | Corriente aprox. |
|---|---|
| 2 servos (picos) | hasta ~1.6 A |
| MAX7219 a brillo 4 | < 0.1 A |
| LEDs | < 0.1 A |
| ESP32 (sin Wi-Fi) | ~0.1 A (por USB) |
| **Total en la fuente de 5 V** | **< 2 A**, dentro de los 3 A |

- Un capacitor de **100 nF** junto al VCC de cada chip (2 × 595, PCF8574, MAX7219 si el módulo no lo
  trae).
- **No conectar** los 5 V de la fuente a VIN mientras el USB está conectado: según la versión de la
  placa no hay diodo entre ambos y una fuente alimentaría a la otra. Para la versión final sin USB, la
  fuente sí puede ir a VIN.

---

## 10. Memoria NVS (flash)

La NVS es una partición de 24 KB de la flash organizada como pares clave-valor, con nivelación de
desgaste. Cada sector de flash soporta del orden de 100 000 ciclos de borrado. Guardamos 3 valores solo
en el flanco de apagado de la ignición: aun con cientos de apagados al día, la flash dura años.

Escribir puede tardar varios milisegundos (más si hay que borrar una página); por eso no se hace en
cada ciclo.

---

## Referencias

- Analog Devices. *MAX7219/MAX7221: Serially interfaced, 8-digit LED display drivers* [Hoja de datos].
- Espressif Systems. *ESP-IDF Programming Guide (ESP32, v6.1)*. https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
- Espressif Systems. *ESP32 Series Datasheet*.
- Espressif Systems. *ESP32 Technical Reference Manual*.
- Espressif Systems. *ESP32 Series SoC Errata* (sección 3.11, GPIO 36 y 39).
- Ganssle, J. G. (2008). *A guide to debouncing* (Rev. 3). The Ganssle Group.
- Feetech. *FS90R continuous rotation servo* [Hoja de datos].
- Texas Instruments. *PCF8574 Remote 8-bit I/O expander for I2C bus* [Hoja de datos].
- Texas Instruments. *SN74HC595 8-bit shift registers with 3-state output registers* [Hoja de datos].
- Texas Instruments. *SN74HCT595* [Hoja de datos].
