#ifndef __CICADA__EXTRACT_GHKM_IMPL__HPP__
#define __CICADA__EXTRACT_GHKM_IMPL__HPP__ 1


#define BOOST_SPIRIT_THREADSAFE
#define PHOENIX_THREADSAFE

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>

#include <boost/fusion/tuple.hpp>
#include <boost/fusion/adapted.hpp>
#include <boost/filesystem.hpp>

#include <string>
#include <vector>
#include <set>
#include <deque>

#include <boost/array.hpp>

#include "cicada/sentence.hpp"
#include "cicada/alignment.hpp"
#include "cicada/vocab.hpp"
#include "cicada/tree_rule.hpp"
#include "cicada/hypergraph.hpp"
#include "cicada/operation/functional.hpp"
#include "cicada/semiring.hpp"
#include "cicada/inside_outside.hpp"

#include "utils/sgi_hash_map.hpp"
#include "utils/sgi_hash_set.hpp"
#include "utils/chart.hpp"

#include <utils/lockfree_list_queue.hpp>
#include <utils/bithack.hpp>
#include <utils/compress_stream.hpp>
#include <utils/tempfile.hpp>
#include <utils/malloc_stats.hpp>
#include <utils/chunk_vector.hpp>
#include <utils/b_heap.hpp>

#include <google/dense_hash_map>
#include <google/dense_hash_set>

struct Bitext
{
  typedef cicada::HyperGraph hypergraph_type;
  typedef cicada::Sentence   sentence_type;
  typedef cicada::Alignment  alignment_type;
  
  hypergraph_type source;
  sentence_type   target;
  alignment_type  alignment;
  
  Bitext() : source(), target(), alignment() {}
  Bitext(const hypergraph_type& __source,
	 const sentence_type&   __target,
	 const alignment_type&  __alignment)
    : source(__source), target(__target), alignment(__alignment) {}
  
  void swap(Bitext& x)
  {
    source.swap(x.source);
    target.swap(x.target);
    alignment.swap(x.alignment);
  }

  void clear()
  {
    source.clear();
    target.clear();
    alignment.clear();
  }
  
  friend
  std::istream& operator>>(std::istream& is, Bitext& bitext)
  {
    namespace qi = boost::spirit::qi;
    namespace standard = boost::spirit::standard;
    
    using qi::phrase_parse;
    using qi::lit;
    using standard::space;

    bitext.clear();
    std::string line;
    if (std::getline(is, line)) {
      std::string::const_iterator iter = line.begin();
      std::string::const_iterator end  = line.end();
      
      if((!bitext.source.assign(iter, end))
	 || (!phrase_parse(iter, end, lit("|||"), space))
	 || (!bitext.target.assign(iter, end))
	 || (!phrase_parse(iter, end, lit("|||"), space))
	 || (!bitext.alignment.assign(iter, end))
	 || iter != end)
	bitext.clear();
    }
    return is;
  }
  
  friend
  std::ostream& operator<<(std::ostream& os, const Bitext& bitext)
  {
    os << bitext.source
       << " ||| " << bitext.target
       << " ||| " << bitext.alignment;
    return os;
  }
  
};

namespace std
{
  inline
  void swap(Bitext& x, Bitext& y)
  {
    x.swap(y);
  }
};


struct RulePair
{
  typedef std::string phrase_type;
  typedef cicada::Alignment alignment_type;
  typedef double count_type;
  
  phrase_type    source;
  phrase_type    target;
  alignment_type alignment;
  count_type     count;

  RulePair() : source(), target(), alignment(), count(0) {}

  friend
  size_t hash_value(RulePair const& x)
  {
    typedef utils::hashmurmur<size_t> hasher_type;
    
    return hasher_type()(x.source.begin(), x.source.end(),
			 hasher_type()(x.target.begin(), x.target.end(),
				       hasher_type()(x.alignment.begin(), x.alignment.end(), 0)));
  }

  friend
  bool operator==(const RulePair& x, const RulePair& y) 
  {
    return x.source == y.source && x.target == y.target && x.alignment == y.alignment;
  }
  
  friend
  bool operator!=(const RulePair& x, const RulePair& y) 
  {
    return x.source != y.source || x.target != y.target || x.alignment != y.alignment;
  }
  
  friend
  bool operator<(const RulePair& x, const RulePair& y)
  {
    return (x.source < y.source
	    || (!(y.source < x.source)
		&& (x.target < y.target
		    || (!(y.target < x.target)
			&& x.alignment < y.alignment))));
  }
  
  friend
  bool operator>(const RulePair& x, const RulePair& y)
  {
    return y < x;
  }
};

BOOST_FUSION_ADAPT_STRUCT(RulePair::alignment_type::point_type,
			  (RulePair::alignment_type::index_type, source)
			  (RulePair::alignment_type::index_type, target)
			  )
BOOST_FUSION_ADAPT_STRUCT(
			  RulePair,
			  (RulePair::phrase_type, source)
			  (RulePair::phrase_type, target)
			  (RulePair::alignment_type, alignment)
			  (RulePair::count_type, count)
			  )

struct RulePairGenerator
{
  typedef RulePair phrase_pair_type;
  
  typedef phrase_pair_type::phrase_type    phrase_type;
  typedef phrase_pair_type::alignment_type alignment_type;
  typedef phrase_pair_type::count_type    count_type;
  
  RulePairGenerator() : grammar() {}
  RulePairGenerator(const RulePairGenerator& x) : grammar() {}

  
  template <typename Iterator>
  struct phrase_pair_generator : boost::spirit::karma::grammar<Iterator, phrase_pair_type()>
  {
    phrase_pair_generator() : phrase_pair_generator::base_type(phrase_pair)
    {
      namespace karma = boost::spirit::karma;
      namespace standard = boost::spirit::standard;
      namespace phoenix = boost::phoenix;
      
      using karma::repeat;
      using standard::char_;
      using karma::double_;
      using karma::int_;
      using standard::space;
      
      phrase %= +char_;
      alignment %= -((int_ << '-' << int_) % ' ');
      phrase_pair %= phrase << " ||| " << phrase << " ||| " << alignment << " ||| " << double20;
    }

    struct real_precision : boost::spirit::karma::real_policies<double>
    {
      static unsigned int precision(double) 
      { 
        return 20;
      }
    };
    
    boost::spirit::karma::real_generator<double, real_precision> double20;
    
    boost::spirit::karma::rule<Iterator, std::string()> phrase;
    boost::spirit::karma::rule<Iterator, alignment_type()> alignment;
    boost::spirit::karma::rule<Iterator, phrase_pair_type()> phrase_pair;
  };

  typedef std::ostream_iterator<char> iterator_type;
  
