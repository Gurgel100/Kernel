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
 * @file lists.h
 * \german
 * Funktionen zur Verwaltung von Listen
 * \endgerman
 *
 * \english
 * CDI specific functions for list management
 * \endenglish
 */

#ifndef _CDI_LISTS_
#define _CDI_LISTS_
#include <stddef.h>
#include <stdint.h>

/**
 * \german
 * Repraesentiert eine Liste.
 *
 * Der Felder der Struktur sind implementierungsabhaengig. Zum Zugriff auf
 * Listen muessen immer die spezifizierten Funktionen benutzt werden.
 * \endgerman
 *
 * \english
 * Represents a list.
 *
 * The fields of this structure are implementation dependent. To access list
 * elements, specific list functions must be used.
 * \endenglish
 */
typedef struct cdi_list_implementation* cdi_list_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \german
 * Erzeugt eine neue Liste
 *
 * @return Neu erzeugte Liste oder NULL, falls kein Speicher reserviert werden
 * konnte
 * \endgerman
 *
 * \english
 * Creates a new list.
 *
 * @return Returns a new list, or NULL if no memory could be allocated for the
 * list.
 * \endenglish
 */
cdi_list_t cdi_list_create(void);

/**
 * \german
 * Gibt eine Liste frei (Werte der Listenglieder müssen bereits
 * freigegeben sein)
 * \endgerman
 *
 * \english
 * Frees the memory associated with a list. (Values of the list members must
 * already be free.)
 * \endenglish
 */
void cdi_list_destroy(cdi_list_t list);

/**
 * \german
 * Fuegt ein neues Element am Anfang (Index 0) der Liste ein
 *
 * @param list Liste, in die eingefuegt werden soll
 * @param value Einzufuegendes Element
 *
 * @return Die Liste, in die eingefuegt wurde, oder NULL, wenn das Element
 * nicht eingefuegt werden konnte (z.B. kein Speicher frei).
 * \endgerman
 *
 * \english
 * Adds a new element to the head of the list.
 *
 * @param list The list to insert into.
 * @param value The element to be added.
 *
 * @return The list @a value has been inserted into, or NULL if @a value could
 * not be inserted (for example, because no memory could be allocated).
 * \endenglish
 */
cdi_list_t cdi_list_push(cdi_list_t list, void* value);

/**
 * \german
 * Entfernt ein Element am Anfang (Index 0) der Liste und gibt seinen Wert
 * zurueck.
 *
 * @param list Liste, aus der das Element entnommen werden soll
 * @return Das entfernte Element oder NULL, wenn die Liste leer war
 * \endgerman
 *
 * \english
 * Removes an element from the head of the list and returns that element's
 * value.
 *
 * @param list The list from which the leading element (head) will be removed.
 * @return The removed element, or NULL if the list was empty.
 * \endenglish
 */
void* cdi_list_pop(cdi_list_t list);

/**
 * \german
 * Prueft, ob die Liste leer ist.
 *
 * @param list Liste, die ueberprueft werden soll
 * @return 1, wenn die Liste leer ist; 0, wenn sie Elemente enthaelt
 * \endgerman
 *
 * \english
 * Tests if a list is empty.
 *
 * @param list The list to test.
 * @return 1 if the list is empty, otherwise 0.
 * \endenglish
 */
size_t cdi_list_empty(cdi_list_t list);

/**
 * \german
 * Gibt ein Listenelement zurueck
 *
 * @param list Liste, aus der das Element gelesen werden soll
 * @param index Index des zurueckzugebenden Elements
 *
 * @return Das angeforderte Element oder NULL, wenn kein Element mit dem
 * angegebenen Index existiert.
 * \endgerman
 *
 * \english
 * Returns the value of a list element at a specified index.
 *
 * @param list The list from which @a index will be read
 * @param index The index of the element to return the value of
 *
 * @return The list element requested or NULL if no element exists at the
 * specified index.
 * \endenglish
 */
void* cdi_list_get(cdi_list_t list, size_t index);

/**
 * \german
 * Fuegt ein neues Listenelement ein. Der Index aller Elemente, die bisher
 * einen groesseeren oder gleich grossen Index haben, verschieben sich
 * um eine Position nach hinten.
 *
 * @param list Liste, in die eingefuegt werden soll
 * @param index Zukuenftiger Index des neu einzufuegenden Elements
 * @param value Neu einzufuegendes Element
 *
 * @return Die Liste, in die eingefuegt wurde, oder NULL, wenn nicht eingefuegt
 * werden konnte (z.B. weil der Index zu gross ist)
 * \endgerman
 *
 * \english
 * Adds a new element (@a value) to @a list. The element is added at index
 * @a index. The index of all elements before @a value shall remain the same,
 * while the index of elements after @a value shall increase by one.
 *
 * @param list The list to insert into
 * @param index The index to be used for the new element
 * @param value The element to be inserted
 *
 * @return The newly ordered list, or NULL if the element could not be added
 * (for example, because the index is too large).
 * \endenglish
 */
cdi_list_t cdi_list_insert(cdi_list_t list, size_t index, void* value);

/**
 * \german
 * Loescht ein Listenelement
 *
 * @param list Liste, aus der entfernt werden soll
 * @param index Index des zu entfernenden Elements
 *
 * @return Das entfernte Element oder NULL, wenn kein Element mit dem
 * angegebenen Index existiert.
 * \endgerman
 *
 * \english
 * Removes an element from a list.
 *
 * @param list The list to remove an element from
 * @param index The index in the list that will be removed
 *
 * @return The element that was removed, or NULL if the element at the
 * specified index does not exist.
 * \endenglish
 */
void* cdi_list_remove(cdi_list_t list, size_t index);

/**
 * \german
 * Gibt die Laenge der Liste zurueck
 *
 * @param list Liste, deren Laenge zurueckgegeben werden soll
 * \endgerman
 *
 * \english
 * Get the size (length) of a list
 *
 * @param list The list whose length is to be returned
 * \endenglish
 */
size_t cdi_list_size(cdi_list_t list);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif
