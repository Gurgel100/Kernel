/*
 * stdlib.c
 *
 *  Created on: 28.07.2012
 *      Author: pascal
 */

#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "ctype.h"
#include "math.h"
#include "assert.h"
#include "userlib.h"
#ifdef BUILD_KERNEL
#include "mm.h"
#include "cpu.h"
#include "vmm.h"
#else
#include "syscall.h"
#endif

#define HEAP_RESERVED	0x01
#define HEAP_FLAGS		0xAA
#define HEAP_ALIGNMENT	16

#define MAX(a, b) ((a > b) ? a : b)

typedef struct{
		void *Prev;
		void *Next;
		size_t Length;
		uint8_t Flags;		//Bit 0: Reserviert (1) oder Frei (0)
							//(Flags): Alle ungeraden Bits 1 und alle geraden 0
		int8_t balance;		//Höhe rechts - Höhe links
		bool is_list_mbr;		//Gibt an, dass der Knoten eindeutig ist
}heap_t;

typedef struct{
	heap_t heap_base;
	union{
		struct{
			void *parent;
			void *smaller;
			void *bigger;
			void *list;
		};
		struct{
			void *prev;
			void *next;
		};
	};
}heap_empty_t;

typedef struct{
	void (*func)(void);
	void *next;
}atexit_list_t;

atexit_list_t *Atexit_List_Base = NULL;

static heap_t *lastHeap = NULL;
static heap_empty_t *base_emptyHeap = NULL;
static char **real_environ;

//Global visible
char **environ;

void abort()
{
#ifdef BUILD_KERNEL
	assert(false);
#else
	syscall_exit(-1);
#endif
}

int atexit(void (*func)(void))
{
#ifndef BUILD_KERNEL
	atexit_list_t *list = Atexit_List_Base;
	if(list == NULL)
	{
		Atexit_List_Base = malloc(sizeof(atexit_list_t));
		Atexit_List_Base->func = func;
		Atexit_List_Base->next = NULL;
	}
	else
	{
		atexit_list_t *new = malloc(sizeof(atexit_list_t));
		new->func = func;
		new->next = list;
		Atexit_List_Base = new;
	}
#endif
	return 0;
}

void exit(int status)
{
#ifdef BUILD_KERNEL
	assert(false && status);
#else
	//Erst registrierte Funktionen aufrufen
	atexit_list_t *list = Atexit_List_Base;
	for(; list && list->next; list = list->next)
		list->func();
	syscall_exit(status);
#endif
}

double atof(const char* str)
{
	char *s = (char*)str;
	double value = 0;
	double sign = 1;
	bool point = false;

	//Whitespace character überspringen
	while(isspace(*s)) s++;

	//Plus- und Minuszeichen
	if(*s == '-')
	{
		sign = -1;
		s++;
	}
	else if(*s == '+')
	{
		s++;
	}

	//Hexadezimal
	if(*s == '0' && tolower(*(s + 1)) == 'x')
	{
		s += 2;
		size_t digit = 16;
		while(isxdigit(*s) || (!point && *s == '.'))
		{
			if(*s == '.')
				point = true;
			else
			{
				int val;
				if(isdigit(*s))
				{
					val = *s - '0';
				}
				else
				{
					val = tolower(*s) - 'a' + 10;
				}
				if(point)
				{
					value += (double)val / digit;
					digit *= 16;
				}
				else
				{
					value = value * 16 + val;
				}
			}
			s++;
		}
		if(tolower(*s++) == 'p')
		{
			long exponent = atol(s);
			value *= pow(16, exponent);
		}
	}
	//INF
	else if(tolower(*s) == 'i' && tolower(*(s + 1)) == 'n' && tolower(*(s + 2)) == 'f')
	{
		value = INFINITY;
	}
	//NAN
	else if(tolower(*s) == 'n' && tolower(*(s + 1)) == 'a' && tolower(*(s + 2)) == 'n')
	{
		value = NAN;
	}
	//Dezimal
	else
	{
		size_t digit = 10;
		while(isdigit(*s) || (!point && *s == '.'))
		{
			if(*s == '.')
				point = true;
			else
			{
				if(point)
				{
					value += (double)(*s -'0') / digit;
					digit *= 10;
				}
				else
				{
					value = value * 10 + (*s - '0');
				}
			}
			s++;
		}
		if(tolower(*s++) == 'e')
		{
			long exponent = atol(s);
			value *= pow(10, exponent);
		}
	}

	return value * sign;
}