  std::ostream& operator()(std::ostream& os, const phrase_pair_type& phrase_pair)
  {
    iterator_type iter(os);
    
    if (! boost::spirit::karma::generate(iter, grammar, phrase_pair))
      throw std::runtime_error("failed generation!");
    
    return os;
  }
  
  phrase_pair_generator<iterator_type> grammar;
};

//
// forest-based translation rule extraction based on
//
// @InProceedings{mi-huang:2008:EMNLP,
//   author    = {Mi, Haitao  and  Huang, Liang},
//   title     = {Forest-based Translation Rule Extraction},
//   booktitle = {Proceedings of the 2008 Conference on Empirical Methods in Natural Language Processing},
//   month     = {October},
//   year      = {2008},
//   address   = {Honolulu, Hawaii},
//   publisher = {Association for Computational Linguistics},
//   pages     = {206--214},
//   url       = {http://www.aclweb.org/anthology/D08-1022}
// }
//
// and
//
// @InProceedings{galley-EtAl:2006:COLACL,
//    author    = {Galley, Michel  and  Graehl, Jonathan  and  Knight, Kevin  and  Marcu, Daniel  and  DeNeefe, Steve  and  Wang, Wei  and  Thayer, Ignacio},
//    title     = {Scalable Inference and Training of Context-Rich Syntactic Translation Models},
//    booktitle = {Proceedings of the 21st International Conference on Computational Linguistics and 44th Annual Meeting of the Association for Computational Linguistics},
//    month     = {July},
//    year      = {2006},
//    address   = {Sydney, Australia},
//    publisher = {Association for Computational Linguistics},
//    pages     = {961--968},
//    url       = {http://www.aclweb.org/anthology/P06-1121},
//    doi       = {10.3115/1220175.1220296}
// }
//


struct ExtractGHKM
{
  typedef size_t    size_type;
  typedef ptrdiff_t difference_type;
  
  typedef RulePair rule_pair_type;
  typedef rule_pair_type::phrase_type phrase_type;
  typedef rule_pair_type::count_type  count_type;
  
#ifdef HAVE_TR1_UNORDERED_SET
  typedef std::tr1::unordered_set<rule_pair_type, boost::hash<rule_pair_type>, std::equal_to<rule_pair_type>,
				  std::allocator<rule_pair_type> > rule_pair_set_type;
#else
 typedef sgi::hash_set<rule_pair_type, boost::hash<rule_pair_type>, std::equal_to<rule_pair_type>,
		      std::allocator<rule_pair_type> > rule_pair_set_type;
#endif
  
  typedef cicada::HyperGraph hypergraph_type;
  typedef cicada::Symbol     word_type;
  typedef cicada::Symbol     symbol_type;
  typedef cicada::Vocab      vocab_type;
  typedef cicada::Sentence   sentence_type;
  typedef cicada::Alignment  alignment_type;
  typedef cicada::TreeRule   tree_rule_type;
  typedef cicada::Rule       rule_type;
  
  typedef alignment_type::point_type point_type;
  
  typedef std::pair<int, int> range_type;
  typedef std::vector<range_type, std::allocator<range_type> > range_set_type;

  struct Span
  {
    typedef std::set<int, std::less<int>, std::allocator<int> > span_type;
    
    typedef span_type::const_iterator const_iterator;
    typedef span_type::iterator       iterator;
    
    Span() : span() {}
    Span(const int first, const int last) : span()
    {
      for (int i = first; i < last; ++ i)
	span.insert(i);
    }
    
    Span& operator|=(const Span& x)
    {
      span.insert(x.span.begin(), x.span.end());
      return *this;
    }

    const_iterator begin() const { return span.begin(); }
    const_iterator end() const { return span.end(); }

    const_iterator lower_bound(int pos) const { return span.lower_bound(pos); }
    
    range_type range() const
    {
      if (span.empty())
	return std::make_pair(0, 0);
      else
	return std::make_pair(*(span.begin()), *(-- span.end()) + 1);
    }
  
    void set(int i)
    {
      span.insert(i);
    }
    
    void clear() { span.clear(); }

    bool intersect(const range_type& range) const
    {
      if (range.first == range.second)
	return false;
      
      if (span.empty())
	return false;
      else
	return span.lower_bound(range.first) != span.lower_bound(range.second);
    }

    bool empty() const { return span.empty(); }
  
    span_type span;
  };

  typedef Span span_type;
  typedef std::vector<span_type, std::allocator<span_type> > span_set_type;

  typedef std::vector<bool, std::allocator<bool> > admissible_set_type;


  typedef hypergraph_type::id_type id_type;
  
  typedef std::vector<id_type, std::allocator<id_type> > edge_set_type;
  typedef std::vector<id_type, std::allocator<id_type> > node_set_type;

  typedef std::vector<node_set_type, std::allocator<node_set_type> > node_map_type;

  typedef std::pair<edge_set_type, node_set_type> frontier_type;

  typedef cicada::semiring::Logprob<double> weight_type;
  typedef std::vector<weight_type, std::allocator<weight_type> > weight_set_type;
  
  struct DerivationEdge
  {
    edge_set_type edges;
    node_set_type tails;

    int height;
    int internal;

    DerivationEdge()
      : edges(), tails(), height(0), internal(0) {}
    DerivationEdge(const edge_set_type& __edges, const node_set_type& __tails)
      : edges(__edges), tails(__tails), height(0), internal(0) {}
    DerivationEdge(const edge_set_type& __edges, const node_set_type& __tails,
		   const int __height,
		   const int __internal)
      : edges(__edges), tails(__tails), height(__height), internal(__internal) {}
    
    void swap(DerivationEdge& x)
    {
      edges.swap(x.edges);
      tails.swap(x.tails);
      std::swap(height, x.height);
      std::swap(internal, x.internal);
    }
  };
  
  struct DerivationNode
  {
    typedef DerivationEdge derivation_edge_type;
    typedef std::deque<derivation_edge_type, std::allocator<derivation_edge_type> > edge_set_type;

    id_type       node;
    range_type    range;
    edge_set_type edges;
  };
  
  typedef DerivationEdge derivation_edge_type;
  typedef DerivationNode derivation_node_type;

  struct less_derivation_edge_type
  {
    bool operator()(const derivation_edge_type& x, const derivation_edge_type& y) const
    {
      return x.internal < y.internal || (!(y.internal < x.internal) && x.height < y.height);
    }
  };
  
  typedef utils::chunk_vector<derivation_node_type, 4096 / sizeof(derivation_node_type), std::allocator<derivation_node_type> > derivation_set_type;

  typedef std::vector<int, std::allocator<int> > index_set_type;
  typedef std::vector<bool, std::allocator<bool> > covered_type;
  typedef std::vector<int, std::allocator<int> > point_set_type;
  typedef std::vector<point_set_type, std::allocator<point_set_type> > alignment_map_type;
  typedef std::vector<rule_pair_type, std::allocator<rule_pair_type> >  rule_pair_list_type;
  typedef std::vector<tree_rule_type, std::allocator<tree_rule_type> > tree_rule_set_type;
  
