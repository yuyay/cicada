// -*- mode: c++ -*-
//
//  Copyright(C) 2010-2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__COMPOSE_CKY__HPP__
#define __CICADA__COMPOSE_CKY__HPP__ 1

#include <vector>
#include <algorithm>
#include <set>

#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>
#include <cicada/lattice.hpp>
#include <cicada/grammar.hpp>
#include <cicada/transducer.hpp>
#include <cicada/hypergraph.hpp>

#include <utils/chunk_vector.hpp>
#include <utils/chart.hpp>
#include <utils/hashmurmur.hpp>
#include <utils/sgi_hash_set.hpp>
#include <utils/sgi_hash_map.hpp>

#include <google/dense_hash_map>
#include <google/dense_hash_set>

namespace cicada
{
  
  struct ComposeCKY
  {
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;

    typedef Symbol symbol_type;
    typedef Vocab  vocab_type;

    typedef Lattice    lattice_type;
    typedef Grammar    grammar_type;
    typedef Transducer transducer_type;
    typedef HyperGraph hypergraph_type;
    
    typedef hypergraph_type::feature_set_type   feature_set_type;
    typedef hypergraph_type::attribute_set_type attribute_set_type;

    typedef attribute_set_type::attribute_type attribute_type;
    
    typedef hypergraph_type::rule_type     rule_type;
    typedef hypergraph_type::rule_ptr_type rule_ptr_type;

    
    ComposeCKY(const symbol_type& __goal, const grammar_type& __grammar, const bool __yield_source=false, const bool __treebank=false)
      : goal(__goal), grammar(__grammar), yield_source(__yield_source), treebank(__treebank),
	attr_span_first("span-first"),
	attr_span_last("span-last")
    {
      goal_rule = rule_type::create(rule_type(vocab_type::GOAL, rule_type::symbol_set_type(1, goal.non_terminal())));
      
      node_map.set_empty_key(symbol_level_type());
      closure.set_empty_key(symbol_type());
      closure_head.set_empty_key(symbol_type());
      closure_tail.set_empty_key(symbol_type());
    }
    
    struct ActiveItem
    {
      ActiveItem(const transducer_type::id_type& __node,
		 const hypergraph_type::edge_type::node_set_type __tails,
		 const feature_set_type& __features,
		 const attribute_set_type& __attributes)
	: node(__node),
	  tails(__tails),
	  features(__features),
	  attributes(__attributes) {}
      ActiveItem(const transducer_type::id_type& __node,
		 const feature_set_type& __features,
		 const attribute_set_type& __attributes)
	: node(__node),
	  tails(),
	  features(__features),
	  attributes(__attributes) {}
      ActiveItem(const transducer_type::id_type& __node,
		 const hypergraph_type::edge_type::node_set_type __tails,
		 const feature_set_type& __features)
	: node(__node),
	  tails(__tails),
	  features(__features),
	  attributes() {}
      ActiveItem(const transducer_type::id_type& __node,
		 const feature_set_type& __features)
	: node(__node),
	  tails(),
	  features(__features),
	  attributes() {}
      ActiveItem(const transducer_type::id_type& __node)
	: node(__node),
	  tails(),
	  features(),
	  attributes() {}
      
      transducer_type::id_type                  node;
      hypergraph_type::edge_type::node_set_type tails;
      feature_set_type                          features;
      attribute_set_type                        attributes;
    };
    
    typedef ActiveItem active_type;
    typedef utils::chunk_vector<active_type, 4096 / sizeof(active_type), std::allocator<active_type> > active_set_type;
    typedef utils::chart<active_set_type, std::allocator<active_set_type> > active_chart_type;
    typedef std::vector<active_chart_type, std::allocator<active_chart_type> > active_chart_set_type;

    typedef hypergraph_type::id_type passive_type;
    typedef std::vector<passive_type, std::allocator<passive_type> > passive_set_type;
    typedef utils::chart<passive_set_type, std::allocator<passive_set_type> > passive_chart_type;

    typedef std::pair<symbol_type, int> symbol_level_type;
    
    struct symbol_level_hash : public utils::hashmurmur<size_t>
    {
      typedef utils::hashmurmur<size_t> hasher_type;
      
