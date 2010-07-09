// -*- mode: c++ -*-

#ifndef __CICADA__SPAN__HPP__
#define __CICADA__SPAN__HPP__ 1

#include <stdint.h>

#include <iostream>
#include <utility>
#include <algorithm>
#include <vector>

#include <cicada/hypergraph.hpp>
#include <cicada/vocab.hpp>
#include <cicada/symbol.hpp>

namespace cicada
{
  
  struct NodeSpan
  {
    typedef HyperGraph hypergraph_type;
    
    typedef hypergraph_type::id_type id_type;
    typedef hypergraph_type::node_type node_type;
    typedef hypergraph_type::edge_type edge_type;

    typedef hypergraph_type::rule_type     rule_type;
    typedef hypergraph_type::rule_ptr_type rule_ptr_type;

    typedef Vocab vocab_type;
    
    template <typename SpanSet>
    void operator()(const hypergraph_type& graph, SpanSet& spans)
    {
      if (graph.is_valid()) {
	spans[graph.goal] = std::make_pair(0, 0);
	traverse(graph, spans, graph.goal);
      }
    }
    
    // top-down traversal to compute spans...
    template <typename SpanSet>
    void traverse(const hypergraph_type& graph, SpanSet& spans, id_type node_id)
    {
      const node_type& node = graph.nodes[node_id];
      
      // parant-span starts from spans[node_id].first;
      
      node_type::edge_set_type::const_iterator eiter_end = node.edges.end();
      for (node_type::edge_set_type::const_iterator eiter = node.edges.begin(); eiter != eiter_end; ++ eiter) {
	const edge_type& edge = graph.edges[*eiter];
	
	int span_pos = spans[node_id].first;
	
	int non_terminal_pos = 0;
	rule_type::symbol_set_type::const_iterator siter_end = edge.rule->source.end();
	for (rule_type::symbol_set_type::const_iterator siter = edge.rule->source.begin(); siter != siter_end; ++ siter) {
	  if (siter->is_non_terminal()) {
	    int non_terminal_index = siter->non_terminal_index() - 1;
	    if (non_terminal_index < 0)
	      non_terminal_index = non_terminal_pos;
	    
	    spans[edge.tails[non_terminal_index]].first = span_pos;
	    
	    traverse(graph, spans, edge.tails[non_terminal_index]);
	    
	    span_pos = spans[edge.tails[non_terminal_index]].second;
	    
	    ++ non_terminal_pos;
	  } else if (*siter != vocab_type::EPSILON)
	    ++ span_pos;
	}
	
	spans[node_id].second = utils::bithack::max(spans[node_id].second, span_pos);
      }
    }
  };

  template <typename SpanSet>
  void node_span(const HyperGraph& graph, SpanSet& spans)
  {
    NodeSpan __node_span;
    
    __node_span(graph, spans);
  }
};

#endif
