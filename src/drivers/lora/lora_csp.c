#include "lora_csp.h"

#include <stdlib.h>

/*
 * Função de transmissão usada pelo libcsp.
 */
int lora_csp_tx(void *driver,
                const uint8_t *data,
                size_t len)
{
    lora_t *lora = (lora_t *)driver;

    if (lora == NULL) {
        return -1;
    }

    return lora_send(lora, data, len);
}


/*
 * Contexto da interface LoRa.
 */
typedef struct {

    lora_t *lora;

    csp_lora_interface_data_t ifdata;

    csp_iface_t iface;

} lora_csp_context_t;


/*
 * Adiciona o rádio LoRa ao CSP.
 */
int lora_csp_add_interface(lora_t *lora,
                           uint16_t address,
                           uint8_t is_default,
                           csp_iface_t **result)
{
    lora_csp_context_t *ctx;

    if (lora == NULL) {
        return CSP_ERR_INVAL;
    }

    ctx = calloc(1, sizeof(*ctx));

    if (ctx == NULL) {
        return CSP_ERR_NOMEM;
    }

    ctx->lora = lora;

    ctx->ifdata.tx_func = lora_csp_tx;
    ctx->ifdata.driver = lora;

    ctx->iface.name = CSP_IF_LORA_NAME;
    ctx->iface.addr = address;
    ctx->iface.netmask = 0;
    ctx->iface.interface_data = &ctx->ifdata;
    ctx->iface.driver_data = ctx;
    ctx->iface.nexthop = NULL;
    ctx->iface.is_default = is_default;

    if (csp_lora_add_interface(&ctx->iface) != CSP_ERR_NONE) {
        free(ctx);
        return CSP_ERR_INVAL;
    }

    if (result != NULL) {
        *result = &ctx->iface;
    }

    return CSP_ERR_NONE;
}


/*
 * Processa pacotes recebidos pelo rádio.
 *
 * Deve ser chamado periodicamente por uma thread/task.
 */
int lora_csp_poll_rx(csp_iface_t *iface)
{
    lora_csp_context_t *ctx;
    uint8_t buffer[256];

    if (iface == NULL) {
        return -1;
    }

    ctx = (lora_csp_context_t *)iface->driver_data;

    if (ctx == NULL || ctx->lora == NULL) {
        return -1;
    }

    /*
     * Não há pacote disponível.
     */
    if (!lora_packet_available(ctx->lora)) {
        return 0;
    }

    /*
     * Lê pacote do SX1276.
     */
    int len = lora_receive(ctx->lora,
                           buffer,
                           sizeof(buffer));

    if (len <= 0) {
        return -1;
    }

    /*
     * Entrega o frame ao CSP.
     */
    csp_lora_rx(iface,
                buffer,
                (size_t)len,
                NULL);

    return 1;
}