  typedef std::pair<range_type, int> range_pos_type;
  typedef std::vector<range_pos_type, std::allocator<range_pos_type> > range_pos_set_type;

  struct node_set_hash : public utils::hashmurmur<size_t>
  {
    size_t operator()(const edge_set_type& edges) const
    {
      return utils::hashmurmur<size_t>::operator()(edges.begin(), edges.end(), 0);
    }
  };

#ifdef HAVE_TR1_UNORDERED_MAP
  typedef std::tr1::unordered_map<edge_set_type, rule_pair_list_type, node_set_hash, std::equal_to<edge_set_type>,
				  std::allocator<std::pair<const edge_set_type, rule_pair_list_type> > > rule_pair_set_local_type;
#else
  typedef sgi::hash_map<edge_set_type, rule_pair_list_type, node_set_hash, std::equal_to<edge_set_type>,
			std::allocator<std::pair<const edge_set_type, rule_pair_list_type> > > rule_pair_set_local_type;
#endif
  
  
  ExtractGHKM(const symbol_type& __non_terminal,
	      const int __max_nodes,
	      const int __max_height,
	      const bool __inverse,
	      const bool __swap_source_target)
    : non_terminal(__non_terminal),
      max_nodes(__max_nodes),
      max_height(__max_height),
      inverse(__inverse),
      swap_source_target(__swap_source_target) {}

  symbol_type non_terminal;
  int max_nodes;
  int max_height;
  bool inverse;
  bool swap_source_target;
  
  range_set_type ranges;
  span_set_type spans;
  span_set_type complements;

  admissible_set_type admissibles;
  
  node_map_type node_map;
  derivation_set_type derivations;

  alignment_map_type alignment_source_target;
  alignment_map_type alignment_target_source;

  weight_set_type weights_inside;
  weight_set_type weights_outside;
  
  void operator()(const hypergraph_type& graph,
		  const sentence_type& sentence,
		  const alignment_type& alignment,
		  rule_pair_set_type& rules)
  {
#if 0
    std::cerr << "hypergraph: " << graph << std::endl
	      << "sentence: " << sentence << std::endl
	      << "alignment: " << alignment << std::endl;
#endif

    ranges.clear();
    spans.clear();
    complements.clear();
    admissibles.clear();
    node_map.clear();

    derivations.clear();
    
    ranges.resize(graph.nodes.size());
    spans.resize(graph.nodes.size());
    complements.resize(graph.nodes.size());
    admissibles.resize(graph.nodes.size());
    node_map.resize(graph.nodes.size());
    

    //std::cerr << "admissible nodes" << std::endl;
    admissible_nodes(graph, sentence, alignment);
    
    //std::cerr << "construct derivations" << std::endl;
    construct_derivations(graph, sentence);
    
    // compute reachable nodes and edges from root...

    //std::cerr << "prune derivations" << std::endl;
    prune_derivations();
        
    // perform compounds extraction
    extract_composed(graph, sentence, alignment, rules);
  }
  
  struct Candidate
  {
    const derivation_edge_type* edge;
    derivation_edge_type edge_composed;
    
    index_set_type j;
    bool composed;
    
    Candidate(const index_set_type& __j)
      : edge(0), edge_composed(), j(__j), composed(true) {}
    Candidate(const derivation_edge_type& __edge, const index_set_type& __j, const bool __composed)
      : edge(&__edge), edge_composed(__edge), j(__j), composed(__composed) {}
  };
  typedef Candidate candidate_type;
  typedef utils::chunk_vector<candidate_type, 4096 / sizeof(candidate_type), std::allocator<candidate_type> > candidate_set_type;

  struct candidate_hash_type : public utils::hashmurmur<size_t>
  {
    size_t operator()(const candidate_type* x) const
    {
      return (x == 0 ? size_t(0) : utils::hashmurmur<size_t>::operator()(x->j.begin(), x->j.end(), intptr_t(x->edge)));
    }
  };
  
  struct candidate_equal_type
  {
    bool operator()(const candidate_type* x, const candidate_type* y) const
    {
      return (x == y) || (x && y && x->edge == y->edge && x->j == y->j);
    }
  };
  
  struct compare_heap_type
  {
    // we use greater, so that when popped from heap, we will grab "less" in back...
    bool operator()(const candidate_type* x, const candidate_type* y) const
    {
      return int(x->composed) > int(y->composed) || (x->composed == y->composed && x->edge_composed.internal > y->edge_composed.internal);
    }
  };
  
