/**
 * acs758_final.c
 * Leitura de corrente com módulo ACS758LCB
 * Auto-calibração do Vzero na inicialização
 *
 * Ligações:
 *   VCC  → 3.3V  (pino 36)
 *   GND  → GND   (pino 38)
 *   OUT2 → GP27  (pino 32, ADC1)
 *
 * IMPORTANTE: ao ligar, mantenha zero corrente nos terminais
 * IP+/IP− por pelo menos 3 segundos para a calibração concluir.
 *
 * CMakeLists.txt:
 *   target_link_libraries(acs758_final pico_stdlib hardware_adc)
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

/* ============================================================
 * Configuração
 * ============================================================ */
#define SENSITIVITY_MV_PER_A  40.0f   /* 40 = LCB-050 | 20 = LCB-100 */

#define ADC_VREF_V      3.3f
#define ADC_RESOLUTION  4096
#define ADC_CHANNEL     1      /* GP27 = ADC1 */
#define ADC_GPIO        9

/* Amostras por leitura normal */
#define NUM_SAMPLES          64

/* Amostras usadas na calibração do Vzero (mais = mais preciso) */
#define CALIB_SAMPLES        512

/* Intervalo entre leituras (ms) */
#define PRINT_INTERVAL_MS    500

/* Limiar de alerta (A) */
#define OVERCURRENT_LIMIT_A  45.0f

/* ============================================================
 * Lê N amostras e retorna a média
 * ============================================================ */
static uint16_t adc_average(uint n)
{
    uint32_t sum = 0;
    for (uint i = 0; i < n; i++) {
        sum += adc_read();
        sleep_us(10);
    }
    return (uint16_t)(sum / n);
}

static float to_voltage(uint16_t raw)
{
    return ((float)raw / ADC_RESOLUTION) * ADC_VREF_V;
}

static float to_current(float v, float vzero)
{
    return (v - vzero) / (SENSITIVITY_MV_PER_A / 1000.0f);
}

/* ============================================================
 * main
 * ============================================================ */
int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    adc_init();
    adc_gpio_init(ADC_GPIO);
    adc_select_input(ADC_CHANNEL);

    /* ----------------------------------------------------------
     * Auto-calibração do Vzero
     * Mantanha zero corrente nos terminais IP+/IP− agora!
     * ---------------------------------------------------------- */
    printf("=========================================\n");
    printf("  ACS758LCB — calibrando Vzero...\n");
    printf("  Mantenha ZERO corrente nos terminais!\n");
    printf("=========================================\n");

    /* Conta regressiva para o usuário ter tempo de garantir zero corrente */
    for (int i = 3; i > 0; i--) {
        printf("  Iniciando em %d...\n", i);
        sleep_ms(1000);
    }

    uint16_t calib_raw = adc_average(CALIB_SAMPLES);
    float vzero = to_voltage(calib_raw);

    printf("\n  Vzero calibrado: raw=%u  V=%.4fV\n", calib_raw, vzero);
    printf("=========================================\n");
    printf("  Sens.    : %.1f mV/A\n", SENSITIVITY_MV_PER_A);
    printf("  Amostras : %d\n",        NUM_SAMPLES);
    printf("=========================================\n\n");

    uint32_t count = 0;

    while (true) {
        count++;

        uint16_t raw = adc_average(NUM_SAMPLES);
        float v      = to_voltage(raw);
        float i_a    = to_current(v, vzero);

        printf("[%5lu]  raw=%4u  V=%.4fV  I=%+7.3f A",
               count, raw, v, i_a);

        if (i_a > OVERCURRENT_LIMIT_A || i_a < -OVERCURRENT_LIMIT_A)
            printf("  *** SOBRE-CORRENTE! ***");

        printf("\n");

        sleep_ms(PRINT_INTERVAL_MS);
    }

    return 0;
}
