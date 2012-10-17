//
//  Copyright(C) 2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include <boost/filesystem.hpp>

#include "insertion.hpp"

#include "cicada/parameter.hpp"
#include "cicada/lexicon.hpp"

#include "utils/piece.hpp"
#include "utils/lexical_cast.hpp"
#include "utils/mathop.hpp"
#include "utils/dense_hash_set.hpp"

namespace cicada
{
  namespace feature
  {
    class InsertionImpl
    {
    public:
      typedef size_t    size_type;
      typedef ptrdiff_t difference_type;
      
      typedef boost::filesystem::path path_type;

      typedef cicada::Symbol   symbol_type;
      typedef cicada::Vocab    vocab_type;
      typedef cicada::Sentence sentence_type;
      typedef cicada::Lattice       lattice_type;
      typedef cicada::HyperGraph    hypergraph_type;

      typedef cicada::FeatureFunction feature_function_type;
      
      typedef feature_function_type::feature_set_type   feature_set_type;
      typedef feature_function_type::attribute_set_type attribute_set_type;

      typedef feature_set_type::feature_type     feature_type;
      typedef attribute_set_type::attribute_type attribute_type;

      typedef feature_function_type::state_ptr_type     state_ptr_type;
      typedef feature_function_type::state_ptr_set_type state_ptr_set_type;
      
      typedef feature_function_type::edge_type edge_type;
      typedef feature_function_type::rule_type rule_type;
      
      typedef rule_type::symbol_set_type phrase_type;
      
      typedef symbol_type word_type;
      
      typedef utils::dense_hash_set<word_type, boost::hash<word_type>, std::equal_to<word_type>, std::allocator<word_type> >::type word_set_type;
      
      
      typedef std::vector<bool, std::allocator<bool> > cache_set_type;
      
      typedef cicada::Lexicon lexicon_type;
      
      InsertionImpl()
	: lexicon(0), uniques(), words(), caches(), skip_sgml_tag(false), unique_source(false)
      { uniques.set_empty_key(word_type());  }
      
      struct skipper_epsilon
      {
	bool operator()(const symbol_type& word) const
	{
	  return word == vocab_type::EPSILON || word == vocab_type::BOS || word == vocab_type::EOS;
	}
      };
      
      struct skipper_sgml
      {
	bool operator()(const symbol_type& word) const
	{
	  return word == vocab_type::EPSILON || word == vocab_type::BOS || word == vocab_type::EOS || word.is_sgml_tag();
	}
      };
      
      double lexicon_score(const edge_type& edge)
      {
	return (skip_sgml_tag
		? lexicon_score(edge, skipper_sgml())
		: lexicon_score(edge, skipper_epsilon()));
      }
      
      template <typename Skipper>
      double lexicon_score(const edge_type& edge,
			   Skipper skipper)
      {
	const phrase_type& phrase = edge.rule->rhs;
	
	difference_type inserted = 0;
	
	phrase_type::const_iterator piter_end = phrase.end();
	for (phrase_type::const_iterator piter = phrase.begin(); piter != piter_end; ++ piter) 
	  if (piter->is_terminal() && ! skipper(*piter)) {
	    const symbol_type& target = *piter;
	    
	    const size_type check = (size_type(target.id()) << 1) + 0;
	    const size_type pos   = (size_type(target.id()) << 1) + 1;

	    if (pos >= caches.size())
	      caches.resize(pos + 1, false);

	    if (! caches[check]) {
	      bool inserted = true;
	      sentence_type::const_iterator siter_end = words.end();
	      for (sentence_type::const_iterator siter = words.begin(); siter != siter_end; ++ siter)
		inserted &= lexicon->operator()(*siter, target) < threshold * lexicon->operator()(*siter);
	      
	      caches[check] = true;
	      caches[pos] = inserted;
	    }
	    
	    inserted += caches[pos];
	  }
	
	return - inserted;
      }

      void assign(const lattice_type& lattice)
      {
	if (skip_sgml_tag)
	  assign(lattice, skipper_sgml());
	else
	  assign(lattice, skipper_epsilon());
      }
      
      template <typename Skipper>
      void assign(const lattice_type& lattice, Skipper skipper)
      {
	clear();
	
	if (unique_source) {
	  lattice_type::const_iterator liter_end = lattice.end();
	  for (lattice_type::const_iterator liter = lattice.begin(); liter != liter_end; ++ liter) {
	    lattice_type::arc_set_type::const_iterator aiter_end = liter->end();
	    for (lattice_type::arc_set_type::const_iterator aiter = liter->begin(); aiter != aiter_end; ++ aiter)
	      if (! skipper(aiter->label))
		uniques.insert(aiter->label);
	  }
	  
	  word_set_type::const_iterator uiter_end = uniques.end();
	  for (word_set_type::const_iterator uiter = uniques.begin(); uiter != uiter_end; ++ uiter)
	    words.push_back(*uiter);
	  
	} else {
	  lattice_type::const_iterator liter_end = lattice.end();
	  for (lattice_type::const_iterator liter = lattice.begin(); liter != liter_end; ++ liter) {
	    lattice_type::arc_set_type::const_iterator aiter_end = liter->end();
	    for (lattice_type::arc_set_type::const_iterator aiter = liter->begin(); aiter != aiter_end; ++ aiter)
	      if (! skipper(aiter->label))
		words.push_back(aiter->label);
	  }
	}
	
	std::sort(words.begin(), words.end());
      }
      
