// -*- mode: c++ -*-

#ifndef __CICADA__FEATURE__BOUNDARY__HPP__
#define __CICADA__FEATURE__BOUNDARY__HPP__ 1

#include <string>

#include <cicada/feature_function.hpp>
#include <cicada/sentence.hpp>
#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>

namespace cicada
{
  namespace feature
  {
    
    class BoundaryImpl;
    
    class Boundary : public FeatureFunction
    {
    public:
      typedef size_t    size_type;
      typedef ptrdiff_t difference_type;
      
      typedef cicada::Symbol   symbol_type;
      typedef cicada::Vocab    vocab_type;
      typedef cicada::Sentence sentence_type;
      
    private:
      typedef FeatureFunction base_type;
      typedef BoundaryImpl        impl_type;
      
    public:
      Boundary(const std::string& parameter);
      Boundary(const Boundary&);
      
      ~Boundary();

      Boundary& operator=(const Boundary&);
      
    private:
      Boundary() {}
      
    public:
      virtual void operator()(state_ptr_type& state,
			      const state_ptr_set_type& states,
			      const edge_type& edge,
			      feature_set_type& features,
			      feature_set_type& estimates) const;
      virtual void operator()(const state_ptr_type& state,
			      const edge_type& edge,
			      feature_set_type& features,
			      feature_set_type& estimates) const;
      virtual void initialize();

      virtual feature_function_ptr_type clone() const { return feature_function_ptr_type(new Boundary(*this)); }
      
    private:
      impl_type* pimpl;
    };

    
  };
};

#endif

