/*
 * \brief  AVL tree
 * \author Norman Feske
 * \date   2006-04-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/avl_tree.h>
#include <base/log.h>

using namespace Genode;


inline void Avl_node_base::_recompute_depth(Policy &policy)
{
	unsigned char old_depth = _depth;
	_depth = max(_child_depth(LEFT), _child_depth(RIGHT)) + 1;

	/* if our own value changes, update parent */
	if (_depth != old_depth && _parent)
		_parent->_recompute_depth(policy);

	/* call recompute hook only for valid tree nodes */
	if (_parent)
		policy.recompute(this);
}


void Avl_node_base::_adopt(Avl_node_base *node, Side i, Policy &policy)
{
	_child[i] = node;
	if (node)
		node->_parent = this;

	_recompute_depth(policy);
}


void Avl_node_base::_rotate_subtree(Avl_node_base *node, Side side, Policy &policy)
{
	int i = (node == _child[0]) ? LEFT : RIGHT;

	Avl_node_base *node_r   = node->_child[!side];
	Avl_node_base *node_r_l = node_r->_child[side];

	/* simple rotation */
	if (node_r->_bias() == !side) {

		node->_adopt(node_r_l, !side, policy);
		node_r->_adopt(node, side, policy);

		_adopt(node_r, i, policy);
	}

	/* double rotation */
	else if (node_r_l) {

		Avl_node_base *node_r_l_l = node_r_l->_child[side];
		Avl_node_base *node_r_l_r = node_r_l->_child[!side];

		node->_adopt(node_r_l_l, !side, policy);
		node_r->_adopt(node_r_l_r, side, policy);

		node_r_l->_adopt(node, side, policy);
		node_r_l->_adopt(node_r, !side, policy);

		_adopt(node_r_l, i, policy);
	}
}


void Avl_node_base::_rebalance_subtree(Avl_node_base *node, Policy &policy)
{
	int v = node->_child_depth(RIGHT) - node->_child_depth(LEFT);

	/* return if subtree is in balance */
	if (abs(v) < 2) return;

	_rotate_subtree(node, (v < 0), policy);
}


void Avl_node_base::insert(Avl_node_base *node, Policy &policy)
{
	if (node == this) {
		error("inserting element ", node, " twice into avl tree!");
		return;
	}

	Side i = LEFT;

	/* for non-root nodes, decide for a branch */
	if (_parent)
		i = policy.higher(this, node);

	if (_child[i])
		_child[i]->insert(node, policy);
	else
		_adopt(node, i, policy);

	/* the inserted node might have changed the depth of the subtree */
	_recompute_depth(policy);

	if (_parent)
		_parent->_rebalance_subtree(this, policy);
}


void Avl_node_base::remove(Policy &policy)
{
	Avl_node_base *lp = 0;
	Avl_node_base *l  = _child[0];

	if (!_parent)
		error("tried to remove AVL node that is not in an AVL tree");

	if (l) {

		/* find right-most node in left sub tree (l) */
		while (l && l->_child[1])
			l = l->_child[1];

		/* isolate right-most node in left sub tree */
		if (l == _child[0])
			_adopt(l->_child[0], LEFT, policy);
		else
			l->_parent->_adopt(l->_child[0], RIGHT, policy);

		/* consistent state */

		/* remember for rebalancing */
		if (l->_parent != this)
			lp = l->_parent;

		/* exchange this and l */
		for (int i = 0; i < 2; i++)
			if (_parent->_child[i] == this)
				_parent->_adopt(l, i, policy);

		l->_adopt(_child[0], LEFT, policy);
		l->_adopt(_child[1], RIGHT, policy);

	} else {

		/* no left sub tree, attach our right sub tree to our parent */
		for (int i = 0; i < 2; i++)
			if (_parent->_child[i] == this)
				_parent->_adopt(_child[1], i, policy);
	}

	/* walk the tree towards its root and rebalance sub trees */
	while (lp && lp->_parent) {
		Avl_node_base *lpp = lp->_parent;
		lpp->_rebalance_subtree(lp, policy);
		lp = lpp;
	}

	/* reset node pointers */
	_child[LEFT] = _child[RIGHT] = 0;
	_parent = 0;
}


Avl_node_base::Avl_node_base() : _parent(0), _depth(1) {
	_child[LEFT] = _child[RIGHT] = 0; }
