/*
 * Copyright (c) 2010 Kevin Wolf
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/projects/COPYING.WTFPL for more details.
 */

#ifndef _CDI_MEM_H_
#define _CDI_MEM_H_

#include <stdint.h>
#include "cdi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \german
 * Beschreibt Anforderungen an einen Speicherbereich
 * \endgerman
 * \english
 * Describes requirements for a memory area
 * \endenglish
 */
typedef enum {
    /**
     * \german
     * Maske fuer ein Bitfeld, das das benoetige physische Alignment beschreibt
     * (das geforderte Alignment ist 2^x Bytes)
     * \endgerman
     * \english
     * Mask for a bit field which describes the required physical alignment
     * (the required alignment is 2^x bytes)
     * \endenglish
     */
    CDI_MEM_ALIGN_MASK      = 0x1f,

    /**
     * \german
     * Physische Adresse muss nicht gueltig sein
     * \endgerman
     * \english
     * Physical address isn't required to be valid
     * \endenglish
     */
    CDI_MEM_VIRT_ONLY       = 0x20,

    /**
     * \german
     * Physisch zusammenhängenden Speicher anfordern
     * \endgerman
     * \english
     * Request physically contiguous memory
     * \endenglish
     */
    CDI_MEM_PHYS_CONTIGUOUS = 0x40,

    /**
     * \german
     * Physischen Speicher < 16 MB anfordern
     * \endgerman
     * \english
     * Request physical memory < 16 MB
     * \endenglish
     */
    CDI_MEM_DMA_16M         = 0x80,

    /**
     * \german
     * Physischen Speicher < 4 GB anfordern
     * \endgerman
     * \english
     * Request physical memory < 4 GB
     * \endenglish
     */
    CDI_MEM_DMA_4G          = 0x100,

    /**
     * \german
     * Speicher muss nicht initialisiert werden (verhindert Kopie bei
     * cdi_mem_require_flags)
     * \endgerman
     * \english
     * Memory doesn't need to be initialised (avoids copy in
     * cdi_mem_require_flags)
     * \endenglish
     */
    CDI_MEM_NOINIT          = 0x200,
} cdi_mem_flags_t;

/**
 * \german
 * Beschreibt einen physischen Speicherberich (Eintrag einer
 * Scatter/Gather-Liste)
 * \endgerman
 * \english
 * Describes an area of physical memory (an entry for a scatter/gather list)
 * \endenglish
 */
struct cdi_mem_sg_item {
    uintptr_t   start;
    size_t      size;
};

/**
 * \german
 * Scatter/Gather-Liste, die die physischen Adressen zu einem bestimmten
 * virtuellen Speicherbereich enthält.
 * \endgerman
 * \english
 * Scatter/Gather list which contains the physical addresses for a given area
 * of virtual memory.
 * \endenglish
 */
struct cdi_mem_sg_list {
    size_t                  num;
    struct cdi_mem_sg_item* items;
};

/**
 * \german
 * Beschreibt einen virtuellen Speicherbereich, je nach Flags mit einer
 * Scatter/Gather-Liste der zugehörigen phyischen Adressen.
 * \endgerman
 * \english
 * Describes an area of virtual memory, depending on the allocation flags with
 * a scatter/gather list of its physical addresses.
 * \endenglish
 */
struct cdi_mem_area {
    size_t                  size;
    void*                   vaddr;
    struct cdi_mem_sg_list  paddr;

    cdi_mem_osdep           osdep;
};

/**
 * \german
 * Reserviert einen Speicherbereich.
 *
 * @param size Größe des Speicherbereichs in Bytes
 * @param flags Flags, die zusätzliche Anforderungen beschreiben
 *
 * @return Eine cdi_mem_area bei Erfolg, NULL im Fehlerfall
 * \endgerman
 * \english
 * Allocates a memory area.
 *
 * @param size Size of the memory area in bytes
 * @param flags Flags that describe additional requirements
 *
 * @return A cdi_mem_area on success, NULL on failure
 * \endenglish
 */
struct cdi_mem_area* cdi_mem_alloc(size_t size, cdi_mem_flags_t flags);

/**
 * \german
 * Reserviert physisch zusammenhägenden Speicher an einer definierten Adresse
 *
 * @param paddr Physische Adresse des angeforderten Speicherbereichs
 * @param size Größe des benötigten Speichers in Bytes
 *
 * @return Eine cdi_mem_area bei Erfolg, NULL im Fehlerfall
 * \endgerman
 * \english
 * Reserves physically contiguous memory at a defined address
 *
 * @param paddr Physical address of the requested memory aread
 * @param size Size of the requested memory area in bytes
 *
 * @return A cdi_mem_area on success, NULL on failure
 * \endenglish
 */
struct cdi_mem_area* cdi_mem_map(uintptr_t paddr, size_t size);

/**
 * \german
 * Gibt einen durch cdi_mem_alloc oder cdi_mem_map reservierten Speicherbereich
 * frei
 * \endgerman
 * \english
 * Frees a memory area that was previously allocated by cdi_mem_alloc or
 * cdi_mem_map
 * \endenglish
 */
void cdi_mem_free(struct cdi_mem_area* p);

/**
 * \german
 * Gibt einen Speicherbereich zurück, der dieselben Daten wie @a p beschreibt,
 * aber mindestens die gegebenen Flags gesetzt hat.
 *
 * Diese Funktion kann denselben virtuellen und physischen Speicherbereich wie
 * @p benutzen oder sogar @p selbst zurückzugeben, solange der gemeinsam
 * benutzte Speicher erst dann freigegeben wird, wenn sowohl @a p als auch der
 * Rückgabewert durch cdi_mem_free freigegeben worden sind.
 *
 * Ansonsten wird ein neuer Speicherbereich reserviert und (außer wenn das
 * Flag CDI_MEM_NOINIT gesetzt ist) die Daten werden aus @a p in den neu
 * reservierten Speicher kopiert.
 * \endgerman
 * \english
 * Returns a memory area that describes the same data as @a p does, but for
 * which at least the given flags are set.
 *
 * This function may use the same virtual and physical memory areas as used in
 * @a p, or it may even return @a p itself. In this case it must ensure that
 * the commonly used memory is only freed when both @a p and the return value
 * of this function have been freed by cdi_mem_free.
 *
 * Otherwise, a new memory area is allocated and data is copied from @a p into
 * the newly allocated memory (unless CDI_MEM_NOINIT is set).
 * \endenglish
 */
struct cdi_mem_area* cdi_mem_require_flags(struct cdi_mem_area* p,
    cdi_mem_flags_t flags);

/**
 * \german
 * Kopiert die Daten von @a src nach @a dest. Beide Speicherbereiche müssen
 * gleich groß sein.
 *
 * Das bedeutet nicht unbedingt eine physische Kopie: Wenn beide
 * Speicherbereiche auf denselben physischen Speicher zeigen, macht diese
 * Funktion nichts. Sie kann auch andere Methoden verwenden, um den Speicher
 * effektiv zu kopieren, z.B. durch geeignetes Ummappen von Pages.
 *
 * @return 0 bei Erfolg, -1 sonst
 * \endgerman
 * \english
 * Copies data from @a src to @a dest. Both memory areas must be of the same
 * size.
 *
 * This does not necessarily involve a physical copy: If both memory areas
 * point to the same physical memory, this function does nothing. It can also
 * use other methods to achieve the same visible effect, e.g. by remapping
 * pages.
 *
 * @return 0 on success, -1 otherwise
 * \endenglish
 */
int cdi_mem_copy(struct cdi_mem_area* dest, struct cdi_mem_area* src);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

