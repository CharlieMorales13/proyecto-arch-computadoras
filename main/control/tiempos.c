#include "tiempos.h"

void tiempos_actualizar(EstadoECU *e)
{
    const uint32_t c = ++e->ciclos;

    e->tick_100ms = (c % 10u) == 0;
    e->tick_250ms = (c % 25u) == 0;
    e->tick_500ms = (c % 50u) == 0;
    e->tick_1s    = (c % 100u) == 0;

    /* Mitad del periodo encendido, mitad apagado */
    e->fase_1hz = (c % 100u) < 50u;     /* periodo 1 s   */
    e->fase_2hz = (c % 50u) < 25u;      /* periodo 500 ms */
    e->fase_5hz = (c % 20u) < 10u;      /* periodo 200 ms */
}
