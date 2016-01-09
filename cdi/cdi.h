/*
 * Copyright (c) 2007 Kevin Wolf
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */

/**
 * \german
 * Core ist das Basismodul von CDI. Es stellt die grundlegenden Datenstrukturen
 * und Funktionen bereit, auf die andere Module aufbauen.
 *
 * \section Datenstrukturen
 * CDI basiert auf drei grundlegenden Datenstrukturen. Die einzelnen Module
 * erweitern diese Strukturen in einer kompatiblen Weise: cdi_net_driver ist
 * beispielsweise eine Struktur, die cdi_driver als erstes Element enthält, so
 * dass sie auf cdi_driver gecastet werden kann. Bei den Strukturen handelt es
 * sich um:
 *
 * - cdi_device beschreibt ein allgemeines Gerät. Es hat insbesondere einen
 *   zugehörigen Treiber und eine Adresse, die es in seinem Bus eindeutig
 *   beschreibt.
 * - cdi_driver beschreibt einen allgemeinen Treiber. Jeder Treiber hat einen
 *   Typ, der beschreibt, zu welchem Modul er gehört und damit auch, in
 *   welchen Datentyp sowohl der Treiber als auch seine Geräte gecastet werden
 *   können. Jeder Treiber hat außerdem eine Reihe von Funktionspointern, die
 *   es der CDI-Bibliothek erlauben, Funktionen im Treiber aufzurufen (z.B. ein
 *   Netzwerkpaket zu versenden)
 * - cdi_bus_data enthält busspezifische Informationen zu einem Gerät wie die
 *   Adresse des Geräts auf dem Bus (z.B. bus.dev.func für PCI-Geräte) oder
 *   andere busbezogene Felder enthalten (z.B. Vendor/Device ID). Diese
 *   Struktur wird zur Initialisierung von Geräten benutzt, d.h. wenn das
 *   Gerät noch nicht existiert.
 *
 * \section core_init Initialisierung
 *
 * Die Initialisierung der Treiber läuft in folgenden Schritten ab:
 *
 * -# Initialisierung der Treiber: Es wird der init-Callback jedes Treibers
 *    aufgerufen. Anschließend wird der Treiber bei CDI und dem Betriebssystem
 *    registriert, so dass er vor und während der Ausführung von init keine
 *    Anfragen entgegennehmen muss, aber direkt anschließend dafür bereit
 *    sein muss.
 * -# Suche und Initialisierung der Geräte: Für jedes verfügbare Gerät wird
 *    init_device nacheinander für alle Treiber aufgerufen, bis ein Treiber
 *    das Gerät akzeptiert (oder alle Treiber erfolglos versucht wurden).
 *    \n
 *    @todo Für jedes Gerät scan_bus aufrufen und die dabei gefundenen
 *    Geräte wiederum initialisieren.
 *\endgerman
 *
 * \english
 * The Core module contains the fundamental CDI data structures and functions
 * on which the other modules are based.
 *
 * \section Data structures
 * CDI is based on three basic structures. The other modules extend these
 * structures in a compatible way: For example, cdi_net_driver is a structure
 * that contains a cdi_driver as its first element so that it can be cast to
 * cdi_device. The structures are:
 *
 * - cdi_device describes a device. Amongst others, it contains fields that
 *   reference the corresponding driver and an address that identifies the
 *   device uniquely on its bus.
 * - cdi_driver describes a driver. It has a type which determines to which
 *   CDI module the driver belongs (and thus which type the struct may be cast
 *   to). Moreover, each driver has some function points which can be used by
 *   the CDI library to call driver functions (e.g. for sending a network
 *   packet)
 * - cdi_bus_data contains bus specific information on a device, like the
 *   address of the device on the bus (e.g. bus.dev.func for PCI) or other
 *   bus related information (e.g. device/vendor ID). This structure is used
 *   for initialisation of devices, i.e. when the device itself doesn't exist
 *   yet.
 *
 * \section core_init Initialisation
 *
 * Drivers are initialised in the following steps:
 *
 * -# Initialisation of drivers: The init() callback of each driver is called.
 *    After the driver has returned from there, it is registered with CDI and
 *    the operating system. The driver needs not to be able to handle requests
 *    before it has completed its init() call, but it must be prepared for them
 *    immediately afterwards.
 * -# Search for and initialisation of devices: For each device init_device is
 *    called in the available drivers until a driver accepts the device (or all
 *    of the drivers reject it).
 *    \n
 *    @todo Call scan_bus for each device and initialise possible child devices
 * \endenglish
 *
 *
 * \defgroup core
 */

#ifndef _CDI_H_
#define _CDI_H_

#include <stdint.h>
#include <stdbool.h>

#include <cdi-osdep.h>
#include <cdi/lists.h>

typedef enum {
    CDI_UNKNOWN         = 0,
    CDI_NETWORK         = 1,
    CDI_STORAGE         = 2,
    CDI_SCSI            = 3,
    CDI_VIDEO           = 4,
    CDI_AUDIO           = 5,
    CDI_AUDIO_MIXER     = 6,
    CDI_USB_HCD         = 7,
    CDI_USB             = 8,
    CDI_FILESYSTEM      = 9,
    CDI_PCI             = 10,
    CDI_AHCI            = 11,
} cdi_device_type_t;

