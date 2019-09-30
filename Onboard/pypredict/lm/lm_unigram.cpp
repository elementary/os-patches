/*
Copyright © 2013, marmuta <marmvta@gmail.com>

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


#include "lm_unigram.h"
#include <numeric>

using namespace std;

//------------------------------------------------------------------------
// UnigramModel
//------------------------------------------------------------------------

// Calculate a vector of probabilities for the ngrams formed
// by history + word[i], for all i.
// Input:  constant history and a vector of candidate words
// Output: vector of probabilities, one value per candidate word
void UnigramModel::get_probs(const std::vector<WordId>& history,
                             const std::vector<WordId>& words,
                             std::vector<double>& probabilities)
{
    std::vector<double>& vp = probabilities;
    int size = words.size();   // number of candidate words
    int num_word_types = get_num_word_types(); 
    int cs = accumulate(m_counts.begin(), m_counts.end(), 0); // total number of occurencess
    if (cs)
    {
        vp.resize(size);
        for(int i=0; i<size; i++)
        {
            WordId wid = words[i];
            CountType count = m_counts.at(wid);
            vp[i] = count / (double) cs;
        }
    }
    else
    {
        fill(vp.begin(), vp.end(), 1.0/num_word_types); // uniform distribution
    }
}

