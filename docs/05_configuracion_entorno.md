# Configuración del entorno

Pasos para dejar el proyecto funcionando en una computadora nueva (Windows). Se hacen una sola vez.

## 1. Instalar las herramientas

1. **Git** (https://git-scm.com) y **VS Code**.
2. En VS Code, instalar la extensión **ESP-IDF** de Espressif.
3. `Ctrl+Shift+P` → **ESP-IDF: Configure ESP-IDF Extension** → **Express** → versión **v6.1**.
   - Tiene que ser **v6.1**: el código usa los drivers nuevos (`i2c_master`, `adc_oneshot`,
     `esp_driver_*`) y con 5.x no compila.
   - Instalar en una ruta **sin espacios**; la que propone por defecto (`C:\esp`, `C:\Espressif`) sirve.

## 2. Descargar el proyecto

En una carpeta sin espacios ni acentos:

```powershell
cd C:\esp\proyectos
git clone https://github.com/CharlieMorales13/proyecto-arch-computadoras.git
```

En VS Code: **File → Open Folder** → `proyecto-arch-computadoras` (la carpeta que tiene `CMakeLists.txt`).

## 3. Configurar el proyecto

1. `Ctrl+Shift+P` → **ESP-IDF: Set Espressif Device Target** → **esp32** → "ESP32 chip (via ESP USB
   Bridge)". Esto crea `sdkconfig` a partir de `sdkconfig.defaults`.
2. Conectar el ESP32 por USB y ver en el **Administrador de dispositivos** qué puerto COM tiene. Si no
   aparece, instalar el driver **CP210x** o **CH340**, según el chip USB de la placa.
3. `Ctrl+Shift+P` → **ESP-IDF: Select Port to Use** → elegir ese COM.

## 4. Compilar, grabar y comprobar

Botón **ESP-IDF: Build, Flash and Monitor** (la flama en la barra inferior). Desde una terminal:

```powershell
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
idf.py build
idf.py -p COM5 flash monitor      # cambia COM5 por tu puerto
```

En el monitor debe aparecer `ECU digital para motocicleta - ESP32` y, cada 5 s, el reporte con
`ciclo peor=... us`. Si se ve eso, el entorno quedó listo.

## 5. Trabajar en equipo con git

Cada quien trabaja en sus módulos (responsables en [01_contexto_proyecto.md](01_contexto_proyecto.md)):

```powershell
git pull                          # siempre antes de empezar
# ...editar sus archivos...
git add main/salidas/servos.c     # solo lo que cambiaron
git commit -m "servos: calibración del neutro"
git push
```

- `main/comun/pines.h` y `main/comun/config.h` los usa todo el equipo: avisar antes de cambiarlos.
- `sdkconfig`, `build/` y `.vscode/settings.json` no se suben; cada computadora genera los suyos.

## Problemas comunes

| Síntoma | Solución |
|---|---|
| Error `La ECU necesita CONFIG_FREERTOS_HZ=1000` | Borrar `sdkconfig` y repetir el paso 3.1 |
| Errores de drivers al compilar (`i2c_master.h`, `adc_oneshot.h`) | No está instalado ESP-IDF v6.1; repetir el paso 1.3 |
| "Failed to connect" al grabar | Mantener presionado el botón **BOOT** de la placa al iniciar la grabación |
| "Port is busy" | Cerrar el monitor serial u otra ventana que use ese COM |
| La placa no arranca con todo conectado | Revisar que nada esté conectado a GPIO 0, 2 o 12 |
