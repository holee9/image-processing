#!/bin/bash
# XPE Worktree Setup Script
# Creates 3 development lanes + worktree directories
# Usage: bash tools/setup-worktrees.sh [--create | --remove]

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PARENT_DIR="$(dirname "$REPO_ROOT")"

# Lane definitions: branch_name | worktree_dir | description
declare -A LANES=(
  ["preprocess"]="dev/preprocess|xpe-pre|Lane A: Preprocessing (common + preprocess)"
  ["postprocess"]="dev/postprocess|xpe-post|Lane B: Postprocessing (enhance + ai + display + dicom + gsvg)"
  ["gui"]="dev/gui|xpe-gui|Lane C: Test GUI App (clients)"
)

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }

create_worktrees() {
  echo ""
  echo "=========================================="
  echo " XPE Worktree Lane Setup"
  echo "=========================================="
  echo ""

  cd "$REPO_ROOT"

  # Ensure we're on main and up to date
  local current_branch
  current_branch=$(git branch --show-current)
  if [[ "$current_branch" != "main" ]]; then
    error "Must be on main branch. Current: $current_branch"
    exit 1
  fi

  info "Current branch: main ($(git log --oneline -1))"

  for lane in "${!LANES[@]}"; do
    IFS='|' read -r branch worktree_dir desc <<< "${LANES[$lane]}"
    local worktree_path="${PARENT_DIR}/${worktree_dir}"

    # Check if worktree already exists
    if git worktree list --porcelain | grep -q "$worktree_path" 2>/dev/null; then
      warn "[$lane] Worktree already exists: $worktree_path"
      continue
    fi

    # Check if branch already exists
    if git show-ref --verify --quiet "refs/heads/$branch"; then
      info "[$lane] Branch '$branch' exists, creating worktree only..."
      git worktree add "$worktree_path" "$branch"
    else
      info "[$lane] Creating branch '$branch' and worktree '$worktree_dir'..."
      info "  Description: $desc"
      git worktree add -b "$branch" "$worktree_path" main
    fi

    info "[$lane] Created: $worktree_path (branch: $branch)"
  done

  echo ""
  info "Worktree setup complete. Summary:"
  echo ""
  git worktree list
  echo ""
  info "Directory structure:"
  for lane in "${!LANES[@]}"; do
    IFS='|' read -r branch worktree_dir desc <<< "${LANES[$lane]}"
    echo "  ${worktree_dir}/ -> ${branch} (${desc%%:*})"
  done
  echo ""
  info "Next steps:"
  echo "  1. cd ${PARENT_DIR}/xpe-pre    # Work on preprocessing"
  echo "  2. cd ${PARENT_DIR}/xpe-post   # Work on postprocessing"
  echo "  3. cd ${PARENT_DIR}/xpe-gui    # Work on GUI"
  echo "  4. Merge to main: git checkout main && git merge --squash dev/preprocess"
}

remove_worktrees() {
  echo ""
  echo "=========================================="
  echo " XPE Worktree Removal"
  echo "=========================================="
  echo ""

  cd "$REPO_ROOT"

  for lane in "${!LANES[@]}"; do
    IFS='|' read -r branch worktree_dir desc <<< "${LANES[$lane]}"
    local worktree_path="${PARENT_DIR}/${worktree_dir}"

    if ! git worktree list --porcelain | grep -q "$worktree_path" 2>/dev/null; then
      warn "[$lane] Worktree not found: $worktree_path (skipping)"
      continue
    fi

    info "[$lane] Removing worktree: $worktree_path"
    git worktree remove "$worktree_path" --force 2>/dev/null || {
      error "[$lane] Failed to remove worktree. Uncommitted changes may exist."
      error "  Manual: git worktree remove $worktree_path"
      continue
    }

    # Optionally delete the branch
    read -p "  Delete branch '$branch' too? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
      git branch -D "$branch" 2>/dev/null && info "  Deleted branch: $branch"
    else
      info "  Kept branch: $branch"
    fi
  done

  echo ""
  info "Remaining worktrees:"
  git worktree list
}

status_worktrees() {
  cd "$REPO_ROOT"
  echo ""
  echo "=========================================="
  echo " XPE Worktree Status"
  echo "=========================================="
  echo ""
  git worktree list
  echo ""
  for lane in "${!LANES[@]}"; do
    IFS='|' read -r branch worktree_dir desc <<< "${LANES[$lane]}"
    local worktree_path="${PARENT_DIR}/${worktree_dir}"

    if [[ -d "$worktree_path" ]]; then
      local wt_branch
      wt_branch=$(cd "$worktree_path" && git branch --show-current)
      local wt_status
      wt_status=$(cd "$worktree_path" && git status --short | head -5)
      local uncommitted
      uncommitted=$(cd "$worktree_path" && git status --short | wc -l)

      echo "[$lane] $worktree_dir (branch: $wt_branch)"
      echo "  Path: $worktree_path"
      echo "  Uncommitted changes: $uncommitted"
      if [[ -n "$wt_status" ]]; then
        echo "$wt_status" | head -5 | sed 's/^/  /'
      fi
      echo ""
    else
      echo "[$lane] $worktree_dir — NOT CREATED"
      echo ""
    fi
  done
}

# Main
case "${1:-status}" in
  --create|-c|create)
    create_worktrees
    ;;
  --remove|-r|remove)
    remove_worktrees
    ;;
  --status|-s|status)
    status_worktrees
    ;;
  *)
    echo "Usage: $0 [--create | --remove | --status]"
    echo ""
    echo "Commands:"
    echo "  --create, -c   Create all 3 lane worktrees"
    echo "  --remove, -r   Remove all worktrees (prompts for branch deletion)"
    echo "  --status, -s   Show worktree status (default)"
    exit 1
    ;;
esac
