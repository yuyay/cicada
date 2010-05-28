#include <stdexcept>
#include <set>

#include "model.hpp"

namespace cicada
{
  
  Model::state_type Model::operator()(const hypergraph_type& graph,
				      const state_set_type& node_states,
				      edge_type& edge,
				      feature_set_type& estimates) const
  {
    state_type state_impl(states_size, 0);
    
    feature_function_type::state_ptr_set_type states(edge.tail_nodes.size());

    std::cerr << "apply features for: " << *(edge.rule) << std::endl;
    
    for (int i = 0; i < models.size(); ++ i) {
      const feature_function_type& feature_function = *models[i];
      
      if (feature_function.state_size())
	for (int k = 0; k < states.size(); ++ k)
	  states[k] = const_cast<char*>(&(node_states[edge.tail_nodes[k]][offsets[i]]));
      
      feature_function_type::state_ptr_type state = (feature_function.state_size() ? &(state_impl[offsets[i]]) : 0);
      
      feature_function(state, states, edge, edge.features, estimates);
    }

    std::cerr << "apply features end" << std::endl;
    
    return state_impl;
  }
  
  
  void Model::operator()(const state_type& state,
			 edge_type& edge) const
  {
    const state_type& state_impl(state);
    
    for (int i = 0; i < models.size(); ++ i) {
      const feature_function_type& feature_function = *models[i];
      
      feature_function_type::state_ptr_type antecedent_state = (feature_function.state_size() ? const_cast<char*>(&(state_impl[offsets[i]])) : 0);
      
      feature_function(antecedent_state, edge.features);
    }
  }
  
  
  void Model::initialize()
  {
    typedef std::set<feature_type, std::less<feature_type>, std::allocator<feature_type> > feature_unique_type;
    
    feature_unique_type feature_names;
    
    offsets.clear();
    offsets.reserve(models.size());
    offsets.resize(models.size());
    states_size = 0;
    
    for (int i = 0; i < models.size(); ++ i) {
      offsets[i] = states_size;
      
      std::cerr << "offset: " << i << " = " << offsets[i] << std::endl;
      
      states_size += models[i]->state_size();
      
      if (feature_names.find(models[i]->feature_name()) != feature_names.end())
	throw std::runtime_error("you have already registered feature: " + static_cast<const std::string&>(models[i]->feature_name()));
      feature_names.insert(models[i]->feature_name());
    }
  }
  
};
