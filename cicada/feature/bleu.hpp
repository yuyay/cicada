// -*- mode: c++ -*-
//
//  Copyright(C) 2010-2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__FEATURE__BLEU__HPP__
#define __CICADA__FEATURE__BLEU__HPP__ 1

#include <string>

#include <cicada/feature_function.hpp>
#include <cicada/sentence.hpp>
#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>
#include <cicada/feature/scorer.hpp>

namespace cicada
{
  namespace feature
  {
    
    class BleuImpl;
    
    class Bleu : public Scorer
    {
    public:
      typedef size_t    size_type;
      typedef ptrdiff_t difference_type;
      
      typedef cicada::Symbol   symbol_type;
      typedef cicada::Vocab    vocab_type;
      typedef cicada::Sentence sentence_type;
      
    private:
      typedef Scorer   base_type;
      typedef BleuImpl impl_type;
      
    public:
      Bleu(const std::string& parameter);
      Bleu(const Bleu&);
      ~Bleu();

      Bleu& operator=(const Bleu&);
      
    private:
      Bleu() {}
      
    public:
      virtual void apply(state_ptr_type& state,
			 const state_ptr_set_type& states,
			 const edge_type& edge,
			 feature_set_type& features,
			 const bool final) const;
      virtual void apply_coarse(state_ptr_type& state,
				const state_ptr_set_type& states,
				const edge_type& edge,
				feature_set_type& features,
				const bool final) const;
      virtual void apply_predict(state_ptr_type& state,
				 const state_ptr_set_type& states,
				 const edge_type& edge,
				 feature_set_type& features,
				 const bool final) const;
      virtual void apply_scan(state_ptr_type& state,
			      const state_ptr_set_type& states,
			      const edge_type& edge,
			      const int dot,
			      feature_set_type& features,
			      const bool final) const;
      virtual void apply_complete(state_ptr_type& state,
				  const state_ptr_set_type& states,
				  const edge_type& edge,
				  feature_set_type& features,
				  const bool final) const;
      virtual void assign(const size_type& id,
			  const hypergraph_type& hypergraph,
			  const lattice_type& lattice,
			  const span_set_type& spans,
			  const sentence_set_type& targets,
			  const ngram_count_set_type& ngram_counts);
            
      virtual void initialize();

      virtual feature_function_ptr_type clone() const { return feature_function_ptr_type(new Bleu(*this)); }
      
      // bleu specific...
      virtual bool error_metric() const { return false; }
      virtual void assign(const score_ptr_type& score);
      
    private:
      impl_type* pimpl;
    };
    
  };
};


#endif

