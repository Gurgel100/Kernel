/*
 * list.c
 *
 *  Created on: 30.07.2013
 *      Author: pascal
 */

#include "lists.h"
#include "stdlib.h"

struct cdi_list_node{
		void *Value;
		void *Next;
};

struct cdi_list_implementation{
		struct cdi_list_node *Anchor;
		size_t Size;
};

/*
 * Erzeugt eine neue Liste
 *
 * @return Neu erzeugte Liste oder NULL, falls kein Speicher reserviert werden
 * konnte
 */
cdi_list_t cdi_list_create(void)
{
	cdi_list_t list;
	list = malloc(sizeof(*list));
	list->Anchor = NULL;
	list->Size = 0;
	return list;
}

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
void cdi_list_destroy(cdi_list_t list)
{
	//Liste leeren (vorsichtshalber)
	while(cdi_list_pop(list) != NULL);
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
cdi_list_t cdi_list_push(cdi_list_t list, void* value)
{
	if(!list)
		return NULL;

	struct cdi_list_node *Node;
	Node = malloc(sizeof(*Node));
	Node->Next = list->Anchor;
	Node->Value = value;

	list->Anchor = Node;
	list->Size++;

	return list;
}

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
void* cdi_list_pop(cdi_list_t list)
{
	if(!list) return NULL;

	struct cdi_list_node *oldNode;
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
void* cdi_list_get(cdi_list_t list, size_t index)
{
	struct cdi_list_node *Node;
	size_t i;
	if(!list) return NULL;
	if(index >= list->Size)
		return NULL;
	Node = list->Anchor;
	for(i = 0; i < index; i++)
		Node = Node->Next;
	return Node->Value;
}

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
void* cdi_list_remove(cdi_list_t list, size_t index)
{
	if (list == NULL || index >= list->Size)
		return NULL;

	struct cdi_list_node **prevNodePtr = &list->Anchor;
	struct cdi_list_node *Node = list->Anchor;

	for (size_t i = 0; i < index; i++) {
		prevNodePtr = &Node->Next;
		Node = Node->Next;
	}

	*prevNodePtr = Node->Next;
	void *value = Node->Value;
	free(Node);

	list->Size--;

	return value;
}

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
size_t cdi_list_size(cdi_list_t list)
{
	if(!list) return 0;

	return list->Size;
}
