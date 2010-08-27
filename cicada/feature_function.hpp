// -*- mode: c++ -*-

#ifndef __CICADA__FEATURE_FUNCTION__HPP__
#define __CICADA__FEATURE_FUNCTION__HPP__ 1

// we will define a basic feature functions
// stateless feature will have zero size feature

#include <vector>

#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>
#include <cicada/hypergraph.hpp>
#include <cicada/lattice.hpp>
#include <cicada/feature_vector.hpp>
#include <cicada/feature.hpp>
#include <cicada/span_vector.hpp>

#include <boost/shared_ptr.hpp>

namespace cicada
{
  class FeatureFunction
  {
  public:
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;

    typedef cicada::Symbol     symbol_type;
    typedef cicada::Vocab      vocab_type;
    typedef cicada::Feature    feature_type;
    typedef cicada::HyperGraph hypergraph_type;
    typedef cicada::SpanVector span_set_type;
    typedef cicada::Lattice    lattice_type;
    typedef cicada::Rule       rule_type;
    
    typedef hypergraph_type::node_type node_type;
    typedef hypergraph_type::edge_type edge_type;
    typedef hypergraph_type::feature_set_type feature_set_type;
    
  public:
    typedef void* state_ptr_type;
    typedef std::vector<state_ptr_type, std::allocator<state_ptr_type> > state_ptr_set_type;

    typedef FeatureFunction                          feature_function_type;
    typedef boost::shared_ptr<feature_function_type> feature_function_ptr_type;
    
  public:
    FeatureFunction()
      : __state_size(0), __sparse_feature(false), __apply_feature(false) {}
    FeatureFunction(int state_size)
      : __state_size(state_size), __sparse_feature(false), __apply_feature(false) {}
    FeatureFunction(int state_size, const feature_type& feature_name)
      : __state_size(state_size), __feature_name(feature_name), __sparse_feature(false), __apply_feature(false) {}
    virtual ~FeatureFunction() {}

  public:
    
    static feature_function_ptr_type create(const std::string& parameter);
    static std::string               lists();
    
  public:
    // feature application 
    // state:  is the result of application (concequence)
    // states: are the states from antecedents
    // edge:   is the current edge (tails will be the tails of new graph, head is the head of old graph)
    //         acces only (*edge.rule) if you are not sure...
    // features: accumulated features.
    //           You should assign by features[feature-name] = score (see feature/ngram.cc), not like features[feature-name] += score
    //           since "apply" can be applied many times for the same edge...
    // estiamtes: upper bound estiamtes
    // final:     final flag indicating that the edge's head is goal
    virtual void apply(state_ptr_type& state,
		       const state_ptr_set_type& states,
		       const edge_type& edge,
		       feature_set_type& features,
		       feature_set_type& estimates,
		       const bool final) const = 0;
    
    // similar to apply, but used for coarse-heuristic functions used in cube-growing
    virtual void apply_coarse(state_ptr_type& state,
			      const state_ptr_set_type& states,
			      const edge_type& edge,
			      feature_set_type& features,
			      feature_set_type& estimates,
			      const bool final) const = 0;
        
    // cloning.. You should be careful for copying objects so that different threads 
    // should not access at the same object. If not sure, "always" gives deep copy.
    virtual feature_function_ptr_type clone() const = 0;
    
    // initialization will be called before and after feature application
    // so that you can clean-up internal data
    virtual void initialize() {}
    
    // You can collect additional data, if necessary.
    // This will be called before feature application.
    virtual void assign(const hypergraph_type& hypergraph) {} // hypergraph before feature application
    virtual void assign(const lattice_type& lattice) {}       // input-lattice (may be empty)
    virtual void assign(const span_set_type& spans) {}        // input-spans (may be empty)
    
    size_type state_size() const { return __state_size; }
    const feature_type& feature_name() const { return __feature_name; }
    bool sparse_feature() const { return __sparse_feature; }
    
    // set by feature application algorithms...
    // This will control the # of binary features...
    bool& apply_feature() { return __apply_feature; }
    bool  apply_feature() const { return __apply_feature; }
    
  protected:
    //
    // You have to set up following parameters:
    //
    
    // the state size used by the feature function. Zero implies state-less. Must be fixed value
    size_type    __state_size;   
    
    // name of the feature. It is used to distinguish feature-functions
    feature_type __feature_name; 
    
    // whether this is a sparse "binary" feature or not
    bool         __sparse_feature; 
    
    // This value will be set by feature application algorithms via apply_feature()
    bool         __apply_feature;  
  };
  
};

#endif
