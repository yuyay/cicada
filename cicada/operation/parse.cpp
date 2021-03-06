//
//  Copyright(C) 2011-2013 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#define BOOST_SPIRIT_THREADSAFE
#define PHOENIX_THREADSAFE

#include <boost/spirit/include/qi.hpp>

#include <iostream>

#include <cicada/operation.hpp>
#include <cicada/parameter.hpp>
#include <cicada/parse.hpp>
#include <cicada/grammar_simple.hpp>
#include <cicada/grammar_unknown.hpp>
#include <cicada/compose_phrase.hpp>

#include <cicada/operation/functional.hpp>
#include <cicada/operation/parse.hpp>

#include <utils/lexical_cast.hpp>
#include <utils/resource.hpp>
#include <utils/piece.hpp>

namespace cicada
{
  namespace operation
  {
    ParseTreeCKY::ParseTreeCKY(const std::string& parameter,
			       const tree_grammar_type& __tree_grammar,
			       const grammar_type& __grammar,
			       const std::string& __goal,
			       const int __debug)
      : base_type("parse-tree-cky"),
	tree_grammar(__tree_grammar), grammar(__grammar),
	goal(__goal),
	weights(0),
	weights_assigned(0),
	size(200),
	weights_one(false),
	weights_fixed(false),
	weights_extra(),
	yield_source(false),
	frontier(false),
	unique_goal(false),
	debug(__debug)
    {
      typedef cicada::Parameter param_type;
	
      param_type param(parameter);
      if (utils::ipiece(param.name()) != "parse-tree-cky" && utils::ipiece(param.name()) != "parse-tree-cyk")
	throw std::runtime_error("this is not a Tree parser");
	
      bool source = false;
      bool target = false;
      
      for (param_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "size")
	  size = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "weights")
	  weights = &base_type::weights(piter->second);
	else if (utils::ipiece(piter->first) == "weights-one")
	  weights_one = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "yield") {
	  if (utils::ipiece(piter->second) == "source")
	    source = true;
	  else if (utils::ipiece(piter->second) == "target")
	    target = true;
	  else
	    throw std::runtime_error("unknown yield: " + piter->second);
	} else if (utils::ipiece(piter->first) == "goal")
	  goal = piter->second;
	else if (utils::ipiece(piter->first) == "grammar")
	  grammar_local.push_back(piter->second);
	else if (utils::ipiece(piter->first) == "tree-grammar")
	  tree_grammar_local.push_back(piter->second);
	else if (utils::ipiece(piter->first) == "frontier")
	  frontier = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "unique" || utils::ipiece(piter->first) == "unique-goal")
	  unique_goal = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "weight") {
	  namespace qi = boost::spirit::qi;
	  namespace standard = boost::spirit::standard;

	  std::string::const_iterator iter = piter->second.begin();
	  std::string::const_iterator iter_end = piter->second.end();

	  std::string name;
	  double      value;
	  
	  if (! qi::phrase_parse(iter, iter_end,
				 qi::lexeme[+(!(qi::lit('=') >> qi::double_ >> (standard::space | qi::eoi))
					      >> (standard::char_ - standard::space))]
				 >> '='
				 >> qi::double_,
				 standard::blank, name, value) || iter != iter_end)
	    throw std::runtime_error("weight parameter parsing failed");
	  
	  weights_extra[name] = value;
	} else
	  std::cerr << "WARNING: unsupported parameter for Tree parser: " << piter->first << "=" << piter->second << std::endl;
      }
	
      if (source && target)
	throw std::runtime_error("Tree parser can work either source or target yield");
	
      yield_source = source;

      if (weights && weights_one)
	throw std::runtime_error("you have weights, but specified all-one parameter");
      
      if (weights_one && ! weights_extra.empty())
	throw std::runtime_error("you have extra weights, but specified all-one parameter");

      if (weights || weights_one)
	weights_fixed = true;
      
      if (! weights)
	weights = &base_type::weights();
      
      const tree_grammar_type& tree_grammar_parse = (tree_grammar_local.empty() ? tree_grammar : tree_grammar_local);
      
      tree_grammar_type::const_iterator giter_end = tree_grammar_parse.end();
      for (tree_grammar_type::const_iterator giter = tree_grammar_parse.begin(); giter != giter_end; ++ giter)
	if (! (*giter)->is_cky())
	  throw std::runtime_error("tree grammar should be indexed with CKY option");
    }
    
    
    void ParseTreeCKY::assign(const weight_set_type& __weights)
    {
      if (! weights_fixed)
	weights_assigned = &__weights;
    }
    
    void ParseTreeCKY::operator()(data_type& data) const
    {
      typedef cicada::semiring::Logprob<double> weight_type;

      if (data.lattice.empty()) return;

      const lattice_type& lattice = data.lattice;
      hypergraph_type& hypergraph = data.hypergraph;
      hypergraph_type parsed;
      
      if (debug)
	std::cerr << name << ": " << data.id << std::endl;
      
      const weight_set_type* weights_parse = (weights_assigned ? weights_assigned : &(weights->weights));
      
      const grammar_type& grammar_parse = (grammar_local.empty() ? grammar : grammar_local);
      const tree_grammar_type& tree_grammar_parse = (tree_grammar_local.empty() ? tree_grammar : tree_grammar_local);
	
      utils::resource start;

      grammar_parse.assign(lattice);
      tree_grammar_parse.assign(lattice);
      
      if (weights_one)
	cicada::parse_tree_cky(goal, tree_grammar_parse, grammar_parse, weight_function_one<weight_type>(), lattice, parsed, size, yield_source, frontier, unique_goal);
      else if (! weights_extra.empty())
	cicada::parse_tree_cky(goal, tree_grammar_parse, grammar_parse, weight_function_extra<weight_type>(*weights_parse, weights_extra.begin(), weights_extra.end()), lattice, parsed, size, yield_source, frontier, unique_goal);
      else
	cicada::parse_tree_cky(goal, tree_grammar_parse, grammar_parse, weight_function<weight_type>(*weights_parse), lattice, parsed, size, yield_source, frontier, unique_goal);
	
      utils::resource end;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " cpu time: " << (end.cpu_time() - start.cpu_time())
		  << " user time: " << (end.user_time() - start.user_time())
		  << " thread time: " << (end.thread_time() - start.thread_time())
		  << std::endl;
      
      if (debug)
	std::cerr << name << ": " << data.id
		  << " # of nodes: " << parsed.nodes.size()
		  << " # of edges: " << parsed.edges.size()
		  << " valid? " << utils::lexical_cast<std::string>(parsed.is_valid())
		  << std::endl;

      statistics_type::statistic_type& stat = data.statistics[name];
      
      ++ stat.count;
      stat.node += parsed.nodes.size();
      stat.edge += parsed.edges.size();
      stat.user_time += (end.user_time() - start.user_time());
      stat.cpu_time  += (end.cpu_time() - start.cpu_time());
      stat.thread_time  += (end.thread_time() - start.thread_time());
	
      hypergraph.swap(parsed);
    }

    ParseTree::ParseTree(const std::string& parameter,
			 const tree_grammar_type& __tree_grammar,
			 const grammar_type& __grammar,
			 const std::string& __goal,
			 const int __debug)
      : base_type("parse-tree"),
	tree_grammar(__tree_grammar), grammar(__grammar),
	goal(__goal),
	weights(0),
	weights_assigned(0),
	size(200),
	weights_one(false),
	weights_fixed(false),
	weights_extra(),
	yield_source(false),
	frontier(false),
	debug(__debug)
    {
      typedef cicada::Parameter param_type;
	
      param_type param(parameter);
      if (utils::ipiece(param.name()) != "parse-tree")
	throw std::runtime_error("this is not a Tree parser");
	
      bool source = false;
      bool target = false;
      
      for (param_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "size")
	  size = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "weights")
	  weights = &base_type::weights(piter->second);
	else if (utils::ipiece(piter->first) == "weights-one")
	  weights_one = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "yield") {
	  if (utils::ipiece(piter->second) == "source")
	    source = true;
	  else if (utils::ipiece(piter->second) == "target")
	    target = true;
	  else
	    throw std::runtime_error("unknown yield: " + piter->second);
	} else if (utils::ipiece(piter->first) == "goal")
	  goal = piter->second;
	else if (utils::ipiece(piter->first) == "grammar")
	  grammar_local.push_back(piter->second);
	else if (utils::ipiece(piter->first) == "tree-grammar")
	  tree_grammar_local.push_back(piter->second);
	else if (utils::ipiece(piter->first) == "frontier")
	  frontier = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "weight") {
	  namespace qi = boost::spirit::qi;
	  namespace standard = boost::spirit::standard;

	  std::string::const_iterator iter = piter->second.begin();
	  std::string::const_iterator iter_end = piter->second.end();

	  std::string name;
	  double      value;
	  
	  if (! qi::phrase_parse(iter, iter_end,
				 qi::lexeme[+(!(qi::lit('=') >> qi::double_ >> (standard::space | qi::eoi))
					      >> (standard::char_ - standard::space))]
				 >> '='
				 >> qi::double_,
				 standard::blank, name, value) || iter != iter_end)
	    throw std::runtime_error("weight parameter parsing failed");
	  
	  weights_extra[name] = value;
	} else
	  std::cerr << "WARNING: unsupported parameter for Tree parser: " << piter->first << "=" << piter->second << std::endl;
      }
	
      if (source && target)
	throw std::runtime_error("Tree parser can work either source or target yield");
	
      yield_source = source;

      if (weights && weights_one)
	throw std::runtime_error("you have weights, but specified all-one parameter");

      if (weights_one && ! weights_extra.empty())
	throw std::runtime_error("you have extra weights, but specified all-one parameter");
      
      if (weights || weights_one)
	weights_fixed = true;
      
      if (! weights)
	weights = &base_type::weights();
      
      const tree_grammar_type& tree_grammar_parse = (tree_grammar_local.empty() ? tree_grammar : tree_grammar_local);
      
      tree_grammar_type::const_iterator giter_end = tree_grammar_parse.end();
      for (tree_grammar_type::const_iterator giter = tree_grammar_parse.begin(); giter != giter_end; ++ giter)
	if ((*giter)->is_cky())
	  throw std::runtime_error("tree grammar should not be indexed with CKY option");
    }
    
    
    void ParseTree::assign(const weight_set_type& __weights)
    {
      if (! weights_fixed)
	weights_assigned = &__weights;
    }
    
    void ParseTree::operator()(data_type& data) const
    {
      typedef cicada::semiring::Logprob<double> weight_type;

      if (! data.hypergraph.is_valid()) return;

      hypergraph_type& hypergraph = data.hypergraph;
      hypergraph_type parsed;
      
      if (debug)
	std::cerr << name << ": " << data.id << std::endl;
      
      const weight_set_type* weights_parse = (weights_assigned ? weights_assigned : &(weights->weights));
      
      const grammar_type& grammar_parse = (grammar_local.empty() ? grammar : grammar_local);
      const tree_grammar_type& tree_grammar_parse = (tree_grammar_local.empty() ? tree_grammar : tree_grammar_local);
	
      utils::resource start;

      grammar_parse.assign(hypergraph);
      tree_grammar_parse.assign(hypergraph);
      
      if (weights_one)
	cicada::parse_tree(goal, tree_grammar_parse, grammar_parse, weight_function_one<weight_type>(), hypergraph, parsed, size, yield_source, frontier);
      else if (! weights_extra.empty())
	cicada::parse_tree(goal, tree_grammar_parse, grammar_parse, weight_function_extra<weight_type>(*weights_parse, weights_extra.begin(), weights_extra.end()), hypergraph, parsed, size, yield_source, frontier);
      else
	cicada::parse_tree(goal, tree_grammar_parse, grammar_parse, weight_function<weight_type>(*weights_parse), hypergraph, parsed, size, yield_source, frontier);
	
      utils::resource end;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " cpu time: " << (end.cpu_time() - start.cpu_time())
		  << " user time: " << (end.user_time() - start.user_time())
		  << " thread time: " << (end.thread_time() - start.thread_time())
		  << std::endl;
      
      if (debug)
	std::cerr << name << ": " << data.id
		  << " # of nodes: " << parsed.nodes.size()
		  << " # of edges: " << parsed.edges.size()
		  << " valid? " << utils::lexical_cast<std::string>(parsed.is_valid())
		  << std::endl;

      statistics_type::statistic_type& stat = data.statistics[name];
      
      ++ stat.count;
      stat.node += parsed.nodes.size();
      stat.edge += parsed.edges.size();
      stat.user_time += (end.user_time() - start.user_time());
      stat.cpu_time  += (end.cpu_time() - start.cpu_time());
      stat.thread_time  += (end.thread_time() - start.thread_time());
	
      hypergraph.swap(parsed);
    }

    ParseCKY::ParseCKY(const std::string& parameter,
		       const grammar_type& __grammar,
		       const std::string& __goal,
		       const int __debug)
      : base_type("parse-cky"),
	grammar(__grammar),
	goal(__goal), 
	weights(0),
	weights_assigned(0),
	size(200),
	weights_one(false),
	weights_fixed(false),
	weights_extra(),
	yield_source(false),
	treebank(false),
	pos_mode(false),
	ordered(false),
	frontier(false),
	unique_goal(false),
	debug(__debug)
    { 
      typedef cicada::Parameter param_type;
      
      param_type param(parameter);
      if (utils::ipiece(param.name()) != "parse-cky" && utils::ipiece(param.name()) != "parse-cyk")
	throw std::runtime_error("this is not a CKY(CYK) parser");
      
      bool source = false;
      bool target = false;
	
      for (param_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "size")
	  size = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "weights")
	  weights = &base_type::weights(piter->second);
	else if (utils::ipiece(piter->first) == "weights-one")
	  weights_one = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "goal")
	  goal = piter->second;
	else if (utils::ipiece(piter->first) == "grammar")
	  grammar_local.push_back(piter->second);
	else if (utils::ipiece(piter->first) == "yield") {
	  if (utils::ipiece(piter->second) == "source")
	    source = true;
	  else if (utils::ipiece(piter->second) == "target")
	    target = true;
	  else
	    throw std::runtime_error("unknown yield: " + piter->second);
	} else if (utils::ipiece(piter->first) == "treebank")
	  treebank = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "pos")
	  pos_mode = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "ordered")
	  ordered = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "frontier")
	  frontier = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "unique" || utils::ipiece(piter->first) == "unique-goal")
	  unique_goal = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "weight") {
	  namespace qi = boost::spirit::qi;
	  namespace standard = boost::spirit::standard;

	  std::string::const_iterator iter = piter->second.begin();
	  std::string::const_iterator iter_end = piter->second.end();

	  std::string name;
	  double      value;
	  
	  if (! qi::phrase_parse(iter, iter_end,
				 qi::lexeme[+(!(qi::lit('=') >> qi::double_ >> (standard::space | qi::eoi))
					      >> (standard::char_ - standard::space))]
				 >> '='
				 >> qi::double_,
				 standard::blank, name, value) || iter != iter_end)
	    throw std::runtime_error("weight parameter parsing failed");
	  
	  weights_extra[name] = value;
	} else
	  std::cerr << "WARNING: unsupported parameter for CKY parser: " << piter->first << "=" << piter->second << std::endl;
      }
	
      if (source && target)
	throw std::runtime_error("CKY parser can work either source or target yield");
	
      yield_source = source;

      if (weights && weights_one)
	throw std::runtime_error("you have weights, but specified all-one parameter");

      if (weights_one && ! weights_extra.empty())
	throw std::runtime_error("you have extra weights, but specified all-one parameter");
      
      if (weights || weights_one)
	weights_fixed = true;
      
      if (! weights)
	weights = &base_type::weights();
    }

    void ParseCKY::assign(const weight_set_type& __weights)
    {
      if (! weights_fixed)
	weights_assigned = &__weights;
    }

    void ParseCKY::operator()(data_type& data) const
    {
      typedef cicada::semiring::Logprob<double> weight_type;

      const lattice_type& lattice = data.lattice;
      hypergraph_type& hypergraph = data.hypergraph;
      hypergraph_type parsed;
      
      hypergraph.clear();
      if (lattice.empty()) return;
    
      if (debug)
	std::cerr << name << ": " << data.id << std::endl;

      const weight_set_type* weights_parse = (weights_assigned ? weights_assigned : &(weights->weights));
      
      const grammar_type& grammar_parse = (grammar_local.empty() ? grammar : grammar_local);

      utils::resource start;
      
      grammar_parse.assign(lattice);
	
      if (weights_one)
	cicada::parse_cky(goal, grammar_parse, weight_function_one<weight_type>(), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier, unique_goal);
      else if (! weights_extra.empty())
	cicada::parse_cky(goal, grammar_parse, weight_function_extra<weight_type>(*weights_parse, weights_extra.begin(), weights_extra.end()), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier, unique_goal);
      else
	cicada::parse_cky(goal, grammar_parse, weight_function<weight_type>(*weights_parse), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier, unique_goal);
      
      utils::resource end;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " cpu time: " << (end.cpu_time() - start.cpu_time())
		  << " user time: " << (end.user_time() - start.user_time())
		  << " thread time: " << (end.thread_time() - start.thread_time())
		  << std::endl;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " # of nodes: " << parsed.nodes.size()
		  << " # of edges: " << parsed.edges.size()
		  << " valid? " << utils::lexical_cast<std::string>(parsed.is_valid())
		  << std::endl;

      statistics_type::statistic_type& stat = data.statistics[name];
      
      ++ stat.count;
      stat.node += parsed.nodes.size();
      stat.edge += parsed.edges.size();
      stat.user_time += (end.user_time() - start.user_time());
      stat.cpu_time  += (end.cpu_time() - start.cpu_time());
      stat.thread_time  += (end.thread_time() - start.thread_time());
    
      hypergraph.swap(parsed);
    }

    ParseAgenda::ParseAgenda(const std::string& parameter,
		       const grammar_type& __grammar,
		       const std::string& __goal,
		       const int __debug)
      : base_type("parse-agenda"),
	grammar(__grammar),
	goal(__goal),
	weights(0),
	weights_assigned(0),
	size(200),
	weights_one(false),
	weights_fixed(false),
	weights_extra(),
	yield_source(false),
	treebank(false),
	pos_mode(false),
	ordered(false),
	frontier(false),
	debug(__debug)
    { 
      typedef cicada::Parameter param_type;
      
      param_type param(parameter);
      if (utils::ipiece(param.name()) != "parse-agenda")
	throw std::runtime_error("this is not an agenda parser");
      
      bool source = false;
      bool target = false;
	
      for (param_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "size")
	  size = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "weights")
	  weights = &base_type::weights(piter->second);
	else if (utils::ipiece(piter->first) == "weights-one")
	  weights_one = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "goal")
	  goal = piter->second;
	else if (utils::ipiece(piter->first) == "grammar")
	  grammar_local.push_back(piter->second);
	else if (utils::ipiece(piter->first) == "yield") {
	  if (utils::ipiece(piter->second) == "source")
	    source = true;
	  else if (utils::ipiece(piter->second) == "target")
	    target = true;
	  else
	    throw std::runtime_error("unknown yield: " + piter->second);
	} else if (utils::ipiece(piter->first) == "treebank")
	  treebank = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "pos")
	  pos_mode = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "ordered")
	  ordered = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "frontier")
	  frontier = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "weight") {
	  namespace qi = boost::spirit::qi;
	  namespace standard = boost::spirit::standard;

	  std::string::const_iterator iter = piter->second.begin();
	  std::string::const_iterator iter_end = piter->second.end();

	  std::string name;
	  double      value;
	  
	  if (! qi::phrase_parse(iter, iter_end,
				 qi::lexeme[+(!(qi::lit('=') >> qi::double_ >> (standard::space | qi::eoi))
					      >> (standard::char_ - standard::space))]
				 >> '='
				 >> qi::double_,
				 standard::blank, name, value) || iter != iter_end)
	    throw std::runtime_error("weight parameter parsing failed");
	  
	  weights_extra[name] = value;
	} else
	  std::cerr << "WARNING: unsupported parameter for agenda parser: " << piter->first << "=" << piter->second << std::endl;
      }
	
      if (source && target)
	throw std::runtime_error("agenda parser can work either source or target yield");
      
      yield_source = source;

      if (weights && weights_one)
	throw std::runtime_error("you have weights, but specified all-one parameter");

      if (weights_one && ! weights_extra.empty())
	throw std::runtime_error("you have extra weights, but specified all-one parameter");
      
      if (weights || weights_one)
	weights_fixed = true;

      if (! weights)
	weights = &base_type::weights();
    }

    void ParseAgenda::assign(const weight_set_type& __weights)
    {
      if (! weights_fixed)
	weights_assigned = &__weights;
    }

    void ParseAgenda::operator()(data_type& data) const
    {
      typedef cicada::semiring::Logprob<double> weight_type;
      
      const lattice_type& lattice = data.lattice;
      hypergraph_type& hypergraph = data.hypergraph;
      hypergraph_type parsed;
      
      hypergraph.clear();
      if (lattice.empty()) return;
    
      if (debug)
	std::cerr << name << ": " << data.id << std::endl;
      
      const weight_set_type* weights_parse = (weights_assigned ? weights_assigned : &(weights->weights));

      const grammar_type& grammar_parse = (grammar_local.empty() ? grammar : grammar_local);

      utils::resource start;

      grammar_parse.assign(lattice);
      
      if (weights_one)
	cicada::parse_agenda(goal, grammar_parse, weight_function_one<weight_type>(), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier);
      else if (! weights_extra.empty())
	cicada::parse_agenda(goal, grammar_parse, weight_function_extra<weight_type>(*weights_parse, weights_extra.begin(), weights_extra.end()), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier);
      else
	cicada::parse_agenda(goal, grammar_parse, weight_function<weight_type>(*weights_parse), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier);
      
      utils::resource end;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " cpu time: " << (end.cpu_time() - start.cpu_time())
		  << " user time: " << (end.user_time() - start.user_time())
		  << " thread time: " << (end.thread_time() - start.thread_time())
		  << std::endl;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " # of nodes: " << parsed.nodes.size()
		  << " # of edges: " << parsed.edges.size()
		  << " valid? " << utils::lexical_cast<std::string>(parsed.is_valid())
		  << std::endl;

      statistics_type::statistic_type& stat = data.statistics[name];
      
      ++ stat.count;
      stat.node += parsed.nodes.size();
      stat.edge += parsed.edges.size();
      stat.user_time += (end.user_time() - start.user_time());
      stat.cpu_time  += (end.cpu_time() - start.cpu_time());
      stat.thread_time  += (end.thread_time() - start.thread_time());
    
      hypergraph.swap(parsed);
    }

    template <typename GR, typename Iterator>
    inline
    bool has_grammar(Iterator first, Iterator last)
    {
      for (/**/; first != last; ++ first) 
	if (dynamic_cast<GR*>(&(*(*first))))
	  return true;
      return false;
    }
    
    
    ParseCoarse::ParseCoarse(const std::string& parameter,
			     const grammar_type& __grammar,
			     const std::string& __goal,
			     const int __debug)
      : base_type("parse-coarse"),
	grammars(), thresholds(), grammar(__grammar), 
	goal(__goal),
	weights(0),
	weights_assigned(0),
	size(200),
	weights_one(false),
	weights_fixed(false),
	weights_extra(),
	yield_source(false),
	treebank(false),
	pos_mode(false),
	ordered(false),
	frontier(false),
	debug(__debug)
    { 
      typedef cicada::Parameter param_type;
      
      param_type param(parameter);
      if (utils::ipiece(param.name()) != "parse-coarse" && utils::ipiece(param.name()) != "parse-coarse")
	throw std::runtime_error("this is not an coarse parser");
      
      bool source = false;
      bool target = false;
      
      for (param_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "size")
	  size = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "weights")
	  weights = &base_type::weights(piter->second);
	else if (utils::ipiece(piter->first) == "weights-one")
	  weights_one = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "yield") {
	  if (utils::ipiece(piter->second) == "source")
	    source = true;
	  else if (utils::ipiece(piter->second) == "target")
	    target = true;
	  else
	    throw std::runtime_error("unknown yield: " + piter->second);
	} else if (utils::ipiece(piter->first) == "treebank")
	  treebank = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "pos")
	  pos_mode = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "ordered")
	  ordered = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "frontier")
	  frontier = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "goal")
	  goal = piter->second;
	else if (utils::ipiece(piter->first) == "grammar")
	  grammar_local.push_back(piter->second);
	else if (utils::ipiece(piter->first) == "weight") {
	  namespace qi = boost::spirit::qi;
	  namespace standard = boost::spirit::standard;
	  
	  std::string::const_iterator iter = piter->second.begin();
	  std::string::const_iterator iter_end = piter->second.end();
	  
	  std::string name;
	  double      value;
	  
	  if (! qi::phrase_parse(iter, iter_end,
				 qi::lexeme[+(!(qi::lit('=') >> qi::double_ >> (standard::space | qi::eoi))
					      >> (standard::char_ - standard::space))]
				 >> '='
				 >> qi::double_,
				 standard::blank, name, value) || iter != iter_end)
	    throw std::runtime_error("weight parameter parsing failed");
	  
	  weights_extra[name] = value;
	} else {
	  namespace qi = boost::spirit::qi;
	  
	  std::string::const_iterator iter = piter->first.begin();
	  std::string::const_iterator iter_end = piter->first.end();
	  
	  int id = -1;
	  if (qi::parse(iter, iter_end, "coarse" >> qi::int_, id) && iter == iter_end) {
	    if (id >= static_cast<int>(grammars.size()))
	      grammars.resize(id + 1);
	    
	    grammars[id].push_back(piter->second);
	  } else if (qi::parse(iter, iter_end, "threshold" >> qi::int_, id) && iter == iter_end) {
	    if (id >= static_cast<int>(thresholds.size()))
	      thresholds.resize(id + 1);
	    thresholds[id] = utils::lexical_cast<double>(piter->second);
	  } else
	    std::cerr << "WARNING: unsupported parameter for coarse parser: " << piter->first << "=" << piter->second << std::endl;
	}
      }
	
      if (source && target)
	throw std::runtime_error("coarse parser can work either source or target yield");
      
      yield_source = source;

      if (weights && weights_one)
	throw std::runtime_error("you have weights, but specified all-one parameter");
      
      if (weights || weights_one)
	weights_fixed = true;

      if (weights_one && ! weights_extra.empty())
	throw std::runtime_error("you have extra weights, but specified all-one parameter");

      if (! weights)
	weights = &base_type::weights();
      
      if (grammars.size() != thresholds.size())
	throw std::runtime_error("# of coarse grammars and # of thresholds do not match");
      if (grammars.empty())
	throw std::runtime_error("no coarse grammar(s)?");
      
      // use either local or globally assigned grammar
      if (! grammar_local.empty())
	grammars.push_back(grammar_local);
      else
	grammars.push_back(grammar);
      
      // assign unknown/pos grammar from the fine-grammar
      if (grammars.back().size() >= 2) {
        grammar_type::transducer_ptr_type unknown;
        grammar_type::transducer_ptr_type pos;
	
        grammar_type::iterator giter_end = grammars.back().end();
        for (grammar_type::iterator giter = grammars.back().begin(); giter != giter_end; ++ giter) {
          if (dynamic_cast<GrammarUnknown*>(&(*(*giter))))
            unknown = *giter;
          else if (dynamic_cast<GrammarPOS*>(&(*(*giter))))
            pos = *giter;
        }
        
        if (unknown) {
          grammar_set_type::iterator giter_end = grammars.end() - 1;
          for (grammar_set_type::iterator giter = grammars.begin(); giter != giter_end; ++ giter)
            if (! has_grammar<GrammarUnknown>(giter->begin(), giter->end()))
              giter->push_back(unknown);
        }
        
        if (pos) {
          grammar_set_type::iterator giter_end = grammars.end() - 1;
          for (grammar_set_type::iterator giter = grammars.begin(); giter != giter_end; ++ giter)
            if (! has_grammar<GrammarPOS>(giter->begin(), giter->end()))
              giter->push_back(pos);
        }
      }

      /// assign OOV grammar from GrammarUnknown!
      grammar_set_type::iterator giter_end = grammars.end();
      for (grammar_set_type::iterator giter = grammars.begin(); giter != giter_end; ++ giter) {
	
	grammar_type::const_iterator uiter_end = giter->end();
	for (grammar_type::const_iterator uiter = giter->begin(); uiter != uiter_end; ++ uiter) {
	  if (dynamic_cast<GrammarUnknown*>(&(*(*uiter)))) {
	    giter->push_back(dynamic_cast<GrammarUnknown*>(&(*(*uiter)))->grammar_oov());
	    break;
	  }
	}
      }
    }

    void ParseCoarse::assign(const weight_set_type& __weights)
    {
      if (! weights_fixed)
	weights_assigned = &__weights;
    }

    void ParseCoarse::operator()(data_type& data) const
    {
      typedef cicada::semiring::Logprob<double> weight_type;
      
      const lattice_type& lattice = data.lattice;
      hypergraph_type& hypergraph = data.hypergraph;
      hypergraph_type parsed;
      
      hypergraph.clear();
      if (lattice.empty()) return;
    
      if (debug)
	std::cerr << name << ": " << data.id << std::endl;
      
      const weight_set_type* weights_parse = (weights_assigned ? weights_assigned : &(weights->weights));

      utils::resource start;

      grammar_set_type::const_iterator giter_end = grammars.end();
      for (grammar_set_type::const_iterator giter = grammars.begin(); giter != giter_end; ++ giter)
	giter->assign(lattice);
      
      if (weights_one)
	cicada::parse_coarse(goal,
			     grammars.begin(), grammars.end(),
			     thresholds.begin(), thresholds.end(),
			     weight_function_one<weight_type>(), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier);
      else if (! weights_extra.empty())
	cicada::parse_coarse(goal,
			     grammars.begin(), grammars.end(),
			     thresholds.begin(), thresholds.end(),
			     weight_function_extra<weight_type>(*weights_parse, weights_extra.begin(), weights_extra.end()), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier);
      else
	cicada::parse_coarse(goal,
			     grammars.begin(), grammars.end(),
			     thresholds.begin(), thresholds.end(),
			     weight_function<weight_type>(*weights_parse), lattice, parsed, size, yield_source, treebank, pos_mode, ordered, frontier);
      
      utils::resource end;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " cpu time: " << (end.cpu_time() - start.cpu_time())
		  << " user time: " << (end.user_time() - start.user_time())
		  << " thread time: " << (end.thread_time() - start.thread_time())
		  << std::endl;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " # of nodes: " << parsed.nodes.size()
		  << " # of edges: " << parsed.edges.size()
		  << " valid? " << utils::lexical_cast<std::string>(parsed.is_valid())
		  << std::endl;

      statistics_type::statistic_type& stat = data.statistics[name];
      
      ++ stat.count;
      stat.node += parsed.nodes.size();
      stat.edge += parsed.edges.size();
      stat.user_time += (end.user_time() - start.user_time());
      stat.cpu_time  += (end.cpu_time() - start.cpu_time());
      stat.thread_time  += (end.thread_time() - start.thread_time());
    
      hypergraph.swap(parsed);
    }

    ParsePhrase::ParsePhrase(const std::string& parameter,
			     const grammar_type& __grammar,
			     const std::string& __goal,
			     const int __debug)
      : base_type("parse-phrase"),
	grammar(__grammar),
	goal(__goal), 
	weights(0),
	weights_assigned(0),
	size(200),
	weights_one(false),
	weights_fixed(false),
	weights_extra(),
	distortion(0),
	yield_source(false),
	frontier(false),
	debug(__debug)
    { 
      typedef cicada::Parameter param_type;
      
      param_type param(parameter);
      if (utils::ipiece(param.name()) != "parse-phrase")
	throw std::runtime_error("this is not a phrase parser");
      
      bool source = false;
      bool target = false;
	
      for (param_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "size")
	  size = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "weights")
	  weights = &base_type::weights(piter->second);
	else if (utils::ipiece(piter->first) == "weights-one")
	  weights_one = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "goal")
	  goal = piter->second;
	else if (utils::ipiece(piter->first) == "grammar")
	  grammar_local.push_back(piter->second);
	else if (utils::ipiece(piter->first) == "yield") {
	  if (utils::ipiece(piter->second) == "source")
	    source = true;
	  else if (utils::ipiece(piter->second) == "target")
	    target = true;
	  else
	    throw std::runtime_error("unknown yield: " + piter->second);
	} else if (utils::ipiece(piter->first) == "distortion")
	  distortion = utils::lexical_cast<int>(piter->second);
	else if (utils::ipiece(piter->first) == "frontier")
	  frontier = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "weight") {
	  namespace qi = boost::spirit::qi;
	  namespace standard = boost::spirit::standard;

	  std::string::const_iterator iter = piter->second.begin();
	  std::string::const_iterator iter_end = piter->second.end();

	  std::string name;
	  double      value;
	  
	  if (! qi::phrase_parse(iter, iter_end,
				 qi::lexeme[+(!(qi::lit('=') >> qi::double_ >> (standard::space | qi::eoi))
					      >> (standard::char_ - standard::space))]
				 >> '='
				 >> qi::double_,
				 standard::blank, name, value) || iter != iter_end)
	    throw std::runtime_error("weight parameter parsing failed");
	  
	  weights_extra[name] = value;
	} else
	  std::cerr << "WARNING: unsupported parameter for phrase parser: " << piter->first << "=" << piter->second << std::endl;
      }
      
      if (source && target)
	throw std::runtime_error("phrase parser can work either source or target yield");
	
      yield_source = source;

      if (weights && weights_one)
	throw std::runtime_error("you have weights, but specified all-one parameter");

      if (weights_one && ! weights_extra.empty())
	throw std::runtime_error("you have extra weights, but specified all-one parameter");
      
      if (weights || weights_one)
	weights_fixed = true;
      
      if (! weights)
	weights = &base_type::weights();
    }

    void ParsePhrase::assign(const weight_set_type& __weights)
    {
      if (! weights_fixed)
	weights_assigned = &__weights;
    }
    
    void ParsePhrase::operator()(data_type& data) const
    {
      typedef cicada::semiring::Logprob<double> weight_type;

      const lattice_type& lattice = data.lattice;
      hypergraph_type& hypergraph = data.hypergraph;
      hypergraph_type parsed;
      
      hypergraph.clear();
      if (lattice.empty()) return;
    
      if (debug)
	std::cerr << name << ": " << data.id << std::endl;

      const weight_set_type* weights_parse = (weights_assigned ? weights_assigned : &(weights->weights));
      
      const grammar_type& grammar_parse = (grammar_local.empty() ? grammar : grammar_local);

      utils::resource start;
      
      grammar_parse.assign(lattice);
      
      
      cicada::compose_phrase(goal, grammar_parse, distortion, lattice, parsed, yield_source, frontier);
      
#if 0
      if (weights_one)
	cicada::parse_phrase(goal, grammar_parse, weight_function_one<weight_type>(), size, distortion, lattice, parsed, yield_source, frontier);
      else if (! weights_extra.empty())
	cicada::parse_phrase(goal, grammar_parse, weight_function_extra<weight_type>(*weights_parse, weights_extra.begin(), weights_extra.end()), size, distortion, lattice, parsed, yield_source, frontier);
      else
	cicada::parse_phrase(goal, grammar_parse, weight_function<weight_type>(*weights_parse), size, distortion, lattice, parsed, yield_source, frontier);
#endif
      
      utils::resource end;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " cpu time: " << (end.cpu_time() - start.cpu_time())
		  << " user time: " << (end.user_time() - start.user_time())
		  << " thread time: " << (end.thread_time() - start.thread_time())
		  << std::endl;
    
      if (debug)
	std::cerr << name << ": " << data.id
		  << " # of nodes: " << parsed.nodes.size()
		  << " # of edges: " << parsed.edges.size()
		  << " valid? " << utils::lexical_cast<std::string>(parsed.is_valid())
		  << std::endl;

      statistics_type::statistic_type& stat = data.statistics[name];
      
      ++ stat.count;
      stat.node += parsed.nodes.size();
      stat.edge += parsed.edges.size();
      stat.user_time += (end.user_time() - start.user_time());
      stat.cpu_time  += (end.cpu_time() - start.cpu_time());
      stat.thread_time  += (end.thread_time() - start.thread_time());
    
      hypergraph.swap(parsed);
    }
    
  };
};
