/*
Copyright © 2012, marmuta <marmvta@gmail.com>

This file is part of Onboard.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LM_DYNAMIC_CACHED_H
#define LM_DYNAMIC_CACHED_H

#include "lm_dynamic_kn.h"

#pragma pack(2)

//------------------------------------------------------------------------
// RecencyNode - tracks time of last use
//------------------------------------------------------------------------

class RecencyNode : public BaseNode
{
    public:
        RecencyNode(WordId wid = -1)
        : BaseNode(wid)
        {
            time = 0;
        }

        void clear()
        {
            time = 0;
        }

        const uint32_t get_time() const
        {
            return time;
        }

        void set_time(int t)
        {
            time = t;
        }

        double get_recency_weight(uint32_t current_time, double halflife)
        {
            // exponential decay,
            // halflife is the number of time steps to halfed weight,
            // or in other words the number of recently used ngrams before
            // the weight drops below 0.5.
            double t = current_time - get_time();
//            return time -5;
//            return get_time();
//            return floor(1e9 * pow(2, -t/halflife));
//            return current_time * pow(2, -t/halflife);
//            return floor(1e9 * pow(2, -t/halflife));
            return pow(2, -t/halflife);
        }

    public:
        uint32_t time;   // time of last use
};

template <class TNODE>
double sum_child_recency_weights(TNODE* node, uint32_t current_time,
                                 double halflife)
{
    double sum = 0;
    for (int i=0; i<(int)node->children.size(); i++)
    {
        RecencyNode* nd = static_cast<RecencyNode*>(node->get_child_at(i));
        sum += nd->get_recency_weight(current_time, halflife);
    }
    return sum;
}

//------------------------------------------------------------------------
// NGramTrieRecency - root node of the ngram trie
//------------------------------------------------------------------------
template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
class NGramTrieRecency : public NGramTrieKN<TNODE, TBEFORELASTNODE, TLASTNODE>

{
    private:
        typedef NGramTrieKN<TNODE, TBEFORELASTNODE, TLASTNODE> Base;

    public:
        NGramTrieRecency(WordId wid = (WordId)-1)
        : Base(wid)
        {
            clear();
        }

        void clear()
        {
            current_time = 0;
            Base::clear();
        }

        void set_current_time(int t)
        {
            current_time = t;
        }

        int increment_node_count(BaseNode* node, const WordId* wids, int n,
                                  int increment);

        double sum_child_recency_weights(BaseNode* node, int level,
                                           uint32_t current_time,
                                           double halflife)
        {
            if (level == this->order)
                return -1;  // undefined for leaf nodes
            if (level == this->order - 1)
                return ::sum_child_recency_weights(
                   static_cast<TBEFORELASTNODE*>(node),
                   current_time, halflife);
            return ::sum_child_recency_weights(
               static_cast<TNODE*>(node), current_time, halflife);
        }

        void get_probs_recency_jelinek_mercer_i(const std::vector<WordId>& history,
                                    const std::vector<WordId>& words,
                                    std::vector<double>& vp,
                                    int num_word_types,
                                    uint32_t recency_halflife,
                                    std::vector<double>& lamdas);

    protected:
        uint32_t current_time;      // time is an ever increasing integer
};

// Add increment to node->count and track time of last use
template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
int NGramTrieRecency<TNODE, TBEFORELASTNODE, TLASTNODE>::
    increment_node_count(BaseNode* node, const WordId* wids, int n,
                         int increment)
{
    this->current_time++;        // time is an ever increasing integer
    static_cast<RecencyNode*>(node)->time = this->current_time;

    return Base::increment_node_count(node, wids, n, increment);
}

// Get probabilities based on time of last use.
// jelinek_mercer, smoothed
template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
void NGramTrieRecency<TNODE, TBEFORELASTNODE, TLASTNODE>::
     get_probs_recency_jelinek_mercer_i(const std::vector<WordId>& history,
                          const std::vector<WordId>& words,
                          std::vector<double>& vp,
                          int num_word_types,
                          uint32_t recency_halflife,
                          std::vector<double>& lamdas)
{
    int i,j;
    int n = history.size() + 1;
    int size = words.size();        // number of candidate words
    std::vector<double> vt(size);   // vector of times, reused for order 1..n

    // order 0
    vp.resize(size);
    fill(vp.begin(), vp.end(), 1.0/num_word_types); // uniform distribution

    // order 1..n
    for(j=0; j<n; j++)
    {
        std::vector<WordId> h(history.begin()+(n-j-1), history.end()); // tmp history
        BaseNode* hnode = this->get_node(h);
        if (hnode)
        {
            int N1prx = this->get_N1prx(hnode, j);   // number of word types following the history
            if (!N1prx)  // break early, don't reset probabilities to 0
                break;   // for unknown histories

            // total number of occurences of the history
            double cs = sum_child_recency_weights(hnode, j, current_time,
                                                    recency_halflife);
            if (cs)
            {
                // get ngram times
                fill(vt.begin(), vt.end(), 0);
                int num_children = this->get_num_children(hnode, j);
                for(i=0; i<num_children; i++)
                {
                    RecencyNode* child = static_cast<RecencyNode*>
                                              (this->get_child_at(hnode, j, i));
                    int index = binsearch(words, child->word_id); // word_indices have to be sorted by index
                    if (index >= 0)
                        vt[index] = child->get_recency_weight(current_time,
                                                              recency_halflife);
                }

                double lambda = lamdas[j]; // normalization factor
                for(i=0; i<size; i++)
                {
                    double pmle = vt[i] / cs;
                    vp[i] = lambda * pmle + (1.0 - lambda) * vp[i];
                }
            }
        }
    }
}

#pragma pack()

//------------------------------------------------------------------------
// CachedDynamicModel - dynamic language model with recency tracking
//------------------------------------------------------------------------

template <class TNGRAMS>
class _CachedDynamicModel : public _DynamicModelKN<TNGRAMS>
{
    public:
        typedef _DynamicModelKN<TNGRAMS> Base;
        static const Smoothing DEFAULT_SMOOTHING = ABS_DISC_I;
        static const double DEFAULT_LAMBDA = 0.3;  // Jelinek-Mercer weights

    public:
        _CachedDynamicModel()
        {
            this->smoothing = DEFAULT_SMOOTHING;
            recency_smoothing = JELINEK_MERCER_I;
            recency_ratio = 0.8;
            recency_halflife = 100;     // 100 words until recency_weight=0.5
        }

        virtual void set_order(int order);
        virtual LMError load(const char* filename);

        virtual void get_node_values(BaseNode* node, int level,
                                     std::vector<int>& values)
        {
            Base::get_node_values(node, level, values);
            values.push_back(static_cast<RecencyNode*>(node)->get_time());
        }

        virtual void set_node_time(BaseNode* node, uint32_t time)
        {
            static_cast<RecencyNode*>(node)->set_time(time);
        }

        void set_recency_halflife(double hl) {recency_halflife = hl;}
        uint32_t get_recency_halflife() {return recency_halflife;}

        void set_recency_ratio(double ratio) {recency_ratio = ratio;}
        double get_recency_ratio() {return recency_ratio;}

        void set_recency_smoothing(Smoothing sm) {recency_smoothing = sm;}
        Smoothing get_recency_smoothing() {return recency_smoothing;}

        virtual std::vector<Smoothing> get_recency_smoothings()
        {
            std::vector<Smoothing> smoothings;
            smoothings.push_back(JELINEK_MERCER_I);
            return smoothings;
        }

        void set_recency_lambdas(const std::vector<double>& lambdas)
        {
            recency_lambdas = lambdas;
            recency_lambdas.resize(this->order, DEFAULT_LAMBDA);
        }
        void get_recency_lambdas(std::vector<double>& lambdas)
        {
            lambdas = recency_lambdas;
        }

    protected:
        virtual void get_probs(const std::vector<WordId>& history,
                               const std::vector<WordId>& words,
                               std::vector<double>& probabilities);

        virtual LMError write_arpa_ngram(
            FILE* f, const BaseNode* node, const std::vector<WordId>& wids);

    protected:
        uint32_t recency_halflife;           // halflife of exponential falloff
                                             // in number of recently used words
                                             // until recency weight=0.5
        double recency_ratio;                // linear interpolation ratio
        Smoothing recency_smoothing;
        std::vector<double> recency_lambdas;  // jelinek_mercer smoothing weights
};

typedef _CachedDynamicModel<NGramTrieRecency<TrieNode<TrieNodeKNBase<RecencyNode> >,
                                  BeforeLastNode<BeforeLastNodeKNBase<RecencyNode>,
                                                 LastNode<RecencyNode> >,
                                  LastNode<RecencyNode> > > CachedDynamicModel;

template <class TNGRAMS>
void _CachedDynamicModel<TNGRAMS>::
set_order(int n)
{
    recency_lambdas.resize(n, DEFAULT_LAMBDA);
    Base::set_order(n);  // calls clear()
}

template <class TNGRAMS>
LMError _CachedDynamicModel<TNGRAMS>::
load(const char* filename)
{
    LMError error = Base::load(filename);

    // set current_time to max time of the loaded nodes
    uint32_t max_time = 0;
    typename TNGRAMS::iterator it ;
    for (it = this->ngrams.begin(); *it; it++)
    {
        RecencyNode* node = static_cast<RecencyNode*>(*it);
        if (max_time < node->get_time())
            max_time = node->get_time();
    }
    this->ngrams.set_current_time(max_time);

    return error;
}

// Calculate a vector of probabilities for the ngrams formed
// from history + word[i], for all i.
// input:  constant history and a vector of candidate words
// output: vector of probabilities, one value per candidate word
template <class TNGRAMS>
void _CachedDynamicModel<TNGRAMS>::get_probs(const std::vector<WordId>& history,
                            const std::vector<WordId>& words,
                            std::vector<double>& probabilities)
{
    // pad/cut history so it's always of length order-1
    int n = std::min((int)history.size(), this->order-1);
    std::vector<WordId> h(this->order-1, UNKNOWN_WORD_ID);
    copy_backward(history.end()-n, history.end(), h.end());

    // get probabilities based on counts
    Base::get_probs(history, words, probabilities);
    if (recency_ratio)
    {
        // get probabilities based on recency
        std::vector<double> vpr;
        switch(recency_smoothing)
        {
            case JELINEK_MERCER_I:
                this->ngrams.get_probs_recency_jelinek_mercer_i(h, words,
                               vpr, this->get_num_word_types(),
                               recency_halflife, recency_lambdas);
                break;

            default:
                break;
        }

        // linearly interpolate both components
        if (vpr.size())
        {
            ASSERT(probabilities.size() == vpr.size());

            for (int i=0; i<(int)probabilities.size(); i++)
            {
                probabilities[i] *= 1.0 - recency_ratio;
                probabilities[i] += vpr[i] * recency_ratio;
            }
        }
    }
}


template <class TNGRAMS>
LMError _CachedDynamicModel<TNGRAMS>::
write_arpa_ngram(FILE* f, const BaseNode* _node, const std::vector<WordId>& wids)
{
    const RecencyNode* node = static_cast<const RecencyNode*>(_node);

    fwprintf(f, L"%d %d", node->get_count(), node->get_time());

    std::vector<WordId>::const_iterator it;
    for(it = wids.begin(); it != wids.end(); it++)
        fwprintf(f, L" %ls", this->id_to_word(*it));

    fwprintf(f, L"\n");

    return ERR_NONE;
}

#endif