struct cdi_driver;

/**
 * \german
 * Beschreibt ein Gerät in Bezug auf den Bus, an dem es hängt. Diese
 * Informationen enthalten in der Regel eine Busadresse und zusätzliche Daten
 * wie Device/Vendor ID
 * \endgerman
 *
 * \english
 * Describes a device in relation to its bus. This information usually contains
 * a bus address and some additional data like device/vendor ID.
 * \endenglish
 *
 */
struct cdi_bus_data {
    cdi_device_type_t   bus_type;
};

/**
 * \german
 * Beschreibt ein Gerät
 * \endgerman
 * \english
 * Describes a device
 * \endenglish
 */
struct cdi_device {
    /**
     * \german
     * Name des Geräts (eindeutig unter den Geräten desselben Treibers)
     * \endgerman
     * \english
     * Name of the device (must be unique among the devices of the same driver)
     * \endenglish
     */
    const char*             name;

    /**
     * \german
     * Treiber, der für das Gerät benutzt wird
     * \endgerman
     * \english
     * Driver used for the device
     * \endenglish
     */
    struct cdi_driver*      driver;

    /**
     * \german
     * Busspezifische Daten zum Gerät
     * \endgerman
     * \english
     * Bus specific data for the device
     * \endenglish
     */
    struct cdi_bus_data*    bus_data;

    // tyndur-spezifisch
    void*               backdev;
};

/**
 * \german
 * Beschreibt einen CDI-Treiber
 * \endgerman
 * \english
 * Describes a CDI driver
 * \endenglish
 */
struct cdi_driver {
    cdi_device_type_t   type;
    cdi_device_type_t   bus;
    const char*         name;

    /**
     * \german
     * Wird von der CDI-Implementierung auf true gesetzt, sobald .init()
     * aufgerufen wurde.
     * \endgerman
     * \english
     * Set by the CDI implementation to true as soon as .init() was called.
     * \endenglish
     */
    bool                initialised;

    /**
     * \german
     * Enthält alle Geräte (cdi_device), die den Treiber benutzen
     * \endgerman
     * \english
     * Contains all devices (cdi_device) which use this driver
     * \endenglish
     */
    cdi_list_t          devices;

    /**
     * \german
     * Versucht ein Gerät zu initialisieren. Die Funktion darf von der
     * CDI-Bibliothek nur aufgerufen werden, wenn bus_data->type dem Typ des
     * Treibers entspricht
     *
     * @return Ein per malloc() erzeugtes neues cdi_device, wenn der Treiber
     * das Gerät akzeptiert. Wenn er das Gerät nicht unterstützt, gibt er
     * NULL zurück.
     * \endgerman
     *
     * \english
     * Tries to initialise a device. The function may only be called by the CDI
     * library if bus_data->type matches the type of the driver.
     *
     * @return A new cdi_device created by malloc() if the driver accepts the
     * device. NULL if the device is not supported by this driver.
     * \endenglish
     */
    struct cdi_device* (*init_device)(struct cdi_bus_data* bus_data);
    void (*remove_device)(struct cdi_device* device);

    int (*init)(void);
    int (*destroy)(void);
};

/**
 * \german
 * Treiber, die ihre eigene main-Funktion implemenieren, müssen diese Funktoin
 * vor dem ersten Aufruf einer anderen CDI-Funktion aufrufen.
 * Initialisiert interne Datenstruktur der Implementierung fuer das jeweilige
 * Betriebssystem und startet anschliessend alle Treiber.
 *
 * Ein wiederholter Aufruf bleibt ohne Effekt. Es bleibt der Implementierung
 * der CDI-Bibliothek überlassen, ob diese Funktion zurückkehrt.
 * \endgerman
 *
 * \english
 * Drivers which implement their own main() function must call this function
 * before they call any other CDI function. It initialises internal data
 * structures of the CDI implementation and starts all drivers.
 *
 * This function should only be called once, additional calls will have no
 * effect. Depending on the implementation, this function may or may not
 * return.
 * \endenglish
 */
void cdi_init(void);

/**
 * \german
 * Initialisiert die Datenstrukturen für einen Treiber
 * (erzeugt die devices-Liste)
 * \endgerman
 *
 * \english
 * Initialises the data structures for a driver
 * \endenglish
 */
void cdi_driver_init(struct cdi_driver* driver);

/**
 * \german
 * Deinitialisiert die Datenstrukturen für einen Treiber
 * (gibt die devices-Liste frei)
 * \endgerman
 *
 * \english
 * Deinitialises the data structures for a driver
 * \endenglish
 */
void cdi_driver_destroy(struct cdi_driver* driver);

/**
 * \german
 * Registriert den Treiber fuer ein neues Geraet
 *
 * @param driver Zu registierender Treiber
 * \endgerman
 *
 * \english
 * Registers a new driver with CDI
 * \endenglish
 */
