//
// query tree given an input hypergraph
//

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <set>
#include <iterator>

#define BOOST_SPIRIT_THREADSAFE
#define PHOENIX_THREADSAFE

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/fusion/tuple.hpp>
#include <boost/fusion/adapted.hpp>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include "cicada/lattice.hpp"
#include "cicada/sentence.hpp"
#include "cicada/hypergraph.hpp"
#include "cicada/query_tree_cky.hpp"
#include "cicada/grammar.hpp"
#include "cicada/tree_grammar.hpp"
#include "cicada/tree_rule_compact.hpp"
#include "cicada/feature_vector_compact.hpp"
#include "cicada/symbol_vector_compact.hpp"

#include "utils/lockfree_list_queue.hpp"
#include "utils/json_string_generator.hpp"
#include "utils/compress_stream.hpp"
#include "utils/filesystem.hpp"
#include "utils/getline.hpp"

typedef boost::filesystem::path path_type;

typedef std::vector<std::string, std::allocator<std::string> > grammar_file_set_type;

typedef cicada::Lattice  lattice_type;
typedef cicada::Sentence sentence_type;

typedef cicada::Grammar         grammar_type;
typedef cicada::TreeGrammar     tree_grammar_type;

typedef cicada::Transducer      transducer_type;
typedef cicada::TreeTransducer  tree_transducer_type;

typedef transducer_type::feature_set_type   feature_set_type;
typedef transducer_type::attribute_set_type attribute_set_type;

typedef transducer_type::rule_type           rule_type;
typedef transducer_type::rule_ptr_type       rule_ptr_type;
typedef transducer_type::rule_pair_type      rule_pair_type;

typedef rule_type::symbol_type     symbol_type;
typedef rule_type::symbol_set_type symbol_set_type;
typedef cicada::SymbolVectorCompact symbol_compact_type;

typedef tree_transducer_type::rule_type      tree_rule_type;
typedef tree_transducer_type::rule_ptr_type  tree_rule_ptr_type;
typedef tree_transducer_type::rule_pair_type tree_rule_pair_type;

typedef cicada::TreeRuleCompact tree_rule_compact_type;

struct rule_pair_string_type
{
  typedef cicada::FeatureVectorCompact feature_set_type;
  
  symbol_type lhs;
  symbol_compact_type source;
  symbol_compact_type target;
  
  feature_set_type   features;
  attribute_set_type attributes;
  
  rule_pair_string_type() {}

  void clear()
  {
    lhs = symbol_type();
    source.clear();
    target.clear();
    features.clear();
    attributes.clear();
  }
};

struct tree_rule_pair_string_type
{
  typedef cicada::FeatureVectorCompact feature_set_type;
  
  tree_rule_compact_type source;
  tree_rule_compact_type target;
  
  feature_set_type   features;
  attribute_set_type attributes;
  
  tree_rule_pair_string_type() {}

  void clear()
  {
    source.clear();
    target.clear();
    features.clear();
    attributes.clear();
  }

};

struct less_rule_pair
{
  bool operator()(const rule_pair_string_type& x, const rule_pair_string_type& y) const
  {
    return (x.lhs < y.lhs
	    || (!(y.lhs < x.lhs)
		&& (x.source < y.source
		    || (!(y.source < x.source)
			&& (x.target < y.target
			    || (!(y.target < x.target)
				&& (x.features < y.features
				    || (!(y.features < x.features)
					&& x.attributes < y.attributes))))))));
  }
};

struct less_tree_rule_pair
{
  bool operator()(const tree_rule_pair_string_type& x, const tree_rule_pair_string_type& y) const
  {
    return (x.source < y.source
	    || (!(y.source < x.source)
		&& (x.target < y.target
		    || (!(y.target < x.target)
			&& (x.features < y.features
			    || (!(y.features < x.features)
				&& x.attributes < y.attributes))))));
  }
};

typedef std::set<rule_pair_string_type, less_rule_pair, std::allocator<rule_pair_string_type> >                rule_pair_unique_type;
typedef std::set<tree_rule_pair_string_type, less_tree_rule_pair, std::allocator<tree_rule_pair_string_type> > tree_rule_pair_unique_type;