      size_t operator()(const symbol_level_type& x) const
      {
	return hasher_type::operator()(x.first, x.second);
      }
    };

    typedef google::dense_hash_map<symbol_level_type, hypergraph_type::id_type, symbol_level_hash, std::equal_to<symbol_level_type> > node_map_type;
    
    typedef google::dense_hash_map<symbol_type, int, boost::hash<symbol_type>, std::equal_to<symbol_type> > closure_level_type;
    typedef google::dense_hash_set<symbol_type, boost::hash<symbol_type>, std::equal_to<symbol_type> > closure_type;

    struct transducer_rule_type
    {
      size_type table;
      transducer_type::id_type node;
      size_type pos;

      transducer_rule_type(const size_type& __table, const transducer_type::id_type& __node, const size_type& __pos)
	: table(__table), node(__node), pos(__pos) {}
      transducer_rule_type()
	: table(), node(), pos() {}
      
      friend
      size_t hash_value(transducer_rule_type const& x)
      {
	return utils::hashmurmur<size_t>()(x);
      }

      friend
      bool operator==(const transducer_rule_type& x, const transducer_rule_type& y)
      {
	return x.table == y.table && x.node == y.node && x.pos == y.pos;
      }

      friend
      bool operator<(const transducer_rule_type& x, const transducer_rule_type& y)
      {
	return (x.table < y.table || (x.table == y.table && (x.node < y.node || (x.node == y.node && x.pos < y.pos))));
      }
    };
    
#ifdef HAVE_TR1_UNORDERED_SET
    typedef std::tr1::unordered_set<transducer_rule_type, boost::hash<transducer_rule_type>, std::equal_to<transducer_rule_type>,
				    std::allocator<transducer_rule_type> > transducer_rule_set_type;
    typedef std::tr1::unordered_set<symbol_level_type, symbol_level_hash, std::equal_to<symbol_level_type>,
				    std::allocator<symbol_level_type> > symbol_level_set_type;
#else
    typedef sgi::hash_set<transducer_rule_type, boost::hash<transducer_rule_type>, std::equal_to<transducer_rule_type>,
			  std::allocator<transducer_rule_type> > transducer_rule_set_type;
    typedef sgi::hash_set<symbol_level_type, symbol_level_hash, std::equal_to<symbol_level_type>,
			  std::allocator<symbol_level_type> > symbol_level_set_type;
#endif

#ifdef HAVE_TR1_UNORDERED_MAP
    typedef std::tr1::unordered_map<symbol_level_type, transducer_rule_set_type, symbol_level_hash, std::equal_to<symbol_level_type>,
				    std::allocator<std::pair<const symbol_level_type, transducer_rule_set_type> > > unary_set_type;
    typedef std::tr1::unordered_map<symbol_level_type, unary_set_type, symbol_level_hash, std::equal_to<symbol_level_type>,
				    std::allocator<std::pair<const symbol_level_type, unary_set_type> > > unary_map_type; 
#else
    typedef sgi::hash_map<symbol_level_type, transducer_rule_set_type, symbol_level_hash, std::equal_to<symbol_level_type>,
			  std::allocator<std::pair<const symbol_level_type, transducer_rule_set_type> > > unary_set_type;
    typedef sgi::hash_map<symbol_level_type, unary_set_type, symbol_level_hash, std::equal_to<symbol_level_type>,
			  std::allocator<std::pair<const symbol_level_type, unary_set_type> > > unary_map_type; 
#endif

    typedef std::pair<symbol_level_type, symbol_level_type> symbol_level_pair_type;
    
    struct symbol_level_pair_hash : public utils::hashmurmur<size_t>
    {
      typedef utils::hashmurmur<size_t> hasher_type;
      
      size_t operator()(const symbol_level_pair_type& x) const
      {
	return hasher_type::operator()(x);
      }
    };

#ifdef HAVE_TR1_UNORDERED_SET
    typedef std::tr1::unordered_set<symbol_level_pair_type, symbol_level_pair_hash, std::equal_to<symbol_level_pair_type>,
				    std::allocator<symbol_level_pair_type> > symbol_level_pair_set_type;
#else
    typedef sgi::hash_set<symbol_level_pair_type, symbol_level_pair_hash, std::equal_to<symbol_level_pair_type>,
			  std::allocator<symbol_level_pair_type> > symbol_level_pair_set_type;
#endif
  