int atoi(const char *str)
{
	return atol(str);
}

long atol(const char *str)
{
	char *s = (char*)str;
	long sign = 1;
	long value = 0;

	//Whitespace character überspringen
	while(isspace(*s)) s++;

	if(*s == '-')
	{
		sign = -1;
		s++;
	}
	else if(*s == '+')
	{
		s++;
	}

	while(isdigit(*s))
	{
		value = value * 10 + (*s - '0');
		s++;
	}

	return value * sign;
}

long int strtol(const char *str, char **endptr, int base)
{
    long retval = 0;
    int overflow = 0;
    char sign = 0;
    int digit;

    while(isspace(*str))
    {
        str++;
    }

    // Mögliches Vorzeichen auswerten
    if(*str == '+' || *str == '-')
    {
        sign = *str;
        str++;
    }

    // Mögliches 0, 0x und 0X auswerten
    if(*str == '0')
    {
        if(str[1] == 'x' || str[1] == 'X')
        {
            if(base == 0)
            {
                base = 16;
            }

            if(base == 16 && isxdigit(str[2]))
            {
                str += 2;
            }
        }
        else if(base == 0)
        {
        	base = 8;
        }
    }
    else if(base == 0)
    {
    	base = 10;
    }

    while(*str)
    {
        if(isdigit(*str) && (*str - '0') < base)
        {
            digit = *str - '0';
        }
        else if(isalpha(*str) && tolower(*str) - 'a' + 10 < base)
        {
            digit = tolower(*str) - 'a' + 10;
        }
        else
        {
            break;
        }

        if(retval > (LONG_MAX - digit) / base)
        {
            overflow = 1;
        }
        retval = retval * base + digit;

        str++;
    }

    if (endptr != NULL)
    {
        *(const char**)endptr = str;
    }

    if (overflow)
    {
        //errno = ERANGE;
        return (sign == '-') ? LONG_MIN : LONG_MAX;
    }

    return (sign == '-') ? -retval : retval;
}

//Speicherverwaltung-------------------------
//Hilfsfunktionen
static void rotate_left(heap_empty_t *node)
{
	heap_empty_t *tmp = node->bigger;
	heap_empty_t *parent = node->parent;

	if(parent != NULL)
	{
		if(parent->smaller == node)
			parent->smaller = tmp;
		else
			parent->bigger = tmp;
	}
	tmp->parent = parent;
	node->parent = tmp;

	if(node == base_emptyHeap)
		base_emptyHeap = tmp;
	if(tmp->smaller != NULL)
		((heap_empty_t*)tmp->smaller)->parent = node;
	node->bigger = tmp->smaller;
	tmp->smaller = node;
}

static void rotate_right(heap_empty_t *node)
{
	heap_empty_t *tmp = node->smaller;
	heap_empty_t *parent = node->parent;

	if(parent != NULL)
	{
		if(parent->smaller == node)
			parent->smaller = tmp;
		else
			parent->bigger = tmp;
	}
	tmp->parent = parent;
	node->parent = tmp;

	if(node == base_emptyHeap)
		base_emptyHeap = tmp;
	if(tmp->bigger != NULL)
		((heap_empty_t*)tmp->bigger)->parent = node;
	node->smaller = tmp->bigger;
	tmp->bigger = node;
}

static void upin(heap_empty_t *node)
{
	if(node->parent != NULL)
	{
		heap_empty_t *parent = node->parent;
		if(node == parent->smaller)	//Node ist linker Sohn des Vaters
		{
			if(parent->heap_base.balance == 1)
				parent->heap_base.balance = 0;
			else if(parent->heap_base.balance == 0)
			{
				parent->heap_base.balance = -1;
				upin(parent);
			}
			else
			{
				if(node->heap_base.balance == -1)
				{
					parent->heap_base.balance = 0;
					node->heap_base.balance = 0;
					rotate_right(parent);
				}
				else	//Doppelrotation nach rechts
				{
					node->heap_base.balance = (((heap_empty_t*)node->bigger)->heap_base.balance == 1) ? -1 : 0;
					parent->heap_base.balance = (((heap_empty_t*)node->bigger)->heap_base.balance == -1) ? 1 : 0;
					((heap_empty_t*)node->bigger)->heap_base.balance = 0;
					rotate_left(node);
					rotate_right(parent);
				}
			}
		}
		else		//Node ist rechter Sohn des Vaters
		{
			if(parent->heap_base.balance == -1)
				parent->heap_base.balance = 0;
			else if(parent->heap_base.balance == 0)
			{
				parent->heap_base.balance = 1;
				upin(parent);
			}
			else
			{
				if(node->heap_base.balance == 1)
				{
					parent->heap_base.balance = 0;
					node->heap_base.balance = 0;
					rotate_left(parent);
				}
				else	//Doppelrotation nach links
				{
					node->heap_base.balance = (((heap_empty_t*)node->smaller)->heap_base.balance == -1) ? 1 : 0;
					parent->heap_base.balance = (((heap_empty_t*)node->smaller)->heap_base.balance == 1) ? -1 : 0;
					((heap_empty_t*)node->smaller)->heap_base.balance = 0;
					rotate_right(node);
					rotate_left(parent);
				}
			}
		}
	}
}

