#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stddef.h>


typedef struct {

    /*
     * Transfere um byte pelo SPI.
     *
     * tx       = byte enviado
     * retorno  = byte recebido
     */
    uint8_t (*spi_transfer)(void *user, uint8_t tx);

    /*
     * Controle do NSS/CS.
     *
     * active = 1 -> CS ativo
     * active = 0 -> CS inativo
     */
    void (*set_cs)(void *user, int active);

    /*
     * Delay em milissegundos.
     */
    void (*delay_ms)(void *user, uint32_t ms);

    /*
     * Dados específicos da plataforma.
     *
     * Raspberry Pi:
     *     SPI Linux
     *
     * STM32:
     *     HAL SPI
     *
     * ESP32:
     *     driver SPI
     */
    void *user;

} lora_hal_t;


typedef struct {

    /*
     * HAL do rádio.
     */
    lora_hal_t hal;

    /*
     * Configuração atual.
     */
    uint32_t frequency;

    uint8_t bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;

    /*
     * 1 = inicializado
     * 0 = não inicializado
     */
    uint8_t initialized;

} lora_t;


/*
 * Inicializa o SX1276.
 */
int lora_init(lora_t *lora);


/*
 * Configura frequência em Hz.
 *
 * Exemplos:
 *
 * 433000000
 * 868000000
 * 915000000
 */
int lora_set_frequency(lora_t *lora,
                       uint32_t frequency);


/*
 * Configura modem LoRa.
 *
 * bandwidth:
 *     7 = 125 kHz
 *
 * spreading_factor:
 *     6..12
 *
 * coding_rate:
 *     1 = 4/5
 *     2 = 4/6
 *     3 = 4/7
 *     4 = 4/8
 */
int lora_set_modem(lora_t *lora,
                   uint8_t bandwidth,
                   uint8_t spreading_factor,
                   uint8_t coding_rate);


/*
 * Coloca o rádio em RX contínuo.
 */
int lora_receive_start(lora_t *lora);


/*
 * Transmite um pacote LoRa.
 */
int lora_send(lora_t *lora,
              const uint8_t *data,
              size_t len);


/*
 * Verifica se existe pacote recebido.
 *
 * Retorna:
 *     1 = disponível
 *     0 = não disponível
 */
int lora_packet_available(lora_t *lora);


/*
 * Recebe um pacote LoRa.
 *
 * Retorna:
 *     > 0 = quantidade de bytes recebidos
 *     0   = nenhum pacote
 *     < 0 = erro
 */
int lora_receive(lora_t *lora,
                 uint8_t *data,
                 size_t max_len);

#endif