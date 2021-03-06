// -*- mode: c++ -*-
//
//  Copyright(C) 2011-2013 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__PRUNE_UNIFORM__HPP__
#define __CICADA__PRUNE_UNIFORM__HPP__ 1

#include <cicada/hypergraph.hpp>
#include <cicada/semiring.hpp>
#include <cicada/sort_topologically.hpp>
#include <cicada/inside_outside.hpp>

#include <boost/random/uniform_int_distribution.hpp>

namespace cicada
{
  template <typename Function, typename Sampler>
  struct PruneUniform
  {
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;

    typedef HyperGraph hypergraph_type;
    
    typedef hypergraph_type::id_type id_type;
    typedef hypergraph_type::node_type node_type;
    typedef hypergraph_type::edge_type edge_type;

    typedef Function function_type;
    typedef Sampler  sampler_type;
    
    typedef std::vector<bool, std::allocator<bool> > removed_type;
    
    typedef std::vector<id_type, std::allocator<id_type> > stack_type;

    struct filter_pruned
    {
      const removed_type& removed;
      
      filter_pruned(const removed_type& __removed) : removed(__removed) {}
      
      template <typename Edge>
      bool operator()(const Edge& edge) const
      {
	return removed[edge.id];
      }
    };
    
    PruneUniform(const function_type& __function,
		 sampler_type& __sampler,
		 const size_type __kbest_size,
		 const bool __validate=true)
      : function(__function),
	sampler(__sampler),
	kbest_size(__kbest_size),
	validate(__validate) {}
    
    void operator()(const hypergraph_type& source, hypergraph_type& target)
    {
      target.clear();
      
      if (! source.is_valid())
	return;
            
      removed_type removed(source.edges.size(), true);
      
      // perform k-sampling here...
      stack_type      stack;
      
      for (size_type k = 0; k != kbest_size; ++ k) {
	stack.clear();
	stack.push_back(source.goal);
	
	while (! stack.empty()) {
	  const id_type node_id = stack.back();
	  stack.pop_back();
	  
	  const node_type& node = source.nodes[node_id];
	  
	  // invalid node!
	  if (node.edges.empty()) break;
	  
	  const size_type pos_sampled = (node.edges.size() > 1
					 ? boost::random::uniform_int_distribution<size_type>(0, node.edges.size() - 1)(sampler.generator())
					 : size_type(0));
	  
	  const id_type edge_id_sampled = node.edges[pos_sampled];
	  
	  removed[edge_id_sampled] = false;
	  
	  const edge_type& edge_sampled = source.edges[edge_id_sampled];
	  edge_type::node_set_type::const_iterator titer_end = edge_sampled.tails.end();
	  for (edge_type::node_set_type::const_iterator titer = edge_sampled.tails.begin(); titer != titer_end; ++ titer)
	    stack.push_back(*titer);
	}
      }
      
      topologically_sort(source, target, filter_pruned(removed), validate);

      if (! target.is_valid())
	target = source;
    }
    
    const function_type& function;
    sampler_type& sampler;
    const size_type kbest_size;
    const bool validate;
  };
  
  
  template <typename Function, typename Sampler>
  inline
  void prune_uniform(const HyperGraph& source, HyperGraph& target, const Function& func, Sampler& sampler, const size_t kbest_size, const bool validate=true)
  {
    PruneUniform<Function, Sampler> __prune(func, sampler, kbest_size, validate);
    
    __prune(source, target);
  }
  
  template <typename Function, typename Sampler>
  inline
  void prune_uniform(HyperGraph& source, const Function& func, Sampler& sampler, const size_t kbest_size, const bool validate=true)
  {
    PruneUniform<Function, Sampler> __prune(func, sampler, kbest_size, validate);

    HyperGraph target;
    
    __prune(source, target);
    
    source.swap(target);
  }

};

#endif