void cdi_driver_register(struct cdi_driver* driver);

/**
 * \german
 * Informiert dass Betriebssystem, dass ein neues Geraet angeschlossen wurde.
 *
 * Der Zweck dieser Funktion ist es, Bustreibern zu ermöglichen, die Geräte
 * auf ihrem Bus anderen Treibern zur Verfügung zu stellen, die über den
 * PCI-Bus nicht erreichbar sind (z.B. einen USB-Massenspeicher). Sie erlaubt
 * es Geräten auch, dem OS die Verfügbarkeit anderer Geräte auch außerhalb
 * einer Controller-Schnittstelle mitzuteilen.
 *
 * Das Betriebssystem kann daraufhin den passenden Treiber laden (oder einen
 * geladenen Treiber informieren), den es anhand der Informationen in @a device
 * finden kann. CDI definiert nicht, welche Aktionen das OS auszuführen hat
 * oder auf welche Weise mögliche Aktionen (wie das Laden eines Treibers)
 * umgesetzt werden.
 *
 * CDI-Implementierungen können diese Funktion auch intern benutzen, um
 * während der Initialisierung die Geräte auf dem PCI-Bus bereitzustellen.
 *
 * @return 0 bei Erfolg, -1 im Fehlerfall
 * \endgerman
 * \english
 * Allows a driver to inform the operating system that a new device has become
 * available.
 *
 * The rationale behind cdi_provide_device is to allow controller interfaces
 * to provide devices on their bus which may not be available through the
 * conventional PCI bus (for example, a USB mass storage device). This also
 * allows devices to inform the OS of the presence of other devices outside
 * of the context of a controller interface, where necessary.
 *
 * The operating system should determine which driver to load (or inform of
 * the new device) based on the cdi_bus_data struct passed. How the operating
 * system decides which driver to load/inform is not defined by CDI. An example
 * would be a simple list generated from a script which parses each driver in
 * the source tree, finds relevant cdi_bus_data structs, and adds them to a
 * list which maps the structs to a string. The OS can then use that string to
 * load a driver somehow.
 *
 * Whilst CDI could provide a pre-defined list of mappings for operating
 * systems to use when implementing cdi_provide_device, it was decided that
 * this would make the interface too rigid. Whilst this method requires a
 * little more effort from the CDI implementor, it also allows operating
 * systems to load non-CDI drivers (eg, from a native driver interface) as
 * defined by the OS-specific mapping list. This would be impossible if CDI
 * rigidly enforced a specific method.
 *
 * Operating systems may also choose to implement cdi_provide_device and then
 * use it when iterating over the PCI bus in order to load drivers dynamically
 * (effectively treating coldplug devices as hotplugged before boot).
 *
 * @return 0 on success or -1 if an error was encountered.
 * \endenglish
 */
int cdi_provide_device(struct cdi_bus_data* device);

/**
 * \german
 * Informiert das Betriebssystem, dass ein neues Gerät angeschlossen wurde und
 * von einem internen Treiber (d.h. einem Treiber, der in dieselbe Binary
 * gelinkt ist) angesteuert werden muss.
 *
 * Diese Funktion kann benutzt werden, um Treibern mit mehreren Komponenten
 * eine klarere Struktur zu geben (z.B. SATA-Platten als vom AHCI-Controller
 * getrennte CDI-Geräte zu modellieren).
 *
 * Vorsicht: Sie sollte nur benutzt werden, wenn eine enge Kopplung zwischen
 * Gerät und Untergeräte unvermeidlich ist, da sie voraussetzt, dass beide
 * Treiber in dieselbe Binary gelinkt sind. Dies mag für monolithische Kernel
 * kein Problem darstellen, aber CDI wird auch in Betriebssystemen mit anderem
 * Design benutzt.
 *
 * Falls der Treiber noch nicht initialisiert ist, wird er initialisiert, bevor
 * das neue Gerät erstellt wird.
 *
 * @param device Adressinformation für den Treiber, um das Gerät zu finden
 * @param driver Treiber, der für das Gerät benutzt werden soll
 *
 * @return 0 bei Erfolg, -1 im Fehlerfall
 * \endgerman
 * \english
 * Informs the operating system that a new device has become available and
 * is to be handled by an internal driver (i.e. a driver linked into the same
 * binary).
 *
 * This function can be used to give drivers for devices with multiple
 * components a clearer structure (e.g. model SATA disks as CDI devices
 * separate from the AHCI controller).
 *
 * Be careful though: It should only be used if a tight coupling between device
 * and subdevice is unvoidable, as it requires linking the code of both drivers
 * into the same binary. This might not be a problem for monolithic kernels,
 * but CDI is used in OSes of different designs.
 *
 * If the driver isn't initialised yet, it is initialised before creating the
 * new device.
 *
 * @param device Addressing information for the driver to find the device
 * @param driver Driver that must be used for this device
 *
 * @return 0 on success or -1 if an error was encountered.
 * \endenglish
 */
int cdi_provide_device_internal_drv(struct cdi_bus_data* device,
                                    struct cdi_driver* driver);

#endif

/*\@}*/