  void extract_composed(const hypergraph_type& graph,
			const sentence_type& sentence,
			const alignment_type& alignment,
			rule_pair_set_type& rule_pairs)
  {
    typedef std::vector<const candidate_type*, std::allocator<const candidate_type*> > candidate_heap_base_type;
    typedef utils::b_heap<const candidate_type*,  candidate_heap_base_type, compare_heap_type, 512 / sizeof(const candidate_type*)> candidate_heap_type;
    typedef google::dense_hash_set<const candidate_type*, candidate_hash_type, candidate_equal_type > candidate_unique_type;
    
    candidate_set_type    candidates;
    candidate_heap_type   cand;
    candidate_unique_type cand_unique;
    cand_unique.set_empty_key(0);
    
    // difficult...
    // we need to keep track of how many nodes / maximum height in the composed rules...
    
    //
    // bottom-up traversal to construct composed rules...
    // actually, we will construct new "composed edges" into new edge set, then, 
    // replace this new edge set with current one, so that we can easily enumerate
    // combination...
    //
    // first, we will implement naive enumeration...
    
    rule_pair_set_local_type rule_pairs_local;
    rule_pair_type rule_pair;

    edge_set_type edges_new;
    node_set_type tails_new;

    
    for (size_t id = 0; id != derivations.size(); ++ id) {
      derivation_node_type& node = derivations[id];

      //std::cerr << "node id: " << id << std::endl;
      
      derivation_node_type::edge_set_type derivation_edges_new;

      candidates.clear();
      cand_unique.clear();
      
      cand.clear();
      cand.reserve(node.edges.size() * 100);

      derivation_node_type::edge_set_type::const_iterator eiter_end = node.edges.end();
      for (derivation_node_type::edge_set_type::const_iterator eiter = node.edges.begin(); eiter != eiter_end; ++ eiter) {
	const derivation_edge_type& edge = *eiter;
	const index_set_type j(edge.tails.size(), -1);
	
	candidates.push_back(candidate_type(edge, j, false));
	
	cand.push(&candidates.back());
	cand_unique.insert(&candidates.back());
      }
      
      while (! cand.empty()) {
	const candidate_type* item = cand.top();
	cand.pop();

	const derivation_edge_type& edge_composed = item->edge_composed;
	
	// construct rule pair and insert into rule pair list
	construct_rule_pair(graph, sentence, node, edge_composed.edges, edge_composed.tails, rule_pair);
	
	if (item->composed)
	  if ((max_nodes > 0 && edge_composed.internal > max_nodes)
	      || (max_height > 0 && edge_composed.height > max_height))
	    std::cerr << (item->composed ? "composed" : "minimal")
		      << " height: " << edge_composed.height
		      << " node: " << edge_composed.internal
		      << " " << rule_pair.source << " ||| " << rule_pair.target << std::endl;
	
	rule_pairs_local[edge_composed.edges].push_back(rule_pair);
	
	if ((max_height <= 0 || edge_composed.height < max_height) && (max_nodes <= 0 || edge_composed.internal < max_nodes))
	  derivation_edges_new.push_back(edge_composed);
	
	// push-successor...

	const derivation_edge_type& edge = *(item->edge);
	  
	candidate_type query(item->j);
	index_set_type& j = query.j;
	query.edge = item->edge;
	  
	for (size_t i = 0; i != j.size(); ++ i) {
	  ++ j[i];
	    
	  if (j[i] < derivations[edge.tails[i]].edges.size() && cand_unique.find(&query) == cand_unique.end()) {
	    edges_new.clear();
	    tails_new.clear();
	      
	    const std::pair<int, bool> composed_stat = compose_tails(j.begin(), j.end(), edge.tails.begin(), edge.internal, tails_new);
	      
	    if (max_nodes <= 0 || composed_stat.first <= max_nodes) {
	      index_set_type::const_iterator jiter_begin = j.begin();
	      index_set_type::const_iterator jiter_end   = j.end();
	      node_set_type::const_iterator  titer_begin = edge.tails.begin();
	      edge_set_type::const_iterator  eiter_begin = edge.edges.begin();
	      edge_set_type::const_iterator  eiter_end   = edge.edges.end();
		
	      const std::pair<int, int> rule_stat = compose_edges(graph, jiter_begin, jiter_end, titer_begin, eiter_begin, eiter_end, edges_new);

	      if (rule_stat.second != composed_stat.first)
		std::cerr << "composed: " << composed_stat.first << " rule: " << rule_stat.second << std::endl;
		
	      if (max_height <= 0 || rule_stat.first <= max_height) {
		candidates.push_back(candidate_type(edge, j, true));
		  
		candidate_type& item = candidates.back();
		  
		item.edge_composed.edges.swap(edges_new);
		item.edge_composed.tails.swap(tails_new);
		item.edge_composed.height = rule_stat.first;
		item.edge_composed.internal = rule_stat.second;
		  
		cand.push(&item);
		cand_unique.insert(&item);
	      }
	    }
	  }
	  
	  -- j[i];
	}
      }

      
      // allocate new derivations!
      node.edges.swap(derivation_edges_new);
      
      // sort for cube-pruning!
      std::sort(node.edges.begin(), node.edges.end(), less_derivation_edge_type());
    }

    // if we share the same edge set, then, we can easily conclude that it is
    // caused by unaligned word ambiguity...
    rule_pair_set_local_type::iterator riter_end = rule_pairs_local.end();
    for (rule_pair_set_local_type::iterator riter = rule_pairs_local.begin(); riter != riter_end; ++ riter) {
      rule_pair_list_type& rule_list = riter->second;
      
      const double factor = 1.0 / rule_list.size();
      
      rule_pair_list_type::iterator liter_end = rule_list.end();
      for (rule_pair_list_type::iterator liter = rule_list.begin(); liter != liter_end; ++ liter) {
	liter->count *= factor;
	
	if (swap_source_target)
	  liter->source.swap(liter->target);
	
	std::pair<rule_pair_set_type::iterator, bool> result = rule_pairs.insert(*liter);
	if (! result.second)
	  const_cast<rule_pair_type&>(*(result.first)).count += liter->count;
      }
    }
  }

  template <typename IndexIterator, typename TailIterator, typename EdgeIterator, typename Edges>
  std::pair<int, int> compose_edges(const hypergraph_type& graph,
				    IndexIterator& iter, IndexIterator last,
				    TailIterator& tail_iter,
				    EdgeIterator& edge_iter, EdgeIterator edge_last,
				    Edges& edges_new)
  {
    // this should not happen...
    if (edge_iter == edge_last) return std::make_pair(0, 0);
    
    edges_new.push_back(*edge_iter);
    
    const hypergraph_type::edge_type& edge = graph.edges[*edge_iter];
    ++ edge_iter;

    int height = 1;
    int num_tails = edge.tails.size();
    
    hypergraph_type::edge_type::node_set_type::const_iterator titer_end = edge.tails.end();
    for (hypergraph_type::edge_type::node_set_type::const_iterator titer = edge.tails.begin(); titer != titer_end; ++ titer) {
      if (edge_iter != edge_last && graph.edges[*edge_iter].head == *titer) {
	const std::pair<int, int> result = compose_edges(graph, iter, last, tail_iter, edge_iter, edge_last, edges_new);

	height = utils::bithack::max(height, result.first + 1);
	num_tails += result.second;
      } else if (iter != last) {
	if (*iter >= 0) {
	  const derivation_edge_type& edge = derivations[*tail_iter].edges[*iter];
	  edges_new.insert(edges_new.end(), edge.edges.begin(), edge.edges.end());
	  
	  height = utils::bithack::max(height, edge.height + 1);
	  num_tails += edge.internal;
	}
	
	++ iter;
	++ tail_iter;
      } else
	std::cerr << "WARNING: tails size and edge set frontier do not match" << std::endl;
    }
    
    return std::make_pair(height, num_tails);
  }
  
  template <typename IndexIterator, typename TailIterator, typename Tails>
  std::pair<int, bool> compose_tails(IndexIterator first, IndexIterator last,
				     TailIterator tail_iter,
				     int internal,
				     Tails& tails_new)
  {
    bool composed_rule = false;
    for (/**/; first != last; ++ first, ++ tail_iter) {
      if (*first < 0) {
	tails_new.push_back(*tail_iter);
      } else {
	const derivation_edge_type& edge = derivations[*tail_iter].edges[*first];
	tails_new.insert(tails_new.end(), edge.tails.begin(), edge.tails.end());
	
	internal += edge.internal;
	composed_rule = true;
	
	// early termination...
	if (max_nodes > 0 && internal > max_nodes)
	  return std::make_pair(internal, composed_rule);
      }
    }
    
    return std::make_pair(internal, composed_rule);
  }
  
  // construct rule-pair related data
  
  point_set_type positions_source;
  point_set_type positions_target;
  tree_rule_set_type trees;
  covered_type covered;
  range_pos_set_type range_pos;
  
