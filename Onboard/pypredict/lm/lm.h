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


#ifndef LM_H
#define LM_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <iconv.h>
#include <errno.h> // EINVAL
#include <wchar.h>
#include <vector>
#include <map>
#include <algorithm>
#include <string>


// break into debugger
// step twice to come back out of the raise() call into known code
#define BREAK raise(SIGINT)

//#undef NDEBUG
#define ASSERT(c) assert(c)
//#ifndef NDEBUG
//#define ASSERT(c) assert(c)
//#else
//#define ASSERT(c) /*c*/
//#endif

#ifndef ALEN
#define ALEN(a) ((int)(sizeof(a)/sizeof(*a)))
#endif

void* MemAlloc(size_t size);
void MemFree(void* p);

// WordId type
typedef uint32_t WordId;
//typedef uint16_t WordId;
#define WIDNONE ((WordId)-1)

// Number of sub-nodes type
//typedef uint16_t InplaceSize;
typedef uint32_t InplaceSize;

// count (ngram frequency) type
typedef uint32_t CountType;


enum ControlWords
{
    UNKNOWN_WORD_ID = 0,
    BEGIN_OF_SENTENCE_ID,
    END_OF_SENTENCE_ID,
    NUMBER_ID,
    NUM_CONTROL_WORDS
};

enum LMError
{
    ERR_NOT_IMPL = -1,
    ERR_NONE = 0,
    ERR_FILE,
    ERR_MEMORY,
    ERR_NUMTOKENS,
    ERR_ORDER_UNEXPECTED,
    ERR_ORDER_UNSUPPORTED,
    ERR_COUNT,
    ERR_UNEXPECTED_EOF,
    ERR_WC2MB,
    ERR_MD2WC,
};

template <class T>
int binsearch(const std::vector<T>& v, T key)
{
    typename std::vector<T>::const_iterator it = lower_bound(v.begin(), v.end(), key);
    if (it != v.end() && *it == key)
        return int(it - v.begin());
    return -1;
}

class StrConv
{
    public:
        StrConv();
        ~StrConv();

        // decode multi-byte to wide-char
        const wchar_t* mb2wc (const char* instr)
        {
            char* inptr = const_cast<char*>(instr);
            size_t inbytes = strlen(instr);

            static char outstr[4096];
            char* outptr = outstr;
            size_t outbytes = sizeof(outstr);

            size_t nconv;

            nconv = iconv (cd_mb_wc, &inptr, &inbytes, &outptr, &outbytes);
            if (nconv == (size_t) -1)
            {
                // Not everything went right.  It might only be
                // an unfinished byte sequence at the end of the
                // buffer.  Or it is a real problem. 
                if (errno != EINVAL)
                {
                    // It is a real problem.  Maybe we ran out of space
                    // in the output buffer or we have invalid input.
                    return NULL;
                }
            }

            // Terminate the output string.
            if (outbytes >= sizeof (wchar_t))
                *((wchar_t *) outptr) = L'\0';

            return (wchar_t *) outstr;
        }

        // encode wide-char to multi-byte
        const char* wc2mb (const wchar_t *instr)
        {
            char* inptr = (char*)instr;
            size_t inbytes = wcslen(instr) * sizeof(*instr);

            static char outstr[4096];
            char* outptr = outstr;
            size_t outbytes = sizeof(outstr);

            size_t nconv = iconv(cd_wc_mb, &inptr, &inbytes,
                                           &outptr, &outbytes);
            if (nconv == (size_t) -1)
            {
                // Not everything went right.  It might only be
                // an unfinished byte sequence at the end of the
                // buffer.  Or it is a real problem. 
                if (errno != EINVAL)
                {
                    // It is a real problem.  Maybe we ran out of space
                    // in the output buffer or we have invalid input.
                    return NULL;
                }
            }

            // Terminate the output string.
            if (outbytes >= sizeof (wchar_t))
                *outptr = '\0';

            return outstr;
        }
    private:
        iconv_t cd_mb_wc;
        iconv_t cd_wc_mb;
};


//------------------------------------------------------------------------
// Dictionary - contains the vocabulary of the language model
//------------------------------------------------------------------------

class Dictionary
{
    public:
        Dictionary()
        {
            sorted = NULL;
            clear();
        }

        void clear();

        WordId word_to_id(const wchar_t* word);
        const wchar_t* id_to_word(WordId wid);
        std::vector<WordId> words_to_ids(const wchar_t** word, int n);

        LMError set_words(const std::vector<wchar_t*>& new_words);
        WordId add_word(const wchar_t* word);

        // get word ids, add unknown words as needed
        bool query_add_words(const wchar_t* const* new_words, int n,
                             std::vector<WordId>& wids,
                             bool allow_new_words = true)
        {
            int i;
            for (i = 0; i < n; i++)
            {
                const wchar_t* word = new_words[i];

                WordId wid = word_to_id(word);
                if (wid == WIDNONE)
                {
                    if (allow_new_words)
                    {
                        wid = add_word(word);
                        if (wid == WIDNONE)
                            return false;
                    }
                    else
                    {
                        wid = UNKNOWN_WORD_ID;
                    }
                }
                wids[i] = wid;
            }
            return true;
        }

        bool contains(const wchar_t* word) {return word_to_id(word) != WIDNONE;}

        void prefix_search(const wchar_t* prefix,
                           std::vector<WordId>* wids_in,  // may be NULL
                           std::vector<WordId>& wids_out,
                           uint32_t options = 0);
        int lookup_word(const wchar_t* word);

        int get_num_word_types() {return words.size();}

        uint64_t get_memory_size();

