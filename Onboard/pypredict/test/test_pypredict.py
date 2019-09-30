#!/usr/bin/python3

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Author: marmuta <marmvta@gmail.com>
#
import os
import tempfile
import unittest
from Onboard.pypredict import *


class _TestPatterns(unittest.TestCase):

    def __init__(self, test, text, result):
        unittest.TestCase.__init__(self, test)
        self.text = text
        self.result = result

    def test_sentence_pattern(self):
        result = SENTENCE_PATTERN.findall(self.text)
        self.assertEqual(result, self.result,
                         "test '%s': '%s' != '%s'" %
                         (self.text, repr(result), repr(self.result)))


class _TestTokenization(unittest.TestCase):

    def __init__(self, test, text, result):
        unittest.TestCase.__init__(self, test)
        self.training_text = text
        self.result = result

    def test_tokenize_text(self):
        tokens, spans = tokenize_text(self.training_text)
        self.assertEqual(tokens, self.result,
                         "test '%s': '%s' != '%s'" %
                         (self.training_text, repr(tokens), repr(self.result)))

    def test_tokenize_context(self):
        tokens, spans = tokenize_context(self.training_text)
        self.assertEqual(tokens, self.result,
                         "test '%s': '%s' != '%s'" %
                         (self.training_text, repr(tokens), repr(self.result)))

    def test_split_sentences(self):
        sentences, spans = split_sentences(self.training_text)
        self.assertEqual(sentences, self.result,
                         "test '%s': '%s' != '%s'" %
                         (self.training_text, repr(sentences), repr(self.result)))


