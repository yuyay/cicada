// -*- mode: c++ -*-
//
//  Copyright(C) 2010-2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__EXPECTED_NGRAM__HPP__
#define __CICADA__EXPECTED_NGRAM__HPP__ 1

#include <vector>
#include <utility>

#include <cicada/hypergraph.hpp>
#include <cicada/inside_outside.hpp>

#include <utils/hashmurmur3.hpp>
#include <utils/simple_vector.hpp>
#include <utils/unordered_set.hpp>
#include <utils/bithack.hpp>

namespace cicada
{
  
  template <typename Function, typename Counts, typename Op>
  class ExpectedNGram
  {
  public:
    typedef HyperGraph hypergraph_type;
    
    typedef hypergraph_type::id_type   id_type;
    typedef hypergraph_type::node_type node_type;
    typedef hypergraph_type::edge_type edge_type;
    typedef hypergraph_type::rule_type rule_type;
    
    typedef rule_type::vocab_type      vocab_type;
    typedef rule_type::symbol_type     symbol_type;
    typedef rule_type::symbol_set_type symbol_set_type;
    
    
    typedef Function function_type;
    typedef typename function_type::value_type weight_type;
    
    typedef Op operation_type;

    typedef symbol_type word_type;

    typedef std::vector<word_type, std::allocator<word_type> > buffer_type;
    
    typedef symbol_set_type context_type;
    
    typedef std::pair<context_type, context_type> context_pair_type;
    typedef std::vector<context_pair_type, std::allocator<context_pair_type> > context_pair_set_type;

    typedef std::vector<weight_type, std::allocator<weight_type> > weight_set_type;

    typedef std::vector<id_type, std::allocator<id_type> > node_set_type;
    typedef std::vector<node_set_type, std::allocator<node_set_type> > node_map_type;

    typedef context_pair_type state_type;
    
    struct state_hash_type : public utils::hashmurmur3<size_t>
    {
      size_t operator()(const state_type& x) const
      {
	typedef utils::hashmurmur3<size_t> hasher_type;
	
	return hasher_type::operator()(x.first.begin(), x.first.end(), hasher_type::operator()(x.second.begin(), x.second.end(), 0));
      }
    };

    typedef typename utils::unordered_set<state_type, state_hash_type, std::equal_to<state_type>, std::allocator<state_type > >::type state_set_type;

    typedef utils::simple_vector<int, std::allocator<int> > index_set_type;

    
  public:
    ExpectedNGram(Function __function,
		  Op __op,
		  const int __order,
		  const bool __bos_eos=false)
      : function(__function),
	op(__op),
	order(__order),
	bos_eos(__bos_eos) {}
    
    void operator()(const hypergraph_type& graph, Counts& counts)
    {
      node_map.clear();
      node_map.reserve(graph.nodes.size());
      node_map.resize(graph.nodes.size());
      
      contexts.clear();
      contexts.reserve(graph.nodes.size() * 128);
      
      inside.clear();
      inside.reserve(graph.nodes.size());
      inside.resize(graph.nodes.size());
      
      outside.clear();
      outside.reserve(graph.nodes.size());
      outside.resize(graph.nodes.size());

      cicada::inside(graph, inside, function);
      cicada::outside(graph, inside, outside, function);

      const weight_type weight_goal = inside[graph.goal];
      
      hypergraph_type::node_set_type::const_iterator niter_end = graph.nodes.end();
      for (hypergraph_type::node_set_type::const_iterator niter = graph.nodes.begin(); niter != niter_end; ++ niter) {
	const node_type& node = *niter;
	const bool is_goal = (node.id == graph.goal);
	
	state_buf.clear();
	
	node_type::edge_set_type::const_iterator eiter_end = node.edges.end();
	for (node_type::edge_set_type::const_iterator eiter = node.edges.begin(); eiter != eiter_end; ++ eiter) {
	  const edge_type& edge = graph.edges[*eiter];
	  
	  weight_type weight = function(edge) * outside[node.id] / weight_goal;
	  edge_type::node_set_type::const_iterator niter_end = edge.tails.end();
	  for (edge_type::node_set_type::const_iterator niter = edge.tails.begin(); niter != niter_end; ++ niter)
	    weight *= inside[*niter];
	  
	  index_set_type j_ends(edge.tails.size(), 0);
	  index_set_type j(edge.tails.size(), 0);
	  
	  for (size_t i = 0; i != edge.tails.size(); ++ i)
	    j_ends[i] = node_map[edge.tails[i]].size();

	  edge_type::node_set_type tails(edge.tails.size());
	  
	  for (;;) {
	    // current tails...
	    for (size_t i = 0; i != edge.tails.size(); ++ i)
	      tails[i] = node_map[edge.tails[i]][j[i]];
	    
	    // operation at virtua node
	    op(edge, weight, counts);
	    
	    // apply various ngram cconetxt...
	    const state_type state = apply(edge, tails, weight, counts, is_goal);
	    
	    if (! is_goal) {
	      typename state_set_type::iterator biter = state_buf.find(state);
	      if (biter == state_buf.end()) {
		const id_type node_id = contexts.size();
		
		contexts.push_back(state);
		node_map[edge.head].push_back(node_id);
		
		state_buf.insert(state);
	      }
	    }
	    
	    // proceed to the next j
	    size_t index = 0;
	    for (/**/; index != edge.tails.size(); ++ index) {
	      ++ j[index];
	      if (j[index] < j_ends[index]) break;
	      j[index] = 0;
	    }
	    // finished!
	    if (index == edge.tails.size()) break;
	  }
	}
      }
    }