static void upout(heap_empty_t *node)
{
	heap_empty_t *parent = node->parent;
	if(parent != NULL)
	{
		if(parent->smaller == node)	//Node ist linker Sohn
		{
			if(parent->heap_base.balance == -1)
			{
				parent->heap_base.balance = 0;
				upout(parent);
			}
			else if(parent->heap_base.balance == 0)
			{
				parent->heap_base.balance = 1;
			}
			else
			{
				heap_empty_t *q = parent->bigger;
				if(q->heap_base.balance == 0)
				{
					q->heap_base.balance = -1;
					rotate_left(parent);
				}
				else if(q->heap_base.balance == 1)
				{
					parent->heap_base.balance = 0;
					q->heap_base.balance = 0;
					rotate_left(parent);
					upout(q);
				}
				else
				{
					//Doppelrotation links
					heap_empty_t *r = q->smaller;
					q->heap_base.balance = (r->heap_base.balance == -1) ? 1 : 0;
					parent->heap_base.balance = (r->heap_base.balance == 1) ? -1 : 0;
					r->heap_base.balance = 0;
					rotate_right(q);
					rotate_left(parent);
					upout(r);
				}
			}
		}
		else	//Node ist rechter Sohn
		{
			if(parent->heap_base.balance == 1)
			{
				parent->heap_base.balance = 0;
				upout(parent);
			}
			else if(parent->heap_base.balance == 0)
			{
				parent->heap_base.balance = -1;
			}
			else
			{
				heap_empty_t *q = parent->smaller;
				if(q->heap_base.balance == 0)
				{
					q->heap_base.balance = 1;
					rotate_right(parent);
				}
				else if(q->heap_base.balance == -1)
				{
					parent->heap_base.balance = 0;
					q->heap_base.balance = 0;
					rotate_right(parent);
					upout(q);
				}
				else
				{
					//Doppelrotation rechts
					heap_empty_t *r = q->bigger;
					q->heap_base.balance = (r->heap_base.balance == 1) ? -1 : 0;
					parent->heap_base.balance = (r->heap_base.balance == -1) ? 1 : 0;
					r->heap_base.balance = 0;
					rotate_left(q);
					rotate_right(parent);
					upout(r);
				}
			}
		}
	}
}

static void add_empty_heap(heap_empty_t *node)
{
	if(node == NULL)
		return;
	node->heap_base.balance = 0;
	node->heap_base.is_list_mbr = false;
	node->smaller = NULL;
	node->bigger = NULL;
	node->list = NULL;
	if(base_emptyHeap == NULL)
	{
		base_emptyHeap = node;
		node->parent = NULL;
	}
	else
	{
		heap_empty_t *root = base_emptyHeap;
		while(true)
		{
			if(node->heap_base.Length < root->heap_base.Length && root->smaller != NULL)
				root = root->smaller;
			else if(node->heap_base.Length > root->heap_base.Length && root->bigger != NULL)
				root = root->bigger;
			else
				break;
		}

		if(node->heap_base.Length == root->heap_base.Length)
		{
			//Knoten in die Liste einfügen
			node->heap_base.is_list_mbr = true;
			node->next = root->list;
			node->prev = root;
			root->list = node;
			if(node->next != NULL)
				((heap_empty_t*)node->next)->prev = node;
		}
		else
		{
			if(node->heap_base.Length < root->heap_base.Length)
			{
				root->smaller = node;
				root->heap_base.balance--;
			}
			else
			{
				root->bigger = node;
				root->heap_base.balance++;
			}
			node->parent = root;

			if(root->heap_base.balance != 0)
				upin(root);
		}
	}
}