    typedef std::vector<symbol_type, std::allocator<symbol_type> > non_terminal_set_type;

    
    
    struct less_non_terminal
    {
      less_non_terminal(const non_terminal_set_type& __non_terminals) : non_terminals(__non_terminals) {}

      bool operator()(const hypergraph_type::id_type& x, const hypergraph_type::id_type& y) const
      {
	return non_terminals[x] < non_terminals[y];
      }
      
      const non_terminal_set_type& non_terminals;
    };
    
    
    void operator()(const lattice_type& lattice,
		    hypergraph_type& graph)
    {
      graph.clear();
      
      if (lattice.empty())
	return;
      
      // initialize internal structure...
      actives.clear();
      passives.clear();
      non_terminals.clear();
      
      actives.resize(grammar.size(), active_chart_type(lattice.size() + 1));
      passives.resize(lattice.size() + 1);
      
      // initialize active chart
      for (size_t table = 0; table != grammar.size(); ++ table) {
	const transducer_type::id_type root = grammar[table].root();
	
	for (size_t pos = 0; pos != lattice.size(); ++ pos)
	  if (grammar[table].valid_span(pos, pos, 0))
	    actives[table](pos, pos).push_back(active_type(root));
      }
      
      for (size_t length = 1; length <= lattice.size(); ++ length)
	for (size_t first = 0; first + length <= lattice.size(); ++ first) {
	  const size_t last = first + length;
	  
	  node_map.clear();
	  
	  //std::cerr << "span: " << first << ".." << last << " distance: " << lattice.shortest_distance(first, last) << std::endl;
	  
	  for (size_t table = 0; table != grammar.size(); ++ table) {
	    const transducer_type& transducer = grammar[table];
	    
	    // we will advance active spans, but constrained by transducer's valid span
	    if (transducer.valid_span(first, last, lattice.shortest_distance(first, last))) {
	      // advance dots....
	      
	      // first, extend active items...
	      active_set_type& cell = actives[table](first, last);
	      for (size_t middle = first + 1; middle < last; ++ middle) {
		const active_set_type&  active_arcs  = actives[table](first, middle);
		const passive_set_type& passive_arcs = passives(middle, last);
		
		extend_actives(transducer, active_arcs, passive_arcs, cell);
	      }

	      if (! treebank || length == 1) {
		// then, advance by terminal(s) at lattice[last - 1];
		const active_set_type&  active_arcs  = actives[table](first, last - 1);
		const lattice_type::arc_set_type& passive_arcs = lattice[last - 1];
		
		active_set_type::const_iterator aiter_begin = active_arcs.begin();
		active_set_type::const_iterator aiter_end = active_arcs.end();
		
		if (aiter_begin != aiter_end) {
		  lattice_type::arc_set_type::const_iterator piter_end = passive_arcs.end();
		  for (lattice_type::arc_set_type::const_iterator piter = passive_arcs.begin(); piter != piter_end; ++ piter) {
		    const symbol_type& terminal = piter->label;
		    const int length = piter->distance;
		    
		    active_set_type& cell = actives[table](first, last - 1 + length);
		    
		    // handling of EPSILON rule...
		    if (terminal == vocab_type::EPSILON) {
		      for (active_set_type::const_iterator aiter = aiter_begin; aiter != aiter_end; ++ aiter)
			cell.push_back(active_type(aiter->node, aiter->tails, aiter->features + piter->features, aiter->attributes));
		    } else {
		      for (active_set_type::const_iterator aiter = aiter_begin; aiter != aiter_end; ++ aiter) {
			const transducer_type::id_type node = transducer.next(aiter->node, terminal);
			if (node == transducer.root()) continue;
			
			cell.push_back(active_type(node, aiter->tails, aiter->features + piter->features, aiter->attributes));
		      }
		    }
		  }
		}
	      }
	    }
	    
	    // complete active items if possible... The active items may be created from child span due to the
	    // lattice structure...
	    // apply rules on actives at [first, last)
	    
	    active_set_type&  cell         = actives[table](first, last);
	    passive_set_type& passive_arcs = passives(first, last);
	    
	    active_set_type::const_iterator citer_end = cell.end();
	    for (active_set_type::const_iterator citer = cell.begin(); citer != citer_end; ++ citer) {
	      const transducer_type::rule_pair_set_type& rules = transducer.rules(citer->node);
	      
	      if (rules.empty()) continue;
	      
	      transducer_type::rule_pair_set_type::const_iterator riter_end = rules.end();
	      for (transducer_type::rule_pair_set_type::const_iterator riter = rules.begin(); riter != riter_end; ++ riter)
		apply_rule(yield_source ? riter->source : riter->target, riter->features + citer->features, riter->attributes + citer->attributes,
			   citer->tails.begin(), citer->tails.end(), node_map, passive_arcs, graph,
			   first, last);
	    }
	  }
	  
	  // handle unary rules...
	  // order is very important...
	  // actually, we need to loop-forever...
	  
#if 0
	  if (! passives(first, last).empty()) {
	    typedef std::pair<symbol_level_type, hypergraph_type::id_type> symbol_node_type;
	    typedef std::vector<symbol_node_type, std::allocator<symbol_node_type> > symbol_node_set_type;

	    passive_set_type& passive_arcs = passives(first, last);

	    symbol_node_set_type symbol_nodes;
	    symbol_node_set_type symbol_nodes_next;
	    
	    passive_set_type::const_iterator piter_end = passive_arcs.end();
	    for (passive_set_type::const_iterator piter = passive_arcs.begin(); piter != piter_end; ++ piter) {
	      unary_closure(non_terminals[*piter]);
	      symbol_nodes.push_back(std::make_pair(symbol_level_type(non_terminals[*piter], 0), *piter));

	      //std::cerr << "passive arc: " << *piter << std::endl;
	    }
	    
	    // we will collect edges from unary_map
	    symbol_level_pair_set_type edges;
	    
	    while (! symbol_nodes.empty()) {
	      symbol_nodes_next.clear();
	      
	      symbol_node_set_type::const_iterator piter_end = symbol_nodes.end();
	      for (symbol_node_set_type::const_iterator piter = symbol_nodes.begin(); piter != piter_end; ++ piter) {
		unary_map_type::const_iterator uiter = unary_map.find(piter->first);
		if (uiter == unary_map.end()) {
		  //std::cerr << "no entry? " << piter->first.first << ':' << piter->first.second << std::endl;
		  continue;
		}
		
		unary_set_type::const_iterator niter_end = uiter->second.end();
		for (unary_set_type::const_iterator niter = uiter->second.begin(); niter != niter_end; ++ niter)
		  if (edges.find(std::make_pair(piter->first, niter->first)) == edges.end()) {
		    edges.insert(std::make_pair(piter->first, niter->first));
		    
#if 0
		    std::cerr << "construct: " << piter->first.first << ':' << piter->first.second
			      << " next: " << niter->first.first << ':' << niter->first.second
			      << std::endl;
#endif
		    
		    transducer_rule_set_type::const_iterator riter_end = niter->second.end();
		    for (transducer_rule_set_type::const_iterator riter = niter->second.begin(); riter != riter_end; ++ riter) {
		      const transducer_type& transducer = grammar[riter->table];
		      const transducer_type::id_type& node = riter->node;
		      
		      const transducer_type::rule_pair_set_type& rules = transducer.rules(node);
		      const transducer_type::rule_pair_type& rule_pair = rules[riter->pos];
		      
		      const rule_ptr_type rule = (yield_source ? rule_pair.source : rule_pair.target);
		      
		      const size_type passive_size_prev = passive_arcs.size();
		      
		      apply_rule(rule, rule_pair.features, rule_pair.attributes,
				 &(piter->second), &(piter->second) + 1, node_map, passive_arcs, graph,
				 first, last, niter->first.second);
		      
		      if (passive_size_prev != passive_arcs.size())
			symbol_nodes_next.push_back(std::make_pair(niter->first, passive_arcs.back()));
		    }
		  }
	      }
	      
	      symbol_nodes.clear();
	      symbol_nodes.swap(symbol_nodes_next);
	    }
	  }
#endif
	  
#if 1
	  if (! passives(first, last).empty()) {
	    //std::cerr << "closure from passives: " << passives(first, last).size() << std::endl;

	    passive_set_type& passive_arcs = passives(first, last);
	    
	    size_t passive_first = 0;
	    
	    closure.clear();
	    passive_set_type::const_iterator piter_end = passive_arcs.end();
	    for (passive_set_type::const_iterator piter = passive_arcs.begin(); piter != piter_end; ++ piter)
	      closure[non_terminals[*piter]] = 0;
	    
	    // run 4 iterations... actually, we should loop until convergence which will be impractical.
	    int  closure_loop = 0;
	    for (;;) {
	      const size_t passive_size = passive_arcs.size();
	      const size_t closure_size = closure.size();
	      
	      closure_head.clear();
	      closure_tail.clear();
	      
	      for (size_t table = 0; table != grammar.size(); ++ table) {
		const transducer_type& transducer = grammar[table];
		
		if (! transducer.valid_span(first, last, lattice.shortest_distance(first, last))) continue;
		
		for (size_t p = passive_first; p != passive_size; ++ p) {
		  const symbol_type non_terminal = non_terminals[passive_arcs[p]];
		  
		  const transducer_type::id_type node = transducer.next(transducer.root(), non_terminal);
		  if (node == transducer.root()) continue;
		  
		  const transducer_type::rule_pair_set_type& rules = transducer.rules(node);
		  
		  if (rules.empty()) continue;
		  
		  // passive_arcs "MAY" be modified!
		  
		  closure_tail.insert(non_terminal);
		  
		  transducer_type::rule_pair_set_type::const_iterator riter_end = rules.end();
		  for (transducer_type::rule_pair_set_type::const_iterator riter = rules.begin(); riter != riter_end; ++ riter) {
		    const rule_ptr_type rule = (yield_source ? riter->source : riter->target);
		    const symbol_type& lhs = rule->lhs;
		    
		    closure_level_type::const_iterator citer = closure.find(lhs);
		    const int level = (citer != closure.end() ? citer->second : 0);
		    
		    closure_head.insert(lhs);
		    
		    apply_rule(rule, riter->features, riter->attributes,
			       &passive_arcs[p], (&passive_arcs[p]) + 1, node_map, passive_arcs, graph,
			       first, last, level + 1);
		  }
		}
	      }
	      
	      if (passive_size == passive_arcs.size()) break;
	      
	      passive_first = passive_size;
	      
	      // we use level-one, that is the label assigned for new-lhs!
	      closure_type::const_iterator hiter_end = closure_head.end();
	      for (closure_type::const_iterator hiter = closure_head.begin(); hiter != hiter_end; ++ hiter)
		closure.insert(std::make_pair(*hiter, 1));
	      
	      // increment non-terminal level when used as tails...
	      closure_type::const_iterator titer_end = closure_tail.end();
	      for (closure_type::const_iterator titer = closure_tail.begin(); titer != titer_end; ++ titer)
		++ closure[*titer];
	      
	      if (closure_size != closure.size())
		closure_loop = 0;
	      else
		++ closure_loop;
	      
	      if (closure_loop == 1) break;
	    }
	  }
#endif
	  
	  // sort passives at passives(first, last) wrt non-terminal label in non_terminals
	  std::sort(passives(first, last).begin(), passives(first, last).end(), less_non_terminal(non_terminals));

	  //std::cerr << "span: " << first << ".." << last << " passives: " << passives(first, last).size() << std::endl;
	  
	  // extend root with passive items at [first, last)
	  for (size_t table = 0; table != grammar.size(); ++ table) {
	    const transducer_type& transducer = grammar[table];
	    
	    if (! transducer.valid_span(first, last, lattice.shortest_distance(first, last))) continue;
	    
	    const active_set_type&  active_arcs  = actives[table](first, first);
	    const passive_set_type& passive_arcs = passives(first, last);
	    
	    active_set_type& cell = actives[table](first, last);
	    
	    extend_actives(transducer, active_arcs, passive_arcs, cell);
	  }
	}
      
      // finally, collect all the parsed rules, and proceed to [goal] rule...
      // passive arcs will not be updated!
      
      // we will clear node map so that we will always create new node..
      node_map.clear();
      
      passive_set_type& passive_arcs = passives(0, lattice.size());
      for (size_t p = 0; p != passive_arcs.size(); ++ p)
	if (non_terminals[passive_arcs[p]] == goal) {
	  //std::cerr << "goal node: " << passive_arcs[p] << std::endl;
	  
	  apply_rule(goal_rule, feature_set_type(), attribute_set_type(), &(passive_arcs[p]), (&passive_arcs[p]) + 1, node_map, passive_arcs, graph,
		     0, lattice.size(),
		     0, true);
	}
      
      // we will sort to remove unreachable nodes......
      graph.topologically_sort();
    }

