/*
 * list.c
 *
 *  Created on: 03.05.2014
 *      Author: pascal
 */

#include "list.h"

struct list_node{
		void *Value;
		void *Next;
};

struct list_implementation{
		struct list_node *Anchor;
		size_t Size;
};

/*
 * Erzeugt eine neue Liste
 *
 * @return Neu erzeugte Liste oder NULL, falls kein Speicher reserviert werden
 * konnte
 */
list_t list_create(void)
{
	list_t list;
	list = malloc(sizeof(*list));
	list->Anchor = NULL;
	list->Size = 0;
	return list;
}

/*
 * Gibt eine Liste frei (Werte der Listenglieder müssen bereits
 * freigegeben sein)
 */
void list_destroy(list_t list)
{
	//Liste leeren (vorsichtshalber)
	while(list_pop(list) != NULL);
	//Speicher freigeben
	free(list);
}

/*
 * Fuegt ein neues Element am Anfang (Index 0) der Liste ein
 *
 * @param list Liste, in die eingefuegt werden soll
 * @param value Einzufuegendes Element
 *
 * @return Die Liste, in die eingefuegt wurde, oder NULL, wenn das Element
 * nicht eingefuegt werden konnte (z.B. kein Speicher frei).
 */
list_t list_push(list_t list, void* value)
{
	if(!list)
		return NULL;

	struct list_node *Node;
	Node = malloc(sizeof(*Node));
	Node->Next = list->Anchor;
	Node->Value = value;

	list->Anchor = Node;
	list->Size++;

	return list;
}

/*
 * Entfernt ein Element am Anfang (Index 0) der Liste und gibt seinen Wert
 * zurueck.
 *
 * @param list Liste, aus der das Element entnommen werden soll
 * @return Das entfernte Element oder NULL, wenn die Liste leer war
 */
void* list_pop(list_t list)
{
	if(!list) return NULL;

	struct list_node *oldNode;
	void *Value;
	if(list->Anchor)
	{
		oldNode = list->Anchor;
		Value = oldNode->Value;
		list->Anchor = oldNode->Next;
		free(oldNode);
		list->Size--;
	}
	else
		return NULL;
	return Value;
}

/*
 * Gibt ein Listenelement zurueck
 *
 * @param list Liste, aus der das Element gelesen werden soll
 * @param index Index des zurueckzugebenden Elements
 *
 * @return Das angeforderte Element oder NULL, wenn kein Element mit dem
 * angegebenen Index existiert.
 */
void* list_get(list_t list, size_t index)
{
	struct list_node *Node;
	size_t i;
	if(!list) return NULL;
	if(index >= list->Size)
		return NULL;
	Node = list->Anchor;
	for(i = 0; i < index; i++)
		Node = Node->Next;
	return Node->Value;
}

/*
 * Gibt die Laenge der Liste zurueck
 *
 * @param list Liste, deren Laenge zurueckgegeben werden soll
 */
size_t list_size(list_t list)
{
	if(!list) return 0;

	return list->Size;
}