    template <typename Tails>
    state_type apply(const edge_type& edge, const Tails& tails, const weight_type& weight, Counts& counts, const bool is_goal)
    {
      const context_type& context = edge.rule->rhs;

      const int context_size = order - 1;
      
      buffer.clear();
      
      if (tails.empty()) {
	context_type::const_iterator citer_end = context.end();
	for (context_type::const_iterator citer = context.begin(); citer != citer_end; ++ citer)
	  if (*citer != vocab_type::EPSILON)
	    buffer.push_back(*citer);
	
	collect_counts(edge, buffer.begin(), buffer.end(), weight, counts);

	const state_type state(static_cast<int>(buffer.size()) <= context_size
			       ? std::make_pair(context_type(buffer.begin(), buffer.end()),
						context_type())
			       : std::make_pair(context_type(buffer.begin(), buffer.begin() + context_size),
						context_type(buffer.end() - context_size, buffer.end())));
	
	if (is_goal && bos_eos) {
	  buffer.insert(buffer.begin(), vocab_type::BOS);
	  buffer.insert(buffer.end(), vocab_type::EOS);
	  
	  collect_counts(edge, buffer.begin(), buffer.begin() + 1, weight, counts);
	  if (buffer.begin() + 1 != buffer.end() - 1)
	    collect_counts(edge, buffer.begin(), buffer.begin() + 1, buffer.end() - 1, weight, counts);
	  
	  collect_counts(edge, buffer.begin(), buffer.end() - 1, buffer.end(), weight, counts);
	  collect_counts(edge, buffer.end() - 1, buffer.end(), weight, counts);
	}
	
	return state;
      } else {
	buffer.reserve(context.size() + tails.size() * order * 2);
	
	int star_first = -1;
	int star_last  = -1;
	
	buffer_type::iterator biter_first = buffer.begin();
	buffer_type::iterator biter       = buffer.begin();
	
	int non_terminal_pos = 0;
	context_type::const_iterator citer_end = context.end();
	for (context_type::const_iterator citer = context.begin(); citer != citer_end; ++ citer) {
	  if (citer->is_non_terminal()) {
	    
	    // collect ngram counts
	    if (biter != buffer.end()) {
	      if (biter_first != biter)
		collect_counts(edge, biter_first, biter, buffer.end(), weight, counts);
	      collect_counts(edge, biter, buffer.end(), weight, counts);
	      biter = buffer.end();
	    }

	    const int __non_terminal_index = citer->non_terminal_index();
	    const int antecedent_index = utils::bithack::branch(__non_terminal_index <= 0, non_terminal_pos, __non_terminal_index - 1);
	    ++ non_terminal_pos;
	    
	    const context_pair_type& context_pair = contexts[tails[antecedent_index]];
	    
	    buffer.insert(buffer.end(), context_pair.first.begin(), context_pair.first.end());
	    if (biter_first != biter && biter != buffer.end())
	      collect_counts(edge, biter_first, biter, buffer.end(), weight, counts);
	    biter = buffer.end();
	    
	    if (! context_pair.second.empty()) {
	      biter_first = buffer.end();
		  
	      star_last = buffer.size();
	      if (star_first < 0)
		star_first = buffer.size();
		  
	      buffer.insert(buffer.end(), context_pair.second.begin(), context_pair.second.end());
	      biter = buffer.end();
	    }
		
	  } else if (*citer != vocab_type::EPSILON)
	    buffer.push_back(*citer);
	}
	    
	if (biter != buffer.end()) {
	  if (biter_first != biter)
	    collect_counts(edge, biter_first, biter, buffer.end(), weight, counts);
	  collect_counts(edge, biter, buffer.end(), weight, counts);
	  biter = buffer.end();
	}
	
	state_type state;
	if (star_first >= 0) {
	  const int prefix_size = utils::bithack::min(star_first, context_size);
	  const int suffix_size = utils::bithack::min(int(buffer.size() - star_last), context_size);
	  
	  state = std::make_pair(context_type(buffer.begin(), buffer.begin() + prefix_size),
				 context_type(buffer.end() - suffix_size, buffer.end()));
	  
	} else {
	  state = (static_cast<int>(buffer.size()) <= context_size
		   ? std::make_pair(context_type(buffer.begin(), buffer.end()),
				    context_type())
		   : std::make_pair(context_type(buffer.begin(), buffer.begin() + context_size),
				    context_type(buffer.end() - context_size, buffer.end())));
	}
	
	if (is_goal && bos_eos) {
	  if (star_first >= 0) {
	    const int prefix_size = utils::bithack::min(star_first, context_size);
	    const int suffix_size = utils::bithack::min(int(buffer.size() - star_last), context_size);
		
	    buffer.insert(buffer.begin(), vocab_type::BOS);
	    buffer.insert(buffer.end(), vocab_type::EOS);
	    
	    collect_counts(edge, buffer.begin(), buffer.begin() + 1, weight, counts);
	    collect_counts(edge, buffer.begin(), buffer.begin() + 1, buffer.begin() + 1 + prefix_size, weight, counts);
	    
	    collect_counts(edge, buffer.end() - suffix_size - 1, buffer.end() - 1, buffer.end(), weight, counts);
	    collect_counts(edge, buffer.end() - 1, buffer.end(), weight, counts);
	  } else {
	    buffer.insert(buffer.begin(), vocab_type::BOS);
	    buffer.insert(buffer.end(), vocab_type::EOS);
	    
	    collect_counts(edge, buffer.begin(), buffer.begin() + 1, weight, counts);
	    if (buffer.begin() + 1 != buffer.end() - 1) 
	      collect_counts(edge, buffer.begin(), buffer.begin() + 1, buffer.end() - 1, weight, counts);
	    
	    collect_counts(edge, buffer.begin(), buffer.end() - 1, buffer.end(), weight, counts);
	    collect_counts(edge, buffer.end() - 1, buffer.end(), weight, counts);
	  }
	}
	
	return state;
      }
    }

