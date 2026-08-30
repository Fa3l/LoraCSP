#include "lora.h"

#include <stddef.h>
#include <stdint.h>

/*
 * SX1276 registers
 */

#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_TX_BASE_ADDR    0x0E
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_FIFO_RX_CURRENT      0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES          0x13
#define REG_MODEM_CONFIG1        0x1D
#define REG_MODEM_CONFIG2        0x1E
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG3        0x26
#define REG_VERSION              0x42

/*
 * SX1276 modes
 */

#define MODE_LONG_RANGE          0x80

#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_RX_CONTINUOUS       0x05

/*
 * LoRa IRQ flags
 */

#define IRQ_RX_DONE              0x40
#define IRQ_TX_DONE              0x08
#define IRQ_PAYLOAD_CRC_ERROR    0x20


/*
 * Chip Select
 */

static void cs(lora_t *lora, int active)
{
    lora->hal.set_cs(lora->hal.user, active);
}


/*
 * SPI transfer
 */

static uint8_t spi(lora_t *lora, uint8_t value)
{
    return lora->hal.spi_transfer(lora->hal.user, value);
}


/*
 * Write register
 */

static void write_reg(lora_t *lora,
                      uint8_t reg,
                      uint8_t value)
{
    cs(lora, 1);

    spi(lora, reg | 0x80);
    spi(lora, value);

    cs(lora, 0);
}


/*
 * Read register
 */

static uint8_t read_reg(lora_t *lora,
                        uint8_t reg)
{
    uint8_t value;

    cs(lora, 1);

    spi(lora, reg & 0x7F);
    value = spi(lora, 0x00);

    cs(lora, 0);

    return value;
}


/*
 * Initialize SX1276
 */

int lora_init(lora_t *lora)
{
    uint8_t version;

    if (lora == NULL ||
        lora->hal.spi_transfer == NULL ||
        lora->hal.set_cs == NULL) {
        return -1;
    }

    /*
     * Coloca o SX1276 em LoRa + Sleep.
     */
    write_reg(lora,
              REG_OP_MODE,
              MODE_LONG_RANGE | MODE_SLEEP);

    /*
     * Aguarda o rádio estabilizar.
     */
    if (lora->hal.delay_ms != NULL) {
        lora->hal.delay_ms(lora->hal.user, 10);
    }

    /*
     * Verifica se o SX1276 responde pelo SPI.
     */
    version = read_reg(lora, REG_VERSION);

    if (version == 0x00 || version == 0xFF) {
        return -1;
    }

    /*
     * Coloca em standby.
     */
    write_reg(lora,
              REG_OP_MODE,
              MODE_LONG_RANGE | MODE_STDBY);

    /*
     * FIFO TX começa em 0.
     */
    write_reg(lora,
              REG_FIFO_TX_BASE_ADDR,
              0x00);

    /*
     * FIFO RX começa em 0.
     */
    write_reg(lora,
              REG_FIFO_RX_BASE_ADDR,
              0x00);

    /*
     * Ponteiro inicial do FIFO.
     */
    write_reg(lora,
              REG_FIFO_ADDR_PTR,
              0x00);

    /*
     * PA_BOOST + potência máxima.
     */
    write_reg(lora,
              REG_PA_CONFIG,
              0x8F);

    /*
     * Limpa todas as interrupções.
     */
    write_reg(lora,
              REG_IRQ_FLAGS,
              0xFF);

    lora->initialized = 1;

    return 0;
}


/*
 * Set frequency
 */

int lora_set_frequency(lora_t *lora,
                       uint32_t frequency)
{
    uint64_t frf;

    if (lora == NULL || !lora->initialized) {
        return -1;
    }

    if (frequency == 0) {
        return -1;
    }

    /*
     * FRF = frequency * 2^19 / 32 MHz
     */
    frf =
        ((uint64_t)frequency << 19) / 32000000ULL;

    write_reg(lora,
              REG_FRF_MSB,
              (uint8_t)((frf >> 16) & 0xFF));

    write_reg(lora,
              REG_FRF_MID,
              (uint8_t)((frf >> 8) & 0xFF));

    write_reg(lora,
              REG_FRF_LSB,
              (uint8_t)(frf & 0xFF));

    lora->frequency = frequency;

    return 0;
}


