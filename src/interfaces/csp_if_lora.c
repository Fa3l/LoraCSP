#include <csp/interfaces/csp_if_lora.h>

#include <csp/csp.h>
#include <csp/csp_crc32.h>
#include <csp/csp_id.h>

#include <string.h>

#include "../csp_buffer_private.h"

static int csp_lora_tx(csp_iface_t *iface,
                       uint16_t via,
                       csp_packet_t *packet,
                       int from_me) {

    (void)via;
    (void)from_me;

    csp_lora_interface_data_t *ifdata = iface->interface_data;

    if (ifdata == NULL || ifdata->tx_func == NULL) {
        csp_buffer_free(packet);
        return CSP_ERR_INVAL;
    }

    /*
     * Adiciona CRC do CSP.
     */
    csp_crc32_append(packet);

    /*
     * Coloca o cabeçalho CSP no início do frame.
     */
    csp_id_prepend(packet);

    /*
     * Entrega o frame para o driver físico do LoRa.
     */
    int ret = ifdata->tx_func(
        ifdata->driver,
        packet->frame_begin,
        packet->frame_length
    );

    /*
     * O driver não fica responsável pelo pacote CSP.
     */
    csp_buffer_free(packet);

    return ret;
}

csp_iface_t csp_if_lora = {
    .name = CSP_IF_LORA_NAME,
    .nexthop = csp_lora_tx,
    .addr = 0,
    .is_default = 0
};

int csp_lora_add_interface(csp_iface_t *iface) {

    if (iface == NULL ||
        iface->name == NULL ||
        iface->interface_data == NULL) {
        return CSP_ERR_INVAL;
    }

    csp_lora_interface_data_t *ifdata = iface->interface_data;

    if (ifdata->tx_func == NULL) {
        return CSP_ERR_INVAL;
    }

    iface->nexthop = csp_lora_tx;

    csp_iflist_add(iface);

    return CSP_ERR_NONE;
}

void csp_lora_rx(csp_iface_t *iface,
                 const uint8_t *buf,
                 size_t len,
                 void *pxTaskWoken) {

    if (iface == NULL || buf == NULL || len == 0) {
        return;
    }

    csp_packet_t *packet;

    if (pxTaskWoken != NULL) {
        packet = csp_buffer_get_always_isr();
    } else {
        packet = csp_buffer_get_always();
    }

    if (packet == NULL) {
        iface->drop++;
        return;
    }

    /*
     * Verifica se o frame cabe no buffer CSP.
     */
    if (len > sizeof(packet->data)) {
        iface->rx_error++;
        csp_buffer_free(packet);
        return;
    }

    /*
     * Prepara o buffer para receber o header CSP.
     */
    csp_id_setup_rx(packet);

    /*
     * Copia o frame recebido pelo LoRa.
     */
    memcpy(packet->frame_begin, buf, len);

    packet->frame_length = len;

    /*
     * Remove o header CSP.
     */
    if (csp_id_strip(packet) < 0) {
        iface->rx_error++;
        csp_buffer_free(packet);
        return;
    }

    /*
     * Verifica CRC.
     */
    if (csp_crc32_verify(packet) != CSP_ERR_NONE) {
        iface->rx_error++;
        csp_buffer_free(packet);
        return;
    }

    /*
     * Entrega o pacote ao CSP.
     */
    csp_qfifo_write(packet, iface, pxTaskWoken);
}
