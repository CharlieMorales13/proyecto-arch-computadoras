/*
 * km_rpm.h — Módulo aritmético de RPM y kilometraje
 * (equivale a contador_km_rpm en VHDL).
 *
 * RPM: rampa de 2000 RPM/s hacia el objetivo del estado:
 *      ARRANQUE y FRENADO -> ralentí (1000); EN_MARCHA -> 3000/5000/7000/9000
 *      según la marcha; resto -> 0 (motor apagado).
 * Km:  integra las vueltas simuladas a partir de la velocidad de las ruedas
 *      (8 vueltas = 1 km, 125 m por vuelta).
 * Servicio: flag_servicio se activa cada vez que el kilometraje cruza un
 *      múltiplo de 16 km (16, 32, 48...). Cuando fsm_diag levanta la petición
 *      quitar_servicio (rutina 4), km_rpm apaga la bandera sin borrar los
 *      kilómetros; el reset maestro borra todo.
 *
 * Escribe: rpm, km_m, flag_servicio.
 */
#pragma once

#include "estado.h"

void km_rpm_actualizar(EstadoECU *e);
