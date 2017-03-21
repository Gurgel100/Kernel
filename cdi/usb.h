/*
 * Copyright (c) 2015 Max Reitz
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */

#ifndef _CDI_USB_H_
#define _CDI_USB_H_

#include <stdint.h>

#include <cdi.h>
#include <cdi-osdep.h>
#include <cdi/usb-structures.h>


struct cdi_usb_hc;

typedef enum {
    CDI_USB_CONTROL,
    CDI_USB_BULK,
    CDI_USB_INTERRUPT,
    CDI_USB_ISOCHRONOUS,
} cdi_usb_endpoint_type_t;

typedef enum {
    CDI_USB_LOW_SPEED,
    CDI_USB_FULL_SPEED,
    CDI_USB_HIGH_SPEED,
    CDI_USB_SUPERSPEED,
} cdi_usb_speed_t;

typedef enum {
    CDI_USB_OK = 0,

    CDI_USB_DRIVER_ERROR,
    CDI_USB_TIMEOUT,
    CDI_USB_BABBLE,
    CDI_USB_STALL,
    CDI_USB_HC_ERROR,
    CDI_USB_BAD_RESPONSE,
} cdi_usb_transmission_result_t;

/* Bitmask of CDI_USB_PORT_* and CDI_USB_*_SPEED values */
typedef uint32_t cdi_usb_port_status_t;

#define CDI_USB_PORT_SPEED_MASK (0x7)
#define CDI_USB_PORT_CONNECTED  (1 << 3)

struct cdi_usb_hub;

/**
 * Describes a hub driver. This is not a CDI driver in the common sense. When
 * used for a root hub, it is part of the host controller driver; when used for
 * a device hub, it is only used internally by the USB driver.
 */
struct cdi_usb_hub_driver {
    /**
     * Makes a port unreachable by traffic which reaches the hub. @index is a
     * 0-based index.
     */
    void (*port_disable)(struct cdi_usb_hub* hub, int index);

    /**
     * Makes a port reachable by traffic which reaches the hub and drives the
     * reset signal for at least 50 ms on the port. @index is a 0-based index.
     */
    void (*port_reset_enable)(struct cdi_usb_hub* hub, int index);

    /**
     * Retrieves information about the given port. This should only be called
     * after the port has been reset using port_reset_enable(). @index is a
     * 0-based index.
     */
    cdi_usb_port_status_t (*port_status)(struct cdi_usb_hub* hub, int index);
};

/**
 * Describes a USB hub. This is not a CDI device in the common sense. When used
 * for a root hub, it is part of the host controller object; when used for a
 * device hub, it is only used internally by the USB driver.
 *
 * For this reason, there is no struct cdi_usb_hub_driver object reachable from
 * this object. If it is a root hub, the hub driver will be part of the host
 * controller driver. Otherwise (if it is a device hub), the hub driver will be
 * internal to the USB driver.
 */
struct cdi_usb_hub {
    /**
     * Number of ports provided by this hub.
     */
    int ports;
};

/**
 * Describes a USB device as given to cdi_provide_device() and the init_device()
 * function provided by USB device drivers.
 */
struct cdi_usb_device {
    struct cdi_bus_data bus_data;

    /**
     * Various information which can be used for identifying a USB device.
     */
    uint16_t vendor_id, product_id;
    uint8_t class_id, subclass_id, protocol_id;

    /**
     * If negative, this logical device is defined on the USB device level;
     * otherwise, this is the index of the interface which this logical device
     * represents.
     */
    int interface;

    /**
     * Number of endpoints available for this device; this does include the
     * default pipe (endpoint 0) which can be used both on the physical device
     * level and from any interface.
     */
    int endpoint_count;

    cdi_usb_device_osdep meta;
};

/**
 * Describes the USB bus driver.
 */
struct cdi_usb_driver {
    struct cdi_driver drv;

    /**
     * Returns the endpoint descriptor for endpoint @ep (0 up until
     * dev->endpoint_count - 1) of device @dev in @desc.
     */
    void (*get_endpoint_descriptor)(struct cdi_usb_device* dev, int ep,
                                    struct cdi_usb_endpoint_descriptor* desc);

    /**
     * Executes a control transfer to endpoint @ep (0 up until
     * dev->endpoint_count - 1) of device @dev, using @setup for the setup stage
     * and @data for the data stage. The length and direction of transfer is
     * deduced from @setup.
     */
    cdi_usb_transmission_result_t
        (*control_transfer)(struct cdi_usb_device* dev, int ep,
                            const struct cdi_usb_setup_packet* setup,
                            void* data);

    /**
     * Executes a bulk transfer to endpoint @ep (0 up until
     * dev->endpoint_count - 1) of device @dev, transferring @size bytes from/to
     * @data. The direction of transfer is deduced from the endpoint given.
     */
    cdi_usb_transmission_result_t (*bulk_transfer)(struct cdi_usb_device* dev,
                                                   int ep, void* data,
                                                   size_t size);
};

/**
 * Describes patterns for USB devices a device driver can use to specify which
 * devices it can handle.
 */
struct cdi_usb_bus_device_pattern {
    struct cdi_bus_device_pattern pattern;

    /**
     * Any field may be negative to signify "don't care".
     */
    int vendor_id, product_id;
    int class_id, subclass_id, protocol_id;
};


/**
 * Registers a USB bus driver. Should be called by the CDI library, ideally,
 * after the USB bus driver's init() function has run.
 */
void cdi_usb_driver_register(struct cdi_usb_driver* drv);

/**
 * Returns the endpoint descriptor for endpoint @ep (0 up until
 * dev->endpoint_count - 1) of device @dev in @desc.
 */
void cdi_usb_get_endpoint_descriptor(struct cdi_usb_device* dev, int ep,
                                     struct cdi_usb_endpoint_descriptor* desc);

/**
 * Executes a control transfer to endpoint @ep (0 up until
 * dev->endpoint_count - 1) of device @dev, using @setup for the setup stage and
 * @data for the data stage. The length and direction of transfer is deduced
 * from @setup.
 */
cdi_usb_transmission_result_t
    cdi_usb_control_transfer(struct cdi_usb_device* dev, int ep,
                             const struct cdi_usb_setup_packet* setup,
                             void* data);

/**
 * Executes a bulk transfer to endpoint @ep (0 up until dev->endpoint_count - 1)
 * of device @dev, transferring @size bytes from/to @data. The direction of
 * transfer is deduced from the endpoint given.
 */
cdi_usb_transmission_result_t cdi_usb_bulk_transfer(struct cdi_usb_device* dev,
                                                    int ep, void* data,
                                                    size_t size);

#endif
