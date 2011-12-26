//
//  Copyright(C) 2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include <set>

#include "sparse_lexicon.hpp"

#include "cicada/lexicon.hpp"
#include "cicada/parameter.hpp"
#include "cicada/cluster.hpp"
#include "cicada/stemmer.hpp"
#include "cicada/cluster_stemmer.hpp"
#include "cicada/feature_vector_linear.hpp"
#include "cicada/feature_vector_unordered.hpp"

#include "utils/piece.hpp"
#include "utils/lexical_cast.hpp"
#include "utils/alloc_vector.hpp"
#include "utils/array_power2.hpp"

#include <google/dense_hash_set>
#include <google/dense_hash_map>

namespace cicada
{
  namespace feature
  {


    class SparseLexiconImpl : public utils::hashmurmur<size_t>
    {
    public:
      typedef size_t    size_type;
      typedef ptrdiff_t difference_type;
      
      typedef cicada::Symbol   symbol_type;
      typedef cicada::Vocab    vocab_type;
      typedef cicada::Sentence sentence_type;
      typedef cicada::Lattice       lattice_type;
      typedef cicada::HyperGraph    hypergraph_type;

      typedef cicada::Cluster  cluster_type;
      typedef cicada::Stemmer  stemmer_type;
      
      typedef cicada::ClusterStemmer normalizer_type;
      typedef std::vector<normalizer_type, std::allocator<normalizer_type> > normalizer_set_type;

      typedef utils::hashmurmur<size_t> hasher_type;

      typedef cicada::FeatureFunction feature_function_type;
      
      typedef feature_function_type::feature_set_type   feature_set_type;
      typedef feature_function_type::attribute_set_type attribute_set_type;

      typedef feature_set_type::feature_type     feature_type;
      typedef attribute_set_type::attribute_type attribute_type;

      typedef FeatureVectorUnordered<feature_set_type::mapped_type> feature_unordered_set_type;
      typedef FeatureVectorLinear<feature_set_type::mapped_type>    feature_linear_set_type;

      typedef feature_function_type::state_ptr_type     state_ptr_type;
      typedef feature_function_type::state_ptr_set_type state_ptr_set_type;
      
      typedef feature_function_type::edge_type edge_type;
      typedef feature_function_type::rule_type rule_type;
      
      typedef rule_type::symbol_set_type phrase_type;
      
      typedef symbol_type word_type;
      
      typedef google::dense_hash_set<word_type, boost::hash<word_type>, std::equal_to<word_type> > word_set_type;

      typedef utils::alloc_vector<feature_linear_set_type, std::allocator<feature_linear_set_type> > cache_set_type;

      struct CacheNormalize
      {
	typedef utils::simple_vector<word_type, std::allocator<word_type> > word_set_type;
	
	word_type::id_type word;
	word_set_type      normalized;
	
	CacheNormalize() : word(word_type::id_type(-1)), normalized() {}
      };
      typedef CacheNormalize cache_normalize_type;
      typedef utils::array_power2<cache_normalize_type, 1024 * 4, std::allocator<cache_normalize_type> > cache_normalize_set_type;
      
      SparseLexiconImpl()
	: uniques(), words(), caches(), skip_sgml_tag(false), unique_source(false), prefix("sparse-lexicon"), forced_feature(false)
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
      