static heap_empty_t *search_empty_heap(size_t size)
{
	heap_empty_t *node = base_emptyHeap;
	while(node != NULL)
	{
		if(size < node->heap_base.Length)
		{
			if(node->smaller != NULL)
				node = node->smaller;
			else
				return node->list ? : node;
		}
		else if(size > node->heap_base.Length)
			node = node->bigger;
		else
			return node->list ? : node;
	}
	return NULL;
}

static void remove_empty_heap(heap_empty_t *node, bool with_list)
{
	if(node == NULL)
		return;
	if(node->heap_base.is_list_mbr)
	{
		if(node->next != NULL)
			((heap_empty_t*)node->next)->prev = node->prev;
		if(node->prev != NULL)
		{
			if(((heap_empty_t*)node->prev)->heap_base.is_list_mbr)
				((heap_empty_t*)node->prev)->next = node->next;
			else
				((heap_empty_t*)node->prev)->list = node->next;
		}
	}
	else
	{
		heap_empty_t *parent = node->parent;
		if(node->list != NULL && !with_list)
		{
			//Ersetze den Knoten durch den nächsten Knoten in der Liste
			heap_empty_t *new_node = node->list;
			heap_empty_t *tmp_next = new_node->next;

			new_node->heap_base.balance = node->heap_base.balance;
			new_node->heap_base.is_list_mbr = false;
			new_node->parent = parent;
			new_node->smaller = node->smaller;
			new_node->bigger = node->bigger;
			new_node->list = tmp_next;

			if(parent != NULL)
			{
				if(parent->smaller == node)
					parent->smaller = new_node;
				else
					parent->bigger = new_node;
			}
			else
				base_emptyHeap = new_node;

			if(new_node->smaller != NULL)
				((heap_empty_t*)new_node->smaller)->parent = new_node;
			if(new_node->bigger != NULL)
				((heap_empty_t*)new_node->bigger)->parent = new_node;
		}
		else
		{
			if(node->smaller == NULL && node->bigger == NULL)	//Knoten hat keine Kinder
			{
				if(parent == NULL)
				{
					base_emptyHeap = NULL;
				}
				else
				{
					if(parent->smaller == node)	//Knoten ist linkes Kind
					{
						if(parent->heap_base.balance == 1)
						{
							upout(node);
							((heap_empty_t*)node->parent)->smaller = NULL;
						}
						else
						{
							parent->smaller = NULL;
							parent->heap_base.balance++;
							if(parent->heap_base.balance == 0)
								upout(parent);
						}
					}
					else	//Knoten ist rechtes Kind
					{
						if(parent->heap_base.balance == -1)
						{
							upout(node);
							((heap_empty_t*)node->parent)->bigger = NULL;
						}
						else
						{
							parent->bigger = NULL;
							parent->heap_base.balance--;
							if(parent->heap_base.balance == 0)
								upout(parent);
						}
					}
				}
			}
			else if(!node->smaller != !node->bigger)	//Knoten hat nur ein Kind
			{
				heap_empty_t *child;
				if(node->smaller != NULL)
					child = node->smaller;
				else
					child = node->bigger;
				child->parent = node->parent;
				if(parent != NULL)
				{
					if(parent->smaller == node)
						parent->smaller = child;
					else
						parent->bigger = child;
					upout(child);
				}
				else
					base_emptyHeap = child;
			}
			else	//Knoten hat zwei Kinder
			{
				//Suche symmetrischer Vorgänger
				heap_empty_t *tmp = node->smaller;
				while(tmp->bigger != NULL)
					tmp = tmp->bigger;

				remove_empty_heap(tmp, true);

				parent = node->parent;

				//Ersetzen des aktuellen Knotens mit dem symmetrischen Vorgänger
				tmp->bigger = node->bigger;
				if(tmp->bigger != NULL)
					((heap_empty_t*)tmp->bigger)->parent = tmp;
				tmp->smaller = node->smaller;
				if(tmp->smaller != NULL)
					((heap_empty_t*)tmp->smaller)->parent = tmp;
				tmp->parent = node->parent;
				tmp->heap_base.balance = node->heap_base.balance;
				if(parent != NULL)
				{
					if(parent->smaller == node)
						parent->smaller = tmp;
					else
						parent->bigger = tmp;
				}
				else
					base_emptyHeap = tmp;
			}
		}
	}
}

