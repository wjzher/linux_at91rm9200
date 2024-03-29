/*
 * Implementation of the hash table type.
 *
 * Author : Stephen Smalley, <sds@epoch.ncsc.mil>
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#ifdef CONFIG_AT91_HASHTAB
#include "hashtab.h"
struct hashtab *hashtab_create(u32 (*hash_value)(struct hashtab *h, void *key),
                               int (*keycmp)(struct hashtab *h, void *key1, void *key2),
                               u32 size)
{
	struct hashtab *p;
	u32 i;

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return p;

	memset(p, 0, sizeof(*p));
	p->size = size;
	p->nel = 0;
	p->hash_value = hash_value;
	p->keycmp = keycmp;
	p->htable = kmalloc(sizeof(*(p->htable)) * size, GFP_KERNEL);
	if (p->htable == NULL) {
		kfree(p);
		return NULL;
	}

	for (i = 0; i < size; i++)
		p->htable[i] = NULL;

	return p;
}

int hashtab_insert(struct hashtab *h, void *key, void *datum)
{
	u32 hvalue;
	struct hashtab_node *prev, *cur, *newnode;

	if (!h || h->nel == HASHTAB_MAX_NODES)
		return -EINVAL;

	hvalue = h->hash_value(h, key);
	prev = NULL;
	cur = h->htable[hvalue];
	while (cur && h->keycmp(h, key, cur->key) != 0) {// keycmp > 0
		prev = cur;
		cur = cur->next;
	}

	if (cur && (h->keycmp(h, key, cur->key) == 0))
		return -EEXIST;

	newnode = kmalloc(sizeof(*newnode), GFP_KERNEL);
	if (newnode == NULL)
		return -ENOMEM;
	memset(newnode, 0, sizeof(*newnode));
	newnode->key = key;
	newnode->datum = datum;
	if (prev) {
		newnode->next = prev->next;
		prev->next = newnode;
	} else {
		newnode->next = h->htable[hvalue];
		h->htable[hvalue] = newnode;
	}

	h->nel++;
	return 0;
}

void *hashtab_search(struct hashtab *h, void *key)
{
	u32 hvalue;
	struct hashtab_node *cur;

	if (!h)
		return NULL;

	hvalue = h->hash_value(h, key);
	cur = h->htable[hvalue];
	while (cur != NULL && h->keycmp(h, key, cur->key) != 0)// keycmp > 0??
		cur = cur->next;

	if (cur == NULL || (h->keycmp(h, key, cur->key) != 0))
		return NULL;

	return cur->datum;
}

void hashtab_destroy(struct hashtab *h, int dk)
{
	u32 i;
	struct hashtab_node *cur, *temp;

	if (!h)
		return;

	for (i = 0; i < h->size; i++) {
		cur = h->htable[i];
		while (cur != NULL) {
			temp = cur;
			cur = cur->next;
			if (temp->datum) {
				kfree(temp->datum);
			}
			if (temp->key && dk) {
				kfree(temp->key);
			}
			kfree(temp);
		}
		h->htable[i] = NULL;
	}

	kfree(h->htable);
	h->htable = NULL;

	kfree(h);
}

int hashtab_map(struct hashtab *h,
		int (*apply)(void *k, void *d, void *args),
		void *args)
{
	u32 i;
	int ret;
	struct hashtab_node *cur;

	if (!h)
		return 0;

	for (i = 0; i < h->size; i++) {
		cur = h->htable[i];
		while (cur != NULL) {
			ret = apply(cur->key, cur->datum, args);
			if (ret)
				return ret;
			cur = cur->next;
		}
	}
	return 0;
}


void hashtab_stat(struct hashtab *h, struct hashtab_info *info)
{
	u32 i, chain_len, slots_used, max_chain_len;
	struct hashtab_node *cur;

	slots_used = 0;
	max_chain_len = 0;
	for (slots_used = max_chain_len = i = 0; i < h->size; i++) {
		cur = h->htable[i];
		if (cur) {
			slots_used++;
			chain_len = 0;
			while (cur) {
				chain_len++;
				cur = cur->next;
			}

			if (chain_len > max_chain_len)
				max_chain_len = chain_len;
		}
	}

	info->slots_used = slots_used;
	info->max_chain_len = max_chain_len;
}
// 将hashtab中的所有元素取出, 拷贝大小为size * h->nel
int hashtab_getall(struct hashtab *h, void *v, int size)
{
	u32 i, slots_remain = h->nel;
	struct hashtab_node *cur;
	unsigned char *pv = v;
	for (i = 0; i < h->size && slots_remain; i++) {
		cur = h->htable[i];
		while (cur) {
			slots_remain--;
			memcpy(pv, cur->datum, size);
			pv += size;
			cur = cur->next;
		}
	}
	return h->nel;
}

void *hashtab_remove(struct hashtab *h, void *k, int dk)
{
	u32 hvalue;
	struct hashtab_node *cur, **pp;
	void *v;
	if (!h)
		return NULL;
	hvalue = h->hash_value(h, k);
	pp = &h->htable[hvalue];
	cur = *pp;
	while (cur) {
		// check hash value
		if (!h->keycmp(h, k, cur->key)) {
			// get
			v = cur->datum;
			h->nel--;
			*pp = cur->next;
			// key也要free吗
			if (dk && cur->key) {
				kfree(cur->key);
			}
			kfree(cur);
			return v;
		}
		pp = &cur->next;
		cur = cur->next;
	}
	return NULL;
}
#endif