class _TestMultiOrder(unittest.TestCase):

    def __init__(self, test, order):
        unittest.TestCase.__init__(self, test)
        self.order = order

    def setUp(self):
        # text snippets from MOBY DICK By Herman Melville from Project Gutenberg
        self.training_text = """
            No, when I go to sea, I go as a simple sailor, right before the mast,
            plumb down into the forecastle, aloft there to the royal mast-head.
            True, they rather order me about some, and make me jump from spar to
            spar, like a grasshopper in a May meadow. And at first, this sort
            of thing is unpleasant enough. And more than all,
            if just previous to putting your hand into the tar-pot, you have been
            lording it as a country schoolmaster, making the tallest boys stand
            in awe of you. The transition is a keen one, I assure you, from a
            schoolmaster to a sailor, and requires a strong decoction of Seneca and
            the Stoics to enable you to grin and bear it. But even this wears off in
            time.
            """
        self.testing_text = """
            I now took the measure of the bench, and found that it was a foot too
            short; but that could be mended with a chair. I then placed the
            first bench lengthwise along the only clear space against the wall,
            leaving a little interval between, for my back to settle down in. But I
            soon found that there came such a draught of cold air over me from under
            the sill of the window, that this plan would never do at all, especially
            as another current from the rickety door met the one from the window,
            and both together formed a series of small whirlwinds in the immediate
            vicinity of the spot where I had thought to spend the night.
            """
        #self.training_text = u"Mary has a little lamb. Mary has a little lamb."
        #self.training_text = self.testing_text = u"a <s>"
        #self.training_text = self.testing_text = u"a b <s> c"
        #self.training_text = self.testing_text = u"a b c"
        self.training_tokens, _spans = tokenize_text(self.training_text)
        self.testing_tokens, _spans = tokenize_text(self.testing_text)

    def test_psum_unigram_model(self):
        model = UnigramModel(self.order)
        model.learn_tokens(self.training_tokens)
        self.probability_sum(model)

    def test_psum_dynamic_model_witten_bell(self):
        model = DynamicModel(self.order)
        model.smoothing = "witten-bell"
        model.learn_tokens(self.training_tokens)
        self.probability_sum(model)

    def test_psum_dynamic_model_absolute_discounting(self):
        model = DynamicModel(self.order)
        model.smoothing = "abs-disc"
        model.learn_tokens(self.training_tokens)
        self.probability_sum(model)

    def test_psum_dynamic_model_kneser_ney(self):
        model = DynamicModelKN(self.order)
        model.smoothing = "kneser-ney"
        model.learn_tokens(self.training_tokens)
        self.probability_sum(model)

    def test_psum_cached_dynamic_model(self):
        model = CachedDynamicModel(self.order)
        model.smoothing = "abs-disc"
        model.learn_tokens(self.training_tokens)
        self.probability_sum(model)

    def test_psum_overlay_model(self): # this sums to 1.0 only for identical models
        model = DynamicModel(self.order)
        model.learn_tokens(self.training_tokens)
        self.probability_sum(overlay([model, model]))

    def test_psum_linint_model(self):
        model = DynamicModel(self.order)
        model.learn_tokens(self.training_tokens)
        self.probability_sum(linint([model, model]))

    def test_psum_loglinint_model(self):
        model = DynamicModel(self.order)
        model.learn_tokens(self.training_tokens)
        self.probability_sum(loglinint([model, model]))

    def test_prune_witten_bell(self):
        model = DynamicModel(self.order)
        model.learn_tokens(self.training_tokens)
        for prune_count in range(5):
            m = model.prune([prune_count])
            m.smoothing = "witten-bell"
            self.probability_sum(m)

    def test_prune_absolute_discounting(self):
        model = DynamicModel(self.order)
        model.learn_tokens(self.training_tokens)
        for prune_count in range(5):
            m = model.prune([prune_count])
            m.smoothing = "abs-disc"
            self.probability_sum(m)

    def _test_prune_kneser_ney(self):
        model = DynamicModelKN(self.order)
        model.learn_tokens(self.training_tokens)
        for prune_count in range(5):
            m = model.prune([prune_count])
            m.smoothing = "kneser-ney"
            self.probability_sum(m)

    def probability_sum(self, model):
        def print(s=""): sys.stderr.write(s + '\n')
        # test sum of probabilities for multiple predictions
        num_tests = 0
        num_bad = 0
        num_with_zero = 0

        for i,t in enumerate(self.testing_tokens):
            context = self.testing_tokens[:i] + [""]
            choices = model.predictp(context,
                                     options = model.NORMALIZE |
                                               model.INCLUDE_CONTROL_WORDS)
            psum = sum(x[1] for x in choices)

            num_tests += 1
            eps = 1e-6

            if abs(1.0 - psum) > eps:
                num_bad += 1
                if num_bad == 1:
                    print()
                print("order %d, pos %d: probabilities don't sum to 1.0; psum=%10f, #results=%6d, context='%s'" % \
                      (self.order, num_tests, psum, len(choices), repr(context[-4:])))

            zerocount = sum(1 for word,p in choices if p == 0)
            if zerocount:
                num_with_zero += 1
                print("order %d, pos %d: %d words with zero probability; psum=%10f, #results=%6d, context='%s'" % \
                      (self.order, num_tests, zerocount, psum, len(choices), repr(context[-4:])))

        self.assertEqual(num_tests, num_tests-num_bad,
                      "order %d, probabilities don't sum to 1.0 for %d of %d predictions" % \
                      (self.order, num_bad, num_tests))

        self.assertEqual(num_tests, num_tests-num_with_zero,
                      "order %d, zero probabilities in %d of %d predictions" % \
                      (self.order, num_with_zero, num_tests))


