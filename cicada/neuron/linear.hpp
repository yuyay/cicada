// -*- mode: c++ -*-
//
//  Copyright(C) 2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__NEURON_LINEAR__HPP__
#define __CICADA__NEURON_LINEAR__HPP__ 1

#include <cicada/neuron/layer.hpp>

namespace cicada
{
  namespace neuron
  {
    class Linear : public Layer
    {
    public:
      Linear(size_type size_input, size_type size_output);
      Linear(const tensor_type& __weight, const tensor_type& __bias);
      Linear(const tensor_ptr_type& __weight, const tensor_ptr_type& __bias);

    public:
      virtual void forward(const tensor_type& data_input);
      virtual void backward(const tensor_type& data_input, const tensor_type& gradient_output);
      virtual void accumulate(const tensor_type& data_input, const tensor_type& gradient_output);
      virtual layer_ptr_type clone(const bool share=false) const;
      virtual void share(const layer_ptr_type& x);
      virtual std::ostream& write(std::ostream& os) const;
    public:
      tensor_ptr_type weight;
      tensor_ptr_type bias;
      tensor_type gradient_weight;
      tensor_type gradient_bias;
    };
  };
};

#endif
