# ECU digital para motocicleta con ESP32

Proyecto de **Arquitectura de Computadoras**, Universidad Anáhuac Oaxaca, Facultad de Ingeniería.
Firmware en C con **ESP-IDF 6.1** y FreeRTOS para un ESP32 DevKit V1 (ESP-WROOM-32), sin Arduino.

Integrantes: Mario Barrita Sánchez Enríquez · Alejandro Cinco Prieto · Héctor Martínez Reyes ·
Carlos Antonio Gomez Morales. Docente: Mtro. Diego Ovadid Acevedo Cruz.

## Qué hace

Reproduce la Unidad de Control Electrónico (ECU) de una motocicleta:

- **Entradas:** ignición, arranque, frenos delantero y trasero, cambio de marcha y su sentido,
  direccionales, intermitentes, reset maestro, potenciómetro de gasolina y teclado 4x4.
- **Control:** máquina de 8 estados con prioridades, marchas de 1 a 4, RPM en rampa, frenado con ABS,
  kilometraje (8 vueltas = 1 km) con aviso de servicio cada 16 km y modo de pruebas con clave `2026#`.
- **Memoria:** marcha, kilometraje y aviso de servicio en la NVS (flash); sobreviven a quitar la energía.
- **Salidas:** 2 servos de giro continuo (ruedas), barra de gasolina, direccionales y LED RGB por dos
  74HCT595, y display de 8 dígitos con MAX7219.

## Arquitectura

Una sola tarea de FreeRTOS llama a los módulos en orden cada 10 ms. Todos comparten la estructura
`EstadoECU`, que funciona como banco de registros: cada campo tiene un solo módulo que lo escribe.

```
tiempos → entradas → teclado → fsm_diag → fsm_ecu → km_rpm → persistencia → servos → salidas_leds → display
```

```
├── CMakeLists.txt             proyecto ESP-IDF
├── sdkconfig.defaults         FreeRTOS a 1 kHz, CPU a 240 MHz, flash de 4 MB
├── docs/                      documentación del proyecto
└── main/
    ├── main.c                 inicializa, corre el ciclo de 10 ms y mide el tiempo de cada módulo
    ├── comun/                 pines.h, config.h, estado.h (EstadoECU), estado.c
    ├── entradas/              entradas.c (antirrebote + ADC), teclado.c (PCF8574)
    ├── control/               tiempos.c, fsm_diag.c, fsm_ecu.c, km_rpm.c
    ├── memoria/               persistencia.c (NVS)
    └── salidas/               servos.c (LEDC), salidas_leds.c (74HCT595), display.c (MAX7219)
```

## Documentación

| Documento | Contenido |
|---|---|
| [docs/01_contexto_proyecto.md](docs/01_contexto_proyecto.md) | Qué es el sistema, documentos de origen, equivalencia FPGA → ESP32, plan y responsables |
| [docs/02_investigacion_hardware.md](docs/02_investigacion_hardware.md) | Datos y problemas de cada componente: ESP32, servos, MAX7219, 74HCT595, PCF8574, alimentación |
| [docs/03_arquitectura_firmware.md](docs/03_arquitectura_firmware.md) | Ciclo de 10 ms, `EstadoECU`, dueños de cada campo, máquina de estados, watchdog y tiempos medidos |
| [docs/04_cambios_al_documento.md](docs/04_cambios_al_documento.md) | Diferencias con el Segundo Avance: lo resuelto, lo ajustado en el código y lo que falta agregar |
| [docs/05_configuracion_entorno.md](docs/05_configuracion_entorno.md) | Cómo instalar ESP-IDF 6.1, clonar, compilar y grabar el proyecto en una computadora nueva |

El mapa de pines está en [main/comun/pines.h](main/comun/pines.h) y los valores numéricos (umbrales,
RPM, calibración de servos) en [main/comun/config.h](main/comun/config.h).

## Compilar y grabar

Requisitos: ESP-IDF **v6.1** (por ejemplo con la extensión ESP-IDF de VS Code o el instalador EIM).

```bash
git clone <url-del-repositorio>
cd proyecto-arquitectura-computadora
idf.py set-target esp32      # la primera vez; genera sdkconfig a partir de sdkconfig.defaults
idf.py build
idf.py -p <PUERTO> flash monitor
```

En Windows, antes de `idf.py`, activa el entorno en PowerShell (la ruta depende de tu instalación):

```powershell
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
```

`sdkconfig` y `.vscode/settings.json` no se suben al repositorio: se generan en cada computadora.
`sdkconfig.defaults` sí se sube y fija lo importante, sobre todo `CONFIG_FREERTOS_HZ=1000` (el código
no compila sin él).

## Estado

- Los 11 módulos están escritos y alineados con el Segundo Avance (18 de octubre de 2026).
- Compila sin errores ni advertencias con ESP-IDF 6.1.
- Probado en la placa: el ciclo tarda ~0.6 ms de los 10 ms disponibles y el watchdog no se dispara
  (ver [docs/03](docs/03_arquitectura_firmware.md), sección 7).

Pendiente:

- Conectar y probar el teclado (el PCF8574 todavía no responde en la placa).
- Calibrar en `config.h` el neutro de cada servo y el rango del potenciómetro.
- Medir el ciclo al apagar la ignición (escritura en la NVS).
- Integración completa en protoboard y maqueta.
