#include "redis.h"

#include <math.h>

/*-----------------------------------------------------------------------------
 * Interval set API
 *----------------------------------------------------------------------------*/

/* ISETs are sets using two data structures to hold the same elements
 * in order to get O(log(N)) INSERT and REMOVE operations into an interval
 * range data structure.
 *
 * The elements are added to an hash table mapping Redis objects to intervals.
 * At the same time the elements are added to an augmented AVL tree that maps 
 * intervals to Redis objects. */


avl *avlCreate(void) {
	avl *avltree;
	
	avltree = zmalloc(sizeof(*avltree));
	avltree->size = 0;
	avltree->root = NULL;
	return avltree;
}

avlNode *avlCreateNode(double lscore, double rscore, robj *obj) {
	avlNode *an = zmalloc(sizeof(*an));
	an->leftScore = lscore;
	an->rightScore = rscore;
	an->subLeftMax = 0;
	an->subRightMax = 0;
	an->balance = 0;
	an->left = NULL;
	an->right = NULL;
	an->parent = NULL;
	an->obj = obj;
	
	return an;
}

avlFreeNode(avlNode *node) {
	if (node->obj)
		decrRefCount(node->obj);
	if (!node->left)
		avlFreeNode(node->left);
	if (!node->right)
		avlFreeNode(node->right);
	zfree(node);
}

avlFree(avl *tree) {
	if (tree->root != NULL)
		avlFreeNode(tree->root);
	zfree(tree);
}


int avlNodeCmp(avlNode *a, avlNode *b) {
	if (a->leftScore < b->leftScore)
		return -1;
	else
		return 1;
}

int avlInsertNode(avlNode *locNode, avlNode *insertNode) {
	/* Insert in the left node */
	if (avlNodeCmp(locNode,insertNode)) {
		if (!locNode->left) {
			locNode->left = insertNode;
			locNode->balance = locNode->balance - 1;
			if (locNode->balance) return 0;
			return 1;
		}
	}
	/* Insert in the right node */
	else {
		if (!locNode->right) {
			locNode->right = insertNode;
			locNode->balance = locNode->balance + 1;
			if (locNode->balance) return 0;
			return 1;
		}
	}
}

avlNode *avlInsert(avl *tree, double lscore, double rscore, robj *obj) {
	avlNode *an = avlCreateNode(lscore, rscore, obj);
	
	if (!tree->root)
		tree->root = an;
	else
		avlInsertNode(tree->root, an);
	}
	
	return an;
}