// -*- mode: c++ -*-
//
//  Copyright(C) 2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__NEURON_TANH__HPP__
#define __CICADA__NEURON_TANH__HPP__ 1

#include <cmath>

#include <cicada/neuron/layer.hpp>

namespace cicada
{
  namespace neuron
  {
    class Tanh : public Layer
    {
    public:
      virtual void forward(const tensor_type& data_input);
      virtual void backward(const tensor_type& data_input, const tensor_type& gradient_output);

      double function(const double& value) const
      {
        return std::tanh(vaue);
      }
      
      double derivative(const double& value) const
      {
	const double z = function(value);
	return 1.0 - z * z;
      }
    };
  };
};

#endif