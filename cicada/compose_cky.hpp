// -*- mode: c++ -*-

#ifndef __CICADA__COMPOSE_CKY__HPP__
#define __CICADA__COMPOSE_CKY__HPP__ 1

#include <vector>
#include <algorithm>

#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>
#include <cicada/lattice.hpp>
#include <cicada/grammar.hpp>
#include <cicada/transducer.hpp>
#include <cicada/hypergraph.hpp>

#include <utils/chunk_vector.hpp>
#include <utils/chart.hpp>
#include <utils/sgi_hash_map.hpp>
#include <utils/hashmurmur.hpp>

namespace cicada
{
  
  struct ComposeCKY
  {
    typedef Symbol symbol_type;
    typedef Vocab  vocab_type;

    typedef Lattice    lattice_type;
    typedef Grammar    grammar_type;
    typedef Transducer transducer_type;
    typedef HyperGraph hypergraph_type;
    
    typedef hypergraph_type::feature_set_type feature_set_type;
    
    typedef hypergraph_type::rule_type     rule_type;
    typedef hypergraph_type::rule_ptr_type rule_ptr_type;

    
    ComposeCKY(const symbol_type& __goal, const grammar_type& __grammar)
      : goal(__goal), grammar(__grammar)
    {
      goal_rule.reset(new rule_type(vocab_type::GOAL,
				    rule_type::symbol_set_type(1, goal.non_terminal(1)),
				    rule_type::symbol_set_type(1, goal.non_terminal(1)),
				    1));
    }
    
    struct ActiveItem
    {
      ActiveItem(const transducer_type::id_type& __node,
		 const hypergraph_type::edge_type::node_set_type __tail_nodes,
		 const feature_set_type& __features)
	: node(__node),
	  tail_nodes(__tail_nodes),
	  features(__features) {}
      ActiveItem(const transducer_type::id_type& __node)
	: node(__node),
	  tail_nodes() {}
      
      transducer_type::id_type                  node;
      hypergraph_type::edge_type::node_set_type tail_nodes;
      feature_set_type                          features;
    };
    
    typedef ActiveItem active_type;
    typedef utils::chunk_vector<active_type, 4096 / sizeof(active_type), std::allocator<active_type> > active_set_type;
    typedef utils::chart<active_set_type, std::allocator<active_set_type> > active_chart_type;
    typedef std::vector<active_chart_type, std::allocator<active_chart_type> > active_chart_set_type;

    typedef hypergraph_type::id_type passive_type;
    typedef std::vector<passive_type, std::allocator<passive_type> > passive_set_type;
    typedef utils::chart<passive_set_type, std::allocator<passive_set_type> > passive_chart_type;

#ifdef HAVE_TR1_UNORDERED_MAP
    typedef std::tr1::unordered_map<symbol_type, hypergraph_type::id_type, boost::hash<symbol_type>, std::equal_to<symbol_type>,
				    std::allocator<std::pair<const symbol_type, hypergraph_type::id_type > > > node_map_type;
#else
    typedef sgi::hash_map<symbol_type, hypergraph_type::id_type, boost::hash<symbol_type>, std::equal_to<symbol_type>,
			  std::allocator<std::pair<const symbol_type, hypergraph_type::id_type > > > node_map_type;
#endif
    typedef utils::chart<node_map_type, std::allocator<node_map_type> > node_map_chart_type;

    typedef std::vector<symbol_type, std::allocator<symbol_type> > non_terminal_set_type;
    
