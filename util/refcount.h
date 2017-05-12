/*
 * refcount.h
 *
 *  Created on: 21.03.2017
 *      Author: pascal
 */

#ifndef REFCOUNT_H_
#define REFCOUNT_H_

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

/*
 * Macro für den Namen des Feldes, das für Referenzzählen verwendet wird
 */
#define REFCOUNT_FIELD_NAME				__refcount
/*
 * Macro das eine Struktur um das für Referenzzählen benötigte Feld erweitert
 */
#define REFCOUNT_FIELD					refcount_t REFCOUNT_FIELD_NAME
/*
 * Macros für entsprechende Funktionen
 */
#define REFCOUNT_INIT(obj, free)	refcount_init(obj, offsetof(typeof(*obj), REFCOUNT_FIELD_NAME), free)
#define REFCOUNT_RETAIN(obj)		refcount_retain(obj, offsetof(typeof(*obj), REFCOUNT_FIELD_NAME))
#define REFCOUNT_RELEASE(obj)		refcount_release(obj, offsetof(typeof(*obj), REFCOUNT_FIELD_NAME))

typedef struct{
	uint64_t ref_count;
	void (*free)(const void*);
}refcount_t;

/*
 * Initialisiert das Referenzcounting.
 * Parameter:	obj = Objekt das Verwaltet werden soll
 * 				offset = Offset im Objekt an dem ein Feld vom Typ refcount_t gefunden werden kann
 * 				free = Funktion, die verwendet werden soll um das Objekt freizugeben
 */
void refcount_init(void *obj, size_t offset, void (*free)(const void*));

/*
 * Reserviert das Objekt obj mittels Referenzcounting.
 * Parameter:	obj = Objekt das Verwaltet werden soll
 * 				offset = Offset im Objekt an dem ein Feld vom Typ refcount_t gefunden werden kann
 * Rückgabe:	True falls das Objekt erfolgreich reserviert wurde, ansonsten false
 */
void *refcount_retain(void *obj, size_t offset);

/*
 * Gibt das Objekt obj mittels Referenzcounting frei.
 * Parameter:	obj = Objekt das Verwaltet werden soll
 * 				offset = Offset im Objekt an dem ein Feld vom Typ refcount_t gefunden werden kann
 */
bool refcount_release(void *obj, size_t offset);

#endif /* REFCOUNT_H_ */