class _TestModel(unittest.TestCase):

    def setUp(self):
        self._tmp_dir = tempfile.TemporaryDirectory(prefix="test_onboard_")
        self._dir = self._tmp_dir.name

    def test_case_insensitive(self):
        model = DynamicModel()
        model.count_ngram(['ABCDE'], 1)

        choices = model.predict(['a'])
        self.assertEqual(choices, [])

        choices = model.predict(['abcde'], options = model.CASE_INSENSITIVE)
        self.assertEqual(choices, ['ABCDE'])

    def test_accent_insensitive(self):
        model = DynamicModel()
        model.count_ngram(['ÉéÈèñ'], 1)
        model.count_ngram(['früh', 'fruchtig'], 1)

        choices = model.predict(['EeEen'])
        self.assertEqual(choices, [])

        choices = model.predict(['EeEen'], options = model.ACCENT_INSENSITIVE)
        self.assertEqual(choices, ['ÉéÈèñ'])

    def test_accent_insensitive_smart(self):
        model = DynamicModel()
        model.count_ngram(['früh', 'fruchtig'], 1)

        choices = model.predict(['fru'])
        self.assertEqual(choices, ['fruchtig'])

        choices = model.predict(['fru'], options = model.ACCENT_INSENSITIVE_SMART)
        self.assertEqual(choices, ['früh', 'fruchtig'])

        choices = model.predict(['frü'], options = model.ACCENT_INSENSITIVE_SMART)
        self.assertEqual(choices, ['früh'])

    def test_ignore_capitalized(self):
        model = DynamicModel()
        model.count_ngram(['ABCDE'], 1)
        model.count_ngram(['abcde'], 1)

        choices = model.predict([''])
        self.assertEqual(choices, ['ABCDE', 'abcde'])

        choices = model.predict([''], options = model.IGNORE_CAPITALIZED)
        self.assertEqual(choices, ['abcde'])

    def test_ignore_non_capitalized(self):
        model = DynamicModel()
        model.count_ngram(['ABCDE'], 1)
        model.count_ngram(['abcde'], 1)

        choices = model.predict([''])
        self.assertEqual(choices, ['ABCDE', 'abcde'])

        choices = model.predict([''], options = model.IGNORE_NON_CAPITALIZED)
        self.assertEqual(choices, ['ABCDE'])

    def test_save_load_unigram_model(self):
        fn = os.path.join(self._dir, "unigram.lm")

        model = UnigramModel()
        tokens = tokenize_text("ccc bbb uu fff ccc ee")[0]
        model.learn_tokens(tokens)
        model.save(fn)

        contents = [x for x in model.iter_ngrams()]
        self.assertEqual(contents,
        [(('<unk>',), 1),
         (('<s>',), 1),
         (('</s>',), 1),
         (('<num>',), 1),
         (('ccc',), 2),
         (('bbb',), 1),
         (('uu',), 1),
         (('fff',), 1),
         (('ee',), 1)]
        )

        # Loading should sort unigrams except the initial control words.
        # Reasons: - Obfuscation of the learned text on second save.
        #          - Making Dictionary::sorted redundant to save memory
        #            and improve performance by working around its insert
        #            inefficiency (becomes crippling with very large
        #            vocabularies, i.e. millions of words)
        model = UnigramModel()
        model.load(fn)
        contents = [x for x in model.iter_ngrams()]
        self.assertEqual(contents,
        [(('<unk>',), 1),
         (('<s>',), 1),
         (('</s>',), 1),
         (('<num>',), 1),
         (('bbb',), 1),
         (('ccc',), 2),
         (('ee',), 1),
         (('fff',), 1),
         (('uu',), 1)]
        )

    def test_save_load_trigram_model(self):
        fn = os.path.join(self._dir, "unigram.lm")

        model = DynamicModel()
        tokens = tokenize_text("ccc bbb uu fff ccc ee")[0]
        model.learn_tokens(tokens)
        model.save(fn)

        contents = [x for x in model.iter_ngrams()]
        self.assertEqual(contents,
            [(('<unk>',), 1, 0),
             (('<s>',), 1, 0),
             (('</s>',), 1, 0),
             (('<num>',), 1, 0),
             (('ccc',), 2, 2),
             (('ccc', 'bbb'), 1, 1),
             (('ccc', 'bbb', 'uu'), 1, 0),
             (('ccc', 'ee'), 1, 0),
             (('bbb',), 1, 1),
             (('bbb', 'uu'), 1, 1),
             (('bbb', 'uu', 'fff'), 1, 0),
             (('uu',), 1, 1),
             (('uu', 'fff'), 1, 1),
             (('uu', 'fff', 'ccc'), 1, 0),
             (('fff',), 1, 1),
             (('fff', 'ccc'), 1, 1),
             (('fff', 'ccc', 'ee'), 1, 0),
             (('ee',), 1, 0)]
        )

        # Loading should sort unigrams except the initial control words.
        model = DynamicModel()
        model.load(fn)
        contents = [x for x in model.iter_ngrams()]
        self.assertEqual(contents,
            [(('<unk>',), 1, 0),
             (('<s>',), 1, 0),
             (('</s>',), 1, 0),
             (('<num>',), 1, 0),
             (('bbb',), 1, 1),
             (('bbb', 'uu'), 1, 1),
             (('bbb', 'uu', 'fff'), 1, 0),
             (('ccc',), 2, 2),
             (('ccc', 'bbb'), 1, 1),
             (('ccc', 'bbb', 'uu'), 1, 0),
             (('ccc', 'ee'), 1, 0),
             (('ee',), 1, 0),
             (('fff',), 1, 1),
             (('fff', 'ccc'), 1, 1),
             (('fff', 'ccc', 'ee'), 1, 0),
             (('uu',), 1, 1),
             (('uu', 'fff'), 1, 1),
             (('uu', 'fff', 'ccc'), 1, 0)]
        )

    def test_read_order(self):
        fn = os.path.join(self._dir, "model.lm")

        self.assertEqual(read_order(fn), None) # file not found

        model = UnigramModel()
        tokens = tokenize_text("ccc bbb uu fff ccc ee")[0]
        model.learn_tokens(tokens)
        model.save(fn)
        self.assertEqual(read_order(fn), 1)

        model = DynamicModel()
        tokens = tokenize_text("ccc bbb uu fff ccc ee")[0]
        model.learn_tokens(tokens)
        model.save(fn)
        self.assertEqual(read_order(fn), 3)