      void lexicon_score(const edge_type& edge,
			 feature_set_type& features)
      {
	if (skip_sgml_tag)
	  lexicon_score(edge, features, skipper_sgml());
	else
	  lexicon_score(edge, features, skipper_epsilon());
      }
      
      
      template <typename Skipper>
      void lexicon_score(const edge_type& edge,
			 feature_set_type& features,
			 Skipper skipper)
      {
	const phrase_type& phrase = edge.rule->rhs;
	
	phrase_type::const_iterator piter_end = phrase.end();
	for (phrase_type::const_iterator piter = phrase.begin(); piter != piter_end; ++ piter) 
	  if (piter->is_terminal() && ! skipper(*piter)) {

	    if (! caches.exists(piter->id())) {
	      feature_unordered_set_type features;
	      
	      sentence_type::const_iterator witer_end = words.end();
	      for (sentence_type::const_iterator witer = words.begin(); witer != witer_end; ++ witer) {
		const std::string name = prefix + ":" + static_cast<const std::string&>(*witer) + "_" + static_cast<const std::string&>(*piter);
		
		if (forced_feature || feature_type::exists(name))
		  features[name] += 1.0;

		if (! normalizers_target.empty()) {
		  const cache_normalize_type::word_set_type& normalized = normalize(*piter,  normalizers_target, cache_target);
		  
		  cache_normalize_type::word_set_type::const_iterator niter_end = normalized.end();
		  for (cache_normalize_type::word_set_type::const_iterator niter = normalized.begin(); niter != niter_end; ++ niter) {
		    const std::string name = prefix + ":" + static_cast<const std::string&>(*witer) + "_" + static_cast<const std::string&>(*niter);
		    
		    if (forced_feature || feature_type::exists(name))
		      features[name] += 1.0;
		  }
		}
	      }
	      
	      caches[piter->id()] = features;
	    }
	    
	    features += caches[piter->id()];
	  }
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
	  for (word_set_type::const_iterator uiter = uniques.begin(); uiter != uiter_end; ++ uiter) {
	    words.push_back(*uiter);
	    
	    if (! normalizers_source.empty()) {
	      const cache_normalize_type::word_set_type& normalized = normalize(*uiter,  normalizers_source, cache_source);
	      
	      words.insert(words.end(), normalized.begin(), normalized.end());
	    }
	  }
	} else {
	  lattice_type::const_iterator liter_end = lattice.end();
	  for (lattice_type::const_iterator liter = lattice.begin(); liter != liter_end; ++ liter) {
	    lattice_type::arc_set_type::const_iterator aiter_end = liter->end();
	    for (lattice_type::arc_set_type::const_iterator aiter = liter->begin(); aiter != aiter_end; ++ aiter)
	      if (! skipper(aiter->label)) {
		words.push_back(aiter->label);
		
		if (! normalizers_source.empty()) {
		  const cache_normalize_type::word_set_type& normalized = normalize(aiter->label,  normalizers_source, cache_source);
		  
		  words.insert(words.end(), normalized.begin(), normalized.end());
		}
	      }
	  }
	}
	
	std::sort(words.begin(), words.end());
      }
      
      const cache_normalize_type::word_set_type& normalize(const word_type& word,
							   normalizer_set_type& normalizers,
							   cache_normalize_set_type& caches)
      {
	cache_normalize_type& cache = caches[hasher_type::operator()(word.id()) & (caches.size() - 1)];
	if (cache.word != word.id()) {
	  cache.word = word.id();
	  cache.normalized.clear();
	  
	  for (size_t i = 0; i != normalizers.size(); ++ i) {
	    const word_type normalized = normalizers[i](word);
	    if (word != normalized)
	      cache.normalized.push_back(normalized);
	  }
	}
	
	return cache.normalized;
      }
      
      void clear()
      {
	uniques.clear();
	words.clear();
	caches.clear();
      }
      
      void clear_cache()
      {
	cache_source.clear();
	cache_target.clear();
      }
      
      normalizer_set_type normalizers_source;
      normalizer_set_type normalizers_target;
      
      cache_normalize_set_type cache_source;
      cache_normalize_set_type cache_target;
      
      word_set_type  uniques;
      sentence_type  words;
      cache_set_type caches;
      
      bool skip_sgml_tag;
      bool unique_source;
      
      std::string prefix;
      bool forced_feature;
    };
    
