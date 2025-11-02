#ifndef G233_SPI_H
#define G233_SPI_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"
#include "qemu/fifo8.h"

#define TYPE_SPI_G233 "g233_spi"

OBJECT_DECLARE_SIMPLE_TYPE(G233SpiState, SPI_G233)

struct G233SpiState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    /// @brief 0： cs0
    ///        1:  cs1
    ///        2: irq for plic
    qemu_irq irq[3];
    uint32_t cr1;
    uint32_t cr2;
    uint32_t sr;
    uint32_t dr;
    uint32_t cs;
    SSIBus *ssi;
    Fifo8 rx_fifo;
};

#endif