  private:
    
    template <typename Iterator>
    void apply_rule(const rule_ptr_type& rule,
		    const feature_set_type& features,
		    const attribute_set_type& attributes,
		    Iterator first,
		    Iterator last,
		    node_map_type& node_map,
		    passive_set_type& passives,
		    hypergraph_type& graph,
		    const int lattice_first,
		    const int lattice_last,
		    const int level = 0,
		    const bool is_goal = false)
    {
      //std::cerr << "rule: " << *rule << std::endl;

      hypergraph_type::edge_type& edge = graph.add_edge(first, last);
      edge.rule = rule;
      edge.features = features;
      edge.attributes = attributes;
      
      // assign metadata...
      edge.attributes[attr_span_first] = attribute_set_type::int_type(lattice_first);
      edge.attributes[attr_span_last]  = attribute_set_type::int_type(lattice_last);

      if (is_goal) {
	if (! graph.is_valid()) {
	  graph.goal = graph.add_node().id;
	  non_terminals.push_back(rule->lhs);
	}
	
	graph.connect_edge(edge.id, graph.goal);
      } else {
	std::pair<node_map_type::iterator, bool> result = node_map.insert(std::make_pair(std::make_pair(rule->lhs, level), 0));
	if (result.second) {
	  hypergraph_type::node_type& node = graph.add_node();
	  non_terminals.push_back(rule->lhs);
	  passives.push_back(node.id);
	  result.first->second = node.id;
	}
	
	graph.connect_edge(edge.id, result.first->second);
      }

#if 0
      std::cerr << "new rule: " << *(edge.rule)
		<< " head: " << edge.head
		<< ' ';
      std::copy(edge.tails.begin(), edge.tails.end(), std::ostream_iterator<int>(std::cerr, " "));
      std::cerr << std::endl;
#endif
      
    }
    
