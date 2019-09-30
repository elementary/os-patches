from Onboard.pypredict import *

def run():
    model = DynamicModelKN(5)
    model.smoothing = "abs-disc"
    if 0:
        for ng, count in [
                   ["a", 2],
                   ["a b", 2],
                   ["b", 2],
            #       ["b a", 1],
                  ]:
            model.count_ngram(ng.split(), count)
    elif 0:
        model.learn_tokens(["a", "b"])
        model.learn_tokens(["a", "b"])
    elif 0:
        tokens = ["a", "b", "b", "b", "a"]
        #tokens = ["a", "b", "b", "<unk>", "b", "a", "c", "d", "b", "a", "<num>"]
        testing_tokens = tokens
    elif 0:
        text = read_corpus("corpora/en/Moby Dick.txt")
        tokens, spans = tokenize_text(text)
    elif 1:
        training_text = """
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
        testing_text = """
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
        tokens, _spans = tokenize_text(training_text)
        testing_tokens, _spans = tokenize_text(testing_text)


    model.learn_tokens(tokens)
    print("counts: ", model.get_counts())




    tokens = testing_tokens
    for prune_count in range(5):
        m = model.prune(prune_count)
        print("counts: ", m.get_counts())

        if 1:
            for ng in m.iter_ngrams():
                print(ng[1], " ".join(ng[0]))
        break
        for smoothing in ["witten-bell", "abs-disc", "kneser-ney"]:
            m.smoothing = smoothing
            for i in range(min(len(tokens), 1000)):
                print ("smoothing=", smoothing, "prune_count=", prune_count)
                context = tokens[i-5:i] + [""]
                choices = m.predictp(context,
                                     options = model.NORMALIZE |
                                               model.INCLUDE_CONTROL_WORDS)
                psum = sum(x[1] for x in choices)
                eps = 1e-6
                if abs(1.0 - psum) > eps:
                    print(psum, context)
                #if not i % 100:
                 #   print (len(tokens), i)

                #context = tokens[i-5:i] + [""]
                #choices = m.predictp(context, filter=False, normalize=True)
                #psum = sum(x[1] for x in choices)

    model.save("model.lm")
    m.save("m.lm")


class Test:
    def __init__(self, order):
        self.order = order
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
#        print
#        print self.training_tokens
#        model = DynamicModel(3)
#        model.smoothing = "kneser-ney"
#        model.learn_tokens(self.training_tokens)
#        for ng in model.iter_ngrams():
#            print ng
#        print model.predictp([u'a', u''], filter=False)
#        print self.model.predictp([u'a', u'b', u''], -1, False)


    def run(self):
        self.test_prune_absolute_discounting()

    def test_prune_absolute_discounting(self):
        model = DynamicModelKN(self.order)
        model.learn_tokens(self.training_tokens)
        for prune_count in range(5):
            m = model.prune(prune_count)
            m.smoothing = "abs-disc"
            self.probability_sum(m)

        tokens = self.testing_tokens
        for prune_count in range(5):
            m = model.prune(prune_count)
            print("counts: ", m.get_counts())

            for smoothing in ["witten-bell", "abs-disc", "kneser-ney"]:
                print ("smoothing: ", smoothing)
                m.smoothing = smoothing
                for i in range(len(tokens)):
                    context = tokens[:i] + [""]
                    choices = m.predictp(context, filter=False, normalize=True)
                    psum = sum(x[1] for x in choices)
                    eps = 1e-6
                    if abs(1.0 - psum) > eps:
                        print(psum, context)

    def probability_sum(self, model):
        for i in range(len(self.testing_tokens)):
            context = self.testing_tokens[:i] + [""]
            choices = model.predictp(context, filter=False, normalize=True)
            psum = sum(x[1] for x in choices)

            eps = 1e-6

            if abs(1.0 - psum) > eps:
                print("order %d: probabilities don't sum to 1.0; psum=%10f, #results=%6d, context='%s'" % \
                      (self.order, psum, len(choices), repr(context[-4:])))


if __name__ == '__main__':
    #for order in range(2, 5+1):
#    t = Test(2)
    run()

