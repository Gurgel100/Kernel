/*
 * avl.h
 *
 *  Created on: 18.09.2015
 *      Author: pascal
 */

/*
 * Copyright by Andris Suter-Dörig
 * MIT License
 */

#ifndef AVL_H_
#define AVL_H_

#include "stdbool.h"
#include "stdint.h"

typedef enum avl_visiting_method_e {
	avl_visiting_pre_order,
	avl_visiting_in_order,
	avl_visiting_post_order
} avl_visiting_method;

typedef struct avl_tree_s {
	struct avl_tree_s* left;
	struct avl_tree_s* right;
	struct avl_tree_s* parent;
	void* value;
	int8_t balance; // right height - left height

} avl_tree;

/*
 * Fügt Element element in *root ein. Gibt zurück ob eingefügt
 */
bool avl_add_s(avl_tree** root, void* element, int (*comp)(const void*, const void*, void*), void* context);

/*
 * Löscht element aus *root. Gibt zurück ob gelöscht
 */
bool avl_remove_s(avl_tree** root, void* element, int (*comp)(const void*, const void*, void*), void* context);

/*
 * Sucht element in root. Gibt zurück ob gefunden
 */
bool avl_search_s(avl_tree* root, void* element, int (*comp)(const void*, const void*, void*), void* context);

/*
 * Besucht alle Knoten.
 */
int avl_visit_s(avl_tree* root, avl_visiting_method method, void (*visiter)(const void*, void*), void* context);

/*
 * Fügt element hinzu
 */
bool avl_add(avl_tree** root, void* element, int (*comp)(const void*, const void*));
bool avl_remove(avl_tree** root, void* element, int (*comp)(const void*, const void*));
bool avl_search(avl_tree* root, void* element, int (*comp)(const void*, const void*));
int avl_visit(avl_tree* root, avl_visiting_method method, void (*visiter)(const void*));

/*
 * Löscht den gesamten Baum
 */
void avl_free(avl_tree* tree);

#endif /* AVL_H_ */
