// -*- mode: c++ -*-

#ifndef __CICADA__FEATURE__MBR__HPP__
#define __CICADA__FEATURE__MBR__HPP__ 1

#include <string>

#include <cicada/feature_function.hpp>
#include <cicada/sentence.hpp>
#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>
#include <cicada/weight_vector.hpp>

namespace cicada
{
  namespace feature
  {
    class MBRImpl;

    class MBR : public FeatureFunction
    {
    public:
      typedef size_t    size_type;
      typedef ptrdiff_t difference_type;
      
      typedef cicada::Symbol   symbol_type;
      typedef cicada::Vocab    vocab_type;
      typedef cicada::Sentence sentence_type;

      typedef cicada::WeightVector<double> weight_set_type;
      
    private:
      typedef FeatureFunction base_type;
      typedef MBRImpl impl_type;
      
    public:
      MBR(const std::string& parameter);
      MBR(const MBR&);
      
      ~MBR();

      MBR& operator=(const MBR&);
      
    private:
      MBR() {}
      
    public:
      virtual void apply(state_ptr_type& state,
			 const state_ptr_set_type& states,
			 const edge_type& edge,
			 feature_set_type& features,
			 feature_set_type& estimates,
			 const bool final) const;
      virtual void apply_coarse(state_ptr_type& state,
				const state_ptr_set_type& states,
				const edge_type& edge,
				feature_set_type& features,
				feature_set_type& estimates,
				const bool final) const;
      
      virtual feature_function_ptr_type clone() const { return feature_function_ptr_type(new MBR(*this)); }
      
      int order() const;
      
      void clear();
      
      void insert(const hypergraph_type& graph, const weight_set_type& weights);
      
    private:
      impl_type* pimpl;
    };
    
  };
};


#endif

