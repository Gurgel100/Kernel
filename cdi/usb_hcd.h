/*
 * Copyright (c) 2015 Max Reitz
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */

#ifndef _CDI_USB_HCD_H_
#define _CDI_USB_HCD_H_

#include <cdi.h>
#include <cdi/usb.h>


/**
 * Opaque value identifying a transaction for the host controller.
 */
typedef void* cdi_usb_hc_transaction_t;

typedef enum {
    CDI_USB_IN,
    CDI_USB_OUT,
    CDI_USB_SETUP,
} cdi_usb_transfer_token_t;


/**
 * Describes a USB host controller.
 */
struct cdi_usb_hc {
    struct cdi_device dev;

    /**
     * Root hub provided by the HC. The hub driver responsible for this root hub
     * is part of the cdi_usb_hcd structure.
     */
    struct cdi_usb_hub rh;
};

/**
 * Describes the USB bus provided by a USB host controller and its root hub. All
 * buses are handled by the USB bus driver, therefore there are no "identifying"
 * fields in this structure.
 */
struct cdi_usb_hc_bus {
    struct cdi_bus_data bus_data;
    struct cdi_usb_hc* hc;
};

/**
 * Contains information about a communication pipe to a USB device endpoint used
 * for USB transactions.
 */
struct cdi_usb_hc_ep_info {
    /// Device ID
    uint8_t dev;

    /// Endpoint ID
    uint8_t ep;

    /// Endpoint type
    cdi_usb_endpoint_type_t ep_type;

    /// Transfer speed
    cdi_usb_speed_t speed;

    /// Maximum packet size for this endpoint
    size_t mps;

    /**
     * For low and full speed devices operated behind a high speed hub: @tt_addr
     * is the USB device address of the translating USB hub (the last high speed
     * hub), @tt_port is the port (0-based index) of that hub where the first
     * non-high-speed device of this device's tree is plugged into.
     */
    uint8_t tt_addr, tt_port;
};

/**
 * Describes a single USB transmission.
 */
struct cdi_usb_hc_transmission {
    /// Transaction this transmission is part of
    cdi_usb_hc_transaction_t ta;

    /// Type of transfer to be performed
    cdi_usb_transfer_token_t token;

    /// Buffer to be used (may be NULL if @size is 0)
    void* buffer;

    /// Number of bytes to be transferred (may be 0, and may exceed the MPS)
    size_t size;

    /**
     * Toggle bit to be used for the first packet issued in this transmission.
     * If @size exceeds the MPS and thus multiple packets are transmitted, the
     * toggle bit will be toggled automatically.
     */
    unsigned toggle;

    /**
     * Result of this transmission. Only valid after wait_transaction() has
     * returned.
     */
    cdi_usb_transmission_result_t* result;
};

/**
 * Describes a USB host controller driver.
 */
struct cdi_usb_hcd {
    struct cdi_driver drv;

    /**
     * USB hub driver which is responsible for all root hubs provided by this
     * HCD.
     */
    struct cdi_usb_hub_driver rh_drv;

    /**
     * Creates a USB transaction. A transaction consists of one or more
     * transmissions to a single endpoint.
     */
    cdi_usb_hc_transaction_t
        (*create_transaction)(struct cdi_usb_hc* hc,
                              const struct cdi_usb_hc_ep_info* target);

    /**
     * Enters a transmission into a transaction. The transmission will not be
     * executed until start_transaction() is invoked.
     *
     * This function may not be called after start_transaction() has been used.
     */
    void (*enqueue)(struct cdi_usb_hc* hc,
                    const struct cdi_usb_hc_transmission* trans);

    /**
     * Starts asynchronous execution of a transaction.
     */
    void (*start_transaction)(struct cdi_usb_hc* hc,
                              cdi_usb_hc_transaction_t ta);

    /**
     * Wait until all transmissions enqueued in the given transaction have been
     * executed. The variables pointed to by the @result field of the
     * transmissions enqueued will only be valid when this function returns.
     *
     * This function may not be invoked before start_transaction().
     */
    void (*wait_transaction)(struct cdi_usb_hc* hc,
                             cdi_usb_hc_transaction_t ta);

    /**
     * Destroys a transaction. Transmissions which have not been waited on may
     * or may not have been executed by the time this function returns. The
     * values of the variables pointed to by the @result field of unwaited-for
     * transmissions are not valid, neither are the buffer contents for
     * receiving transmissions.
     */
    void (*destroy_transaction)(struct cdi_usb_hc* hc,
                                cdi_usb_hc_transaction_t ta);
};

#endif