static void setupNewHeapEntry(heap_t *old, heap_t *new)
{
	new->Prev = old;
	new->Next = old->Next;
	if(new->Next != NULL)
		((heap_t*)new->Next)->Prev = new;
	old->Next = new;
	new->Flags = HEAP_FLAGS;
}

void *calloc(size_t nitems, size_t size)
{
	void *Address = malloc(nitems * size);
	if(Address == NULL) return NULL;
	memset(Address, 0, nitems * size);
	return Address;
}

void free(void *ptr)
{
	heap_t *heap, *tmpheap;
	if(ptr == NULL) return;
	heap = ptr - sizeof(heap_t);
	//Ist dies eine gültige Addresse
	if(heap->Flags == (HEAP_FLAGS | HEAP_RESERVED))
	{
		//Wenn möglich Speicherbereiche zusammenführen
		if(heap->Prev != NULL)
		{
			tmpheap = heap->Prev;
			if(((uintptr_t)tmpheap + tmpheap->Length + sizeof(heap_t)) == (uintptr_t)heap
					&& !(tmpheap->Flags & HEAP_RESERVED))	//Wenn der Speicherbereich vornedran frei ist, dann mit diesem fusionieren
			{
				//Aus AVL-Baum entfernen
				remove_empty_heap((heap_empty_t*)tmpheap, false);
				tmpheap->Next = heap->Next;
				tmpheap->Length += heap->Length + sizeof(heap_t);
				heap = tmpheap;

				//Prev-Eintrag des nächsten Eintrages korrigieren
				if(heap->Next != NULL)
				{
					tmpheap = heap->Next;
					tmpheap->Prev = heap;
				}
			}
		}
		if(heap->Next != NULL)
		{
			tmpheap = heap->Next;
			if(((uintptr_t)heap + heap->Length + sizeof(heap_t)) == (uintptr_t)tmpheap
					&& !(tmpheap->Flags & HEAP_RESERVED))	//Wenn der Speicherbereich hintendran frei ist, dann mit diesem fusionieren
			{
				//Aus AVL-Baum entfernen
				remove_empty_heap((heap_empty_t*)tmpheap, false);
				heap->Next = tmpheap->Next;
				heap->Length += tmpheap->Length + sizeof(heap_t);

				//Prev-Eintrag des nächsten Eintrages korrigieren
				if(heap->Next != NULL)
				{
					tmpheap = heap->Next;
					tmpheap->Prev = heap;
				}
				else if(tmpheap == lastHeap)
				{
					lastHeap = heap;
				}
			}
		}
		heap->Flags = HEAP_FLAGS;

		//Wenn möglich überflüssige Pages als unbenutzt markieren. 1 Page bleibt mindestens benutzt
		if(heap->Length > 4096)
		{
			//Aufrunden auf nächste Pagegrenze
			uintptr_t bottom = ((uintptr_t)heap + 0xFFF) & ~0xFFF;
			//Die Page des nächsten Heaps dürfen wir nicht freigeben
			uintptr_t top = ((uintptr_t)heap + heap->Length + sizeof(heap_t)) & ~0xFFF;

			if(top > bottom)
			{
				if((uintptr_t)heap >= bottom) bottom += 4096;

				uint64_t Pages = (top - bottom) / 4096;

				if(Pages > 0)
				{
#ifdef BUILD_KERNEL
					vmm_unusePages((void*)bottom, Pages);
#else
					syscall_unusePages((void*)bottom, Pages);
#endif
				}
			}
		}

		//Speicherbereich in AVL-Baum eintragen
		add_empty_heap((heap_empty_t*)heap);
	}
}

