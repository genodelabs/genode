/*
 * \brief  AVL tree
 * \author Norman Feske
 * \date   2006-04-12
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__AVL_TREE_H_
#define _INCLUDE__UTIL__AVL_TREE_H_

#include <util/misc_math.h>

namespace Genode {
	
	class Avl_node_base;
	template <typename> class Avl_node;
	template <typename> class Avl_tree;
}


class Genode::Avl_node_base
{
	protected:

		/**
		 * Internal policy interface
		 *
		 * The implementation of this interface is provided by the AVL tree.
		 */
		struct Policy
		{
			virtual ~Policy() { }

			/**
			 * Compare two nodes
			 *
			 * \retval false if n2 is lower than n1
			 * \retval true  if n2 is higher than or equal to n1
			 *
			 * This method must be provided by the derived class.
			 * It determines the order of nodes inside the AVL tree.
			 */
			virtual bool higher(Avl_node_base *n1, Avl_node_base *n2) const = 0;

			/**
			 * Node recomputation hook
			 *
			 * If a node gets rearranged, this method is called.
			 * It can be used to update AVL-tree-position dependent
			 * meta data.
			 */
			virtual void recompute(Avl_node_base *) { }
		};

		Avl_node_base *_child[2];  /* left and right subtrees */
		Avl_node_base *_parent;    /* parent of subtree       */
		unsigned char  _depth;     /* depth of subtree        */

	public:

		typedef bool Side;

		enum { LEFT = false, RIGHT = true };

	private:

		/**
		 * Determine depth of subtree
		 */
		inline int _child_depth(Side i) {
			return _child[i] ? _child[i]->_depth : 0; }

		/**
		 * Update depth of node
		 */
		void _recompute_depth(Policy &policy);

		/**
		 * Determine left-right bias of both subtrees
		 */
		inline Side _bias() {
			return (_child_depth(RIGHT) > _child_depth(LEFT)); }

		/**
		 * Insert subtree into specified side of the node
		 */
		void _adopt(Avl_node_base *node, Side i, Policy &policy);

		/**
		 * Rotate subtree
		 *
		 * \param side   direction of rotate operation
		 * \param node   subtree to rotate
		 *
		 * The local node_* variable names describe node locations for
		 * the left (default) rotation. For example, node_r_l is the
		 * left of the right of node.
		 */
		void _rotate_subtree(Avl_node_base *node, Side side, Policy &policy);

		/**
		 * Rebalance subtree
		 *
		 * \param node   immediate child that needs balancing
		 *
		 * 'this' is parent of the subtree to rebalance
		 */
		void _rebalance_subtree(Avl_node_base *node, Policy &policy);

	public:

		/**
		 * Constructor
		 */
		Avl_node_base();

		/**
		 * Insert new node into subtree
		 */
		void insert(Avl_node_base *node, Policy &policy);

		/**
		 * Remove node from tree
		 */
		void remove(Policy &policy);
};


/**
 * AVL node
 *
 * \param NT  type of the class derived from 'Avl_node'
 *
 * Each object to be stored in the AVL tree must be derived from 'Avl_node'.
 * The type of the derived class is to be specified as template argument to
 * enable 'Avl_node' to call virtual methods specific for the derived class.
 *
 * The NT class must implement a method called 'higher' that takes a pointer to
 * another NT object as argument and returns a bool value. The bool value is
 * true if the specified node is higher or equal in the tree order.
 */
template <typename NT>
struct Genode::Avl_node : Avl_node_base
{
	/**
	 * Return child of specified side, or nullptr if there is no child
	 *
	 * This method can be called by the NT objects to traverse the tree.
	 */
	inline NT *child(Side i) const { return static_cast<NT *>(_child[i]); }

	/**
	 * Default policy
	 *
	 * \noapi
	 */
	void recompute() { }

	/**
	 * Apply a functor (read-only) to every node within this subtree
	 *
	 * \param functor  function that takes a const NT reference
	 */
	template <typename FUNC>
	void for_each(FUNC && functor) const
	{
		if (NT * l = child(Avl_node<NT>::LEFT))  l->for_each(functor);
		functor(*static_cast<NT const *>(this));
		if (NT * r = child(Avl_node<NT>::RIGHT)) r->for_each(functor);
	}
};


/**
 * Root node of the AVL tree
 *
 * The real nodes are always attached at the left branch of this root node.
 */
template <typename NT>
class Genode::Avl_tree : Avl_node<NT>
{
	private:

		/**
		 * Auto-generated policy class specific for NT
		 */
		class Policy : public Avl_node_base::Policy
		{
			bool higher(Avl_node_base *n1, Avl_node_base *n2) const
			{
				return static_cast<NT *>(n1)->higher(static_cast<NT *>(n2));
			}

			void recompute(Avl_node_base *node)
			{
				static_cast<NT *>(node)->recompute();
			}

		} _policy;

	public:

		/**
		 * Insert node into AVL tree
		 */
		void insert(Avl_node<NT> *node) { Avl_node_base::insert(node, _policy); }

		/**
		 * Remove node from AVL tree
		 */
		void remove(Avl_node<NT> *node) { node->remove(_policy); }

		/**
		 * Request first node of the tree
		 *
		 * \return  first node, or nullptr if the tree is empty
		 */
		inline NT *first() const { return this->child(Avl_node<NT>::LEFT); }

		/**
		 * Apply a functor (read-only) to every node within the tree
		 *
		 * \param functor  function that takes a const NT reference
		 *
		 * The iteration order corresponds to the order of the keys
		 */
		template <typename FUNC>
		void for_each(FUNC && functor) const {
			if (first()) first()->for_each(functor); }
};

#endif /* _INCLUDE__UTIL__AVL_TREE_H_ */
