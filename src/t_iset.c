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
    
    avltree->dict = dictCreate(&isetDictType,NULL);
    
	return avltree;
}

avlNode *avlCreateNode(double lscore, double rscore, robj *obj) {
	avlNode *an = zmalloc(sizeof(*an));
	an->scores[0] = lscore;
	an->scores[1] = rscore;
	an->subLeftMax = -INFINITY;
	an->subRightMax = -INFINITY;
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
	if (node->left)
		avlFreeNode(node->left);
	if (node->right)
		avlFreeNode(node->right);
	zfree(node);
}

void avlFree(avl *tree) {
	if (tree->root != NULL)
		avlFreeNode(tree->root);
    dictRelease(tree->dict);
	zfree(tree);
}

int avlNodeCmp(avlNode *a, avlNode *b) {
	if (a->scores[0] < b->scores[0])
		return -1;
	else if (a->scores[0] > b->scores[0])
		return 1;
	else {
		if (a->scores[1] > b->scores[1])
			return -1;
		else if (a->scores[1] < b->scores[1])
			return 1;
		else
			return 0;
	}
}

void avlLeftRotation(avl * tree, avlNode *locNode) {
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
	// New root
	else {
        tree->root = newRoot;
	}
}

void avlRightRotation(avl * tree, avlNode *locNode) {
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
	// New root
	else {
        tree->root = newRoot;
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

void avlUpdateMaxScores(avlNode *locNode) {
	double oldNodeMax;
	
	while (locNode) {
		if (locNode->left) {
			oldNodeMax = locNode->left->scores[1];
			oldNodeMax = (oldNodeMax > locNode->left->subLeftMax) ? oldNodeMax : locNode->left->subLeftMax;
			oldNodeMax = (oldNodeMax > locNode->left->subRightMax) ? oldNodeMax : locNode->left->subRightMax;
			if (locNode->subLeftMax < oldNodeMax)
				locNode->subLeftMax = oldNodeMax;
		}
		else {
			locNode->subLeftMax = -INFINITY;
		}
		if (locNode->right) {
			oldNodeMax = locNode->right->scores[1];
			oldNodeMax = (oldNodeMax > locNode->right->subLeftMax) ? oldNodeMax : locNode->right->subLeftMax;
			oldNodeMax = (oldNodeMax > locNode->right->subRightMax) ? oldNodeMax : locNode->right->subRightMax;
			if (locNode->subRightMax < oldNodeMax)
				locNode->subRightMax = oldNodeMax;
		}
		else {
			locNode->subRightMax = -INFINITY;
		}
		locNode = locNode->parent;
	}
}

int avlInsertNode(avl * tree, avlNode *locNode, avlNode *insertNode) {
	/* Insert in the left node */
	if (avlNodeCmp(locNode,insertNode) > -1) {
		if (!locNode->left) {
			locNode->left = insertNode;
			insertNode->parent = locNode;
			locNode->balance = locNode->balance - 1;	
			avlUpdateMaxScores(locNode);	
			return locNode->balance ? 1 : 0;
		}
		else {
			// Left node is occupied, insert it into the subtree
			if (avlInsertNode(tree,locNode->left,insertNode)) {
				locNode->balance = locNode->balance - 1;
				if (locNode->balance == 0)
					return 0;
				else if (locNode->balance == -1)
					return 1;
					
				// Tree is unbalanced at this point
				if (locNode->left->balance < 0) {
					// Left-Left, single right rotation needed
					avlRightRotation(tree,locNode);
                    if (locNode->right)
                        locNode->right->balance = 0;
					locNode->parent->balance = 0;
					
					locNode->subLeftMax = locNode->parent->subRightMax;
					locNode->parent->subRightMax = -INFINITY;
				}
				else {
					// Left-Right, left rotation then right rotation needed
					avlLeftRotation(tree,locNode->left);
					avlRightRotation(tree,locNode);
					avlResetBalance(locNode->parent);
					
					locNode->subLeftMax = locNode->parent->subRightMax;
					locNode->parent->left->subRightMax = locNode->parent->subLeftMax;
					locNode->parent->subRightMax = -INFINITY;
					locNode->parent->subLeftMax = -INFINITY;
				}
				avlUpdateMaxScores(locNode->parent); 
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
			avlUpdateMaxScores(locNode);
			return locNode->balance ? 1 : 0;
		}
		else {
			// Right node is occupied, insert it into the subtree
			if (avlInsertNode(tree,locNode->right,insertNode)) {
				locNode->balance = locNode->balance + 1;
				if (locNode->balance == 0)
					return 0;
				else if (locNode->balance == 1)
					return 1;
					
				// Tree is unbalanced at this point
				if (locNode->right->balance > 0) {
					// Right-Right, single left rotation needed
					avlLeftRotation(tree,locNode);
                    
                    if (locNode->left)
                        locNode->left->balance = 0;
					locNode->parent->balance = 0;
					
					locNode->subRightMax = locNode->parent->subLeftMax;
					locNode->parent->subLeftMax = -INFINITY;
				}
				else {
					// Right-Left, right rotation then left rotation needed
					avlRightRotation(tree,locNode->right);
					avlLeftRotation(tree,locNode);
					avlResetBalance(locNode->parent);
					
					locNode->subRightMax = locNode->parent->subLeftMax;
					locNode->parent->right->subLeftMax = locNode->parent->subRightMax;
					locNode->parent->subRightMax = -INFINITY;
					locNode->parent->subLeftMax = -INFINITY;
				}
				avlUpdateMaxScores(locNode->parent); 
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
		avlInsertNode(tree, tree->root, an);
	
	tree->size = tree->size + 1;
	
	return an;
}

void avlRemoveFromParent(avlNode *locNode, avlNode *replacementNode) {
    if (locNode->parent) {
        if (locNode->parent->left == locNode)
            locNode->parent->left = replacementNode;
        else
            locNode->parent->right = replacementNode;
    }
}

int avlRemoveNode(avl * tree, avlNode *locNode, avlNode *delNode, char freeNodeMem) {
	int diff = avlNodeCmp(locNode, delNode);
	int heightDelta;
	avlNode *replacementNode;
		
	// This is the node we want removed
	if (diff == 0) {	
		// Remove if leaf node or replace with child if only one child		
		if (!locNode->left) {
			if (!locNode->right) {
				avlRemoveFromParent(locNode,NULL);
				if (locNode->parent)
					avlUpdateMaxScores(locNode->parent);
				if (freeNodeMem)
					avlFreeNode(locNode);
				return -1;
			}
			avlRemoveFromParent(locNode,locNode->right);
			if (locNode->parent)
				avlUpdateMaxScores(locNode->parent);
			if (freeNodeMem)
				avlFreeNode(locNode);
			return -1;
		}
		if (!locNode->right) {
			avlRemoveFromParent(locNode,locNode->left);
			if (locNode->parent)
				avlUpdateMaxScores(locNode->parent);
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
		heightDelta = avlRemoveNode(tree, locNode,replacementNode,0);
		replacementNode->left = locNode->left;
		replacementNode->right = locNode->right;
		locNode->right->parent = replacementNode;
		locNode->left->parent = replacementNode;
		replacementNode->balance = locNode->balance;
		if (locNode->parent)
			avlUpdateMaxScores(locNode->parent);
		if (freeNodeMem)
			avlFreeNode(locNode);
			
		if (replacementNode->balance == 0)
			return heightDelta;
			
		return 0;
	}
	
	// The node is in the left subtree
	else if (diff > 0) {
		if (locNode->left) {
			heightDelta = avlRemoveNode(tree, locNode->left,delNode,1);
			if (heightDelta) {
				locNode->balance = locNode->balance + 1;
				if (locNode->balance == 0)
					return -1;
				else if (locNode->balance == 1)
					return 0;
					
				if (locNode->right->balance == 1) {
					avlLeftRotation(tree,locNode);
					locNode->parent->balance = 0;
					locNode->parent->left->balance = 0;
					
					locNode->subRightMax = locNode->parent->subLeftMax;
					locNode->parent->subLeftMax = -INFINITY;
					avlUpdateMaxScores(locNode->parent);
					
					return -1;
				}
				else if (locNode->right->balance == 0){
					avlLeftRotation(tree,locNode);
					locNode->parent->balance = -1;
					locNode->parent->left->balance = 1;
					
					locNode->subRightMax = locNode->parent->subLeftMax;
					locNode->parent->subLeftMax = -INFINITY;
					avlUpdateMaxScores(locNode->parent); 
					
					return 0;					
				}
				avlRightRotation(tree,locNode->right);
				avlLeftRotation(tree,locNode);
				avlResetBalance(locNode->parent);
				
				locNode->subRightMax = locNode->parent->subLeftMax;
				locNode->parent->right->subLeftMax = locNode->parent->subRightMax;
				locNode->parent->subRightMax = -INFINITY;
				locNode->parent->subLeftMax = -INFINITY;
				
				avlUpdateMaxScores(locNode->parent);
				
				return -1;
			}
		}
	}
	
	// The node is in the right subtree
	else if (diff < 0) {
		if (locNode->right) {
			heightDelta = avlRemoveNode(tree, locNode->right,delNode,1);
			if (heightDelta) {
				locNode->balance = locNode->balance + 1;
				if (locNode->balance == 0)
					return -1;
				else if (locNode->balance == 1)
					return 0;
					
				if (locNode->left->balance == -1) {
					avlRightRotation(tree,locNode);
					locNode->parent->balance = 0;
					locNode->parent->right->balance = 0;
					
					locNode->subLeftMax = locNode->parent->subRightMax;
					locNode->parent->subRightMax = -INFINITY;
					avlUpdateMaxScores(locNode->parent);
					
					return -1;
				}
				else if (locNode->left->balance == 0){
					avlRightRotation(tree,locNode);
					locNode->parent->balance = 1;
					locNode->parent->right->balance = -1;
					
					locNode->subLeftMax = locNode->parent->subRightMax;
					locNode->parent->subRightMax = -INFINITY;
					avlUpdateMaxScores(locNode->parent);
					
					return 0;					
				}
				avlLeftRotation(tree,locNode->left);
				avlRightRotation(tree,locNode);
				avlResetBalance(locNode->parent);
				
				locNode->subLeftMax = locNode->parent->subRightMax;
				locNode->parent->left->subRightMax = locNode->parent->subLeftMax;
				locNode->parent->subRightMax = -INFINITY;
				locNode->parent->subLeftMax = -INFINITY;
				
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
	int removed = avlRemoveNode(tree, tree->root, delNode, 1);
	avlFreeNode(delNode);
	
	if (removed)
		tree->size = tree->size - 1;
		
	if (tree->size == 0)
		tree->root = NULL;
		
	return removed;
}

/*
This structure is a simple linked list that is built during the 
avlFind method. On each successful stab, the stabbed node is 
added to the list and the list tail is updated to the new node.

The genericStabCommand maintains a pointer to the list head, which
is the initial node passed to avlFind.
*/
typedef struct avlResultNode {
    avlNode * data;
    struct avlResultNode * next;
} avlResultNode;

avlResultNode *avlCreateResultNode(avlNode *data) {
	avlResultNode *arn = zmalloc(sizeof(*arn));
    arn->data = data;
    arn->next = NULL;
    return arn;
}

void avlFreeResults(avlResultNode *node) {
    if (node->next)
        avlFreeResults(node->next);
    zfree(node);
}

avlResultNode * avlStab(avlNode *node, double min, double max, avlResultNode *results) {
    
    // If the minimum endpoint of the interval falls to the right of the current node's interval and
    // any sub-tree intervals, there cannot be an interval match
    if (min > node->subRightMax && min > node->subLeftMax && min > node->scores[1])
        return results;
        
    // Search the node's left subtree
    if (node->left)
        results = avlStab(node->left, min, max, results);
        
    // Check to see if this node overlaps. 
    // For now we're only going to check for containment.
    if (min >= node->scores[0] && max <= node->scores[1]) {
        avlResultNode * newResult = avlCreateResultNode(node);
        newResult->next = results;
        results = newResult;
    }
    
    // If the max endpoint of the interval falls to the left of the start of the current node's
    // interval, there cannot be an interval match to the right of this node
    if (max < node->scores[0])
        return results;
        
    // Search the node's right subtree
    if (node->right)
        results = avlStab(node->right, min, max, results);
        
    return results;
}

/*-----------------------------------------------------------------------------
 * Interval set commands 
 *----------------------------------------------------------------------------*/

void iaddCommand(redisClient *c) {
    robj *key = c->argv[1];
    robj *iobj;
    robj *ele;
    double min = 0, max = 0;
    double *mins, *maxes, curmin = 0.0, curmax = 0.0;
    int j, elements = (c->argc-2)/2;
    int added = 0;
    avlNode * addedNode;

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

        unsigned char* eptr;

        ele = c->argv[4+j*3];

        /* XXX
            Commenting this section out for the time being -
            we will need to add an element->avlNode hashmap to 
            quickly check if an element has already been added
            to the tree and quickly remove said element before
            updating the elements score.
           XXX */
        /* If object is found in iobj */
        /*
        if ((eptr = avlFind(iobj->ptr,ele,&curmin,&curmax)) != NULL) {
            if (curmin != min || curmax != max) {
                // remove and re-insert
                // TODO 

                signalModifiedKey(c->db,key);
                server.dirty++;
            }
        } else {
        */
            /* insert into the tree */
            /* XXX: do we need the cast here? Answer from Ken! I believe so,
            as robj.ptr is declared as a void, and avlInsert expects an avl pointer */
            
            addedNode = avlInsert((avl *) (iobj->ptr), min, max, ele);
            incrRefCount(ele); /* Added to AVL tree. */
            redisAssertWithInfo(c,NULL,dictAdd(((avl *) (iobj->ptr))->dict,ele,&addedNode->scores) == DICT_OK);
            incrRefCount(ele); /* Added to dictionary. */

            signalModifiedKey(c->db,key);
            server.dirty++;
        //}

        added++;
    }

    zfree(mins);
    zfree(maxes);
    
    addReplyLongLong(c,added);
}

/* This command implements ISTAB, ISTABINTERVAL. */
void genericStabCommand(redisClient *c, robj *lscoreObj, robj *rscoreObj, int withintervals) {
    double lscore, rscore;
    robj *key = c->argv[1];
    robj *iobj;
    avlResultNode * resnode;
    avlResultNode * reswalker;
    void *replylen = NULL;
    unsigned long resultslen = 0;
    avl * tree;

    if (getDoubleFromObjectOrReply(c,lscoreObj,&lscore,NULL) != REDIS_OK) {
        addReplyError(c,"left endpoint is not a float");
        return;
    }
    
    if (getDoubleFromObjectOrReply(c,rscoreObj,&rscore,NULL) != REDIS_OK) {
        addReplyError(c,"right endpoint is not a float");
        return;
    }


    if ((iobj = lookupKeyReadOrReply(c,key,shared.nokeyerr)) == NULL ||
        checkType(c,iobj,REDIS_ISET)) return;
        
    tree = (avl *) (iobj->ptr);
    resnode = avlStab(((avl *) (iobj->ptr))->root, lscore, rscore, NULL);
    
    // TODO: Walk through the results and return them. Include intervals when 
    // 'withintervals' is set.
    
    /* No results. */
    if (resnode == NULL) {
        addReply(c, shared.emptymultibulk);
        return;
    }
    
    /* We don't know in advance how many matching elements there are in the
     * list, so we push this object that will represent the multi-bulk
     * length in the output buffer, and will "fix" it later */
    replylen = addDeferredMultiBulkLength(c);
    reswalker = resnode;
    
    while (reswalker != NULL) {
        resultslen++;
        addReplyBulk(c,(reswalker->data)->obj);
        if (withintervals) {
            addReplyDouble(c,(reswalker->data)->scores[0]);
            addReplyDouble(c,(reswalker->data)->scores[1]);
        }
        reswalker = reswalker->next;
    }
    
    if (withintervals)
        resultslen *= 3;
    
    setDeferredMultiBulkLength(c, replylen, resultslen);
    
    if (resnode)
        avlFreeResults(resnode);
}

void istabCommand(redisClient *c) {
    int withintervals = 0;
    if (c->argc > 3) {
        if (!strcasecmp(c->argv[3]->ptr,"withintervals"))
            withintervals = 1;
        else
            addReply(c,shared.syntaxerr);
    }
    genericStabCommand(c, c->argv[2], c->argv[2], withintervals);
}

void istabIntervalCommand(redisClient *c) {
    int withintervals = 0;
    if (c->argc > 4) {
        if (!strcasecmp(c->argv[4]->ptr,"withintervals"))
            withintervals = 1;
        else
            addReply(c,shared.syntaxerr);
    }
    genericStabCommand(c, c->argv[2], c->argv[3], withintervals);
}