void *malloc(size_t size)
{
	heap_t *heap, *tmp_heap;
	void *Address;
	if(size == 0)
		return NULL;

	//Size sollte ein Vielfaches von 8 sein und mindestens gross genug für die Erweiterung des Baumes
	size = MAX(sizeof(heap_empty_t) - sizeof(heap_t), ((size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1)));

	//Nach passendem Eintrag suchen
	heap_empty_t *node = search_empty_heap(size);

	if(node == NULL)
	{
		size_t pages = (size + sizeof(heap_t)) / 4096 + 1;
#ifdef BUILD_KERNEL
		node = mm_SysAlloc(pages);
#else
		node = syscall_allocPages(pages);
#endif
		if(node == NULL) return NULL;
		node->heap_base.Next = NULL;
		if(lastHeap == NULL)
			node->heap_base.Prev = NULL;
		else
		{
			node->heap_base.Prev = lastHeap;
			lastHeap->Next = node;
		}
		lastHeap = (heap_t*)node;
		node->heap_base.Length = pages * 4096 - sizeof(heap_t);
		node->heap_base.Flags = HEAP_FLAGS;
	}
	else
		remove_empty_heap((heap_empty_t*)node, false);

	//Eintrag anpassen
	heap = (heap_t*)node;
	Address = (void*)((uintptr_t)heap + sizeof(heap_t));
	if(node->heap_base.Length >= size + sizeof(heap_empty_t))
	{
		tmp_heap = Address + size;
		setupNewHeapEntry(heap, tmp_heap);
		tmp_heap->Length = heap->Length - size - sizeof(heap_t);
		heap->Length = size;				//Länge anpassen

		//Eintragen in AVL-Baum
		add_empty_heap((heap_empty_t*)tmp_heap);
		if(tmp_heap->Next == NULL)
			lastHeap = tmp_heap;
	}
	heap->Flags |= HEAP_RESERVED;		//Als reserviert markieren

	assert(((uintptr_t)Address & (HEAP_ALIGNMENT - 1)) == 0);

	return Address;
}

void *realloc(void *ptr, size_t size)
{
	if(ptr == NULL && size == 0)
		return NULL;
	if(ptr == NULL)
		return malloc(size);
	if(size == 0)
	{
		free(ptr);
		return NULL;
	}
	heap_t *Heap, *tmpHeap;
	void *Address = NULL;
	Heap = ptr - sizeof(heap_t);

	//Size sollte ein Vielfaches von 8 sein und mindestens gross genug für die Erweiterung des Baumes
	size = MAX(sizeof(heap_empty_t) - sizeof(heap_t), ((size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1)));
	//Ist dieser Heap gültig?
	if(Heap->Flags == (HEAP_FLAGS | HEAP_RESERVED))
	{
		Address = ptr;
		//TODO: Vielleicht könnte man hier auch Speicherplatz freigeben?
		//Wenn der PLatz noch da ist müssen wir nichts untenehmen
		if(Heap->Length < size)
		{
			//Ist hinten noch freier Platz?
			if(Heap->Next != NULL)
			{
				tmpHeap = Heap->Next;
				//Wenn hintendran noch freier Platz ist können wir unseren einfach vergrössern
				if(!(tmpHeap->Flags & HEAP_RESERVED) && Heap->Length + tmpHeap->Length + sizeof(heap_t) >= size)
				{
					//Aus AVL-Baum entfernen
					remove_empty_heap((heap_empty_t*)tmpHeap, false);
					//Müssen wir den Header auch noch nehmen?
					if(tmpHeap->Length < (size - Heap->Length) || tmpHeap->Length <= sizeof(heap_empty_t) - sizeof(heap_t))
					{
						Heap->Next = tmpHeap->Next;
						if(tmpHeap->Next != NULL)
						{
							((heap_t*)tmpHeap->Next)->Prev = Heap;
						}
						Heap->Length += tmpHeap->Length + sizeof(heap_t);
						if(lastHeap == tmpHeap)
							lastHeap = Heap;
					}
					//Ansonsten verschieben wir den Header des nächsten Eintrags einfach
					else
					{
						heap_t *oldHeap = tmpHeap;
						tmpHeap = memmove((void*)Heap + sizeof(heap_t) + size, tmpHeap, sizeof(heap_t));
						Heap->Next = tmpHeap;
						tmpHeap->Length -= size - Heap->Length;
						//Eintrag des übernächsten Eintrags aktualisieren
						if(tmpHeap->Next != NULL)
						{
							((heap_t*)tmpHeap->Next)->Prev = tmpHeap;
						}
						Heap->Length = size;
						if(lastHeap == oldHeap)
							lastHeap = tmpHeap;
						//Wieder in den AVL-Baum eintragen
						add_empty_heap((heap_empty_t*)tmpHeap);
					}
				}
				else
				{
					Address = malloc(size);
					if(Address)
					{
						if(size > Heap->Length)
							memcpy(Address, ptr, Heap->Length);
						else
							memcpy(Address, ptr, size);
						free(ptr);
					}
				}
			}
			else
			{
				Address = malloc(size);
				if(Address)
				{
					if(size > Heap->Length)
						memcpy(Address, ptr, Heap->Length);
					else
						memcpy(Address, ptr, size);
					free(ptr);
				}
			}
		}
	}

	assert(((uintptr_t)Address & (HEAP_ALIGNMENT - 1)) == 0);

	return Address;
}