    void operator()(const lattice_type& lattice,
		    hypergraph_type& graph)
    {
      graph.clear();
      
      if (lattice.empty())
	return;
      
      // initialize internal structure...
      actives.clear();
      passives.clear();
      nodes.clear();
      non_terminals.clear();
      
      actives.resize(grammar.size(), active_chart_type(lattice.size() + 1));
      passives.resize(lattice.size() + 1);
      nodes.resize(lattice.size() + 1);
      
      // initialize active chart
      for (int table = 0; table < grammar.size(); ++ table) {
	const transducer_type::id_type root = grammar[table].root();
	
	for (int pos = 0; pos < lattice.size(); ++ pos)
	  if (grammar[table].valid_span(pos, pos, 0))
	    actives[table](pos, pos).push_back(active_type(root));
      }
      
      for (int length = 1; length <= lattice.size(); ++ length)
	for (int first = 0; first + length <= lattice.size(); ++ first) {
	  const int last = first + length;
	  
	  for (int table = 0; table < grammar.size(); ++ table) {
	    const transducer_type& transducer = grammar[table];

	    if (! transducer.valid_span(first, last, lattice.shortest_distance(first, last))) continue;
	    
	    // advance dots....
	    
	    // first, extend active items...
	    active_set_type& cell = actives[table](first, last);
	    
	    for (int middle = first + 1; middle < last; ++ middle) {
	      const active_set_type&  active_arcs  = actives[table](first, middle);
	      const passive_set_type& passive_arcs = passives(middle, last);
	      
	      extend_actives(transducer, active_arcs, passive_arcs, cell);
	    }

	    
	    // then, advance by terminal(s) at lattice[last - 1];
	    {
	      const active_set_type&  active_arcs  = actives[table](first, last - 1);
	      const lattice_type::arc_set_type& passive_arcs = lattice[last - 1];

	      active_set_type::const_iterator aiter_begin = active_arcs.begin();
	      active_set_type::const_iterator aiter_end = active_arcs.end();

	      lattice_type::arc_set_type::const_iterator piter_end = passive_arcs.end();
	      for (lattice_type::arc_set_type::const_iterator piter = passive_arcs.begin(); piter != piter_end; ++ piter) {
		const symbol_type& terminal = piter->label;
		const int length = piter->distance;
		
		active_set_type& cell = actives[table](first, last - 1 + length);
		
		for (active_set_type::const_iterator aiter = aiter_begin; aiter != aiter_end; ++ aiter) {
		  const transducer_type::id_type node = transducer.next(aiter->node, terminal);
		  if (node == transducer.root()) continue;
		  
		  cell.push_back(active_type(node, aiter->tail_nodes, aiter->features + piter->features));
		}
	      }
	    }
	    
	    
	    // apply rules on actives at [first, last)
	    node_map_type& node_map        = nodes(first, last);
	    passive_set_type& passive_arcs = passives(first, last);
	    
	    active_set_type::const_iterator citer_end = cell.end();
	    for (active_set_type::const_iterator citer = cell.begin(); citer != citer_end; ++ citer) {
	      const transducer_type::rule_set_type& rules = transducer.rules(citer->node);
	      
	      if (rules.empty()) continue;
	      
	      transducer_type::rule_set_type::const_iterator riter_end = rules.end();
	      for (transducer_type::rule_set_type::const_iterator riter = rules.begin(); riter != riter_end; ++ riter)
		apply_rule(*riter, citer->features, citer->tail_nodes.begin(), citer->tail_nodes.end(), node_map, passive_arcs, graph);
	    }
	  }
	  
	  // handle unary rules...
	  // order is very important...
	  
	  passive_set_type& passive_arcs = passives(first, last);
	  node_map_type& node_map        = nodes(first, last);
	  
	  for (int table = 0; table < grammar.size(); ++ table) {
	    const transducer_type& transducer = grammar[table];

	    if (! transducer.valid_span(first, last, lattice.shortest_distance(first, last))) continue;
	    
	    for (int p = 0; p < passive_arcs.size(); ++ p) {
	      const symbol_type& non_terminal = non_terminals[passive_arcs[p]];
	      
	      const transducer_type::id_type node = transducer.next(transducer.root(), non_terminal);
	      if (node == transducer.root()) continue;
	      
	      const transducer_type::rule_set_type& rules = transducer.rules(node);
	      
	      if (rules.empty()) continue;
	      
	      // passive_arcs "MAY" be modified!
	      
	      transducer_type::rule_set_type::const_iterator riter_end = rules.end();
	      for (transducer_type::rule_set_type::const_iterator riter = rules.begin(); riter != riter_end; ++ riter)
		apply_rule(*riter, feature_set_type(), &passive_arcs[p], (&passive_arcs[p]) + 1, node_map, passive_arcs, graph);
	    }
	  }
	  
	  // extend applied unary rules...
	  
	  for (int table = 0; table < grammar.size(); ++ table) {
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
      node_map_type& node_map        = nodes(0, lattice.size());
      passive_set_type& passive_arcs = passives(0, lattice.size());
      for (int p = 0; p < passive_arcs.size(); ++ p)
	if (non_terminals[passive_arcs[p]] == goal)
	  apply_rule(goal_rule, feature_set_type(), &(passive_arcs[p]), (&passive_arcs[p]) + 1, node_map, passive_arcs, graph);
      
      // we will sort to remove unreachable nodes......
      graph.topologically_sort();
    }

  private:
    
    template <typename Tp>
    struct less_non_terminal_index
    {
      bool operator()(const Tp& x, const Tp& y) const
      {
	return x.first.non_terminal_index() < y.first.non_terminal_index();
      }
    };
    
    template <typename Iterator>
    void apply_rule(const rule_ptr_type& rule,
		    const feature_set_type& features,
		    Iterator first,
		    Iterator last,
		    node_map_type& node_map,
		    passive_set_type& passives,
		    hypergraph_type& graph)
    {
      hypergraph_type::edge_type& edge = graph.add_edge(first, last);
      edge.rule = rule;
      edge.features = rule->features;
      
      if (! features.empty())
	edge.features += features;
      
      // we will sort tail_nodes with its corresponding index in source-side..
      if (edge.tail_nodes.size() > 1) {
	typedef std::pair<symbol_type, hypergraph_type::id_type> symbol_id_type;
	typedef std::vector<symbol_id_type, std::allocator<symbol_id_type> > non_terminal_set_type;
	
	non_terminal_set_type non_terminals;
	non_terminals.reserve(edge.tail_nodes.size());
	
	hypergraph_type::edge_type::node_set_type::const_iterator niter = edge.tail_nodes.begin();
	rule_type::symbol_set_type::const_iterator siter_end = rule->source.end();
	for (rule_type::symbol_set_type::const_iterator siter = rule->source.begin(); siter != siter_end; ++ siter, ++ niter)
	  if (siter->is_non_terminal())
	    non_terminals.push_back(std::make_pair(*siter, *niter));
	
	// we will use stable sort so that we can maintain relative ordering...
	std::stable_sort(non_terminals.begin(), non_terminals.end(), less_non_terminal_index<symbol_id_type>());
	
	// then, re-assign...
	{
	  hypergraph_type::edge_type::node_set_type::iterator niter = edge.tail_nodes.begin();
	  
	  non_terminal_set_type::const_iterator siter_end = non_terminals.end();
	  for (non_terminal_set_type::const_iterator siter = non_terminals.begin(); siter != siter_end; ++ siter, ++ niter)
	    *niter = siter->second;
	}
      }
      
      node_map_type::iterator niter = node_map.find(rule->lhs);
      if (niter == node_map.end()) {
	hypergraph_type::node_type& node = graph.add_node();
	
	if (rule->lhs == goal_rule->lhs)
	  graph.goal = node.id;
	else
	  passives.push_back(node.id);
	
	if (node.id >= non_terminals.size())
	  non_terminals.resize(node.id + 1);
	non_terminals[node.id] = rule->lhs;
	
	niter = node_map.insert(std::make_pair(rule->lhs, node.id)).first;
      }
      
      graph.connect_edge(edge.id, niter->second);
    }
    
    void extend_actives(const transducer_type& transducer,
			const active_set_type& actives, 
			const passive_set_type& passives,
			active_set_type& cell)
    {
      active_set_type::const_iterator aiter_begin = actives.begin();
      active_set_type::const_iterator aiter_end = actives.end();
	      
      passive_set_type::const_iterator piter_begin = passives.begin();
      passive_set_type::const_iterator piter_end = passives.end();
	      
      for (active_set_type::const_iterator aiter = aiter_begin; aiter != aiter_end; ++ aiter)
	for (passive_set_type::const_iterator piter = piter_begin; piter != piter_end; ++ piter) {
	  const symbol_type& non_terminal = non_terminals[*piter];
	  
	  const transducer_type::id_type node = transducer.next(aiter->node, non_terminal);
	  if (node == transducer.root()) continue;
	  
	  hypergraph_type::edge_type::node_set_type tail_nodes(aiter->tail_nodes.size() + 1);
	  std::copy(aiter->tail_nodes.begin(), aiter->tail_nodes.end(), tail_nodes.begin());
	  tail_nodes.back() = *piter;
	  
	  cell.push_back(active_type(node, tail_nodes, aiter->features));
	}
    }


    
  private:
    const symbol_type goal;
    const grammar_type grammar;
    
    rule_ptr_type goal_rule;
    
    active_chart_set_type  actives;
    passive_chart_type     passives;
    node_map_chart_type    nodes;
    
    non_terminal_set_type non_terminals;
  };
};

#endif
