#!/usr/bin/env bash

# Create tag
read -r "Tag: " tag
git -c user.signingkey "B928720AEC532117" \
    tag -s -a "$tag"
git push --tags

# Create assets
make
cd build/ || exit
tar -czvf "zps-$tag.tar.gz" zps README.md LICENSE man/zps.1
gpg --local-user "B928720AEC532117" \
    --detach-sign "zps-$tag.tar.gz"
shasum -a 512 "zps-$tag.tar.gz" > "zps-$tag.tar.gz.sha512"

# Upload assets
# https://github.com/buildkite/github-release
gh-release "v$tag" "zps-$tag".* \
    --tag "$tag" \
    --github-repository "orhun/zps" \

# Clean up
rm -f "zps-$tag".*
cd .. || exit
echo "New release locked and ready (v$tag)"