int abs(int x)
{
	return (x < 0) ? -x : x;
}

long labs(long x)
{
	return (x < 0) ? -x : x;
}

div_t div(int numer, int denom)
{
	div_t res = {
		.quot = numer / denom,
		.rem = numer % denom
	};
	return res;
}

ldiv_t ldiv(long int numer, long int denom)
{
	ldiv_t res = {
		.quot = numer / denom,
		.rem = numer % denom
	};
	return res;
}

int32_t rand()
{
	uint32_t Number = 0;
	return Number;
}

int64_t lrand()
{
	int64_t Number = 0;
#ifdef BUILD_KERNEL
	if(cpuInfo.rdrand)
	{
	}
#endif
	return Number;
}

//Environment variables
static void check_environ()
{
	if(real_environ != environ)
	{
		if(real_environ != NULL)
		{
			char **env = real_environ;
			while(*env != NULL)
			{
				free(*env);
				env++;
			}
			free(real_environ);
		}

		size_t count = count_envs((const char**)environ);
		real_environ = malloc((count + 1) * sizeof(char*));
		for(size_t i = 0; i < count; i++)
		{
			real_environ[i] = strdup(environ[i]);
		}
		real_environ[count] = NULL;
		environ = real_environ;
	}
}

static char *getenvvar(const char *name, char **value, size_t *index)
{
	check_environ();

	if(real_environ == NULL)
		return NULL;

	char **env = real_environ;
	size_t i = 0;
	while(env[i] != NULL)
	{
		char *env_name_end = strchr(env[i], '=');
		size_t env_name_len = env_name_end - env[i];
		if(strlen(name) <= env_name_len && strncmp(name, env[i], env_name_len) == 0)
		{
			if(value != NULL) *value = env_name_end + 1;
			break;
		}
		i++;
	}
	if(index != NULL) *index = i;
	return env[i];
}

//Internal functions
void init_envvars(char **env)
{
	environ = env;
	check_environ();
}

char *getenv(const char *name)
{
	char *val = NULL;
	getenvvar(name, &val, NULL);
	return val;
}

int setenv(const char *name, const char *value, int overwrite)
{
	//EINVAL
	if(strlen(name) == 0 || strchr(name, '=') != NULL)
		return -1;

	char *val;
	size_t index = 0;
	char *env = getenvvar(name, &val, &index);
	if(env == NULL)
	{
		//Resize environ
		char **new_environ = realloc(real_environ, (index + 2) * sizeof(char*));
		//ENOMEM
		if(new_environ == NULL)
		{
			return -1;
		}
		real_environ = new_environ;
		environ = real_environ;
		real_environ[index + 1] = NULL;
		env = malloc(strlen(name) + 1 + strlen(value) + 1);
		//ENOMEM
		if(env == NULL)
		{
			return -1;
		}

		strcpy(env, name);
		strcat(env, "=");
		strcat(env, value);
		real_environ[index] = env;
	}
	else if(overwrite)
	{
		char *new_env = realloc(env, (val - env) + strlen(value));
		//ENOMEM
		if(new_env == NULL)
		{
			return -1;
		}
		real_environ[index] = new_env;
		strcpy(new_env + (val - env), value);
	}

	return 0;
}

int unsetenv(const char *name)
{
	//EINVAL
	if(strlen(name) == 0 || strchr(name, '=') != NULL)
		return -1;

	size_t index;
	char *env = getenvvar(name, NULL, &index);
	if(env != NULL)
	{
		size_t count = count_envs((const char**)real_environ);
		free(real_environ[index]);
		memmove(&real_environ[index], &real_environ[index + 1], (count - index + 1) * sizeof(char*));
		char **new_environ = realloc(real_environ, count * sizeof(char*));
		//ENOMEM
		if(new_environ == NULL)
		{
			return -1;
		}
		real_environ = new_environ;
		environ = real_environ;
	}

	return 0;
}