typedef std::vector<rule_pair_type, std::allocator<rule_pair_type> >                rule_pair_set_type;
typedef std::vector<tree_rule_pair_type, std::allocator<tree_rule_pair_type> > tree_rule_pair_set_type;

template <typename Iterator>
struct features_generator : boost::spirit::karma::grammar<Iterator, feature_set_type()>
{
  features_generator() : features_generator::base_type(features)
  {
    namespace karma = boost::spirit::karma;
    namespace standard = boost::spirit::standard;
    
    features %= " ||| " << ((standard::string << '=' << double20) % ' ');
  }
  
  struct real_precision : boost::spirit::karma::real_policies<double>
  {
    static unsigned int precision(double) 
    { 
      return std::numeric_limits<double>::digits10 + 1;
    }
  };
  
  boost::spirit::karma::real_generator<double, real_precision> double20;
  boost::spirit::karma::rule<Iterator, feature_set_type()>     features;
};

template <typename Iterator>
struct attributes_generator : boost::spirit::karma::grammar<Iterator, attribute_set_type()>
{
  attributes_generator() : attributes_generator::base_type(attributes)
  {
    namespace karma = boost::spirit::karma;
    namespace standard = boost::spirit::standard;
    
    data %= int64_ | double10 | string;
    attribute %= standard::string << '=' << data;
    attributes %= " ||| " << (attribute % ' ');
  }
    
  struct real_precision : boost::spirit::karma::real_policies<double>
  {
    static unsigned int precision(double) 
    { 
      return std::numeric_limits<double>::digits10 + 1;
    }
  };
    
  boost::spirit::karma::real_generator<double, real_precision> double10;
  boost::spirit::karma::int_generator<attribute_set_type::int_type, 10, false> int64_;
  utils::json_string_generator<Iterator, true> string;
  
  boost::spirit::karma::rule<Iterator, attribute_set_type::data_type()> data;
  boost::spirit::karma::rule<Iterator, attribute_set_type::value_type()> attribute;
  boost::spirit::karma::rule<Iterator, attribute_set_type()> attributes;
};


path_type input_file = "-";
path_type output_tree_file = "-";
path_type output_rule_file = "-";

grammar_file_set_type grammar_files;
bool grammar_list = false;

grammar_file_set_type tree_grammar_files;
bool tree_grammar_list = false;

bool input_sentence_mode = false;
bool input_lattice_mode = false;

int threads = 1;

int debug = 0;


struct Task
{
  typedef utils::lockfree_list_queue<std::string, std::allocator<std::string> > queue_type;
  
  Task(queue_type& __queue,
       const tree_grammar_type& __tree_grammar,
       const grammar_type& __grammar) : queue(__queue), tree_grammar(__tree_grammar), grammar(__grammar) {}
  
  void operator()()
  {
    const tree_grammar_type tree_grammar_local(tree_grammar.clone());
    const grammar_type      grammar_local(grammar.clone());

    cicada::QueryTreeCKY query(tree_grammar_local, grammar_local);

    tree_rule_pair_set_type tree_rules;
    rule_pair_set_type      rules;
    
    tree_rule_pair_string_type tree_rule_string;
    rule_pair_string_type rule_string;
        
    lattice_type lattice;
    sentence_type sentence;
    
    std::string line;
    for (;;) {
      queue.pop_swap(line);
      if (line.empty()) break;
      
      if (input_lattice_mode)
	lattice.assign(line);
      else {
	sentence.assign(line);
	lattice = lattice_type(sentence);
      }
      
      tree_rules.clear();
      rules.clear();

      grammar_local.assign(lattice);
      tree_grammar_local.assign(lattice);
      
      query(lattice, std::back_inserter(tree_rules), std::back_inserter(rules));

      if (debug)
	std::cerr << "# of tree rules: " << tree_rules.size()
		  << " # of rules: " << rules.size()
		  << std::endl;
      
      tree_rule_pair_set_type::iterator titer_end = tree_rules.end();
      for (tree_rule_pair_set_type::iterator titer = tree_rules.begin(); titer != titer_end; ++ titer) 
	if (titer->source || titer->target) {
	  tree_rule_string.clear();
	  
	  if (titer->source)
	    tree_rule_string.source = *(titer->source);
	  if (titer->target)
	    tree_rule_string.target = *(titer->target);
	  
	  tree_rule_string.features = titer->features;
	  tree_rule_string.attributes.swap(titer->attributes);
	  
	  tree_rules_unique.insert(tree_rule_string);
	}
      
      rule_pair_set_type::iterator riter_end = rules.end();
      for (rule_pair_set_type::iterator riter = rules.begin(); riter != riter_end; ++ riter)
	if (riter->source || riter->target) {
	  rule_string.clear();
	
	  rule_string.lhs = (riter->source ? riter->source->lhs : riter->target->lhs);
	
	  if (riter->source)
	    rule_string.source = riter->source->rhs;
	  if (riter->target)
	    rule_string.target = riter->target->rhs;
	  
	  rule_string.features = riter->features;
	  rule_string.attributes.swap(riter->attributes);
	
	  rules_unique.insert(rule_string);
	}
    }
    
  }