    bool extend_actives(const transducer_type& transducer,
			const active_set_type& actives, 
			const passive_set_type& passives,
			active_set_type& cell)
    {
      active_set_type::const_iterator aiter_begin = actives.begin();
      active_set_type::const_iterator aiter_end = actives.end();
      
      passive_set_type::const_iterator piter_begin = passives.begin();
      passive_set_type::const_iterator piter_end = passives.end();

      bool found = false;
      
      if (piter_begin != piter_end)
	for (active_set_type::const_iterator aiter = aiter_begin; aiter != aiter_end; ++ aiter)
	  if (transducer.has_next(aiter->node)) {
	    symbol_type label;
	    transducer_type::id_type node = transducer.root();

	    hypergraph_type::edge_type::node_set_type tails(aiter->tails.size() + 1);
	    std::copy(aiter->tails.begin(), aiter->tails.end(), tails.begin());
	    
	    for (passive_set_type::const_iterator piter = piter_begin; piter != piter_end; ++ piter) {
	      const symbol_type& non_terminal = non_terminals[*piter];
	      
	      if (label != non_terminal) {
		node = transducer.next(aiter->node, non_terminal);
		label = non_terminal;
	      }
	      
	      if (node == transducer.root()) continue;
	      
	      tails.back() = *piter;
	      
	      cell.push_back(active_type(node, tails, aiter->features, aiter->attributes));
	      
	      found = true;
	    }
	  }
      
      return found;
    }

