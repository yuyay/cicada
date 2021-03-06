// -*- mode: c++ -*-
//
//  Copyright(C) 2010-2013 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__OPERATION__OUTPUT__HPP__
#define __CICADA__OPERATION__OUTPUT__HPP__ 1

#include <iostream>
#include <vector>

#include <cicada/operation.hpp>

#include <utils/sampler.hpp>

namespace cicada
{
  namespace operation
  {
    class Output : public Operation
    {
    private:
      typedef utils::sampler<boost::mt19937> sampler_type;
      typedef weight_set_type::feature_type feature_type;
      typedef std::vector<feature_type, std::allocator<feature_type> > remove_set_type;

    public:
      Output(const std::string& parameter, output_data_type& __output_data, const int __debug);

      void assign(const weight_set_type& __weights);
  
      void clear();
  
      void operator()(data_type& data) const;
      
      output_data_type& output_data;
  
      path_type file;
      path_type directory;
  
      const weights_path_type* weights;
      const weight_set_type*   weights_assigned;
      bool weights_one;
      bool weights_fixed;

      feature_set_type weights_extra;
      
      int  kbest_size;
      bool kbest_unique;
      bool kbest_sample;
      bool kbest_uniform;
      double diversity;
      sampler_type sampler;
      
      std::string insertion_prefix;

      bool yield_string;
      bool yield_terminal_pos;
      bool yield_tree;
      bool yield_graphviz;
      bool yield_treebank;
      bool yield_alignment;
      bool yield_dependency;
      bool yield_span;

      bool debinarize;
      bool graphviz;
      bool statistics;
      bool lattice_mode;
      bool forest_mode;
      bool span_mode;
      bool alignment_mode;
      bool dependency_mode;
      bool bitext_mode;
      bool no_id;

      remove_set_type removes;
  
      int debug;
    };

  };
};


#endif
