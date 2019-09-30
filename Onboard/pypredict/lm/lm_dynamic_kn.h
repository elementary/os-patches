/*
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

Author: marmuta <marmvta@gmail.com>
*/

#ifndef LM_DYNAMIC_KN_H
#define LM_DYNAMIC_KN_H

#include <assert.h>
#include "lm_dynamic.h"

#pragma pack(2)


//------------------------------------------------------------------------
// BeforeLastNodeKN - second to last node of the ngram trie, bigram for order 3
//------------------------------------------------------------------------
template <class TBASE>
class BeforeLastNodeKNBase : public TBASE
{
    public:
        BeforeLastNodeKNBase(WordId wid = (WordId)-1)
        : TBASE(wid)
        {
            N1pxr = 0;
        }
        int get_N1pxr() {return N1pxr;}

    public:
        uint32_t N1pxr;    // number of word types wid-n+1 that precede wid-n+2..wid in the training data
};

//------------------------------------------------------------------------
// TrieNodeKN - node for all lower levels of the ngram trie, unigrams for order 3
//------------------------------------------------------------------------
template <class TBASE>
class TrieNodeKNBase : public TBASE
{
    public:
        TrieNodeKNBase(WordId wid = (WordId)-1)
        : TBASE(wid)
        {
            clear();
        }

        void clear()
        {
            N1pxr = 0;
            N1pxrx = 0;
            TBASE::clear();
        }

        int get_N1pxrx()
        {
            return N1pxrx;
        }

    public:
        // Nomenclature:
        // N1p: number of word types with count>=1 (1p=one plus)
        // x: word, free running variable over all word types wi
        // r: remainder, remaining part of the full ngram
        uint32_t N1pxr;    // number of word types wi-n+1 that precede
                           // wi-n+2..wi in the training data
        uint32_t N1pxrx;   // number of permutations around center part
};

//------------------------------------------------------------------------
// NGramTrieKN - root node of the ngram trie
//------------------------------------------------------------------------
template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
class NGramTrieKN : public NGramTrie<TNODE, TBEFORELASTNODE, TLASTNODE>

{
    private:
        typedef NGramTrie<TNODE, TBEFORELASTNODE, TLASTNODE> Base;

    public:
        NGramTrieKN(WordId wid = (WordId)-1)
        : Base(wid)
        {
        }

        int increment_node_count(BaseNode* node, const WordId* wids, int n,
                                  int increment);

        int get_N1pxr(BaseNode* node, int level);
        int get_N1pxrx(BaseNode* node, int level);

        void get_probs_kneser_ney_i(const std::vector<WordId>& history,
                                    const std::vector<WordId>& words,
                                    std::vector<double>& vp,
                                    int num_word_types,
                                    const std::vector<double>& Ds);
};

// Add increment to node->count and incrementally update kneser-ney counts
template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
int NGramTrieKN<TNODE, TBEFORELASTNODE, TLASTNODE>::
    increment_node_count(BaseNode* node, const WordId* wids, int n,
                         int increment)
{
    // only the first time for each ngram
    if (increment && node->count == 0)
    {
        // get/add node for ngram (wids) excluding predecessor
        // ex: ngram = ["We", "saw"] -> wxr = ["saw"] with predecessor "We"
        // Predecessors exist for unigrams or greater, predecessor of unigrams
        // are all unigrams. In that case use the root to store N1pxr.
        std::vector<WordId> wxr(wids+1, wids+n);
        BaseNode *nd = this->add_node(wxr);
        if (!nd)
            return -1;
        ((TBEFORELASTNODE*)nd)->N1pxr++; // count number of word types wid-n+1
                                         // that precede wid-n+2..wid in the
                                         // training data

        // get/add node for ngram (wids) excluding predecessor and successor
        // ex: ngram = ["We", "saw", "whales"] -> wxrx = ["saw"]
        //     with predecessor "We" and successor "whales"
        // Predecessors and successors exist for bigrams or greater. wxrx is
        // an empty vector for bigrams. In that case use the root to store N1pxrx.
        if (n >= 2)
        {
            std::vector<WordId> wxrx(wids+1, wids+n-1);
            BaseNode* nd = this->add_node(wxrx);
            if (!nd)
                return -1;
            ((TNODE*)nd)->N1pxrx++;  // count number of word types wid-n+1 that precede wid-n+2..wid in the training data
        }
    }

    return Base::increment_node_count(node, wids, n, increment);
}

