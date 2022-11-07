#!/usr/bin/env bash

set -e

project_dir="$(pwd)/.."

# Create tag
read -rp "Tag: " tag
git -c user.signingkey="B928720AEC532117" tag -s -a "$tag"

# Create assets
cd "$project_dir"
make
cd "$project_dir/build/" || exit
tar -czvf "zps-$tag.tar.gz" zps ../README.md \
    ../LICENSE ../man/zps.1 zps.desktop
gpg --local-user "B928720AEC532117" \
    --detach-sign "zps-$tag.tar.gz"
shasum -a 512 "zps-$tag.tar.gz" > "zps-$tag.tar.gz.sha512"

# Upload assets
# https://github.com/buildkite/github-release
gh-release "v$tag" "zps-$tag".* \
    --tag "$tag" \
    --github-repository "orhun/zps" \

# Push tag
git push --tags

# Clean up
#rm -f "zps-$tag".*
cd .. || exit
echo "New release locked and ready (v$tag)"