    SparseLexicon::SparseLexicon(const std::string& parameter)
      : pimpl(0)
    {
      typedef cicada::Parameter parameter_type;
      typedef boost::filesystem::path path_type;
      
      const parameter_type param(parameter);
      
      if (utils::ipiece(param.name()) != "sparse-lexicon")
	throw std::runtime_error("this is not sparse lexicon feature: " + parameter);
      
      impl_type::normalizer_set_type normalizers_source;
      impl_type::normalizer_set_type normalizers_target;
      
      bool skip_sgml_tag = false;
      bool unique_source = false;
      
      std::string name;
      
      for (parameter_type::const_iterator piter = param.begin(); piter != param.end(); ++ piter) {
	if (utils::ipiece(piter->first) == "cluster-source") {
	  if (! boost::filesystem::exists(piter->second))
	    throw std::runtime_error("no cluster file: " + piter->second);
	  
	  normalizers_source.push_back(impl_type::normalizer_type(&cicada::Cluster::create(piter->second)));
	} else if (utils::ipiece(piter->first) == "cluster-target") {
	  if (! boost::filesystem::exists(piter->second))
	    throw std::runtime_error("no cluster file: " + piter->second);
	  
	  normalizers_target.push_back(impl_type::normalizer_type(&cicada::Cluster::create(piter->second)));
	} else if (utils::ipiece(piter->first) == "stemmer-source")
	  normalizers_source.push_back(impl_type::normalizer_type(&cicada::Stemmer::create(piter->second)));
	else if (utils::ipiece(piter->first) == "stemmer-target")
	  normalizers_target.push_back(impl_type::normalizer_type(&cicada::Stemmer::create(piter->second)));
	else if (utils::ipiece(piter->first) == "skip-sgml-tag")
	  skip_sgml_tag = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "unique-source")
	  unique_source = utils::lexical_cast<bool>(piter->second);
	else if (utils::ipiece(piter->first) == "name")
	  name = piter->second;
	else
	  std::cerr << "WARNING: unsupported parameter for sparse lexicon: " << piter->first << "=" << piter->second << std::endl;
      }
      
      std::auto_ptr<impl_type> lexicon_impl(new impl_type());

      lexicon_impl->normalizers_source.swap(normalizers_source);
      lexicon_impl->normalizers_target.swap(normalizers_target);
      
      lexicon_impl->skip_sgml_tag = skip_sgml_tag;
      lexicon_impl->unique_source = unique_source;
      lexicon_impl->prefix = (name.empty() ? std::string("sparse-lexicon") : name);
      
      base_type::__state_size = 0;
      base_type::__feature_name = (name.empty() ? std::string("sparse-lexicon") : name);
      base_type::__sparse_feature = true;
      
      pimpl = lexicon_impl.release();
    }
    
    SparseLexicon::~SparseLexicon() { std::auto_ptr<impl_type> tmp(pimpl); }
    
    SparseLexicon::SparseLexicon(const SparseLexicon& x)
      : base_type(static_cast<const base_type&>(x)),
	pimpl(new impl_type(*x.pimpl))
    {
      pimpl->clear_cache();
    }

    SparseLexicon& SparseLexicon::operator=(const SparseLexicon& x)
    {
      static_cast<base_type&>(*this) = static_cast<const base_type&>(x);
      *pimpl = *x.pimpl;
      
      pimpl->clear_cache();
      
      return *this;
    }
    
    void SparseLexicon::apply(state_ptr_type& state,
			      const state_ptr_set_type& states,
			      const edge_type& edge,
			      feature_set_type& features,
			      const bool final) const
    {
      const_cast<impl_type*>(pimpl)->forced_feature = base_type::apply_feature();
      
      feature_set_type feats;
      
      pimpl->lexicon_score(edge, feats);

      features.update(feats, static_cast<const std::string&>(base_type::feature_name()));
    }

    void SparseLexicon::apply_coarse(state_ptr_type& state,
				     const state_ptr_set_type& states,
				     const edge_type& edge,
				     feature_set_type& features,
				     const bool final) const
    {
      apply(state, states, edge, features, final);
    }
    
    void SparseLexicon::apply_predict(state_ptr_type& state,
				      const state_ptr_set_type& states,
				      const edge_type& edge,
				      feature_set_type& features,
				      const bool final) const
    {
      apply(state, states, edge, features, final);
    }

    
    void SparseLexicon::apply_scan(state_ptr_type& state,
				   const state_ptr_set_type& states,
				   const edge_type& edge,
				   const int dot,
				   feature_set_type& features,
				   const bool final) const
    {}
    void SparseLexicon::apply_complete(state_ptr_type& state,
				       const state_ptr_set_type& states,
				       const edge_type& edge,
				       feature_set_type& features,
				       const bool final) const
    {}
    
    
    void SparseLexicon::assign(const size_type& id,
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