      void clear()
      {
	uniques.clear();
	words.clear();
	caches.clear();
      }
      
      lexicon_type* lexicon;
      
      word_set_type  uniques;
      sentence_type  words;
      cache_set_type caches;
      
      bool skip_sgml_tag;
      bool unique_source;
      double threshold;
    };
    
    Insertion::Insertion(const std::string& parameter)
      : pimpl(0)
    {
      typedef cicada::Parameter parameter_type;
      typedef boost::filesystem::path path_type;
      
      const parameter_type param(parameter);
      
      if (utils::ipiece(param.name()) != "insertion")
	throw std::runtime_error("this is not a insertion feature: " + parameter);

      std::string path;
      
      bool populate = false;
      bool skip_sgml_tag = false;
      bool unique_source = false;
      double threshold = 0.5;
      
      std::string name;
      
      for (parameter_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "file")
	  path = piter->second;
	else if (utils::ipiece(piter->first) == "populate")
	  populate = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "skip-sgml-tag")
	  skip_sgml_tag = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "unique-source")
	  unique_source = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "threshold")
	  threshold = utils::lexical_cast<double>(piter->second);
	else if (utils::ipiece(piter->first) == "name")
	  name = piter->second;
	else
	  std::cerr << "WARNING: unsupported parameter for insertion: " << piter->first << "=" << piter->second << std::endl;
      }
      
      if (path.empty())
	throw std::runtime_error("no insertion file? " + path);
      
      std::auto_ptr<impl_type> impl(new impl_type());
      
      impl->skip_sgml_tag = skip_sgml_tag;
      impl->unique_source = unique_source;
      impl->threshold = threshold;
      
      // open!
      impl->lexicon = &cicada::Lexicon::create(path);
      
      if (! impl->lexicon)
	throw std::runtime_error("no Insertion");
      
      if (populate)
	impl->lexicon->populate();
      
      base_type::__state_size = 0;
      base_type::__feature_name = (name.empty() ? std::string("insertion") : name);
      
      pimpl = impl.release();
    }
    
    Insertion::~Insertion() { std::auto_ptr<impl_type> tmp(pimpl); }
    
    Insertion::Insertion(const Insertion& x)
      : base_type(static_cast<const base_type&>(x)),
	pimpl(new impl_type(*x.pimpl))
    {
      pimpl->lexicon = 0;
      if (x.pimpl->lexicon)
	pimpl->lexicon = &cicada::Lexicon::create(x.pimpl->lexicon->path().string());
    }

    Insertion& Insertion::operator=(const Insertion& x)
    {
      static_cast<base_type&>(*this) = static_cast<const base_type&>(x);
      *pimpl = *x.pimpl;
      
      pimpl->lexicon = 0;
      if (x.pimpl->lexicon)
	pimpl->lexicon = &cicada::Lexicon::create(x.pimpl->lexicon->path().string());
      
      return *this;
    }
    
    void Insertion::apply(state_ptr_type& state,
			  const state_ptr_set_type& states,
			  const edge_type& edge,
			  feature_set_type& features,
			  const bool final) const
    {
      const double value = pimpl->lexicon_score(edge);
      
      if (value != 0.0)
	features[base_type::__feature_name] = value;
      else
	features.erase(base_type::__feature_name);
    }

    void Insertion::apply_coarse(state_ptr_type& state,
				 const state_ptr_set_type& states,
				 const edge_type& edge,
				 feature_set_type& features,
				 const bool final) const
    {
      apply(state, states, edge, features, final);
    }
    
    void Insertion::apply_predict(state_ptr_type& state,
				  const state_ptr_set_type& states,
				  const edge_type& edge,
				  feature_set_type& features,
				  const bool final) const
    {
      apply(state, states, edge, features, final);
    }

    
    void Insertion::apply_scan(state_ptr_type& state,
			       const state_ptr_set_type& states,
			       const edge_type& edge,
			       const int dot,
			       feature_set_type& features,
			       const bool final) const
    {}
    
    void Insertion::apply_complete(state_ptr_type& state,
				   const state_ptr_set_type& states,
				   const edge_type& edge,
				   feature_set_type& features,
				   const bool final) const
    {}
    
    void Insertion::assign(const size_type& id,
			   const hypergraph_type& hypergraph,
			   const lattice_type& lattice,
			   const span_set_type& spans,
			   const sentence_set_type& targets,
			   const ngram_count_set_type& ngram_counts)
    {
      //
      // how do we assign lexion feature from hypergraph...???
      // we assume that the lattice is always filled with the source-word...!
      //
      
      pimpl->clear();
      if (! lattice.empty())
	pimpl->assign(lattice);
    }
    
  };
};