    protected:
        int search_index(const char* word)
        {
            int index;
            if (sorted)
                index = binsearch_sorted(word);
            else
            {
                // search non-control words
                index = binsearch_words(word);

                // else try to find a control word match
                if (index >= (int)words.size() ||
                    strcmp(words[index], word) != 0)
                {
                    for (int i=0; i<sorted_words_begin; i++)
                        if (strcmp(words[i], word) == 0)
                        {
                            index = i;
                            break;
                        }
                }
            }
            return index;
        }

        // binary search for index of insertion point (std:lower_bound())
        int binsearch_sorted(const char* word)
        {
            int lo = 0;
            int hi = sorted->size();
            while (lo < hi)
            {
                int mid = (lo+hi)>>1;
                int cmp = strcmp(words[(*sorted)[mid]], word);
                if (cmp < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        // binary search for index of insertion point (std:lower_bound())
        int binsearch_words(const char* word)
        {
            int lo = sorted_words_begin;
            int hi = words.size();
            while (lo < hi)
            {
                int mid = (lo+hi)>>1;
                int cmp = strcmp(words[mid], word);
                if (cmp < 0)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        void update_sorting(const char* word, WordId wid);

    protected:
        std::vector<char*> words;
        std::vector<WordId>* sorted;  // only when words aren't already sorted
        int sorted_words_begin;
        StrConv conv;
};


//------------------------------------------------------------------------
// LanguageModel - base class of language models
//------------------------------------------------------------------------

class LanguageModel
{
    public:
        enum PredictOptions
        {
            CASE_INSENSITIVE         = 1<<0, // case insensitive completion,
                                             // affects all characters
            CASE_INSENSITIVE_SMART   = 1<<1, // case insensitive completion,
                                             // only for lower case chars
            ACCENT_INSENSITIVE       = 1<<2, // accent insensitive completion
                                             // affects all characters
            ACCENT_INSENSITIVE_SMART = 1<<3, // accent insensitive completion
                                             // only for non-accent characters
            IGNORE_CAPITALIZED       = 1<<4, // ignore capitalized words,
                                             // only affects first character
            IGNORE_NON_CAPITALIZED   = 1<<5, // ignore non-capitalized words
                                             // only affects first character
            INCLUDE_CONTROL_WORDS    = 1<<6, // include <s>, <num>, ...
            NO_SORT                  = 1<<7, // don't sort by weight

            // Default to not do explicit normalization for performance
            // reasons. Often results will be implicitely normalized already
            // and predictions for word choices just need the correct word order.
            // Normalization has to be enabled for entropy/perplexity
            // calculations or other verification purposes.
            NORMALIZE              = 1<<8, // explicit normalization for
                                           // overlay and loglinint, everything
                                           // else ought to be normalized already.
            FILTER_OPTIONS         = CASE_INSENSITIVE |
                                     ACCENT_INSENSITIVE |
                                     ACCENT_INSENSITIVE_SMART |
                                     IGNORE_CAPITALIZED |
                                     IGNORE_NON_CAPITALIZED,
            DEFAULT_OPTIONS        = 0
        };

    public:
        LanguageModel()
        {
        }

        virtual ~LanguageModel()
        {
        }

        virtual void clear()
        {
            dictionary.clear();
        }

        // never fails
        virtual WordId word_to_id(const wchar_t* word)
        {
            WordId wid = dictionary.word_to_id(word);
            if (wid == WIDNONE)
                return UNKNOWN_WORD_ID;   // map to always existing <unk> entry
            return wid;
        }

        std::vector<WordId> words_to_ids(const std::vector<wchar_t*>& words)
        {
            std::vector<WordId> wids;
            std::vector<wchar_t*>::const_iterator it;
            for(it=words.begin(); it!=words.end(); it++)
                wids.push_back(word_to_id(*it));
            return wids;
        }

        // never fails
        const wchar_t* id_to_word(WordId wid)
        {
            static const wchar_t* not_found = L"";
            const wchar_t* w = dictionary.id_to_word(wid);
            if (!w)
                return not_found;
            return w;
        }

        int lookup_word(const wchar_t* word)
        {
            return dictionary.lookup_word(word);
        }

        typedef struct {std::wstring word; double p;} Result;
        virtual void predict(std::vector<LanguageModel::Result>& results,
                             const std::vector<wchar_t*>& context,
                             int limit=-1,
                             uint32_t options = DEFAULT_OPTIONS);

        virtual double get_probability(const wchar_t* const* ngram, int n);

        virtual int get_num_word_types() {return dictionary.get_num_word_types();}

        virtual LMError load(const char* filename) = 0;
        virtual LMError save(const char* filename) = 0;

    protected:
        const wchar_t* split_context(const std::vector<wchar_t*>& context,
                                     std::vector<wchar_t*>& history);
        virtual void get_words_with_predictions(
                                     const std::vector<WordId>& history,
                                     std::vector<WordId>& wids)
        {}
        virtual void get_candidates(const std::vector<WordId>& history,
                                    const wchar_t* prefix,
                                    std::vector<WordId>& wids,
                                    uint32_t options);
        virtual void get_probs(const std::vector<WordId>& history,
                               const std::vector<WordId>& words,
                               std::vector<double>& probabilities)
        {}
        LMError read_utf8(const char* filename, wchar_t*& text);

    public:
        Dictionary dictionary;
};


//------------------------------------------------------------------------
// NGramModel - base class of n-gram language models, may go away
//------------------------------------------------------------------------

class NGramModel : public LanguageModel
{
    public:
        NGramModel()
        {
            order = 0;
        }

        virtual int get_order()
        {
            return order;
        }

        virtual void set_order(int n)
        {
            order = n;
            clear();
        }

        virtual int get_max_order()
        {
            return 0;  // 0: unlimited
        }

        #ifndef NDEBUG
        void print_ngram(const std::vector<WordId>& wids);
        #endif

    public:
        int order;
};

#endif

