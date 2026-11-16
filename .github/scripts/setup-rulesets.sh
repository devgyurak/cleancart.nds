#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
#
# Applies the branch/tag rulesets in .github/rulesets/ and the merge settings
# the branch flow relies on. Safe to re-run (existing rulesets are updated).
#
#   .github/scripts/setup-rulesets.sh [owner/repo]
#
# Branch flow:
#   work branch (<prefix>/<summary>) --PR, squash--> develop
#   develop --release PR, merge commit--> main --tag <game>-v<x.y.z>--> release
#
# Rulesets are repository settings, not files: committing the JSON changes
# nothing until this script runs. Needs gh logged in with admin rights.

set -euo pipefail

RULESETS_DIR="$(dirname "$0")/../rulesets"
REPO="${1:-$(gh repo view --json nameWithOwner -q .nameWithOwner)}"
echo "repository: $REPO"

apply_ruleset() {
  local file="$1" name existing_id
  name="$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['name'])" "$file")"
  existing_id="$(gh api "repos/$REPO/rulesets" --jq ".[] | select(.name == \"$name\") | .id")"
  if [ -n "$existing_id" ]; then
    echo "  update '$name' (id=$existing_id)"
    gh api --method PUT "repos/$REPO/rulesets/$existing_id" --input "$file" >/dev/null
  else
    echo "  create '$name'"
    gh api --method POST "repos/$REPO/rulesets" --input "$file" >/dev/null
  fi
}

apply_ruleset "$RULESETS_DIR/develop-protection.json"
apply_ruleset "$RULESETS_DIR/main-protection.json"
apply_ruleset "$RULESETS_DIR/tag-protection.json"

# Squash merges into develop keep the PR title (with its prefix) as the commit
# summary; merged work branches are deleted.
echo "  merge settings"
gh api --method PATCH "repos/$REPO" >/dev/null \
  -F allow_merge_commit=true -F allow_squash_merge=true -F allow_rebase_merge=true \
  -F delete_branch_on_merge=true \
  -f squash_merge_commit_title=PR_TITLE -f squash_merge_commit_message=PR_BODY

cat <<'EOF'

develop (work PRs land here):
  - changes only through PRs; no direct push, force push or deletion
  - one approval from a code owner (.github/CODEOWNERS: @devgyurak); new commits
    dismiss it, and the last pusher can't be the approver
  - required check 'pr-guard': PR title starts with ADD:/FIX:/REF:/DEL:/DOCS:/UPT:
  - review threads must be resolved; squash, merge or rebase allowed

main (releases only):
  - changes only through PRs from develop ('pr-guard' rejects any other head)
  - same approval rules as develop; merge commits only

tags (trigger the release workflow):
  - only repository admins may create, move or delete tags

Maintainer bypass: the admin role (only @devgyurak) has bypass_mode=pull_request,
so the maintainer can merge their own PRs without an approval (nobody can approve
their own PR). Everyone else needs the maintainer's approval. Direct pushes to
develop/main stay blocked for everyone, admins included.
Code-owner review only takes effect once .github/CODEOWNERS is on the base branch.

Check:
  gh api repos/<owner>/<repo>/rulesets --jq '.[] | "\(.id) \(.name) \(.target) \(.enforcement)"'
  git push origin main      # must be rejected
EOF
