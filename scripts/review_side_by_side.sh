#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT"

# Format:
# label | left file | right file | group
PAIRS=(
  "C: upstream Sign ↔ Algorithm-1 Base|ref/sign.c|ref/basesig.c|protocol"
  "C: Algorithm-1 Base ↔ Algorithm-2 LAS|ref/basesig.c|ref/las.c|protocol"

  "Rust: upstream ML-DSA ↔ Algorithm-1 Base|rust/fips204-las/src/ml_dsa.rs|rust/fips204-las/src/basesig.rs|protocol"
  "Rust: Algorithm-1 Base ↔ Algorithm-2 LAS|rust/fips204-las/src/basesig.rs|rust/fips204-las/src/las.rs|protocol"

  "C codec: upstream packing.c ↔ LAS serialize.c|ref/packing.c|ref/serialize.c|codec"
  "C codec API: upstream packing.h ↔ LAS serialize.h|ref/packing.h|ref/serialize.h|codec"

  "Rust codec: upstream encodings.rs ↔ LAS serialize.rs|rust/fips204-las/src/encodings.rs|rust/fips204-las/src/serialize.rs|codec"
  "Rust bit packing: upstream conversion.rs ↔ LAS serialize.rs|rust/fips204-las/src/conversion.rs|rust/fips204-las/src/serialize.rs|codec"

  "C: KeyGen arithmetic ↔ relation Gen|ref/basesig.c|ref/relation.c|relation"
  "Rust: KeyGen arithmetic ↔ relation Gen|rust/fips204-las/src/basesig.rs|rust/fips204-las/src/relation.rs|relation"

  "C ↔ Rust: shared Setup|ref/setup.h|rust/fips204-las/src/setup.rs|parity"
  "C ↔ Rust: Setup implementation|ref/setup.c|rust/fips204-las/src/setup.rs|parity"
  "C ↔ Rust: protocol types|ref/las_types.h|rust/fips204-las/src/las_types.rs|parity"
  "C ↔ Rust: relation layer|ref/relation.c|rust/fips204-las/src/relation.rs|parity"
  "C ↔ Rust: serialization layer|ref/serialize.c|rust/fips204-las/src/serialize.rs|parity"
  "C ↔ Rust: Algorithm-1 Base|ref/basesig.c|rust/fips204-las/src/basesig.rs|parity"
  "C ↔ Rust: Algorithm-2 LAS|ref/las.c|rust/fips204-las/src/las.rs|parity"
)

choose_tool() {
  if command -v code >/dev/null 2>&1; then
    TOOL="code"
  elif command -v meld >/dev/null 2>&1; then
    TOOL="meld"
  else
    TOOL="git"
  fi
}

open_pair() {
  local index="$1"
  local label left right group

  IFS='|' read -r label left right group <<< "${PAIRS[$index]}"

  echo
  echo "============================================================"
  echo "$label"
  echo "LEFT : $left"
  echo "RIGHT: $right"
  echo "============================================================"

  if [[ ! -f "$left" ]]; then
    echo "SKIP: missing $left"
    return
  fi

  if [[ ! -f "$right" ]]; then
    echo "SKIP: missing $right"
    return
  fi

  case "$TOOL" in
    code)
      code --reuse-window --diff "$left" "$right"
      ;;
    meld)
      meld "$left" "$right" >/dev/null 2>&1 &
      ;;
    git)
      git difftool --no-index -y -- "$left" "$right" || true
      ;;
  esac
}

open_group() {
  local requested_group="$1"
  local i label left right group

  for i in "${!PAIRS[@]}"; do
    IFS='|' read -r label left right group <<< "${PAIRS[$i]}"

    if [[ "$requested_group" == "all" || "$group" == "$requested_group" ]]; then
      open_pair "$i"
    fi
  done
}

show_menu() {
  local i label left right group

  echo
  echo "Side-by-side implementation review"
  echo "Diff tool: $TOOL"
  echo

  for i in "${!PAIRS[@]}"; do
    IFS='|' read -r label left right group <<< "${PAIRS[$i]}"
    printf "%2d) %s\n" "$((i + 1))" "$label"
  done

  echo
  echo " p) Protocol transformations"
  echo " c) Serialization/codec"
  echo " r) KeyGen ↔ relation Gen"
  echo " x) C ↔ Rust parity"
  echo " a) Open everything"
  echo " q) Quit"
  echo
}

choose_tool

case "${1:-menu}" in
  protocol)
    open_group "protocol"
    ;;
  codec)
    open_group "codec"
    ;;
  relation)
    open_group "relation"
    ;;
  parity)
    open_group "parity"
    ;;
  all)
    open_group "all"
    ;;
  menu)
    while true; do
      show_menu
      read -r -p "Selection: " selection

      case "$selection" in
        p) open_group "protocol" ;;
        c) open_group "codec" ;;
        r) open_group "relation" ;;
        x) open_group "parity" ;;
        a) open_group "all" ;;
        q) exit 0 ;;
        ''|*[!0-9]*)
          echo "Invalid selection."
          ;;
        *)
          index=$((selection - 1))
          if (( index >= 0 && index < ${#PAIRS[@]} )); then
            open_pair "$index"
          else
            echo "Invalid selection."
          fi
          ;;
      esac
    done
    ;;
  *)
    echo "Usage:"
    echo "  $0"
    echo "  $0 protocol"
    echo "  $0 codec"
    echo "  $0 relation"
    echo "  $0 parity"
    echo "  $0 all"
    exit 1
    ;;
esac