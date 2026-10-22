# Avance con el hardware disponible (rama `avance-hardware`)

Para el avance del 3er parcial se armó la ECU con los componentes que ya tiene el equipo. La máquina de
estados, las RPM, el kilometraje, la memoria NVS y el modo de pruebas **no cambian**: solo se cambiaron los
módulos que tocan el hardware, igual que en la FPGA se cambia el XDC y el módulo de salida sin tocar la lógica.

| En el diseño (rama `main`) | En este avance | Archivo |
|---|---|---|
| 2 servos de giro continuo (LEDC) | Motor DC con reductor y llanta, encendido por un relé | `salidas/servos.c` |
| 2 × 74HCT595 | 7 LEDs directo a GPIO | `salidas/salidas_leds.c` |
| MAX7219 de 8 dígitos | LCD 16x2 (módulo I2C o paralelo de 4 bits) | `salidas/display.c` |
| Botón de cambio + switch de sentido | Eje Y del joystick | `entradas/entradas.c` |
| Teclado 4x4 con PCF8574 | No se conecta (el firmware lo detecta y sigue) | — |
| — | Bus I2C compartido entre LCD y teclado | `comun/bus_i2c.c` |

Sin cambios: `main.c`, `fsm_ecu.c`, `fsm_diag.c`, `km_rpm.c`, `tiempos.c`, `persistencia.c`.
`teclado.c` solo cambió para usar el bus compartido.

## Antes de grabar: dos opciones en el código

1. **Tipo de LCD** (`main/comun/pines.h`): viene en `0` porque la LCD del equipo es de 16 pines. Cambiar a
   `1` solo si se usa una LCD con módulo I2C de 4 pines (GND, VCC, SDA, SCL).
2. **Tipo de relé** (`main/comun/config.h`): `#define RELE_ACTIVO_EN_BAJO 1` para el módulo de relé común
   (se activa con 0 V). Si el relé hace clic al revés, cambiar a `0`.

## Conexiones

Todas las tierras unidas: ESP32, MB102, relé, LCD, joystick y sensores.

### Entradas

| Señal | Componente | Conexión | GPIO |
|---|---|---|---|
| Ignición | Botón de presionar | Pin a 3.3 V con resistencia de 10 kΩ; botón entre el pin y GND. Cada pulsación enciende o apaga (`IGNICION_CON_BOTON` en `config.h`; en 0 funciona con un cable o switch fijo) | 34 |
| Arranque | Botón del joystick (SW) | SW al pin, GND del joystick a GND | 16 |
| Cambio de marcha | Eje Y del joystick (VRy) | Joystick alimentado a **3.3 V** (no a 5 V) | 35 |
| Freno delantero | Microinterruptor de palanca | COM a GND, NO al pin | 17 |
| Freno trasero | Módulo TCRT5000 | VCC a **3.3 V**, DO al pin (acercar la mano = frenar) | 19 (I2C) / 39 (paralelo) |
| Gasolina | Potenciómetro de 10 kΩ | Extremos a 3.3 V y GND, cursor al pin | 32 |

### Salidas

| Señal | Componente | GPIO |
|---|---|---|
| Motor | Entrada IN del módulo de relé | 25 |
| Barra de gasolina | LED rojo, amarillo, verde, verde (cada uno con 220–470 Ω a GND) | 13, 14, 27, 26 |
| LED de estado | LED rojo, verde, azul (cada uno con 220–470 Ω a GND) | 18, 23, 5 |
| LED de vida | LED integrado de la placa | 2 |

### LCD con módulo I2C (`LCD_I2C 1`)

| LCD | Conexión |
|---|---|
| GND | GND |
| VCC | 5 V del MB102 |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

El firmware busca la LCD en 0x27 y 0x3F (y en las demás direcciones posibles); el monitor imprime cuál
encontró. Si la LCD enciende pero solo se ven cuadros o nada, girar el potenciómetro azul de contraste
que trae atrás el módulo.

### LCD en paralelo de 4 bits (`LCD_I2C 0`)