template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
int NGramTrieKN<TNODE, TBEFORELASTNODE, TLASTNODE>::
    get_N1pxr(BaseNode* node, int level)
{
    if (level == this->order)
        return 0;
    if (level == this->order - 1)
        return static_cast<TBEFORELASTNODE*>(node)->N1pxr;
    return static_cast<TNODE*>(node)->N1pxr;
}

template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
int NGramTrieKN<TNODE, TBEFORELASTNODE, TLASTNODE>::
    get_N1pxrx(BaseNode* node, int level)
{
    if (level == this->order)
        return 0;
    if (level == this->order - 1)
        return 0;
    return static_cast<TNODE*>(node)->get_N1pxrx();
}

// kneser-ney smoothed probabilities
template <class TNODE, class TBEFORELASTNODE, class TLASTNODE>
void NGramTrieKN<TNODE, TBEFORELASTNODE, TLASTNODE>::
     get_probs_kneser_ney_i(const std::vector<WordId>& history,
                            const std::vector<WordId>& words,
                            std::vector<double>& vp,
                            int num_word_types,
                            const std::vector<double>& Ds)
{
    // only fixed history size allowed; don't remove unknown words
    // from the history, mark them with UNKNOWN_WORD_ID instead.
    ASSERT((int)history.size() == order-1);

    int i,j;
    int n = history.size() + 1;
    int size = words.size();   // number of candidate words
    std::vector<int32_t> vc(size);  // vector of counts, reused for order 1..n

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

            // orders 1..n-1
            if (j < n-1)
            {
                // Exclude children without predecessor from the count of
                // successors. This corrects normalization errors for the case
                // that the language model wasn't trained from a single
                // continous stream of tokens, i.e. some tokens don't have
                // successors. This happenes by default with the predefined
                // control words <unk>, <s>, ..., but can also happen when
                // incrementally adding text fragments to a language model.
                int num_children = this->get_num_children(hnode, j);
                for(i=0; i<num_children; i++)
                {
                    // children here may be of type TrieNode or BeforeLastNode,
                    // play safe and cast to the latter.
                    TBEFORELASTNODE* child = static_cast<TBEFORELASTNODE*>
                                    (this->get_child_at(hnode, j, i));

                    if (child->get_N1pxr() == 0)  // no predecessors?
                        N1prx--;  // exclude it from the count of successors
                }

                // number of permutations around history h
                int N1pxrx = get_N1pxrx(hnode, j);
                if (N1pxrx)
                {
                    // get number of word types seen to precede history h
                    if (h.size() == 0) // empty history?
                    {
                        // We're at the root and there are many children, all
                        // unigrams to be accurate. So the number of child nodes
                        // is >= the number of candidate words.
                        // Luckily a childs word_id can be directly looked up
                        // in the unigrams because they are always sorted by word_id
                        // as well. -> take that shortcut for root.
                        for(i=0; i<size; i++)
                        {
                            //printf("%d %d %d %d %d\n", size, j, i, words[i], (int)ngrams.children.size());
                            TNODE* node = static_cast<TNODE*>(this->children[words[i]]);
                            vc[i] = node->N1pxr;
                        }
                    }
                    else
                    {
                        // We're at some level > 0 and very likely there are much
                        // less child nodes than candidate words. E.g. everything
                        // from bigrams up has in all likelihood only few children.
                        // -> Turn the algorithm around and search the child nodes
                        // in the candidate words.
                        fill(vc.begin(), vc.end(), 0);
                        int num_children = this->get_num_children(hnode, j);
                        for(i=0; i<num_children; i++)
                        {
                            // children here may be of type TrieNode or BeforeLastNode,
                            // play safe and cast to the latter.
                            TBEFORELASTNODE* child = static_cast<TBEFORELASTNODE*>
                                            (this->get_child_at(hnode, j, i));

                            // word_indices have to be sorted by index
                            int index = binsearch(words, child->word_id);
                            if (index != -1)
                                vc[index] = child->N1pxr;
                        }
                    }

                    double D = Ds[j];
                    double l1 = D / float(N1pxrx) * N1prx; // normalization factor
                                                           // 1 - lambda
                    for(i=0; i<size; i++)
                    {
                        double a = vc[i] - D;
                        if (a < 0)
                            a = 0;
                        vp[i] = a / N1pxrx + l1 * vp[i];
                    }
                }

            }
            // order n
            else
            {
                // total number of occurences of the history
                int cs = this->sum_child_counts(hnode, j);
                if (cs)
                {
                    // get ngram counts
                    fill(vc.begin(), vc.end(), 0);
                    int num_children = this->get_num_children(hnode, j);
                    for(i=0; i<num_children; i++)
                    {
                        BaseNode* child = this->get_child_at(hnode, j, i);
                        int index = binsearch(words, child->word_id); // word_indices have to be sorted by index
                        if (index >= 0)
                            vc[index] = child->get_count();
                    }

                    double D = Ds[j];
                    double l1 = D / float(cs) * N1prx; // normalization factor
                                                           // 1 - lambda
                    for(i=0; i<size; i++)
                    {
                        double a = vc[i] - D;
                        if (a < 0)
                            a = 0;
                        vp[i] = a / float(cs) + l1 * vp[i];
                    }
                }
            }
        }
    }
}
#pragma pack()