    void unary_closure(const symbol_type& non_terminal)
    {
      if (unary_map.find(symbol_level_type(non_terminal, 0)) != unary_map.end()) return;

      //std::cerr << "unary closure: " << non_terminal << std::endl;
      
      symbol_level_set_type nodes;
      symbol_level_set_type nodes_all;
      symbol_level_set_type nodes_next;

      unary_map[symbol_level_type(non_terminal, 0)];
      
      closure.clear();
      closure[non_terminal] = 0;
      
      nodes.insert(symbol_level_type(non_terminal, 0));
      nodes_all.insert(symbol_level_type(non_terminal, 0));
      
      int  closure_loop = 0;
      for (;;) {
	const size_t closure_size = closure.size();
	
	closure_head.clear();
	closure_tail.clear();

	nodes_next.clear();
	
	for (size_t table = 0; table != grammar.size(); ++ table) {
	  const transducer_type& transducer = grammar[table];
	  
	  symbol_level_set_type::const_iterator piter_end = nodes.end();
	  for (symbol_level_set_type::const_iterator piter = nodes.begin(); piter != piter_end; ++ piter) {
	    const symbol_type& non_terminal = piter->first;
	    
	    const transducer_type::id_type node = transducer.next(transducer.root(), non_terminal);
	    if (node == transducer.root()) continue;
	    
	    const transducer_type::rule_pair_set_type& rules = transducer.rules(node);
	    
	    if (rules.empty()) continue;

	    //std::cerr << "non-terminal: " << non_terminal << std::endl;
	    
	    closure_tail.insert(non_terminal);
	    
	    size_type pos = 0;
	    transducer_type::rule_pair_set_type::const_iterator riter_end = rules.end();
	    for (transducer_type::rule_pair_set_type::const_iterator riter = rules.begin(); riter != riter_end; ++ riter, ++ pos) {
	      const rule_ptr_type rule = (yield_source ? riter->source : riter->target);
	      const symbol_type& lhs = rule->lhs;
	      
	      closure_level_type::const_iterator citer = closure.find(lhs);
	      const int level = (citer != closure.end() ? citer->second : 0);
	      
	      // we will assign (lhs, level + 1)
	      
	      closure_head.insert(lhs);
	      
	      unary_map[*piter][symbol_level_type(lhs, level + 1)].insert(transducer_rule_type(table, node, pos));
	      
	      if (nodes_all.find(symbol_level_type(lhs, level + 1)) == nodes_all.end()) {
		nodes_all.insert(symbol_level_type(lhs, level + 1));
		nodes_next.insert(symbol_level_type(lhs, level + 1));

		//std::cerr << "prev: " << non_terminal << ':' << piter->second << " next: " << lhs << ':' << (level + 1) << std::endl;
	      }
	    }
	  }
	}
	
	if (nodes_next.empty()) break;
	
	nodes.clear();
	nodes.swap(nodes_next);
	
	// we use level-one, that is the label assigned for new-lhs!
	closure_type::const_iterator hiter_end = closure_head.end();
	for (closure_type::const_iterator hiter = closure_head.begin(); hiter != hiter_end; ++ hiter)
	  closure.insert(std::make_pair(*hiter, 1));
	
	// increment non-terminal level when used as tails...
	closure_type::const_iterator titer_end = closure_tail.end();
	for (closure_type::const_iterator titer = closure_tail.begin(); titer != titer_end; ++ titer)
	  ++ closure[*titer];
	
	if (closure_size != closure.size())
	  closure_loop = 0;
	else
	  ++ closure_loop;
	
	if (closure_loop == 4) break;
      }
      
    }
    
  private:
    const symbol_type goal;
    const grammar_type& grammar;
    const bool yield_source;
    const bool treebank;
    const attribute_type attr_span_first;
    const attribute_type attr_span_last;
    
    rule_ptr_type goal_rule;

    active_chart_set_type  actives;
    passive_chart_type     passives;

    node_map_type         node_map;
    closure_level_type    closure;
    closure_type          closure_head;
    closure_type          closure_tail;
    non_terminal_set_type non_terminals;

    unary_map_type unary_map;
  };
  
  inline
  void compose_cky(const Symbol& goal, const Grammar& grammar, const Lattice& lattice, HyperGraph& graph, const bool yield_source=false, const bool treebank=false)
  {
    ComposeCKY(goal, grammar, yield_source, treebank)(lattice, graph);
  }
};

#endif
