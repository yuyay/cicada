// -*- mode: c++ -*-
//
//  Copyright(C) 2012 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __UTILS__SAMPLER__HPP__
#define __UTILS__SAMPLER__HPP__ 1

#include <algorithm>
#include <functional>
#include <numeric>
#include <cmath>

#include <boost/random.hpp>

#include <utils/random_seed.hpp>

namespace utils
{

  template <typename Generator>
  struct sampler
  {
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
    
    typedef Generator generator_type;

    sampler()
      : gen()
    {
      gen.seed(utils::random_seed());
    }
    
    explicit sampler(const unsigned int seed)
      : gen()
    {
      gen.seed(seed);
    }
    
  public:
    generator_type& generator() { return gen; }
    
    double operator()() { return uniform(); }
    
    double uniform() { return boost::uniform_01<double>()(gen); }
    
    double normal(const double& mean, const double& var)
    {
      return boost::normal_distribution<double>(mean, var)(gen);
    }
    
    double poisson(const int lambda)
    {
      return boost::poisson_distribution<int>(lambda)(gen);
    }
    
    bool bernoulli(const double& p)
    {
      return boost::bernoulli_distribution<double>(p)(gen);
    }
    
    double exponential(const double& lambda)
    {
      return boost::exponential_distribution<double>(lambda)(gen);
    }

    double gamma(const double& alpha, const double& beta)
    {
      // in PRML, beta is inverse! I decided to use follow PRML style gamma distribution, not
      // boost::math style beta...
      return boost::gamma_distribution<double>(alpha, 1.0 / beta)(gen);
      
#if 0
      double b, c, u, v, w, y, x, z;
      
      if (a > 1) { // Best's rejection method. Devroye (1986) p.410
        b = a - 1;
        c = 3 * a - 0.75;
	
        bool accept = false;
        while (! accept) {
	  u = uniform();
	  v = uniform();
	  w = u * (1 - u);
	  y = std::sqrt(c / w) * (u - 0.5);
	  x = b + y;
	  
	  if (x >= 0) {
	    z = 64.0 * w * w * w * v * v;
	    accept = (z <= 1.0 - (2.0 * y * y) / x || std::log(z) <= 2.0 * (b * std::log(x / b) - y));
	  }
        } 
      } else { // Johnk's method. Devroye (1986) p.418
        do {
	  x = std::pow(uniform(), 1 / a);
	  y = std::pow(uniform(), 1 / (1 - a));
        } while (x + y > 1);
	
        x = exponential(1.0) * x / (x + y);
      }
      return x / scale;
#endif
    }
    
    double beta(const double& a, const double& b)
    {
      const double x = gamma(a, 1);
      const double y = gamma(b, 1);
      
      return x / (x + y);
    }
    
  private:
    Generator gen;
  };
  
  
};

#endif