def suite():

    # input-text, text-tokens, context-tokens, sentences
    tests = [
         ["", [], [], []],
         ["abc", ["abc"], ["abc"], ["abc"]],
         ["We saw wha", ['We', 'saw', 'wha'], ['We', 'saw', 'wha'],
             ['We saw wha']],
         ["We saw whales", ['We', 'saw', 'whales'],
             ['We', 'saw', 'whales'],
             ['We saw whales']],
         ["We saw whales ", ['We', 'saw', 'whales'],
             ['We', 'saw', 'whales', ''],
             ['We saw whales']],
         ["We  saw     whales", ['We', 'saw', 'whales'],
             ['We', 'saw', 'whales'],
             ['We  saw     whales']],
         ["Hello there! We saw whales ",
             ['Hello', 'there', '<s>', 'We', 'saw', 'whales'],
             ['Hello', 'there', '<s>', 'We', 'saw', 'whales', ''],
             ['Hello there!', 'We saw whales']],
         ["Hello there! We saw 5 whales ",
             ['Hello', 'there', '<s>', 'We', 'saw', '<num>', 'whales'],
             ['Hello', 'there', '<s>', 'We', 'saw', '<num>', 'whales', ''],
             ['Hello there!', 'We saw 5 whales']],
         ["Hello there! We #?/=$ saw 5 whales ",
             ['Hello', 'there', '<s>', 'We', 'saw', '<num>', 'whales'],
             ['Hello', 'there', '<s>', 'We', 'saw', '<num>', 'whales', ''],
             ['Hello there!', 'We #?/=$ saw 5 whales']],
         [".", [], [''], ['.']],
         [". ", ['<s>'], ['<s>', ''], ['.', '']],
         [". sentence.", ['<s>', 'sentence'], ['<s>', 'sentence', ''],
             ['.', 'sentence.']],
         ["sentence.", ['sentence'], ['sentence', ''], ['sentence.']],
         ["sentence. ", ['sentence', '<s>'], ['sentence', '<s>', ''],
             ['sentence.', '']],
         ["sentence. sentence.", ['sentence', '<s>', 'sentence'],
             ['sentence', '<s>', 'sentence', ''],
             ['sentence.', 'sentence.']],
         ["sentence. sentence. ", ['sentence', '<s>', 'sentence', '<s>'],
             ['sentence', '<s>', 'sentence', '<s>', ''],
             ['sentence.', 'sentence.', '']],
         ["sentence.\n sentence. ", ['sentence', '<s>', 'sentence', '<s>'],
             ['sentence', '<s>', 'sentence', '<s>', ''],
             ['sentence.', 'sentence.', '']],
         ["sentence. \nsentence.", ['sentence', '<s>', 'sentence'],
             ['sentence', '<s>', 'sentence', ''],
             ['sentence.', 'sentence.']],
         ["sentence. \n", ['sentence', '<s>'], ['sentence', '<s>', ''],
             ['sentence.', '']],
         ['sentence "quote." sentence.',
             ['sentence', 'quote', '<s>', 'sentence'],
             ['sentence', 'quote', '<s>', 'sentence', ''],
             ['sentence "quote."', 'sentence.']],
         ["sentence <s>", ['sentence'], ['sentence', ''], ['sentence']],
         [""""double quotes" 'single quotes'""",
             ['double', 'quotes', 'single', "quotes'"],
             ['double', 'quotes', 'single', "quotes'"],
             ['"double quotes" \'single quotes\'']],
         ["(parens) [brackets] {braces}",
             ['parens', 'brackets', 'braces'],
             ['parens', 'brackets', 'braces', ''],
             ['(parens) [brackets] {braces}']],
         ["\nnewline ", ['newline'], ['newline', ''], ['newline']],
         ["double\n\nnewline ", ['double', '<s>', 'newline'],
             ['double', '<s>', 'newline', ''], ['double', 'newline']],
         ["double_newline\n\n", ['double_newline', '<s>'],
             ['double_newline', '<s>', ''], ['double_newline', '']],
         ["double_newline \n \n \n", ['double_newline', '<s>'],
             ['double_newline', '<s>', ''], ['double_newline', '']],
         ["dash-dash", ["dash-dash"], ["dash-dash"], ["dash-dash"]],
         ["dash-", ['dash'], ['dash-'], ['dash-']],
         ["single quote's", ['single', "quote's"], ['single', "quote's"],
             ["single quote's"]],
         ["single quote'", ['single', "quote'"], ['single', "quote'"],
             ["single quote'"]],
         ["under_score's", ["under_score's"], ["under_score's"],
             ["under_score's"]],
         ["Greek Γ´", ['Greek', 'Γ´'], ['Greek', 'Γ´'], ["Greek Γ´"]], # U+00b4
         ["Greek Γ΄", ['Greek', 'Γ΄'], ['Greek', 'Γ΄'], ["Greek Γ΄"]], # U+0384

         # command line handling
         ["-option --option", ['-option', '--option'], ['-option', '--option'],
             ['-option --option']],
         ["cmd -", ['cmd'], ['cmd', '-'], ['cmd -']],
         ["cmd - ", ['cmd'], ['cmd', '-', ''], ['cmd -']],
         ["cmd --", ['cmd'], ['cmd', '--'], ['cmd --']],
         ["cmd -- ", ['cmd'], ['cmd', '--', ''], ['cmd --']],
         ["cmd ---", ['cmd', '<unk>'], ['cmd', '<unk>'], ['cmd ---']],
         ["|", ['|'], ['|'], ['|']],
         ["find | grep", ['find', '|', 'grep'], ['find', '|', 'grep'],
             ['find | grep']],
         ["cat /", ['cat'], ['cat', ''], ['cat /']],

         # passing through control words
         ["<unk> <s> </s> <num>", ['<unk>', '<s>', '</s>', '<num>'],
             ['<unk>', '<s>', '</s>', '<num>', ''],
             ['<unk>', '</s> <num>']],

         # <unk>
         ["repeats: a aa aaa aaaa aaaaa",
             ['repeats', '<s>', 'a', 'aa', 'aaa', '<unk>', '<unk>'],
             ['repeats', '<s>', 'a', 'aa', 'aaa', '<unk>', '<unk>'],
             ['repeats:', 'a aa aaa aaaa aaaaa']],

         # <num>
         ["1", ["<num>"], ["<num>"], ["1"]],
         ["123", ['<num>'], ['<num>'], ["123"]],
         ["-3", ["<num>"], ["<num>"], ["-3"]],
         ["+4", ["<num>"], ["<num>"], ["+4"]],
         ["123.456", ["<num>"], ["<num>"], ["123.456"]],
         ["123,456", ["<num>"], ["<num>"], ["123,456"]],
         ["100,000.00", ["<num>"], ["<num>"], ["100,000.00"]],
         ["100.000,00", ["<num>"], ["<num>"], ["100.000,00"]],
         [".5", ["<num>"], ["<num>"], [".5"]],

         # begin of text markers
         ["<bot:txt> word", ['<bot:txt>', 'word'], ['<bot:txt>', 'word'],
             ['<bot:txt> word']],
         ["<bot:term> word", ['<bot:term>', 'word'], ['<bot:term>', 'word'],
             ['<bot:term> word']],
         ["<bot:url> word", ['<bot:url>', 'word'], ['<bot:url>', 'word'],
             ['<bot:url> word']],
         ["<bot:txt> sentence. sentence. ",
             ['<bot:txt>', 'sentence', '<s>', 'sentence', '<s>'],
             ['<bot:txt>', 'sentence', '<s>', 'sentence', '<s>', ''],
             ['<bot:txt> sentence.', 'sentence.', '']],

         # URLs
         ["www",  ['www'], ['www'],     ['www']],
         ["www ", ['www'], ['www', ''], ['www']],
         ["www.", ['www'], ['www', ''], ['www.']],
         ["www,", ['www'], ['www', ''], ['www,']],
         ["http://user:pass@www.do-mai_n.nl/path/name.ext",
             ['http', 'user', '<unk>', 'www', 'do-mai_n', 'nl', 'path', 'name', 'ext'],
             ['http', 'user', '<unk>', 'www', 'do-mai_n', 'nl', 'path', 'name', 'ext'],
             ['http://user:pass@www.do-mai_n.nl/path/name.ext']],
        ]

    # Low-level regex pattern tests
    # Important are the text and the number of resulting list elements,
    # less so the exact distribution of whitespace.
    sentence_pattern_tests =[
        ["s1", ["s1"]],
        ["s1.", ["s1."]],
        ["s1. ", ["s1.", " "]],
        ["s1. s2. ", ["s1.", " s2.", " "]],
        ["<bot:txt> s1. s2. ", ['<bot:txt> s1.', ' s2.', ' ']],

        ["s1\n", ["s1\n"]],
        ["s1. \n", ["s1.", " \n"]],

        ["s1\n\n", ["s1\n", "\n"]],
        ["s1\n \n", ["s1\n ", "\n"]],
        ["s1 \n \n", ["s1 \n ", "\n"]],

        ["s1\n\n\n\n\n", ["s1\n\n\n\n", "\n"]],

        ["s1.\ns2. ", ["s1.", "\ns2.", " "]],
        ["s1.\ns2.\ns3. ", ["s1.", "\ns2.", "\ns3.", " "]],

        ["s1. \ns2. ", ["s1.", " \ns2.", " "]],
        ["s1. \ns2. \ns3. ", ["s1.", " \ns2.", " \ns3.", " "]],

        ["s1. s2 <s> s3\n\n", ['s1.', ' s2 <s>', ' s3\n', '\n']],
        ["s1. s2 <s> s3\n\ns4", ['s1.', ' s2 <s>', ' s3\n', '\ns4']],
    ]

    suites = []

    suite = unittest.TestSuite()
    for i,a in enumerate(tests):
        suite.addTest(_TestTokenization('test_tokenize_text', a[0], a[1]))
        suite.addTest(_TestTokenization('test_tokenize_context', a[0], a[2]))
        suite.addTest(_TestTokenization('test_split_sentences', a[0], a[3]))
    suites.append(suite)

    suite = unittest.TestSuite()
    for i,a in enumerate(sentence_pattern_tests):
        suite.addTest(_TestPatterns('test_sentence_pattern', a[0], a[1]))
    suites.append(suite)

    suite = unittest.TestSuite()
    test_methods = unittest.TestLoader().getTestCaseNames(_TestMultiOrder)
    for order in range(2, 5+1):
        for method in test_methods:
            suite.addTest(_TestMultiOrder(method, order))
    suites.append(suite)

    suite = unittest.TestLoader().loadTestsFromTestCase(_TestModel)
    suites.append(suite)

    alltests = unittest.TestSuite(suites)
    return alltests


def test():
    unittest.TextTestRunner(verbosity=1).run(suite())

class TestSuiteAllTests(unittest.TestSuite):
    def __init__(self):
        self.add(suite())

if __name__ == '__main__':
    unittest.main()