  template <typename Tp>
  struct less_first
  {
    bool operator()(const Tp& x, const Tp& y) const
    {
      return x.first < y.first;
    }
  };
  
  void construct_rule_pair(const hypergraph_type& graph,
			   const sentence_type& sentence,
			   const derivation_node_type& node,
			   const edge_set_type& edges,
			   const node_set_type& tails,
			   rule_pair_type& rule_pair)
  {
    positions_source.clear();
    covered.clear();
    covered.resize(alignment_source_target.size(), false);
    
    tree_rule_type rule_source;
    weight_type weight = weights_outside[node.node];
    

    //std::cerr << "construct source" << std::endl;
    {
      int frontier_pos = 0;
      int index = 1;
      edge_set_type::const_iterator iter = edges.begin();
      edge_set_type::const_iterator iter_end = edges.end();
      construct_rule(graph, iter, iter_end, rule_source, index, frontier_pos, positions_source, covered, weight);
    }
    
    rule_pair.count = weight;
    
    //std::cerr << "construct target" << std::endl;
    
    range_pos.clear();
    trees.clear();
    positions_target.clear();
    positions_target.resize(sentence.size());
    
    if (tails.empty()) {
      trees.insert(trees.end(), sentence.begin() + node.range.first, sentence.begin() + node.range.second);
      for (int i = node.range.first; i != node.range.second; ++ i)
	positions_target[i] = i - node.range.first;
    } else {
      int index = 1;
      range_pos.reserve(tails.size());
      node_set_type::const_iterator titer_end = tails.end();
      for (node_set_type::const_iterator titer = tails.begin(); titer != titer_end; ++ titer, ++ index)
	range_pos.push_back(std::make_pair(derivations[*titer].range, index));
      
      std::sort(range_pos.begin(), range_pos.end(), less_first<range_pos_type>());
      
      int pos_first = node.range.first;
      range_pos_set_type::const_iterator riter_end = range_pos.end();
      for (range_pos_set_type::const_iterator riter = range_pos.begin(); riter != riter_end; ++ riter) {
	const int pos_last = riter->first.first;
	
	int mapped_pos = trees.size();
	for (int i = pos_first; i != pos_last; ++ i, ++ mapped_pos)
	  positions_target[i] = mapped_pos;
	
	trees.insert(trees.end(), sentence.begin() + pos_first, sentence.begin() + pos_last);
	trees.push_back(non_terminal.non_terminal(riter->second));
	
	pos_first = riter->first.second;
      }
      
      const int pos_last = node.range.second;
      int mapped_pos = trees.size();
      for (int i = pos_first; i != pos_last; ++ i, ++ mapped_pos)
	positions_target[i] = mapped_pos;
      
      trees.insert(trees.end(), sentence.begin() + pos_first, sentence.begin() + pos_last);
    }
    
    tree_rule_type rule_target(non_terminal, trees.begin(), trees.end());

    //
    // construct word alignment...
    // we can easily reconsruct from positions_{source, target} + covered
    //

    //std::cerr << "construct alignment" << std::endl;
    
    rule_pair.alignment.clear();
    int pos_src = 0;
    
    for (size_t src = 0; src != covered.size(); ++ src)
      if (covered[src]) {
	point_set_type::const_iterator aiter_begin = alignment_source_target[src].begin();
	point_set_type::const_iterator aiter_end   = alignment_source_target[src].end();
	
	for (point_set_type::const_iterator aiter = aiter_begin; aiter != aiter_end; ++ aiter)
	  rule_pair.alignment.push_back(std::make_pair(positions_source[pos_src], positions_target[*aiter]));
	
	++ pos_src;
      }

    rule_pair.source.clear();
    rule_pair.target.clear();
    
    boost::iostreams::filtering_ostream os_source;
    boost::iostreams::filtering_ostream os_target;
    
    os_source.push(boost::iostreams::back_inserter(rule_pair.source));
    os_target.push(boost::iostreams::back_inserter(rule_pair.target));
    
    os_source << rule_source;
    os_target << rule_target;
  }
  
  template <typename Iterator, typename PosMap, typename Covered>
  void construct_rule(const hypergraph_type& graph,
		      Iterator& iter,
		      Iterator last,
		      tree_rule_type& tree_rule,
		      int& index,
		      int& frontier_pos,
		      PosMap& pos_map,
		      Covered& covered,
		      weight_type& weight)
  {
    // pre-order traversal...
    
    if (iter == last) {
      tree_rule.label = tree_rule.label.non_terminal();
      return;
    }
    
    const hypergraph_type::edge_type& edge = graph.edges[*iter];
    
    for (int pos = edge.first; pos != edge.last; ++ pos)
      covered[pos] = true;
    
    weight *= cicada::operation::weight_function_one<weight_type>()(edge);
    
    tree_rule = tree_rule_type(edge.rule->lhs, edge.rule->rhs.begin(), edge.rule->rhs.end());
    
    ++ iter;
    
    
    size_t tail_pos = 0;
    tree_rule_type::iterator titer_end = tree_rule.end();
    for (tree_rule_type::iterator titer = tree_rule.begin(); titer != titer_end; ++ titer) {
      if (titer->label.is_non_terminal()) {
	
	const id_type node_id = edge.tails[tail_pos];
	
	if (iter != last && node_id == graph.edges[*iter].head)
	  construct_rule(graph, iter, last, *titer, index, frontier_pos, pos_map, covered, weight);
	else {
	  for (int pos = ranges[node_id].first; pos != ranges[node_id].second; ++ pos)
	    covered[pos] = false;
	  
	  weight *= weights_inside[node_id];
	  
	  titer->label = titer->label.non_terminal(index ++);
	  ++ frontier_pos;
	}
	
	++ tail_pos;
      } else {
	pos_map.push_back(frontier_pos);
	++ frontier_pos;
      }
    }
  }
  
  enum color_type {
    white,
    gray,
    black
  };
  
  struct dfs_type
  {
    int node;
    int edge;
    int tail;
    
    dfs_type(const int& _node, const int& _edge, const int& _tail) 
      : node(_node), edge(_edge), tail(_tail) {}
  };

  typedef std::vector<int, std::allocator<int> > reloc_set_type;
  typedef std::vector<color_type, std::allocator<color_type> > color_set_type;
  typedef std::vector<dfs_type, std::allocator<dfs_type> > stack_type;
  
