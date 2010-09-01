#include "wer.hpp"

#include <boost/functional/hash.hpp>

namespace cicada
{
  namespace eval
  {

    struct WERScorerConstant
    {
      struct COSTS
      {
	static const double insertion;
	static const double deletion;
	static const double substitution;
      };
    };
    
    const double WERScorerConstant::COSTS::insertion    = 1.0;
    const double WERScorerConstant::COSTS::deletion     = 1.0;
    const double WERScorerConstant::COSTS::substitution = 1.0;

    class WERScorerImpl : public WERScorerConstant
    {
    private:
      friend class WERScorer;
      
    public:
      typedef cicada::Sentence sentence_type;
      typedef cicada::Symbol   word_type;
      
      struct Score
      {
	double score;
	double insertion;
	double deletion;
	double substitution;
	
	Score() : score(), insertion(0), deletion(0), substitution(0) {}

	Score(const double& __score,
	      const double& __insertion,
	      const double& __deletion,
	      const double& __substitution)
	  : score(__score), insertion(__insertion), deletion(__deletion), substitution(__substitution) {}
      };

      typedef Score value_type;
      
      WERScorerImpl() { }
      WERScorerImpl(const sentence_type& __ref) : ref(__ref) {}
      
      value_type operator()(const sentence_type& sentence)
      {
	const sentence_type& a = sentence;
	const sentence_type& b = ref;

	const size_t a_size = a.size();
	const size_t b_size = b.size();
	
	if (a_size == 0) return value_type(b_size, b_size, 0, 0);
	if (b_size == 0) return value_type(a_size, 0, a_size, 0);
	
	std::vector<value_type, std::allocator<value_type> > curr(b_size + 1, value_type());
	std::vector<value_type, std::allocator<value_type> > prev(b_size + 1, value_type());
	
	for (size_t j = 1; j <= b_size; ++ j) {
	  prev[j].score     = prev[j - 1].score + COSTS::insertion;
	  prev[j].insertion = prev[j - 1].insertion + COSTS::insertion;
	}
	
	for (size_t i = 1; i <= a_size; ++ i) {
	  curr[0].score    = prev[0].score + COSTS::deletion;
	  curr[0].deletion = prev[0].deletion + COSTS::deletion;
	  
	  for (size_t j = 1; j <= b_size; ++ j) {
	    const double subst = COSTS::substitution * (a[i - 1] != b[j - 1]);
	    
	    const double score_del = prev[j].score     + COSTS::deletion;
	    const double score_ins = curr[j - 1].score + COSTS::insertion;
	    const double score_sub = prev[j - 1].score + subst;
	    
	    curr[j] = prev[j - 1];
	    curr[j].score        += subst;
	    curr[j].substitution += subst;

	    if (score_ins < curr[j].score) {
	      curr[j] = curr[j - 1];
	      curr[j].score     += COSTS::insertion;
	      curr[j].insertion += COSTS::insertion;
	    }
	    
	    if (score_del < curr[j].score) {
	      curr[j] = prev[j];
	      curr[j].score    += COSTS::deletion;
	      curr[j].deletion += COSTS::deletion;
	    }
	  }
	  std::swap(prev, curr);
	}
	return prev[b_size];
      }
      
    private:
      sentence_type ref;
    };
   
    WERScorer::WERScorer(const WERScorer& x)
      : Scorer(static_cast<const Scorer&>(*this))
    {
      for (impl_set_type::const_iterator iter = x.impl.begin(); iter != x.impl.end(); ++ iter)
	impl.push_back(new impl_type(*(*iter)));
    }
    
    WERScorer::~WERScorer()
    {
      clear();
    }
    
    WERScorer& WERScorer::operator=(const WERScorer& x)
    {
      clear();
      
      static_cast<Scorer&>(*this) = static_cast<const Scorer&>(x);

      for (impl_set_type::const_iterator iter = x.impl.begin(); iter != x.impl.end(); ++ iter)
	impl.push_back(new impl_type(*(*iter)));
      
      return *this;
    }
    
    void WERScorer::clear()
    {
      for (impl_set_type::iterator iter = impl.begin(); iter != impl.end(); ++ iter)
	delete *iter;
      
      impl.clear();
    }
    
    void WERScorer::insert(const sentence_type& sentence)
    {
      if (split) {
	sentence_type sentence_split;
	split_non_ascii_characters(sentence, sentence_split);
	impl.push_back(new impl_type(sentence_split));
      } else
	impl.push_back(new impl_type(sentence));
    }
    
    WERScorer::score_ptr_type WERScorer::score(const sentence_type& __sentence) const
    {
      sentence_type sentence_split;
      if (split)
	split_non_ascii_characters(__sentence, sentence_split);
      
      const sentence_type& sentence = (split ? sentence_split : __sentence);

      double score_best = std::numeric_limits<double>::infinity();

      std::auto_ptr<WER> wer(new WER());
      
      for (impl_set_type::const_iterator iter = impl.begin(); iter != impl.end(); ++ iter) {
	impl_type& evaluator = const_cast<impl_type&>(*(*iter));
	
	impl_type::value_type value = evaluator(sentence);
	
	if (value.score < score_best) {
	  score_best = value.score;
	  
	  wer->insertion    = value.insertion;
	  wer->deletion     = value.deletion;
	  wer->substitution = value.substitution;
	}
	
	wer->references += evaluator.ref.size();
      }
      
      if (! impl.empty())
	wer->references /= impl.size();

      return score_ptr_type(wer.release());
    }
  };
};