    template <typename Iterator>
    void collect_counts(const edge_type& edge, Iterator first, Iterator iter, Iterator last, const weight_type& weight, Counts& counts)
    {
      const int context_size = order - 1;
      
      first = std::max(iter - context_size, first);
	
      for (/**/; first != iter; ++ first)
	for (Iterator iter2 = iter; iter2 != std::min(first + order, last); ++ iter2) 
	  op(edge, weight, counts, first, iter2 + 1);
      
	  //counts[typename Counts::key_type(first, iter2 + 1)] += weight;
    }


    template <typename Iterator>
    void collect_counts(const edge_type& edge, Iterator first, Iterator last, const weight_type& weight, Counts& counts)
    {
      for (/**/; first != last; ++ first)
	for (Iterator iter = first; iter != std::min(first + order, last); ++ iter)
	  op(edge, weight, counts, first, iter + 1);
      
	  //counts[typename Counts::key_type(first, iter + 1)] += weight;
    }
    
  private:
    context_pair_set_type contexts;
    weight_set_type       inside;
    weight_set_type       outside;

    node_map_type  node_map;
    state_set_type state_buf;

    buffer_type buffer;
    
    function_type  function;
    operation_type op;
    int  order;
    bool bos_eos;
  };
  
  namespace impl
  {
    template <typename Counts>
    struct expected_ngram_op
    {
      template <typename Edge, typename Weight>
      void operator()(const Edge& edge, const Weight& weight, Counts& counts) const
      {
	// no op
      }

      template <typename Edge, typename Weight, typename Iterator>
      void operator()(const Edge& edge, const Weight& weight, Counts& counts, Iterator first, Iterator last) const
      {
	counts[typename Counts::key_type(first, last)] += weight;
      }
    };
  };
  
  
  template <typename Function, typename Counts>
  inline
  void expected_ngram(const HyperGraph& graph, Function function, Counts& counts, const int order, const bool bos_eos=false)
  {
    ExpectedNGram<Function, Counts, impl::expected_ngram_op<Counts> > __expected(function, impl::expected_ngram_op<Counts>(), order, bos_eos);
    
    __expected(graph, counts);
  }

  template <typename Function, typename Counts, typename Operation>
  inline
  void expected_ngram(const HyperGraph& graph, Function function, Operation op, Counts& counts, const int order, const bool bos_eos=false)
  {
    ExpectedNGram<Function, Counts, Operation > __expected(function, op, order, bos_eos);
    
    __expected(graph, counts);
  }
  
};

#endif
