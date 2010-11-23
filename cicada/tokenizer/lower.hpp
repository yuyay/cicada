// -*- mode: c++ -*-

#ifndef __CICADA__TOKENIZER__LOWER__HPP__
#define __CICADA__TOKENIZER__LOWER__HPP__ 1

#include <vector>
#include <string>

#include <cicada/stemmer.hpp>
#include <cicada/tokenizer.hpp>

namespace cicada
{
  namespace tokenizer
  {
    class Lower : public cicada::Tokenizer
    {
    public:
      Lower() : lower(&cicada::Stemmer::create("lower")) {}
      Lower(const Lower& x) : lower(&cicada::Stemmer::create("lower")) {}
      Lower& operator=(const Lower& x)
      {
	lower = &cicada::Stemmer::create("lower");
	return *this;
      }
      
    protected:
      virtual void tokenize(const sentence_type& source, sentence_type& tokenized) const
      {
	tokenized.clear();
	sentence_type::const_iterator siter_end = source.end();
	for (sentence_type::const_iterator siter = source.begin(); siter != siter_end; ++ siter)
	  tokenized.push_back(lower->operator()(*siter));
      }
      
    private:
      cicada::Stemmer* lower;
    };
  };
};

#endif