  void prune_derivations()
  {
    // topologically sort...
    // I'm not sure whether we need this... but this is simply to make sure 
    // the extracted GHKM rules are reachable from root of the parse forest
    
    reloc_set_type reloc_node(derivations.size(), -1);
    color_set_type color(derivations.size(), white);
    stack_type stack;
    
    stack.reserve(derivations.size());
    stack.push_back(dfs_type(derivations.size() - 1, 0, 0));
    
    int node_count = 0;
    
    while (! stack.empty()) {
      const dfs_type& dfs = stack.back();
      
      int node_id     = dfs.node;
      size_t pos_edge = dfs.edge;
      size_t pos_tail = dfs.tail;
      
      stack.pop_back();
      
      const derivation_node_type* curr_node = &(derivations[node_id]);
      
      while (pos_edge != curr_node->edges.size()) {
	const derivation_edge_type& curr_edge = curr_node->edges[pos_edge];
	
	if (pos_tail == curr_edge.tails.size()) {
	  // reach end...
	  ++ pos_edge;
	  pos_tail = 0;
	  continue;
	}
	
	const int        tail_node  = curr_edge.tails[pos_tail];
	const color_type tail_color = color[tail_node];
	
	switch (tail_color) {
	case white:
	  ++ pos_tail;
	  stack.push_back(dfs_type(node_id, pos_edge, pos_tail));
	  
	  node_id = tail_node;
	  curr_node = &(derivations[node_id]);
	  
	  color[node_id] = gray;
	  pos_edge = 0;
	  pos_tail = 0;
	  
	  break;
	case black:
	  ++ pos_tail;
	  break;
	case gray:
	  ++ pos_tail;
	  // loop!!!!
	  break;
	}
      }
      
      color[node_id] = black;
      reloc_node[node_id] = node_count ++;
    }
    
    // construct new derivations...
    derivation_set_type derivations_new(node_count);
    for (size_t i = 0; i != derivations.size(); ++ i)
      if (reloc_node[i] >= 0) {
	const derivation_node_type& node_old = derivations[i];
	derivation_node_type& node_new = derivations_new[reloc_node[i]];
	
	node_new.node  = node_old.node;
	node_new.range = node_old.range;
	
	derivation_node_type::edge_set_type::const_iterator eiter_end = node_old.edges.end();
	for (derivation_node_type::edge_set_type::const_iterator eiter = node_old.edges.begin(); eiter != eiter_end; ++ eiter) {
	  
	  node_set_type tails;
	  bool is_valid = true;
	  
	  node_set_type::const_iterator titer_end = eiter->tails.end();
	  for (node_set_type::const_iterator titer = eiter->tails.begin(); is_valid && titer != titer_end; ++ titer) {
	    tails.push_back(reloc_node[*titer]);
	    is_valid = tails.back() >= 0;
	  }
	  
	  if (is_valid)
	    node_new.edges.push_back(derivation_edge_type(eiter->edges, tails, eiter->height, eiter->internal));
	}
      }
    
    derivations.swap(derivations_new);
    derivations_new.clear();
  }
  
