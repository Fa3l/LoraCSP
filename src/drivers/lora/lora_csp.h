#ifndef LORA_CSP_H
#define LORA_CSP_H

#include "lora.h"

#include <csp/csp.h>
#include <csp/interfaces/csp_if_lora.h>

int lora_csp_tx(void *driver,
                const uint8_t *data,
                size_t len);

int lora_csp_add_interface(lora_t *lora,
                           uint16_t address,
                           uint8_t is_default,
                           csp_iface_t **result);

int lora_csp_poll_rx(csp_iface_t *iface);

#endif