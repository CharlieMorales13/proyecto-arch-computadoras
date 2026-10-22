#include "display.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pines.h"

#if LCD_I2C
#include "bus_i2c.h"
#include "driver/i2c_master.h"
#else
#include "driver/gpio.h"
#endif

static const char *TAG = "display";

#define COLS    16
#define FILAS   2

/* Comandos del controlador HD44780 */
#define CMD_BORRAR          0x01
#define CMD_MODO_ENTRADA    0x06    /* el cursor avanza a la derecha */
#define CMD_DISPLAY_ON      0x0C    /* pantalla encendida, sin cursor */
#define CMD_FUNCION_4BITS   0x28    /* bus de 4 bits, 2 renglones, 5x8 */
#define CMD_DDRAM           0x80    /* | dirección: renglón 0 = 0x00, renglón 1 = 0x40 */

#define CAR_BLOQUE          ((char)0xFF)    /* cuadro lleno en la ROM de caracteres */

static bool s_habilitado;
static bool s_compuesto;
static char s_pantalla[FILAS][COLS];    /* lo que ya está escrito en la LCD; 0 = desconocido */
static uint16_t s_pausa;                /* ciclos que faltan para reintentar tras un error */
static bool s_en_error;
static int s_paso = -1;                 /* paso de re-sincronización en curso; -1 = ninguno */

/* ============================ Capa física ============================
 * Las dos versiones ofrecen lo mismo: poner_nibble() y enviar().
 * Con I2C los nibbles se acumulan en un búfer y se mandan en UNA
 * transferencia; en paralelo se escriben al momento en los GPIO. */
#if LCD_I2C
/* Módulo PCF8574 típico: P0 = RS, P1 = RW, P2 = E, P3 = luz, P4..P7 = D4..D7 */
#define BIT_RS      0x01
#define BIT_E       0x04
#define BIT_LUZ     0x08

static i2c_master_dev_handle_t s_lcd;
static uint8_t s_buf[4 * (1 + LCD_CARACTERES_POR_CICLO)];  /* 4 bytes de I2C por byte de LCD */
static size_t s_n;
static uint8_t s_luz = BIT_LUZ;

/* Cada nibble son dos escrituras al PCF8574: con E en 1 y con E en 0 (la LCD
 * lee en el flanco de bajada). A 100 kHz cada byte de I2C dura ~90 µs, más
 * que los 37 µs que necesita la LCD entre comandos. */
static void poner_nibble(uint8_t nib, bool rs)
{
    const uint8_t v = (uint8_t)(nib << 4) | (rs ? BIT_RS : 0) | s_luz;
    if (s_n + 2 <= sizeof(s_buf)) {
        s_buf[s_n++] = v | BIT_E;
        s_buf[s_n++] = v;
    }
}

static esp_err_t enviar(void)
{
    if (s_n == 0) {
        return ESP_OK;
    }
    const esp_err_t err = i2c_master_transmit(s_lcd, s_buf, s_n, LCD_ESPERA_MS);
    s_n = 0;
    return err;
}

