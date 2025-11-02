#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/char/pl011.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/qdev-clock.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "chardev/char-fe.h"
#include "chardev/char-serial.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "trace.h"
#include "hw/ssi/ssi.h"


#include "g233_spi.h"

// DeviceState *g233_spi_create(hwaddr addr, qemu_irq irq)
// {
//     DeviceState *dev;
//     SysBusDevice *s;

//     dev = qdev_new(TYPE_SPI_G233);
//     s = SYS_BUS_DEVICE(dev);
//     sysbus_realize_and_unref(s, &error_fatal);
//     sysbus_mmio_map(s, 0, addr);
//     sysbus_connect_irq(s, 0, irq);

//     return dev;
// }

enum {
    CR1,
    CR2,
    SR,
    DR,
    CSCTRL,
};

enum CR1_S {
    MTSR = 1<<2,
    SPE = 1<<6,
};

enum CR2_S {
    SSOE = 1<<4,
    ERRIE = 1<<5,
    RXNEIE = 1<<6,
    TXEIE = 1<<7,
};

enum SR_S {
    RXNE = 1<<0,
    TXE = 1<<1,
    UNDERRUN = 1<<2,
    OVERRUN = 1<<3,

};

enum CS_S {
    CS0_EN = 1<<0,
    CS1_EN = 1<<1,
    CS2_EN = 1<<2,
    CS3_EN = 1<<3,
    CS0_ACT = 1<<4,
    CS1_ACT = 1<<5,
    CS2_ACT = 1<<6,
    CS3_ACT = 1<<7,
};

static void spi_cs_may_change(G233SpiState *s)
{
    bool cs0 = (s->cs & CS0_EN) && (s->cs & CS0_ACT);
    bool cs1 = (s->cs & CS1_EN) && (s->cs & CS1_ACT);
    qemu_set_irq(s->irq[0], cs0?0:1);
    qemu_set_irq(s->irq[1], cs1?0:1);
}

static void spi_may_interrupt(G233SpiState *s) 
{
    bool interrupt = false;
    if ((s->cr2 & ERRIE) && (s->sr & (UNDERRUN|OVERRUN))) {
        interrupt = true;
    }
    if ((s->cr2 & RXNEIE) && (s->sr & RXNE)) {
        interrupt = true;
    }
    if ((s->cr2 & TXEIE) && (s->sr & TXE)) {
        interrupt = true;
    }
    qemu_set_irq(s->irq[2], interrupt);
}

static void spi_transfer_data(G233SpiState *s, uint32_t data)
{
    bool cs0 = (s->cs & CS0_EN) && (s->cs & CS0_ACT);
    bool cs1 = (s->cs & CS1_EN) && (s->cs & CS1_ACT);

    bool m_and_e = (s->cr1 & MTSR) && (s->cr1 & SPE);

    if (m_and_e && cs0^cs1) {
        s->sr &= ~TXE;
        uint8_t rx = ssi_transfer(s->ssi, data);
        if (fifo8_is_full(&s->rx_fifo)) {
            s->sr |= OVERRUN;
        } else {
            fifo8_push(&s->rx_fifo, rx);
        }
        s->sr |= RXNE;
        s->sr |= TXE;
    }

    spi_may_interrupt(s);
}

static void g233_spi_reset(DeviceState *dev)
{
    G233SpiState *s = SPI_G233(dev);
    s->cr1 = 0;
    s->cr2 = 0;
    s->sr = 0x2;
    s->dr = 0xc;
    s->cs = 0;
    spi_cs_may_change(s);
    fifo8_reset(&s->rx_fifo);
}


static uint64_t spi_read(void *opaque, hwaddr offset,
    unsigned size)
{
    G233SpiState *s = SPI_G233(opaque);
    if(size!=4) {
        qemu_log_mask(LOG_GUEST_ERROR, "size(%d) != 4", size);
    }
    uint32_t data = 0;

    switch (offset>>2)
    {
    case CR1:
        data = s->cr1;
        break;
    case CR2:
        data = s->cr2;
        break;
    case SR:
        data = s->sr;
        break;
    case DR:
        if (!fifo8_is_empty(&s->rx_fifo)) {
            data = fifo8_pop(&s->rx_fifo);
        }
        if (fifo8_is_empty(&s->rx_fifo)) {
            s->sr &= ~RXNE;
        }
        spi_may_interrupt(s);
        break;
    case CSCTRL:
        data = s->cs;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "offset "HWADDR_FMT_plx"unexpected!!!", offset);
        break;
    }
    return (uint64_t)data;
}

static void spi_write(void *opaque, hwaddr offset,
    uint64_t value, unsigned size)
{
    G233SpiState *s = SPI_G233(opaque);

    if(size!=4) {
        qemu_log_mask(LOG_GUEST_ERROR, "size(%d) != 4", size);
    }
    uint32_t data = value & 0xff;

    switch (offset>>2)
    {
    case CR1:
        s->cr1 = data&0b01000100;
        break;
    case CR2:
        s->cr2 = data&0b11111110;
        spi_may_interrupt(s);
        break;
    case SR:
        s->sr &= (~(data & 0b1100));
        break;
    case DR:
        spi_transfer_data(s, data);
        break;
    case CSCTRL:
        s->cs = data;
        spi_cs_may_change(s);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "offset "HWADDR_FMT_plx"unexpected!!!", offset);
        break;
    }
}

static const MemoryRegionOps gspi_ops = {
    .read = spi_read,
    .write = spi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};


static void g233_spi_init(Object *obj)
{
}

static void gspi_realize(DeviceState *dev, Error **errp)
{
    G233SpiState *s = SPI_G233(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &gspi_ops, s, "g233_spi", 0x14);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    
    qdev_init_gpio_out(dev, s->irq, ARRAY_SIZE(s->irq));
    s->ssi = ssi_create_bus(dev, "g233spi.ssi");
    fifo8_create(&s->rx_fifo, 1);
}

// static const Property g233_spi_properties[] = {

// };



static void g233_spi_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = gspi_realize;
    device_class_set_legacy_reset(dc, g233_spi_reset);
    // device_class_set_props(dc, g233_spi_properties);
}

static const TypeInfo g233_spi_info = {
    .name = TYPE_SPI_G233,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(G233SpiState),
    .instance_init = g233_spi_init,
    .class_init = g233_spi_class_init,
};

static void g233_spi_register_types(void)
{
    type_register_static(&g233_spi_info);
}

type_init(g233_spi_register_types)