int putenv(char *str)
{
	char *tmp = strdup(str);
	//ENOMEM
	if(tmp == NULL)
		return -1;

	const char *name = tmp;
	char *equal = strchr(name, '=');
	if(equal == NULL)
	{
		return -1;
	}
	*equal = '\0';

	size_t index;
	char *env = getenvvar(name, NULL, &index);
	size_t count = count_envs((const char**)real_environ);
	if(env == NULL)
	{
		char **new_environ = realloc(real_environ, (count + 2) * sizeof(char*));
		//ENOMEM
		if(new_environ == NULL)
		{
			free(tmp);
			return -1;
		}
		real_environ = new_environ;
		environ = real_environ;

		real_environ[count] = str;
	}
	else
	{
		//Replace variable
		free(real_environ[index]);
		real_environ[index] = str;
	}

	free(tmp);

	return 0;
}



//Algorithms
static int cmph(const void* a, const void* b, int (*cmp)(const void*, const void*)) {
	return cmp(a, b);
}

static size_t pivot(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*, void*), void* context) {
	if (cmp(base,base + (n - 1) * size, context) < 0) {
		if (cmp(base,base + n / 2 * size, context) < 0) {
			if (cmp(base + n / 2 * size,base + (n - 1) * size, context) < 0) return n / 2;
			else return n - 1;
		}
	} else {
		if (cmp(base,base + n / 2 * size, context) > 0) {
			if (cmp(base + n / 2 * size,base + (n - 1) * size, context) < 0) return n - 1;
			else return n / 2;
		}
	}
	return 0;
}

static void swap(void* base, size_t size, size_t i, size_t j) {
	if (i == j) return;
	int8_t tmp[size];
	memcpy(tmp, base + (size * i), size);
	memcpy(base + (size * i), base + (size * j), size);
	memcpy(base + (size * j), tmp, size);
}

void qsort_s(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*, void*), void* context) {
	int8_t* data = base;
	size_t count = n;
	size_t largestPivotIndex = 0;
	while (n > 2) {
		size_t pivotIndex = pivot(data, count, size, cmp, context);
		swap(data, size, 0, pivotIndex);
		size_t i = 1;
		size_t j = count - 1;
		while (i < j) {
			while (i < j && cmp(data + (i * size), data, context) <= 0) i++;
			while (i < j && cmp(data + (j * size), data, context) > 0) j--;
			swap(data, size, i, j);
		}
		if (count == 1) pivotIndex = 0;
		else if (cmp(data + (i * size), data, context) <= 0) pivotIndex = i;
		else pivotIndex = i - 1;
		swap(data, size, 0, pivotIndex);

		if (count < n) swap(data, size, pivotIndex + 1, count);
		else largestPivotIndex = pivotIndex;
		count = pivotIndex;
		while (count <= 2 && n > 2) {
			if (count == 2 && cmp(data, data + size, context) > 0) swap(data, size, 0, 1);
			data += size * (count + 1);
			n -= count + 1;
			if (largestPivotIndex == count) {
				largestPivotIndex = 0;
				count = n;
				continue;
			}
			largestPivotIndex -= count + 1;
			if (largestPivotIndex) {
				for (count = 0; (cmp(data, data + ((count + 1) * size), context) >= 0) && (count + 1 < n) && (count < largestPivotIndex); count++) ;
				swap(data, size, 0, count);
			} else {
				data += size;
				count = --n;
			}
		}
	}
	if (n == 2 && cmp(data, data + size, context) > 0) swap(data, size, 0, 1);
}

void* bsearch_s(const void* key, const void* base, size_t n, size_t size, int (*cmp)(const void*, const void*, void*), void* context) {
	if (!n) return NULL;
	size_t min = 0;
	size_t max = n - 1;
	size_t i = (min + max) / 2;
	while (1) {
		int r = cmp(base + (size * i), key, context);
		if (r && min + 1 >= max) return NULL;
		if (r < 0) {
			min = i;
			i = (min + max + 1) / 2;
		} else if (r > 0) {
			max = i;
			i = (min + max) / 2;
		} else {
			return (void*)base + (size * i);
		}
	}
}

void qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*)) {
	qsort_s(base, n, size, (int (*)(const void*, const void*, void*))cmph, cmp);
}

void* bsearch(const void* key, const void* base, size_t n, size_t size, int (*cmp)(const void*, const void*)) {
	return bsearch_s(key, base, n, size, (int (*)(const void*, const void*, void*))cmph, cmp);
}