  void construct_derivations(const hypergraph_type& graph,
			     const sentence_type& sentence)
  {
    typedef std::deque<frontier_type, std::allocator<frontier_type> > queue_type;
    typedef google::dense_hash_map<range_type, id_type, utils::hashmurmur<size_t>, std::equal_to<range_type> > range_node_map_type;
    typedef google::dense_hash_set<range_type, utils::hashmurmur<size_t>, std::equal_to<range_type> > range_set_type;

    // construc derivations wrt non-aligned words...

    size_t goal_node = size_t(-1);
    
    for (size_t id = 0; id != graph.nodes.size(); ++ id) 
      if (admissibles[id]) {
	const hypergraph_type::node_type& node = graph.nodes[id];

	//std::cerr << "admissible node: " << id << std::endl;
	//
	// compute minimal range and maximum outer-range
	//
	
	const range_type range_min(spans[id].range());
	
	span_type::const_iterator riter_first = complements[id].lower_bound(range_min.first);
	span_type::const_iterator riter_last  = complements[id].lower_bound(range_min.second);
	
	const range_type range_max(riter_first == complements[id].begin() ? 0 : *(-- riter_first) + 1,
				   riter_last != complements[id].end() ? *riter_last : static_cast<int>(sentence.size()));

#if 0
	std::cerr << "range min: " << range_min.first << ".." << range_min.second
		  << " range max: " << range_max.first << ".." << range_max.second
		  << std::endl;
#endif
	
	const bool is_goal(id == graph.goal);
	
	range_node_map_type buf;
	buf.set_empty_key(range_type(0, 0));
	
	queue_type queue;
	
	// construct initial frontiers...
	hypergraph_type::node_type::edge_set_type::const_iterator eiter_end = node.edges.end();
	for (hypergraph_type::node_type::edge_set_type::const_iterator eiter = node.edges.begin(); eiter != eiter_end; ++ eiter) {
	  const hypergraph_type::edge_type& edge = graph.edges[*eiter];
	  
	  queue.resize(queue.size() + 1);
	  frontier_type& frontier = queue.back();
	  
	  frontier.first.push_back(*eiter);
	  
	  hypergraph_type::edge_type::node_set_type::const_iterator titer_end = edge.tails.end();
	  for (hypergraph_type::edge_type::node_set_type::const_iterator titer = edge.tails.begin(); titer != titer_end; ++ titer)
	    if (! admissibles[*titer])
	      frontier.second.push_back(*titer);
	}

	while (! queue.empty()) {
	  const frontier_type& frontier = queue.front();
	  
	  if (frontier.second.empty()) {
#if 0
	    std::cerr << "completed frontier: ";
	    std::copy(frontier.first.begin(), frontier.first.end(), std::ostream_iterator<int>(std::cerr, " "));
	    std::cerr << std::endl;
	    {
	      edge_set_type::const_iterator eiter_end = frontier.first.end();
	      for (edge_set_type::const_iterator eiter = frontier.first.begin(); eiter != eiter_end; ++ eiter) {
		const hypergraph_type::edge_type& edge = graph.edges[*eiter];
		
		std::cerr << "rule: " << *(edge.rule)
			  << " head: " << edge.head
			  << " tails: ";
		std::copy(edge.tails.begin(), edge.tails.end(), std::ostream_iterator<int>(std::cerr, " "));
		std::cerr << std::endl;
	      }
	    }
#endif
	    
	    // construct rule from "fragment", frontier.first
	    // by enumerating edge and its tails in depth-first manner...
	    // use recursive call for simplicity...
	    
	    // compute "tails" for the fragment
	    
	    node_set_type tails;
	    edge_set_type::const_iterator eiter = frontier.first.begin();
	    edge_set_type::const_iterator eiter_end = frontier.first.end();
	    const std::pair<int, int> rule_stat = construct_tails(graph, eiter, eiter_end, tails);
	    
	    
#if 0
	    std::cerr << "tails: ";
	    std::copy(tails.begin(), tails.end(), std::ostream_iterator<int>(std::cerr, " "));
	    std::cerr << std::endl;
#endif
	    
	    // iterate over tails and compute span...
	    // code taken from apply_exact...
	    
	    index_set_type j_ends(tails.size(), 0);
	    index_set_type j(tails.size(), 0);
	    
	    for (size_t i = 0; i != tails.size(); ++ i) {
	      j_ends[i] = node_map[tails[i]].size();
	      //std::cerr << "tail: " << tails[i] << " node size: " << j_ends[i] << std::endl;
	    }
	    
	    node_set_type tails_next(tails.size());

	    range_set_type ranges;
	    ranges.set_empty_key(range_type(0, 0));
	    
	    for (;;) {
	      bool is_valid = true;
	      
	      range_type range(range_min);
	      ranges.clear();
	      
	      for (size_t i = 0; is_valid && i != tails.size(); ++ i) {
		tails_next[i] = node_map[tails[i]][j[i]];
		
		const range_type& range_antecedent = derivations[tails_next[i]].range;
		
		if (! ranges.empty()) {
		  range_set_type::const_iterator riter_end = ranges.end();
		  for (range_set_type::const_iterator riter = ranges.begin(); is_valid && riter != riter_end; ++ riter)
		    is_valid = range_antecedent.second <= riter->first || riter->second <= range_antecedent.first;
		}
		
		if (is_valid) {
		  ranges.insert(range_antecedent);
		  
		  range.first  = utils::bithack::min(range.first, range_antecedent.first);
		  range.second = utils::bithack::max(range.second, range_antecedent.second);
		}
	      }
	      
	      if (is_valid) {
		// our tail-ranges are valid... and we will consider alternatives from range to range_max...
		
#if 0
		std::cerr << "re-computed range min: " << range.first << ".." << range.second
			  << " range max: " << range_max.first << ".." << range_max.second
			  << std::endl;
#endif

		
		if (is_goal) {
		  if (goal_node == size_t(-1)) {
		    goal_node = derivations.size();
		    derivations.resize(goal_node + 1);
		    
		    // range is always [0, sentence.size())
		    derivations.back().node = id;
		    derivations.back().range = range_type(0, sentence.size());
		  }
		  
		  
		  derivations[goal_node].edges.push_back(derivation_edge_type(frontier.first, tails_next, rule_stat.first, rule_stat.second));
		} else {
		  // we will compute all possible ranges...
		  for (int first = range_max.first; first <= range.first; ++ first)
		    for (int last = range.second; last <= range_max.second; ++ last) {
		      const range_type range_next(first, last);

		      //std::cerr << "constructing for range: " << first << ".." << last << std::endl;
		      
		      range_node_map_type::iterator biter = buf.find(range_next);
		      if (biter == buf.end()) {
			derivations.resize(derivations.size() + 1);
			
			derivations.back().node = id;
			derivations.back().range = range_next;
			
			node_map[id].push_back(derivations.size() - 1);
			
			biter = buf.insert(std::make_pair(range_next, derivations.size() - 1)).first;
		      }
		      
		      derivations[biter->second].edges.push_back(derivation_edge_type(frontier.first, tails_next, rule_stat.first, rule_stat.second));
		    }
		}
	      }
	      
	      size_t index = 0;
	      for (/**/; index != tails.size(); ++ index) {
		++ j[index];
		if (j[index] < j_ends[index]) break;
		j[index] = 0;
	      }
	      
	      // finished!
	      if (index == tails.size()) break;
	    }
	    
	  } else {
	    // incomplete... futher expand!
	    const hypergraph_type::node_type& node = graph.nodes[frontier.second.front()];
	    
	    hypergraph_type::node_type::edge_set_type::const_iterator eiter_end = node.edges.end();
	    for (hypergraph_type::node_type::edge_set_type::const_iterator eiter = node.edges.begin(); eiter != eiter_end; ++ eiter) {
	      const hypergraph_type::edge_type& edge = graph.edges[*eiter];
	      
	      queue.resize(queue.size() + 1);
	      frontier_type& frontier_next = queue.back();
	      
	      frontier_next.first = frontier.first;
	      frontier_next.first.push_back(*eiter);
	      
	      hypergraph_type::edge_type::node_set_type::const_iterator titer_end = edge.tails.end();
	      for (hypergraph_type::edge_type::node_set_type::const_iterator titer = edge.tails.begin(); titer != titer_end; ++ titer)
		if (! admissibles[*titer])
		  frontier_next.second.push_back(*titer);
	      
	      frontier_next.second.insert(frontier_next.second.end(), frontier.second.begin() + 1, frontier.second.end());
	    }
	  }
	  
	  queue.pop_front();
	}
      }
  }
    
  template <typename Iterator, typename Tails>
  std::pair<int, int> construct_tails(const hypergraph_type& graph,
				      Iterator& iter,
				      Iterator last,
				      Tails& tails)
  {
    if (iter == last) return std::make_pair(0, 0);
    
    const hypergraph_type::edge_type& edge = graph.edges[*iter];
    ++ iter;
    
    int max_height = 1;
    int num_tails = edge.tails.size();
    
    hypergraph_type::edge_type::node_set_type::const_iterator titer_end = edge.tails.end();
    for (hypergraph_type::edge_type::node_set_type::const_iterator titer = edge.tails.begin(); titer != titer_end; ++ titer) {
      if (iter != last && *titer == graph.edges[*iter].head) {
	const std::pair<int, int> result = construct_tails(graph, iter, last, tails);
	
	max_height = utils::bithack::max(max_height, result.first + 1);
	num_tails += result.second;
      } else
	tails.push_back(*titer);
    }
    
    return std::make_pair(max_height, num_tails);
  }
  