/*
 * Configure LoRa modem
 */

int lora_set_modem(lora_t *lora,
                   uint8_t bandwidth,
                   uint8_t spreading_factor,
                   uint8_t coding_rate)
{
    uint8_t config1;
    uint8_t config2;
    uint8_t config3;

    if (lora == NULL || !lora->initialized) {
        return -1;
    }

    /*
     * BW:
     *
     * 0 = 7.8 kHz
     * 1 = 10.4 kHz
     * 2 = 15.6 kHz
     * 3 = 20.8 kHz
     * 4 = 31.25 kHz
     * 5 = 41.7 kHz
     * 6 = 62.5 kHz
     * 7 = 125 kHz
     * 8 = 250 kHz
     * 9 = 500 kHz
     */

    if (bandwidth > 9) {
        return -1;
    }

    /*
     * SF permitido pelo SX1276:
     *
     * 6 até 12
     */
    if (spreading_factor < 6 ||
        spreading_factor > 12) {
        return -1;
    }

    /*
     * Coding rate:
     *
     * 1 = 4/5
     * 2 = 4/6
     * 3 = 4/7
     * 4 = 4/8
     */
    if (coding_rate < 1 ||
        coding_rate > 4) {
        return -1;
    }

    /*
     * Config1:
     *
     * BW
     * Coding Rate
     * Explicit Header
     */
    config1 =
        (uint8_t)((bandwidth << 4) |
                  (coding_rate << 1));

    /*
     * Config2:
     *
     * SF
     * CRC ON
     */
    config2 =
        (uint8_t)((spreading_factor << 4) |
                  0x04);

    /*
     * Low Data Rate Optimize.
     *
     * Para SF7/BW125 não é necessário.
     *
     * É habilitado para configurações
     * com símbolo suficientemente longo.
     */
    config3 = 0x00;

    /*
     * Low Data Rate Optimize para
     * configurações lentas.
     */
    if ((spreading_factor >= 11 && bandwidth == 7) ||
        (spreading_factor == 12 && bandwidth == 6)) {
        config3 |= 0x08;
    }

    write_reg(lora,
              REG_MODEM_CONFIG1,
              config1);

    write_reg(lora,
              REG_MODEM_CONFIG2,
              config2);

    write_reg(lora,
              REG_MODEM_CONFIG3,
              config3);

    /*
     * Preamble = 8 símbolos.
     */
    write_reg(lora,
              REG_PREAMBLE_MSB,
              0x00);

    write_reg(lora,
              REG_PREAMBLE_LSB,
              0x08);

    lora->bandwidth = bandwidth;
    lora->spreading_factor = spreading_factor;
    lora->coding_rate = coding_rate;

    return 0;
}


/*
 * Start continuous RX
 */

int lora_receive_start(lora_t *lora)
{
    if (lora == NULL || !lora->initialized) {
        return -1;
    }

    /*
     * Standby antes de configurar RX.
     */
    write_reg(lora,
              REG_OP_MODE,
              MODE_LONG_RANGE | MODE_STDBY);

    /*
     * Limpa interrupções anteriores.
     */
    write_reg(lora,
              REG_IRQ_FLAGS,
              0xFF);

    /*
     * FIFO RX começa em 0.
     */
    write_reg(lora,
              REG_FIFO_ADDR_PTR,
              0x00);

    /*
     * RX contínuo.
     */
    write_reg(lora,
              REG_OP_MODE,
              MODE_LONG_RANGE | MODE_RX_CONTINUOUS);

    return 0;
}


/*
 * Send LoRa packet
 */