  queue_type&              queue;
  const tree_grammar_type& tree_grammar;
  const grammar_type&      grammar;
  
  tree_rule_pair_unique_type tree_rules_unique;
  rule_pair_unique_type      rules_unique;
};

void options(int argc, char** argv);

int main(int argc, char** argv)
{
  try {
    options(argc, argv);
    
    if (grammar_list) {
      std::cout << grammar_type::lists();
      return 0;
    }
    
    if (tree_grammar_list) {
      std::cout << tree_grammar_type::lists();
      return 0;
    }
    
    if (int(input_lattice_mode) + input_sentence_mode == 0)
      input_sentence_mode = true;
    if (int(input_lattice_mode) + input_sentence_mode > 1)
      throw std::runtime_error("either lattice or sentence input");

    threads = utils::bithack::max(1, threads);

    // read grammars...
    grammar_type grammar(grammar_files.begin(), grammar_files.end());
    if (debug)
      std::cerr << "grammar: " << grammar.size() << std::endl;
    
    tree_grammar_type tree_grammar(tree_grammar_files.begin(), tree_grammar_files.end());
    if (debug)
      std::cerr << "tree grammar: " << tree_grammar.size() << std::endl;

    typedef Task task_type;
    typedef std::vector<task_type, std::allocator<task_type> > task_set_type;
    
    task_type::queue_type queue(threads);
    task_set_type tasks(threads, task_type(queue, tree_grammar, grammar));
    
    boost::thread_group workers;
    for (int i = 0; i != threads; ++ i)
      workers.add_thread(new boost::thread(boost::ref(tasks[i])));
    
    utils::compress_istream is(input_file, 1024 * 1024);
    
    std::string line;
    while (utils::getline(is, line))
      if (! line.empty())
	queue.push_swap(line);
    
    for (int i = 0; i != threads; ++ i)
      queue.push(std::string());
    
    workers.join_all();

    tree_rule_pair_unique_type tree_rules_unique;
    rule_pair_unique_type      rules_unique;
    
    for (int i = 0; i != threads; ++ i) {
      if (tree_rules_unique.empty())
	tree_rules_unique.swap(tasks[i].tree_rules_unique);
      else
	tree_rules_unique.insert(tasks[i].tree_rules_unique.begin(), tasks[i].tree_rules_unique.end());
      
      tasks[i].tree_rules_unique.clear();
      
      if (rules_unique.empty())
	rules_unique.swap(tasks[i].rules_unique);
      else
	rules_unique.insert(tasks[i].rules_unique.begin(), tasks[i].rules_unique.end());
      
      tasks[i].rules_unique.clear();
    }
    tasks.clear();
    

    typedef std::ostream_iterator<char> oiter_type;

    features_generator<oiter_type>   generate_features;
    attributes_generator<oiter_type> generate_attributes;
    
    if (! output_tree_file.empty()) {
      namespace karma = boost::spirit::karma;
      namespace standard = boost::spirit::standard;
      
      utils::compress_ostream os(output_tree_file, 1024 * 1024);
      
      tree_rule_pair_unique_type::const_iterator iter_end = tree_rules_unique.end();
      for (tree_rule_pair_unique_type::const_iterator iter = tree_rules_unique.begin(); iter != iter_end; ++ iter) {
	os << iter->source.decode() << " ||| " << iter->target.decode();
	
	if (! iter->features.empty()) {
	  feature_set_type features(iter->features.begin(), iter->features.end());
	  karma::generate(oiter_type(os), generate_features, features);
	}
	
	if (! iter->attributes.empty())
	  karma::generate(oiter_type(os), generate_attributes, iter->attributes);
	os << '\n';
      }
    }
    
    if (! output_rule_file.empty()) {
      namespace karma = boost::spirit::karma;
      namespace standard = boost::spirit::standard;
      
      utils::compress_ostream os(output_rule_file, 1024 * 1024);
      
      rule_pair_unique_type::const_iterator iter_end = rules_unique.end();
      for (rule_pair_unique_type::const_iterator iter = rules_unique.begin(); iter != iter_end; ++ iter) {
	karma::generate(oiter_type(os),
			standard::string << " ||| " << -(standard::string % ' ') << " ||| " << -(standard::string % ' '),
			iter->lhs,
			symbol_set_type(iter->source.begin(), iter->source.end()),
			symbol_set_type(iter->target.begin(), iter->target.end()));
	
	if (! iter->features.empty()) {
	  feature_set_type features(iter->features.begin(), iter->features.end());
	  karma::generate(oiter_type(os), generate_features, features);
	}
	
	if (! iter->attributes.empty())
	  karma::generate(oiter_type(os), generate_attributes, iter->attributes);
	os << '\n';
      }
    }
  }
  catch (const std::exception& err) {
    std::cerr << "error: " << err.what() << std::endl;
    return 1;
  }
  return 0;
}

