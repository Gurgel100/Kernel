/*
 * Copyright by Andris Suter-Dörig
 * MIT License
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "avl.h"

static int _avl_cmph(const void* a, const void* b, void* cmp) {
	int (*lcmp)(const void*, const void*) = cmp;
	return lcmp(a, b);
}

static void _avl_visiterh(const void* val, void* visiter) {
	void (*lvisiter)(const void*) = visiter;
	lvisiter(val);
}

typedef struct avl_tree_s {
	struct avl_tree_s* left;
	struct avl_tree_s* right;
	struct avl_tree_s* parent;
	void* value;
} avl_tree;

static int8_t _avl_balance(avl_tree* tree) { // right height - left height
	return ((int64_t)tree->parent & 7) - 4;
}

static void _avl_set_balance(avl_tree* tree, int8_t balance) {
	tree->parent = (avl_tree*)(((uintptr_t)tree->parent & ~7) | (balance + 4));
}

static void _avl_inc_balance(avl_tree* tree) {
	tree->parent = (avl_tree*)((uintptr_t)tree->parent + 1);
}

static void _avl_dec_balance(avl_tree* tree) {
	tree->parent = (avl_tree*)((uintptr_t)tree->parent - 1);
}

static avl_tree* _avl_parent(avl_tree* tree) {
	return (avl_tree*)((uintptr_t)tree->parent & ~7);
}

static void _avl_set_parent(avl_tree* tree, avl_tree* parent) {
	tree->parent = (avl_tree*)((uintptr_t)parent | ((uintptr_t)tree->parent & 7));
}

static avl_tree* _avl_insert(void* val, avl_tree* parent, bool left) {
	avl_tree* new;
	new = calloc(1, sizeof(avl_tree));
	assert(new);
	_avl_set_balance(new, 0);
	new->value = val;
	if (!parent) return new;
	if (left) {
		parent->left = new;
		_avl_dec_balance(parent);
	} else {
		parent->right = new;
		_avl_inc_balance(parent);
	}
	_avl_set_parent(new, parent);
	return parent;
}

static avl_tree* _avl_remove(avl_tree* old) {
	avl_tree* current = old;
	if (current->left) current = current->left;
	while (current->right) current = current->right;
	old->value = current->value;
	if (_avl_parent(current)) {
		if (_avl_parent(current)->left == current) {
			_avl_inc_balance(_avl_parent(current));
			_avl_parent(current)->left = current->left;
		} else {
			_avl_dec_balance(_avl_parent(current));
			_avl_parent(current)->right = current->left;
		}
		if (current->left) _avl_set_parent(current->left, _avl_parent(current));
	}
	avl_tree* ret = _avl_parent(current);
	free(current);
	return ret;
}

static avl_tree* _avl_rotate_left(avl_tree* current) {
	avl_tree* ret = current->right;
	avl_tree* tmp = ret->left;
	ret->left = current;
	_avl_set_parent(ret, _avl_parent(current));
	_avl_set_parent(current, ret);
	if (tmp) _avl_set_parent(tmp, current);
	if (_avl_parent(ret)) {
		if (_avl_parent(ret)->left == current) _avl_parent(ret)->left = ret;
		else _avl_parent(ret)->right = ret;
	}
	current->right = tmp;
	int8_t leftbalance = _avl_balance(ret) >= 0? -1 + _avl_balance(current) - _avl_balance(ret) : -1 + _avl_balance(current);
	int8_t topbalance = _avl_balance(ret) > 0? _avl_balance(current) - 2 : _avl_balance(ret) - 1;
	_avl_set_balance(current, leftbalance);
	_avl_set_balance(ret, topbalance);
	return ret;
}

static avl_tree* _avl_rotate_right(avl_tree* current) {
	avl_tree* ret = current->left;
	avl_tree* tmp = ret->right;
	ret->right = current;
	_avl_set_parent(ret, _avl_parent(current));
	_avl_set_parent(current, ret);
	if (tmp) _avl_set_parent(tmp, current);
	if (_avl_parent(ret)) {
		if (_avl_parent(ret)->left == current) _avl_parent(ret)->left = ret;
		else _avl_parent(ret)->right = ret;
	}
	current->left = tmp;
	int8_t rightbalance = _avl_balance(ret) <= 0? 1 + _avl_balance(current) - _avl_balance(ret) : 1 + _avl_balance(current);
	int8_t topbalance = _avl_balance(ret) < 0? _avl_balance(current) + 2 : _avl_balance(ret) + 1;
	_avl_set_balance(current, rightbalance);
	_avl_set_balance(ret, topbalance);
	return ret;
}

static avl_tree* _avl_rotate_insert(avl_tree* current) {
	avl_tree* prev = NULL;
	bool propagate = true;
	while (current != NULL) {
		if (propagate) {
			propagate = (_avl_balance(current) == -1 || _avl_balance(current) == 1);
			if (_avl_balance(current) > 1) {
				if (_avl_balance(current->right) <= 0) _avl_rotate_right(current->right);
				current = _avl_rotate_left(current);
			} else if (_avl_balance(current) < -1) {
				if (_avl_balance(current->left) >= 0) _avl_rotate_left(current->left);
				current = _avl_rotate_right(current);
			}
			if (propagate && _avl_parent(current)) {
				if (_avl_parent(current)->left == current) _avl_dec_balance(_avl_parent(current));
				else _avl_inc_balance(_avl_parent(current));
			}
		}
		prev = current;
		current = _avl_parent(current);
	}
	return prev;
}

static avl_tree* _avl_rotate_delete(avl_tree* current) {
	avl_tree* prev = NULL;
	bool propagate = true;
	while (current != NULL) {
		if (propagate) {
			if (_avl_balance(current) > 1) {
				if (_avl_balance(current->right) < 0) _avl_rotate_right(current->right);
				else if (_avl_balance(current->right) == 0) propagate = false;
				current = _avl_rotate_left(current);
			} else if (_avl_balance(current) < -1) {
				if (_avl_balance(current->left) > 0) _avl_rotate_left(current->left);
				else if (_avl_balance(current->left) == 0) propagate = false;
				current = _avl_rotate_right(current);
			} else if (_avl_balance(current) * _avl_balance(current) == 1) {
				propagate = false;
			}
			if (propagate && _avl_parent(current)) {
				if (_avl_parent(current)->left == current) _avl_inc_balance(_avl_parent(current));
				else _avl_dec_balance(_avl_parent(current));
			}
		}
		prev = current;
		current = _avl_parent(current);
	}
	return prev;
}

bool avl_add_s(avl_tree** root, void* element, int (*comp)(const void*, const void*, void*), void* context) {
	int res = 1;
	avl_tree* prev = NULL;
	avl_tree* current = *root;
	while (current && (res = comp(element, current->value, context))) {
		prev = current;
		current = res < 0? current->left : current->right;
	}
	if (!res) return false;
	*root = _avl_rotate_insert(_avl_insert(element, prev, res < 0));
	return true;
}

bool avl_remove_s(avl_tree** root, void* element, int (*comp)(const void*, const void*, void*), void* context) {
	int res = 1;
	avl_tree* current = *root;
	while (current && (res = comp(element, current->value, context))) current = res < 0? current->left : current->right;
	if (res) return false;
	*root = _avl_rotate_delete(_avl_remove(current));
	return true;
}

bool avl_search_s(avl_tree* root, void* element, int (*comp)(const void*, const void*, void*), void* context) {
	int res = 1;
	while (root && (res = comp(element, root->value, context))) root = res < 0? root->left : root->right;
	return !res;
}

static void _avl_visit_s(avl_tree* root, avl_visiting_method method, int* count, void (*visiter)(const void*, void*), void* context) {
	(*count)++;
	if (method == avl_visiting_pre_order) visiter(root->value, context);
	if (root->left) _avl_visit_s(root->left, method, count, visiter, context);
	if (method == avl_visiting_in_order) visiter(root->value, context);
	if (root->right) _avl_visit_s(root->right, method, count, visiter, context);
	if (method == avl_visiting_post_order) visiter(root->value, context);
}

int avl_visit_s(avl_tree* root, avl_visiting_method method, void (*visiter)(const void*, void*), void* context) {
	int count = 0;
	if (root) _avl_visit_s(root, method, &count, visiter, context);
	return count;
}

bool avl_add(avl_tree** root, void* element, int (*comp)(const void*, const void*)) {
	return avl_add_s(root, element, _avl_cmph, comp);
}

bool avl_remove(avl_tree** root, void* element, int (*comp)(const void*, const void*)) {
	return avl_remove_s(root, element, _avl_cmph, comp);
}

bool avl_search(avl_tree* root, void* element, int (*comp)(const void*, const void*)) {
	return avl_search_s(root, element, _avl_cmph, comp);
}

int avl_visit(avl_tree* root, avl_visiting_method method, void (*visiter)(const void*)) {
	return avl_visit_s(root, method, _avl_visiterh, visiter);
}

void avl_free(avl_tree* tree) {
	while (tree) {
		if (tree->left) {
			tree = tree->left;
		} else if (tree->right) {
			tree = tree->right;
		} else {
			if (_avl_parent(tree)) {
				if (_avl_parent(tree)->left == tree) _avl_parent(tree)->left = NULL;
				else _avl_parent(tree)->right = NULL;
			}
			avl_tree* tmp = tree;
			tree = _avl_parent(tree);
			free(tmp);
		}
	}
}