//------------------------------------------------------------------------
// DynamicModelKN - dynamically updatable language model with kneser-ney support
//------------------------------------------------------------------------
template <class TNGRAMS>
class _DynamicModelKN : public _DynamicModel<TNGRAMS>
{
    public:
        typedef _DynamicModel<TNGRAMS> Base;

        static const Smoothing DEFAULT_SMOOTHING = KNESER_NEY_I;

    public:
        _DynamicModelKN()
        {
            this->smoothing = DEFAULT_SMOOTHING;
        }

        virtual std::vector<Smoothing> get_smoothings()
        {
            std::vector<Smoothing> smoothings = Base::get_smoothings();
            smoothings.push_back(KNESER_NEY_I);
            return smoothings;
        }

        virtual void get_node_values(BaseNode* node, int level,
                                    std::vector<int>& values)
        {
            Base::get_node_values(node, level, values);
            values.push_back(this->ngrams.get_N1pxrx(node, level));
            values.push_back(this->ngrams.get_N1pxr(node, level));
        }

    protected:
        virtual void get_probs(const std::vector<WordId>& history,
                                    const std::vector<WordId>& words,
                                    std::vector<double>& probabilities);

    private:
        virtual int increment_node_count(BaseNode* node, const WordId* wids,
                                         int n, int increment)
        {return this->ngrams.increment_node_count(node, wids, n, increment);}
};

typedef _DynamicModelKN<NGramTrieKN<TrieNode<TrieNodeKNBase<BaseNode> >,
                                  BeforeLastNode<BeforeLastNodeKNBase<BaseNode>,
                                                 LastNode<BaseNode> >,
                                  LastNode<BaseNode> > > DynamicModelKN;

// Calculate a vector of probabilities for the ngrams formed
// by history + word[i], for all i.
// input:  constant history and a vector of candidate words
// output: vector of probabilities, one value per candidate word
template <class TNGRAMS>
void _DynamicModelKN<TNGRAMS>::get_probs(const std::vector<WordId>& history,
                                         const std::vector<WordId>& words,
                                         std::vector<double>& probabilities)
{
    // pad/cut history so it's always of length order-1
    int n = std::min((int)history.size(), this->order-1);
    std::vector<WordId> h(this->order-1, UNKNOWN_WORD_ID);
    copy_backward(history.end()-n, history.end(), h.end());

    switch(this->smoothing)
    {
        case KNESER_NEY_I:
            this->ngrams.get_probs_kneser_ney_i(h, words, probabilities,
                                          this->get_num_word_types(), this->Ds);
            break;

        default:
            Base::get_probs(history, words, probabilities);
            break;
    }
}

#endif
