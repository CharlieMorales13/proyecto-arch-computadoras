#include "estado.h"

static const char *const NOMBRES[ESTADO_TOTAL] = {
    [ESTADO_APAGADO]         = "APAGADO",
    [ESTADO_BLOQUEADO]       = "BLOQUEADO",
    [ESTADO_NEUTRAL]         = "NEUTRAL",
    [ESTADO_ARRANQUE]        = "ARRANQUE",
    [ESTADO_EN_MARCHA]       = "EN_MARCHA",
    [ESTADO_FRENADO]         = "FRENADO",
    [ESTADO_MODO_PRUEBAS]    = "MODO_PRUEBAS",
    [ESTADO_SIN_COMBUSTIBLE] = "SIN_COMBUSTIBLE",
};

const char *estado_nombre(estado_ecu_t e)
{
    return (e < ESTADO_TOTAL) ? NOMBRES[e] : "?";
}
