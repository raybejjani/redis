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

void avlFreeNode(avlNode *node) {
	if (node->obj)
		decrRefCount(node->obj);
	if (!node->left)
		avlFreeNode(node->left);
	if (!node->right)
		avlFreeNode(node->right);
	zfree(node);
}

void avlFree(avl *tree) {
	if (tree->root != NULL)
		avlFreeNode(tree->root);
	zfree(tree);
}


int avlNodeCmp(avlNode *a, avlNode *b) {
	if (a->leftScore < b->leftScore)
		return -1;
	else if (a->leftScore > b->leftScore)
		return 1;
	else {
		if (a->rightScore > b->rightScore)
			return -1;
		else if (a->rightScore < b->rightScore)
			return 1;
		else
			return 0;
	}
}

void avlLeftRotation(avlNode *locNode) {
	avlNode *newRoot = locNode->right;
	locNode->right = newRoot->left;
	newRoot->left = locNode;
	if (locNode->right)
		locNode->right->parent = locNode;
	newRoot->parent = locNode->parent;
	locNode->parent = newRoot;
	if (newRoot->parent) {
		if (avlNodeCmp(newRoot->parent,newRoot) > -1)
			newRoot->parent->left = newRoot;
		else
			newRoot->parent->right = newRoot;
	}
}

void avlRightRotation(avlNode *locNode) {
	avlNode *newRoot = locNode->left;
	locNode->left = newRoot->right;
	newRoot->right = locNode;
	if (locNode->left)
		locNode->right->parent = locNode;
	newRoot->parent = locNode->parent;
	locNode->parent = newRoot;
	if (newRoot->parent) {
		if(avlNodeCmp(newRoot->parent,newRoot) > -1)
			newRoot->parent->left = newRoot;
		else
			newRoot->parent->right = newRoot;
	}
}

void avlResetBalance(avlNode *locNode) {
	switch(locNode->balance) {
		case -1:
		locNode->left->balance = 0;
		locNode->right->balance = 1;
		break;
		case 0:
		locNode->left->balance = 0;
		locNode->right->balance = 0;
		break;
		case 1:
		locNode->left->balance = -1;
		locNode->right->balance = 0;
		break;
	}
	locNode->balance = 0;
}

int avlInsertNode(avlNode *locNode, avlNode *insertNode) {
	/* Insert in the left node */
	if (avlNodeCmp(locNode,insertNode) > -1) {
		if (!locNode->left) {
			locNode->left = insertNode;
			insertNode->parent = locNode;
			locNode->balance = locNode->balance - 1;
			return locNode->balance ? 0 : 1;
		}
		else {
			// Left node is occupied, insert it into the subtree
			if (avlInsertNode(locNode->left,insertNode)) {
				locNode->balance = locNode->balance - 1;
				if (locNode->balance == 0)
					return 0;
				else if (locNode->balance == -1)
					return 1;
					
				// Tree is unbalanced at this point
				if (locNode->left->balance < 0) {
					// Left-Left, single right rotation needed
					avlRightRotation(locNode);
					locNode->right->balance = 0;
					locNode->parent->balance = 0;
				}
				else {
					// Left-Right, left rotation then right rotation needed
					avlLeftRotation(locNode->left);
					avlRightRotation(locNode);
					avlResetBalance(locNode->parent);
				}
			}
			return 0;
		}
	}
	/* Insert in the right node */
	else {
		if (!locNode->right) {
			locNode->right = insertNode;
			insertNode->parent = locNode;
			locNode->balance = locNode->balance + 1;
			return locNode->balance ? 0 : 1;
		}
		else {
			// Right node is occupied, insert it into the subtree
			if (avlInsertNode(locNode->right,insertNode)) {
				locNode->balance = locNode->balance - 1;
				if (locNode->balance == 0)
					return 0;
				else if (locNode->balance == -1)
					return 1;
					
				// Tree is unbalanced at this point
				if (locNode->right->balance > 0) {
					// Right-Right, single left rotation needed
					avlLeftRotation(locNode);
					locNode->left->balance = 0;
					locNode->parent->balance = 0;
				}
				else {
					// Right-Left, right rotation then left rotation needed
					avlRightRotation(locNode->right);
					avlLeftRotation(locNode);
					avlResetBalance(locNode->parent);
				}
			}
			return 0;
		}
	}
}


avlNode *avlInsert(avl *tree, double lscore, double rscore, robj *obj) {
	avlNode *an = avlCreateNode(lscore, rscore, obj);
	
	if (!tree->root)
		tree->root = an;
	else
		avlInsertNode(tree->root, an);
	
	tree->size = tree->size + 1;
	
	tree->size = tree->size + 1;
	
	tree->size = tree->size + 1;
	
	return an;
}


void avlRemoveFromParent(avlNode *locNode, avlNode *replacementNode) {
	if (locNode->parent->left == locNode)
		locNode->parent->left = replacementNode;
	else
		locNode->parent->right = replacementNode;
}


