#!/bin/sh

if [ -z "$CI_MERGE_REQUEST_DIFF_BASE_SHA" ]; then
    UPSTREAM_BRANCH="$(git rev-parse --abbrev-ref --symbolic-full-name @{u})"
else
    UPSTREAM_BRANCH="$CI_MERGE_REQUEST_DIFF_BASE_SHA"
fi

cp scripts/default.cfg latest-uncrustify-config.cfg

git diff --quiet
DIRTY_TREE="$?"

if [ "$DIRTY_TREE" -ne 0 ]; then
    git stash
    git stash apply
fi

find -name '*.[ch]' -exec uncrustify -q -c latest-uncrustify-config.cfg --replace {} \;

echo > after
find -name '*.[ch]' -exec git diff -- {} \; >> after

git reset --hard $UPSTREAM_BRANCH
find -name '*.[ch]' -exec uncrustify -q -c latest-uncrustify-config.cfg --replace {} \;

echo > before
find -name '*.[ch]' -exec git diff -- {} \; >> before

interdiff -B --no-revert-omitted before after > diff

if [ -n "$(cat diff | grep -vE '^only in patch[12]:')" ]; then
    echo "Uncrustify found style abnormalities" 2>&1
    cat diff
    exit 1
fi

git reset --hard HEAD@{1}

if [ "$DIRTY_TREE" -ne 0 ]; then
    git stash pop
fi

echo "No new style abnormalities found by uncrustify!"
exit 0

