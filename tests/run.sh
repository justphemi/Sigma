#!/bin/sh
# Runs the Sigma test suite.
#   tests/cases/*.sgm   must print exactly what's in the matching .out file
#   tests/errors/*.sgm  must fail, with the text in the matching .err file
#                       appearing in the error message
cd "$(dirname "$0")/.." || exit 1
SIG="${SIG:-./build/sig}"
[ -x "$SIG" ] || { echo "build first: ./build.sh"; exit 1; }

pass=0
failed=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

for f in tests/cases/*.sgm; do
  want="${f%.sgm}.out"
  [ -f "$want" ] || continue  # helper files (yoinked by other tests)
  (cd tests/cases && "../../$SIG" "$(basename "$f")" < /dev/null > "$tmp/out" 2>&1)
  if diff -u "$want" "$tmp/out" > "$tmp/diff"; then
    pass=$((pass + 1))
  else
    failed=$((failed + 1))
    echo "FAIL $f"
    cat "$tmp/diff"
  fi
done

for f in tests/errors/*.sgm; do
  want=$(cat "${f%.sgm}.err")
  if "$SIG" check "$f" > "$tmp/out" 2>&1; then
    failed=$((failed + 1))
    echo "FAIL $f: expected a compile error, but it compiled"
  elif grep -qF -- "$want" "$tmp/out"; then
    pass=$((pass + 1))
  else
    failed=$((failed + 1))
    echo "FAIL $f: expected an error containing: $want"
    sed 's/^/    /' "$tmp/out"
  fi
done

echo "$pass passed, $failed failed"
[ "$failed" -eq 0 ]