int avlRemoveNode(avlNode *locNode, avlNode *delNode, char freeNodeMem) {
	int diff = avlNodeCmp(locNode, delNode);
	int heightDelta;
	avlNode *replacementNode;
		
	// This is the node we want removed
	if (diff == 0) {	
		// Remove if leaf node or replace with child if only one child		
		if (!locNode->left) {
			if (!locNode->right) {
				avlRemoveFromParent(locNode,NULL);
				if (freeNodeMem)
					avlFreeNode(locNode);
				return -1;
			}
			avlRemoveFromParent(locNode,locNode->right);
			if (freeNodeMem)
				avlFreeNode(locNode);
			return -1;
		}
		if (!locNode->right) {
			avlRemoveFromParent(locNode,locNode->left);
			if (freeNodeMem)
				avlFreeNode(locNode);
			return -1;
		}
		
		// If two children, replace from subtree
		if (locNode->balance < 0) {
			// Replace with the node's in-order predecessor
			replacementNode = locNode->left;
			while (replacementNode->right)
				replacementNode = replacementNode->right;
		}
		else {
			// Replace with the node's in-order successor
			replacementNode = locNode->right;
			while (replacementNode->left)
				replacementNode = replacementNode->left;
		}
		
		// Remove the replacementNode from the tree
		heightDelta = avlRemoveNode(locNode,replacementNode,0);
		replacementNode->left = locNode->left;
		replacementNode->right = locNode->right;
		locNode->right->parent = replacementNode;
		locNode->left->parent = replacementNode;
		replacementNode->balance = locNode->balance;
		
		if (freeNodeMem)
			avlFreeNode(locNode);
			
		if (replacementNode->balance == 0)
			return heightDelta;
			
		return 0;
	}
	
	// The node is in the left subtree
	else if (diff > 0) {
		if (locNode->left) {
			heightDelta = avlRemoveNode(locNode->left,delNode,1);
			if (heightDelta) {
				locNode->balance = locNode->balance + 1;
				if (locNode->balance == 0)
					return -1;
				else if (locNode->balance == 1)
					return 0;
					
				if (locNode->right->balance == 1) {
					avlLeftRotation(locNode);
					locNode->parent->balance = 0;
					locNode->parent->left->balance = 0;
					return -1;
				}
				else if (locNode->right->balance == 0){
					avlLeftRotation(locNode);
					locNode->parent->balance = -1;
					locNode->parent->left->balance = 1;
					return 0;					
				}
				avlRightRotation(locNode->right);
				avlLeftRotation(locNode);
				avlResetBalance(locNode->parent);
				return -1;
			}
		}
	}
	
	// The node is in the right subtree
	else if (diff < 0) {
		if (locNode->right) {
			heightDelta = avlRemoveNode(locNode->right,delNode,1);
			if (heightDelta) {
				locNode->balance = locNode->balance + 1;
				if (locNode->balance == 0)
					return -1;
				else if (locNode->balance == 1)
					return 0;
					
				if (locNode->left->balance == -1) {
					avlRightRotation(locNode);
					locNode->parent->balance = 0;
					locNode->parent->right->balance = 0;
					return -1;
				}
				else if (locNode->left->balance == 0){
					avlRightRotation(locNode);
					locNode->parent->balance = 1;
					locNode->parent->right->balance = -1;
					return 0;					
				}
				avlLeftRotation(locNode->left);
				avlRightRotation(locNode);
				avlResetBalance(locNode->parent);
				return -1;
			}
		}
	}
	
	return 0;
}


int avlRemove(avl *tree, double lscore, double rscore) {
	if (!tree->root)
		return 0;
	
	avlNode *delNode = avlCreateNode(lscore, rscore, NULL);
	int removed = avlRemoveNode(tree->root, delNode, 1);
	avlFreeNode(delNode);
	
	if (removed)
		tree->size = tree->size - 1;
		
	return removed;
}

/*-----------------------------------------------------------------------------
 * Interval set commands 
 *----------------------------------------------------------------------------*/

/* This generic command implements both ZADD and ZINCRBY. */
void iaddCommand(redisClient *c) {
    robj *key = c->argv[1];
    robj *iobj;
    robj *curobj;
    double min = 0, max = 0;
    double *mins, *maxes;
    int j, elements = (c->argc-2)/2;
    int added = 0;

    /* 5, 8, 11... arguments */
    if ((c->argc - 2) % 3) {
        addReply(c,shared.syntaxerr);
        return;
    }

    /* Start parsing all the scores, we need to emit any syntax error
     * before executing additions to the sorted set, as the command should
     * either execute fully or nothing at all. */
    mins = zmalloc(sizeof(double)*elements);
    maxes = zmalloc(sizeof(double)*elements);
    for (j = 0; j < elements; j++) {
        /* mins are 2, 5, 8... */
        if (getDoubleFromObjectOrReply(c,c->argv[2+j*3],&mins[j],NULL)
            != REDIS_OK)
        {
            zfree(mins);
            zfree(maxes);
            return;
        }
        /* maxes are 3, 6, 9... */
        if (getDoubleFromObjectOrReply(c,c->argv[3+j*3],&maxes[j],NULL)
            != REDIS_OK)
        {
            zfree(mins);
            zfree(maxes);
            return;
        }
    }

    /* Lookup the key and create the interval tree if does not exist. */
    iobj = lookupKeyWrite(c->db,key);
    if (iobj == NULL) {
        iobj = createIsetObject();
        dbAdd(c->db,key,iobj);
    } else {
        if (iobj->type != REDIS_ISET) {
            addReply(c,shared.wrongtypeerr);
            zfree(mins);
            zfree(maxes);
            return;
        }
    }

    for (j = 0; j < elements; j++) {
        min = mins[j];
        max = maxes[j];

        curobj = avlCreateNode(min, max, iobj);
        avlInsert(iobj, min, max, curobj);
        added++;
    }

    zfree(mins);
    zfree(maxes);
    addReplyLongLong(c,added);
}