void options(int argc, char** argv)
{
  namespace po = boost::program_options;
  
  po::options_description opts_config("configuration options");
  opts_config.add_options()
    ("input",       po::value<path_type>(&input_file)->default_value(input_file),   "input file")
    ("output-tree", po::value<path_type>(&output_tree_file)->default_value(output_tree_file), "output tree-rule file")
    ("output-rule", po::value<path_type>(&output_rule_file)->default_value(output_rule_file), "output rule file")
    
    ("input-sentence",   po::bool_switch(&input_sentence_mode),   "sentence input")
    ("input-lattice",    po::bool_switch(&input_lattice_mode),    "lattice input")
    
    // grammar
    ("grammar",           po::value<grammar_file_set_type >(&grammar_files)->composing(),      "grammar specification(s)")
    ("grammar-list",      po::bool_switch(&grammar_list),                                      "list of available grammar specifications")
    ("tree-grammar",      po::value<grammar_file_set_type >(&tree_grammar_files)->composing(), "tree grammar specification(s)")
    ("tree-grammar-list", po::bool_switch(&tree_grammar_list),                                 "list of available grammar specifications")
    ;
  
  po::options_description opts_command("command line options");
  opts_command.add_options()
    ("config",  po::value<path_type>(),                    "configuration file")
    ("threads", po::value<int>(&threads),                  "# of threads (highly experimental)")
    ("debug",   po::value<int>(&debug)->implicit_value(1), "debug level")
    ("help", "help message");
  
  po::options_description desc_config;
  po::options_description desc_command;
  po::options_description desc_visible;
  
  desc_config.add(opts_config);
  desc_command.add(opts_config).add(opts_command);
  desc_visible.add(opts_config).add(opts_command);
  
  po::variables_map variables;

  po::store(po::parse_command_line(argc, argv, desc_command, po::command_line_style::unix_style & (~po::command_line_style::allow_guessing)), variables);
  if (variables.count("config")) {
    const path_type path_config = variables["config"].as<path_type>();
    if (! boost::filesystem::exists(path_config))
      throw std::runtime_error("no config file: " + path_config.string());
    
    utils::compress_istream is(path_config);
    po::store(po::parse_config_file(is, desc_config), variables);
  }
  
  po::notify(variables);

  if (variables.count("help")) {
    
    std::cout << argv[0] << " [options]\n"
	      << desc_visible << std::endl;
    exit(0);
  }
}
