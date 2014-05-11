/*
 * list.h
 *
 *  Created on: 03.05.2014
 *      Author: pascal
 */

#ifndef LIST_H_
#define LIST_H_

#include "stddef.h"
#include "stdint.h"

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
typedef struct list_implementation* list_t;


/*
 * Erzeugt eine neue Liste
 *
 * @return Neu erzeugte Liste oder NULL, falls kein Speicher reserviert werden
 * konnte
 */
list_t list_create(void);

/*
 * Gibt eine Liste frei (Werte der Listenglieder müssen bereits
 * freigegeben sein)
 */
void list_destroy(list_t list);

/*
 * Fuegt ein neues Element am Anfang (Index 0) der Liste ein
 *
 * @param list Liste, in die eingefuegt werden soll
 * @param value Einzufuegendes Element
 *
 * @return Die Liste, in die eingefuegt wurde, oder NULL, wenn das Element
 * nicht eingefuegt werden konnte (z.B. kein Speicher frei).
 */
list_t list_push(list_t list, void* value);

/*
 * Entfernt ein Element am Anfang (Index 0) der Liste und gibt seinen Wert
 * zurueck.
 *
 * @param list Liste, aus der das Element entnommen werden soll
 * @return Das entfernte Element oder NULL, wenn die Liste leer war
 */
void* list_pop(list_t list);

/*
 * Prueft, ob die Liste leer ist.
 *
 * @param list Liste, die ueberprueft werden soll
 * @return 1, wenn die Liste leer ist; 0, wenn sie Elemente enthaelt
 */
size_t list_empty(list_t list);

/*
 * Gibt ein Listenelement zurueck
 *
 * @param list Liste, aus der das Element gelesen werden soll
 * @param index Index des zurueckzugebenden Elements
 *
 * @return Das angeforderte Element oder NULL, wenn kein Element mit dem
 * angegebenen Index existiert.
 */
void* list_get(list_t list, size_t index);

/*
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
 */
list_t list_insert(list_t list, size_t index, void* value);

/*
 * Loescht ein Listenelement
 *
 * @param list Liste, aus der entfernt werden soll
 * @param index Index des zu entfernenden Elements
 *
 * @return Das entfernte Element oder NULL, wenn kein Element mit dem
 * angegebenen Index existiert.
 */
void* list_remove(list_t list, size_t index);

/*
 * Gibt die Laenge der Liste zurueck
 *
 * @param list Liste, deren Laenge zurueckgegeben werden soll
 */
size_t list_size(list_t list);

#endif /* LIST_H_ */