int lora_send(lora_t *lora,
              const uint8_t *data,
              size_t len)
{
    int timeout_ms = 5000;

    if (lora == NULL ||
        data == NULL ||
        !lora->initialized) {
        return -1;
    }

    if (len == 0 || len > 255) {
        return -1;
    }

    /*
     * Coloca o rádio em standby.
     */
    write_reg(lora,
              REG_OP_MODE,
              MODE_LONG_RANGE | MODE_STDBY);

    /*
     * FIFO começa no endereço TX.
     */
    write_reg(lora,
              REG_FIFO_ADDR_PTR,
              0x00);

    /*
     * Escreve payload no FIFO.
     */
    cs(lora, 1);

    spi(lora, REG_FIFO | 0x80);

    for (size_t i = 0; i < len; i++) {
        spi(lora, data[i]);
    }

    cs(lora, 0);

    /*
     * Define tamanho do payload.
     */
    write_reg(lora,
              REG_PAYLOAD_LENGTH,
              (uint8_t)len);

    /*
     * Limpa flags anteriores.
     */
    write_reg(lora,
              REG_IRQ_FLAGS,
              0xFF);

    /*
     * Entra em TX.
     */
    write_reg(lora,
              REG_OP_MODE,
              MODE_LONG_RANGE | MODE_TX);

    /*
     * Espera TX_DONE com timeout.
     */
    while ((read_reg(lora, REG_IRQ_FLAGS) &
            IRQ_TX_DONE) == 0) {

        if (timeout_ms-- <= 0) {

            /*
             * Evita deixar o rádio preso em TX.
             */
            write_reg(lora,
                      REG_OP_MODE,
                      MODE_LONG_RANGE | MODE_STDBY);

            return -1;
        }

        if (lora->hal.delay_ms != NULL) {
            lora->hal.delay_ms(lora->hal.user, 1);
        }
    }

    /*
     * Limpa TX_DONE.
     */
    write_reg(lora,
              REG_IRQ_FLAGS,
              IRQ_TX_DONE);

    /*
     * Volta para standby.
     */
    write_reg(lora,
              REG_OP_MODE,
              MODE_LONG_RANGE | MODE_STDBY);

    return 0;
}


/*
 * Check if packet is available
 */

int lora_packet_available(lora_t *lora)
{
    uint8_t irq;

    if (lora == NULL || !lora->initialized) {
        return 0;
    }

    irq = read_reg(lora, REG_IRQ_FLAGS);

    return (irq & IRQ_RX_DONE) != 0;
}


/*
 * Receive LoRa packet
 */

int lora_receive(lora_t *lora,
                 uint8_t *data,
                 size_t max_len)
{
    uint8_t irq;
    uint8_t len;
    uint8_t fifo_addr;

    if (lora == NULL ||
        data == NULL ||
        !lora->initialized) {
        return -1;
    }

    /*
     * Verifica flags.
     */
    irq = read_reg(lora, REG_IRQ_FLAGS);

    /*
     * Nenhum pacote disponível.
     */
    if (!(irq & IRQ_RX_DONE)) {
        return 0;
    }

    /*
     * Erro de CRC.
     */
    if (irq & IRQ_PAYLOAD_CRC_ERROR) {

        write_reg(lora,
                  REG_IRQ_FLAGS,
                  IRQ_PAYLOAD_CRC_ERROR);

        return -1;
    }

    /*
     * Número de bytes recebidos.
     */
    len = read_reg(lora,
                   REG_RX_NB_BYTES);

    /*
     * Buffer pequeno demais.
     */
    if (len > max_len) {

        /*
         * Limpa RX_DONE para não ficar
         * preso no mesmo pacote.
         */
        write_reg(lora,
                  REG_IRQ_FLAGS,
                  IRQ_RX_DONE);

        return -1;
    }

    /*
     * Endereço atual do FIFO RX.
     */
    fifo_addr =
        read_reg(lora,
                 REG_FIFO_RX_CURRENT);

    /*
     * Aponta para o início do pacote.
     */
    write_reg(lora,
              REG_FIFO_ADDR_PTR,
              fifo_addr);

    /*
     * Lê payload do FIFO.
     */
    cs(lora, 1);

    spi(lora, REG_FIFO & 0x7F);

    for (uint8_t i = 0; i < len; i++) {
        data[i] = spi(lora, 0x00);
    }

    cs(lora, 0);

    /*
     * Limpa RX_DONE.
     */
    write_reg(lora,
              REG_IRQ_FLAGS,
              IRQ_RX_DONE);

    return (int)len;
}