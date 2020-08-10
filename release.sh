#!/bin/bash

# Create tag
read -p "Tag: " tag
git tag -s -a "$tag"
git push --tags

# Create assets
make
cd build/
tar czvf "zps-$tag.tar.gz" zps
shasum -a 512 "zps-$tag.tar.gz" > "zps-$tag.sha512"
gpg --detach-sign "zps-$tag.tar.gz" # 0xB928720AEC532117

# Upload assets
# https://github.com/buildkite/github-release
gh-release "v$tag" "zps-$tag".* \
    --tag "$tag" \
    --github-repository "orhun/zps" \

# Clean up
rm "zps-$tag".*
cd ..
echo "New release locked and ready (v$tag)"