| Pin de la LCD | Conexión |
|---|---|
| 1 VSS | GND |
| 2 VDD | 5 V del MB102 |
| 3 V0 | Cursor de un potenciómetro de 5 kΩ (extremos a 5 V y GND): contraste |
| 4 RS | GPIO 19 |
| 5 RW | GND (solo se escribe) |
| 6 E | GPIO 4 |
| 7–10 D0–D3 | Sin conectar |
| 11 D4 | GPIO 15 |
| 12 D5 | GPIO 33 |
| 13 D6 | GPIO 21 |
| 14 D7 | GPIO 22 |
| 15 A | 5 V con resistencia de 220 Ω (luz de fondo) |
| 16 K | GND |

En este modo el freno trasero (TCRT5000) pasa al **GPIO 39**, porque el 19 lo usa la LCD.

### Alimentación del motor

- El MB102 da 5 V al relé, al motor y a la LCD. El ESP32 se alimenta por USB.
- Motor: 5 V del MB102 → contacto **COM** del relé → contacto **NO** → motor → GND.
- Un capacitor cerámico de 100 nF en paralelo con el motor (reduce el ruido de las escobillas).
- El motor nunca va a un pin del ESP32.
- Si el relé es solo el componente azul (sin placa), necesita un transistor NPN (2N2222 o BC547) con
  resistencia de 1 kΩ en la base y un diodo 1N4007 en paralelo con la bobina; en ese caso
  `RELE_ACTIVO_EN_BAJO 0`.

## Demostración (unos 2 minutos)

Con el monitor serial abierto (`idf.py -p <PUERTO> flash monitor`):

1. **Ignición.** Presionar el botón de ignición. El monitor muestra `APAGADO -> NEUTRAL`, el LED de estado se pone
   blanco (los tres encendidos) y la LCD muestra `NEUTRAL M1` y los km.
2. **Gasolina.** Girar el potenciómetro: la barra sube y baja. En reserva el LED rojo parpadea a 2 Hz; en
   vacío pasa a `SIN GASOLINA`, el LED de estado se pone rojo y no deja arrancar.
3. **Arranque.** Presionar el botón del joystick: `NEUTRAL -> ARRANQUE`, LED verde, el relé hace clic y la
   llanta gira. Al segundo pasa a `EN MARCHA`.
4. **Marchas.** Mover el joystick arriba o abajo y regresarlo al centro: la LCD muestra la marcha (M1–M4) y
   las RPM suben con rampa a 3000, 5000, 7000 o 9000.
5. **Frenos.** Presionar la palanca: `FRENADO`, LED rojo, la velocidad baja y el motor se detiene. Con la
   palanca y la mano frente al TCRT5000 a la vez: `FRENO ABS` en la LCD y el rojo parpadea a 5 Hz.
6. **Memoria.** Presionar el botón de ignición para apagar y otra vez para encender (o desconectar la placa): los km y la marcha se
   conservan porque se guardan en la NVS al apagar la ignición.

## Calibración

- **Joystick.** Al arrancar, el monitor imprime `joystick en reposo: N`. N debe quedar entre
  `JOY_CENTRO_MIN` y `JOY_CENTRO_MAX` (`config.h`). Si al empujar hacia arriba la marcha baja, cambiar
  `JOY_ALTO_SUBE` a `0`.
- **Gasolina.** Cada 5 s el monitor imprime `gas=N (adc M)`. Si el potenciómetro no llega a los extremos,
  ajustar `ADC_MIN` y `ADC_MAX`.

## Limitaciones conocidas (para decirlas en la presentación)

- El relé solo enciende o apaga el motor: la marcha se ve en la LCD y en las RPM, no en la velocidad de la
  llanta. Con un driver L298N o L9110S y PWM del LEDC el motor DC puede ir a la velocidad de cada marcha.
- Sin teclado no se puede entrar al modo de pruebas. El HC-05 puede sustituirlo más adelante (enviar
  `2026#` desde el celular).
- Direccionales, intermitentes y reset maestro no están conectados en este avance.
- El kilometraje sigue siendo simulado a partir de la velocidad calculada. El TCRT5000 podría contar las
  vueltas reales de la llanta.
- El módulo I2C de la LCD trae resistencias de pull-up a 5 V en SDA y SCL. En la práctica funciona con el
  ESP32, pero para la versión final conviene un convertidor de nivel (el mismo que usaría el MAX7219).
