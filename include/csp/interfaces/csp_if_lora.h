#ifndef CSP_IF_LORA_H
#define CSP_IF_LORA_H

#include <csp/csp.h>

#define CSP_IF_LORA_NAME "LORA"

/**
 * Interface CSP para rádio LoRa.
 *
 * O driver_data deve apontar para uma estrutura fornecida
 * pelo driver físico do rádio.
 */
typedef struct {
    int (*tx_func)(void *driver, const uint8_t *data, size_t len);
    void *driver;
} csp_lora_interface_data_t;

extern csp_iface_t csp_if_lora;

int csp_lora_add_interface(csp_iface_t *iface);

void csp_lora_rx(csp_iface_t *iface,
                 const uint8_t *buf,
                 size_t len,
                 void *pxTaskWoken);

#endif