  void admissible_nodes(const hypergraph_type& graph,
			const sentence_type& sentence,
			const alignment_type& alignment)
  {
    // compute inside/outside score

    //std::cerr << "compute inside outside score" << std::endl;
    weights_inside.clear();
    weights_outside.clear();

    weights_inside.resize(graph.nodes.size());
    weights_outside.resize(graph.nodes.size());
    
    cicada::inside(graph, weights_inside, cicada::operation::weight_function_one<weight_type>());
    cicada::outside(graph, weights_inside, weights_outside, cicada::operation::weight_function_one<weight_type>());
    
    alignment_source_target.clear();
    alignment_target_source.clear();
    alignment_target_source.resize(sentence.size());
    
    if (inverse) {
      alignment_type::const_iterator aiter_end = alignment.end();
      for (alignment_type::const_iterator aiter = alignment.begin(); aiter != aiter_end; ++ aiter) {

	const int source = aiter->target;
	const int target = aiter->source;
	
	if (source >= static_cast<int>(alignment_source_target.size()))
	  alignment_source_target.resize(source + 1);
	if (target >= static_cast<int>(alignment_target_source.size()))
	  alignment_target_source.resize(target + 1);
	
	alignment_source_target[source].push_back(target);
	alignment_target_source[target].push_back(source);
      }
    } else {
      alignment_type::const_iterator aiter_end = alignment.end();
      for (alignment_type::const_iterator aiter = alignment.begin(); aiter != aiter_end; ++ aiter) {
	
	if (aiter->source >= static_cast<int>(alignment_source_target.size()))
	  alignment_source_target.resize(aiter->source + 1);
	if (aiter->target >= static_cast<int>(alignment_target_source.size()))
	  alignment_target_source.resize(aiter->target + 1);
      
	alignment_source_target[aiter->source].push_back(aiter->target);
	alignment_target_source[aiter->target].push_back(aiter->source);
      }
    }
    
    // inside-outside!
    
    // first, bottom-up traversal to compute span...
    // we assume that every edge is annoated with its corresponding span information...
    // do we compute by span-forest again...?
    // we also assume that hypergrpah is parse-forest in that the each node share the same span and category...

    //std::cerr << "compute target spans" << std::endl;
    for (size_t id = 0; id != graph.nodes.size(); ++ id) {
      const hypergraph_type::node_type& node = graph.nodes[id];
      
      hypergraph_type::node_type::edge_set_type::const_iterator eiter_end = node.edges.end();
      for (hypergraph_type::node_type::edge_set_type::const_iterator eiter = node.edges.begin(); eiter != eiter_end; ++ eiter) {
	const hypergraph_type::edge_type& edge = graph.edges[*eiter];
	
	// copy range...
	ranges[id] = range_type(edge.first, edge.last);

	if (edge.last - 1 >= static_cast<int>(alignment_source_target.size()))
	  alignment_source_target.resize(edge.last);

	for (int i = edge.first; i != edge.last; ++ i) {
	  point_set_type::const_iterator aiter_begin = alignment_source_target[i].begin();
	  point_set_type::const_iterator aiter_end   = alignment_source_target[i].end();
	  
	  for (point_set_type::const_iterator aiter = aiter_begin; aiter != aiter_end; ++ aiter)
	    spans[id].set(*aiter);
	}
      }
    }
    
    // second, top-down traversal to compute complement
    //std::cerr << "compute target complements" << std::endl;
    for (int id = graph.nodes.size() - 1; id >= 0; -- id) {

      //std::cerr << "complement: " << id << std::endl;
      const hypergraph_type::node_type& node = graph.nodes[id];
      
      hypergraph_type::node_type::edge_set_type::const_iterator eiter_end = node.edges.end();
      for (hypergraph_type::node_type::edge_set_type::const_iterator eiter = node.edges.begin(); eiter != eiter_end; ++ eiter) {
	const hypergraph_type::edge_type& edge = graph.edges[*eiter];
	
	hypergraph_type::edge_type::node_set_type::const_iterator titer_begin = edge.tails.begin();
	hypergraph_type::edge_type::node_set_type::const_iterator titer_end   = edge.tails.end();
	
	for (hypergraph_type::edge_type::node_set_type::const_iterator titer = titer_begin; titer != titer_end; ++ titer) {
	  complements[*titer] |= complements[id];
	  for (hypergraph_type::edge_type::node_set_type::const_iterator iiter = titer_begin; iiter != titer_end; ++ iiter)
	    if (iiter != titer)
	      complements[*titer] |= spans[*iiter];
	}
      }
    }

    // check whether admissible or not...
    // do we also compute non-aligned words...?
    
    //std::cerr << "check admissible" << std::endl;
    for (size_t id = 0; id != graph.nodes.size(); ++ id)
      admissibles[id] = ! spans[id].empty() && ! complements[id].intersect(spans[id].range());
  }
};


struct Task
{
  typedef boost::filesystem::path path_type;
  typedef std::vector<path_type, std::allocator<path_type> > path_set_type;

  typedef Bitext bitext_type;
  
  typedef ExtractGHKM::rule_pair_type     rule_pair_type;
  typedef ExtractGHKM::rule_pair_set_type rule_pair_set_type;
  
  typedef utils::lockfree_list_queue<bitext_type, std::allocator<bitext_type> > queue_type;
  
  Task(queue_type& __queue,
       const path_type& __output,
       const std::string& non_terminal,
       const int max_nodes,
       const int max_height,
       const bool inverse,
       const bool swap,
       const double __max_malloc)
    : queue(__queue),
      output(__output),
      extractor(non_terminal, max_nodes, max_height, inverse, swap),
      max_malloc(__max_malloc) {}
  
  queue_type&   queue;
  path_type     output;
  path_set_type paths;
  
  ExtractGHKM extractor;
  RulePairGenerator generator;
  
  double max_malloc;
  
  template <typename Tp>
  struct less_ptr
  {
    bool operator()(const Tp* x, const Tp* y) const
    {
      return *x < *y;
    }
  };
  
  void dump(const rule_pair_set_type& rule_pairs)
  {
    typedef std::vector<const rule_pair_type*, std::allocator<const rule_pair_type*> > sorted_type;
    
    // sorting...
    sorted_type sorted(rule_pairs.size());
    {
      sorted_type::iterator siter = sorted.begin();
      rule_pair_set_type::const_iterator citer_end = rule_pairs.end();
      for (rule_pair_set_type::const_iterator citer = rule_pairs.begin(); citer != citer_end; ++ citer, ++ siter)
	*siter = &(*citer);
    }
    std::sort(sorted.begin(), sorted.end(), less_ptr<rule_pair_type>());
    
    const path_type path_tmp = utils::tempfile::file_name(output / "counts-XXXXXX");
    utils::tempfile::insert(path_tmp);
    const path_type path = path_tmp.file_string() + ".gz";
    utils::tempfile::insert(path);
    
    utils::compress_ostream os(path, 1024 * 1024);
    sorted_type::const_iterator siter_end = sorted.end();
    for (sorted_type::const_iterator siter = sorted.begin(); siter != siter_end; ++ siter)
      generator(os, *(*siter)) << '\n';
    
    paths.push_back(path);
  }
  
  void operator()()
  {
    bitext_type bitext;
    rule_pair_set_type rule_pairs;
    
    const int iteration_mask = (1 << 10) - 1;
    
    for (int iter = 0;/**/; ++ iter) {
      queue.pop_swap(bitext);
      
      if (! bitext.source.is_valid()) break;
      
      extractor(bitext.source, bitext.target, bitext.alignment, rule_pairs);
      
      if (((iter & iteration_mask) == iteration_mask) && (utils::malloc_stats::used() > size_t(max_malloc * 1024 * 1024 * 1024))) {
	dump(rule_pairs);
	rule_pairs.clear();
      }
    }
    
    if (! rule_pairs.empty()) {
      dump(rule_pairs);
      rule_pairs.clear();
    }
  }
};

#endif