static esp_err_t buscar_lcd(i2c_master_bus_handle_t bus, uint16_t *dir)
{
    const uint16_t primero[2] = { LCD_DIRECCION, LCD_DIRECCION_A };
    for (int i = 0; i < 2; i++) {
        if (i2c_master_probe(bus, primero[i], 20) == ESP_OK) {
            *dir = primero[i];
            return ESP_OK;
        }
    }
    /* Otras direcciones posibles del PCF8574 / PCF8574A, sin las del teclado */
    for (uint16_t a = 0x20; a <= 0x3F; a++) {
        if ((a > 0x27 && a < 0x38) || a == PCF8574_DIRECCION || a == PCF8574A_DIRECCION) {
            continue;
        }
        if (i2c_master_probe(bus, a, 20) == ESP_OK) {
            *dir = a;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t iniciar_fisico(void)
{
    i2c_master_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(bus_i2c_obtener(&bus), TAG, "bus I2C");

    uint16_t dir = 0;
    if (buscar_lcd(bus, &dir) != ESP_OK) {
        ESP_LOGE(TAG, "la LCD no responde en el bus I2C (SDA %d, SCL %d)", PIN_I2C_SDA, PIN_I2C_SCL);
        return ESP_ERR_NOT_FOUND;
    }
    const i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dir,
        .scl_speed_hz = LCD_I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev, &s_lcd), TAG, "dispositivo LCD");
    ESP_LOGI(TAG, "LCD I2C en 0x%02X", dir);
    return ESP_OK;
}

#else  /* LCD en paralelo, 4 bits */

static const gpio_num_t PINES_D[4] = { PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7 };

static void poner_nibble(uint8_t nib, bool rs)
{
    gpio_set_level(PIN_LCD_RS, rs);
    for (int i = 0; i < 4; i++) {
        gpio_set_level(PINES_D[i], (nib >> i) & 1u);
    }
    gpio_set_level(PIN_LCD_E, 1);
    esp_rom_delay_us(1);                /* pulso de E de al menos 450 ns */
    gpio_set_level(PIN_LCD_E, 0);
    esp_rom_delay_us(1);
}

static esp_err_t enviar(void)
{
    return ESP_OK;                      /* ya se escribió en poner_nibble() */
}

static esp_err_t iniciar_fisico(void)
{
    uint64_t mascara = (1ULL << PIN_LCD_RS) | (1ULL << PIN_LCD_E);
    for (int i = 0; i < 4; i++) {
        mascara |= 1ULL << PINES_D[i];
    }
    const gpio_config_t cfg = {
        .pin_bit_mask = mascara,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio de la LCD");
    gpio_set_level(PIN_LCD_E, 0);
    ESP_LOGI(TAG, "LCD en paralelo (RS %d, E %d, D4..D7 %d %d %d %d)", PIN_LCD_RS, PIN_LCD_E,
             PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
    return ESP_OK;
}
#endif

static void poner_byte(uint8_t b, bool rs)
{
    poner_nibble(b >> 4, rs);
    poner_nibble(b & 0x0F, rs);
#if !LCD_I2C
    esp_rom_delay_us(50);               /* la LCD tarda 37 µs en ejecutar cada byte */
#endif
}

/* ======================= Inicialización del HD44780 =======================
 * Secuencia del datasheet para pasar a 4 bits desde CUALQUIER estado: tres
 * veces 0x3 y luego 0x2. Como funciona aunque la LCD esté desfasada, se
 * repite cada 2 s (un paso por ciclo) para que el ruido del motor y del relé
 * no deje basura en pantalla. No borra la memoria de la LCD. */
typedef struct {
    uint8_t valor;
    bool nibble;                        /* true = solo medio byte (inicio) */
} paso_t;

static const paso_t SECUENCIA[] = {
    { 0x3, true }, { 0x3, true }, { 0x3, true }, { 0x2, true },
    { CMD_FUNCION_4BITS, false }, { CMD_DISPLAY_ON, false }, { CMD_MODO_ENTRADA, false },
};
#define NUM_PASOS   ((int)(sizeof(SECUENCIA) / sizeof(SECUENCIA[0])))

static esp_err_t enviar_paso(int i)
{
    if (SECUENCIA[i].nibble) {
        poner_nibble(SECUENCIA[i].valor, false);
    } else {
        poner_byte(SECUENCIA[i].valor, false);
    }
    return enviar();
}

esp_err_t display_iniciar(void)
{
    ESP_RETURN_ON_ERROR(iniciar_fisico(), TAG, "LCD");

    /* Antes del ciclo de 10 ms sí se puede esperar */
    vTaskDelay(pdMS_TO_TICKS(50));      /* > 40 ms desde que la LCD recibe 5 V */
    for (int i = 0; i < NUM_PASOS; i++) {
        ESP_RETURN_ON_ERROR(enviar_paso(i), TAG, "inicialización");
        vTaskDelay(pdMS_TO_TICKS(5));   /* > 4.1 ms */
    }
    poner_byte(CMD_BORRAR, false);
    ESP_RETURN_ON_ERROR(enviar(), TAG, "borrar");
    vTaskDelay(pdMS_TO_TICKS(5));       /* borrar tarda 1.52 ms */

    memset(s_pantalla, ' ', sizeof(s_pantalla));
    s_habilitado = true;
    return ESP_OK;
}

/* ============================ Contenido ============================ */

static const char *nombre_corto(const EstadoECU *e)
{
    switch (e->estado) {
    case ESTADO_APAGADO:         return "APAGADO";
    case ESTADO_BLOQUEADO:       return "BLOQUEADO";
    case ESTADO_NEUTRAL:         return "NEUTRAL";
    case ESTADO_ARRANQUE:        return "ARRANQUE";
    case ESTADO_EN_MARCHA:       return "EN MARCHA";
    case ESTADO_FRENADO:         return (e->freno_del && e->freno_tra) ? "FRENO ABS" : "FRENADO";
    case ESTADO_MODO_PRUEBAS:    return "MODO PRUEBAS";
    case ESTADO_SIN_COMBUSTIBLE: return "SIN GASOLINA";
    default:                     return "?";
    }
}

/* Copia el texto a un renglón y rellena con espacios hasta 16 columnas */
static void renglon(char destino[COLS], const char *txt)
{
    size_t n = strlen(txt);
    if (n > COLS) {
        n = COLS;
    }
    memcpy(destino, txt, n);
    memset(destino + n, ' ', COLS - n);
}

/*   ARRANQUE, EN_MARCHA, FRENADO     "EN MARCHA    M3" / "5000rpm   12.3km"
 *   NEUTRAL, SIN_COMBUSTIBLE         "NEUTRAL      M1" / "SERV     12.3km"
 *   BLOQUEADO                        "BLOQUEADO"       / "Clave: **"
 *   MODO_PRUEBAS                     "MODO PRUEBAS"    / "Rutina n"; rutina 2 = todo encendido
 *   APAGADO                          en blanco y sin luz de fondo */
static void componer(const EstadoECU *e, char lcd[FILAS][COLS])
{
    char l1[40] = "";
    char l2[40] = "";
    const unsigned long decimas = e->km_m / 100u;
    const unsigned long km = decimas / 10u;

    switch (e->estado) {
    case ESTADO_APAGADO:
        break;

    case ESTADO_ARRANQUE:
    case ESTADO_EN_MARCHA:
    case ESTADO_FRENADO:
        snprintf(l1, sizeof(l1), "%-12s M%u", nombre_corto(e), e->marcha);
        snprintf(l2, sizeof(l2), "%4urpm %4lu.%lukm", e->rpm, km % 10000u, decimas % 10u);
        break;

    case ESTADO_BLOQUEADO:
        snprintf(l1, sizeof(l1), "%s", nombre_corto(e));
        snprintf(l2, sizeof(l2), "Clave: %.*s", e->clave_digitos, "****");
        break;

    case ESTADO_MODO_PRUEBAS:
        if (e->rutina == RUTINA_DISPLAY) {
            memset(lcd, CAR_BLOQUE, FILAS * COLS);  /* prueba de todos los puntos */
            return;
        }
        snprintf(l1, sizeof(l1), "%s", nombre_corto(e));
        if (e->rutina == RUTINA_MENU) {
            snprintf(l2, sizeof(l2), "Elija rutina 1-4");
        } else {
            snprintf(l2, sizeof(l2), "Rutina %u", e->rutina);
        }
        break;

    default:    /* NEUTRAL y SIN_COMBUSTIBLE: kilómetros con una décima */
        snprintf(l1, sizeof(l1), "%-12s M%u", nombre_corto(e), e->marcha);
        snprintf(l2, sizeof(l2), "%-4s %6lu.%lukm", e->flag_servicio ? "SERV" : "", km % 1000000u, decimas % 10u);
        break;
    }
    renglon(lcd[0], l1);
    renglon(lcd[1], l2);
}

/* Escribe a lo más LCD_CARACTERES_POR_CICLO caracteres a partir del primero
 * que sea distinto de lo que ya está en pantalla. Así una pantalla completa
 * se reparte en varios ciclos (~60 ms) y ningún ciclo pasa de ~3 ms. */
static esp_err_t escribir_cambios(const EstadoECU *e)
{
    for (int f = 0; f < FILAS; f++) {
        for (int c = 0; c < COLS; c++) {
            if (e->lcd[f][c] == s_pantalla[f][c]) {
                continue;
            }
            int fin = c + LCD_CARACTERES_POR_CICLO;
            if (fin > COLS) {
                fin = COLS;
            }
            poner_byte(CMD_DDRAM | (f ? 0x40 : 0x00) | (uint8_t)c, false);
            for (int k = c; k < fin; k++) {
                poner_byte((uint8_t)e->lcd[f][k], true);
            }
            const esp_err_t err = enviar();
            if (err == ESP_OK) {
                memcpy(&s_pantalla[f][c], &e->lcd[f][c], (size_t)(fin - c));
            }
            return err;
        }
    }
    return ESP_OK;
}

static void manejar_error(esp_err_t err)
{
    if (err == ESP_OK) {
        if (s_en_error) {
            ESP_LOGI(TAG, "la LCD responde de nuevo");
            s_en_error = false;
        }
        return;
    }
    if (!s_en_error) {
        ESP_LOGW(TAG, "la LCD no responde (%s); reintento cada %d ms",
                 esp_err_to_name(err), LCD_PAUSA_CICLOS * CICLO_MS);
        s_en_error = true;
    }
    s_pausa = LCD_PAUSA_CICLOS;
    s_paso = 0;                         /* al volver se re-sincroniza y se redibuja */
}

void display_actualizar(EstadoECU *e)
{
    if (!s_habilitado) {
        return;
    }

    /* El texto se recalcula cada 100 ms (y en el primer ciclo) */
    if (e->tick_100ms || !s_compuesto) {
        componer(e, e->lcd);
        s_compuesto = true;
#if LCD_I2C
        s_luz = (e->estado == ESTADO_APAGADO) ? 0 : BIT_LUZ;
#endif
    }

    if (s_pausa > 0) {
        s_pausa--;
        return;
    }

    if (s_paso < 0 && (e->ciclos % LCD_REINICIO_CICLOS) == 0) {
        s_paso = 0;
    }

    esp_err_t err;
    if (s_paso >= 0) {
        /* Un paso de re-sincronización por ciclo (10 ms > 4.1 ms que pide la LCD) */
        err = enviar_paso(s_paso);
        if (err == ESP_OK && ++s_paso >= NUM_PASOS) {
            s_paso = -1;
            memset(s_pantalla, 0, sizeof(s_pantalla));     /* fuerza a redibujar todo */
        }
    } else {
        err = escribir_cambios(e);
    }
    manejar_error(err);
}
