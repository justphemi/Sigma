# Sigma Programming Language

<p align="center">
  <img src="https://i.ibb.co/wNcYc2gs/svgviewer-png-output.png" width="200" alt="Sigma Logo">
</p>

<p align="center">
  <strong>A compiled language with a clean, Gen-Z syntax and C-level speed</strong>
</p>

<p align="center">
  <a href="#install">Install</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#the-language">The Language</a> •
  <a href="#performance">Performance</a> •
  <a href="#upgrading-from-sigma-01">Upgrading from 0.1</a>
</p>

---

```sigma
cook fib(n) {
    fr n < 2 { serve n }
    serve fib(n - 1) + fib(n - 2)
}

squad: ["Ana", "Ben", "Cy"]
grind i, name in squad {
    yap("{i + 1}. {name} says fib(20) = {fib(20)}")
}
```

## What is Sigma?

Sigma is a dynamically typed language that compiles to optimized native code
(via C). You write it like Python or JavaScript, with no type annotations, and
the compiler **infers types**. Anything it can prove is always a number
becomes a plain machine `double`, so number crunching runs at the same speed
as hand-written C.

- **Fast**: number-heavy code compiles to the same C you'd write by hand
- **Clean**: no semicolons, no `$`, no boilerplate, and string interpolation built in
- **Friendly errors**: errors show the line, a caret and a hint ("did you mean `total`?")
- **One binary**: `sig` compiles and runs, and caches builds so re-runs start instantly
- **Batteries included**: lists, maps, lambdas, try/catch, file I/O, imports

## Install

You need a C compiler (clang or gcc). macOS already has one once you run
`xcode-select --install`; on Linux, install `build-essential` or `gcc`.

```bash
git clone https://github.com/justphemi/Sigma.git
cd Sigma
./build.sh            # builds ./build/sig
./build.sh install    # optional: copies sig to /usr/local/bin
```

Check it worked:

```bash
sig version
```

**VS Code:** syntax highlighting lives in `sigma-icons/` (grammar: `syntaxes/sigma.tmLanguage.json`).

## Quick Start

Create `hello.sgm`:

```sigma
name: "Sigma"
yap("Hello, {name}!")
```

Run it:

```bash
sig hello.sgm
```

```
Hello, Sigma!
```

The first run compiles (about 1 second). Later runs of the same code come straight
from the cache (`~/.cache/sigma`).

## The Language

### Cheat sheet

| Sigma | Means | Example |
|---|---|---|
| `x: 5` | assign | `name: "Ada"` |
| `yap(...)` | print | `yap("hi", name)` |
| `ask(prompt)` | read a line of input | `name: ask("name? ")` |
| `fr` / `nah fr` / `nah` | if / else if / else | `fr x > 1 { ... } nah { ... }` |
| `grind` | every kind of loop | `grind i in 0..10 { ... }` |
| `dip` / `skip` | break / continue | `fr done { dip }` |
| `cook` | define a function | `cook add(a, b) { serve a + b }` |
| `serve` | return | `serve result` |
| `x => ...` | lambda | `nums.map(x => x * 2)` |
| `lock` | constant | `lock PI: 3.14159` |
| `yolo` / `oops` | try / catch | `yolo { ... } oops e { ... }` |
| `yeet` | throw | `yeet "something broke"` |
| `yoink` | import a file | `yoink "utils.sgm"` |
| `vibecheck(cond, msg)` | assert | `vibecheck(age >= 0, "bad age")` |
| `true` / `false` (or `nocap` / `cap`) | booleans | `ready: nocap` |
| `nil` | nothing | `fr x == nil { ... }` |
| `--` / `--- ... ---` | comments | `-- note to self` |

### Variables

```sigma
age: 45
weight: 61.835
name: "Christopher"
online: true

age += 1          -- also -=, *=, /=, %=
age++
```

A variable belongs to the function it's assigned in. Blocks don't hide it, so
a value set inside `fr` or `grind` is still there afterwards. Top-level
variables are globals that functions can read and update.

```sigma
lock MAX_USERS: 1000
MAX_USERS: 5      -- compile error: 'MAX_USERS' is locked
```

### Strings

```sigma
name: "Sigma"
yap("hello {name}, 2 + 2 = {2 + 2}")   -- interpolation: any expression in { }
yap('single quotes work too')
yap("literal braces: \{ \}")

s: "  Hello, World  "
s.trim()  s.upper()  s.lower()  s.split(",")  s.replace("l", "L")
s.has("World")  s.find("o")  s.starts("He")  s.ends("!")  s.slice(0, 5)
s.repeat(3)  s.reverse()  s.chars()  len(s)  s[0]  s[-1]
```

### Conditions

```sigma
fr score >= 90 {
    yap("A")
} nah fr score >= 80 {
    yap("B")
} nah {
    yap("keep grinding")
}

fr age >= 18 and not banned { yap("welcome") }   -- and / or / not  (or && || !)
```

### Loops

`grind` covers every kind of loop:

```sigma
grind i in 0..5 { yap(i) }               -- 0 1 2 3 4
grind i in 1..=5 { yap(i) }              -- 1 2 3 4 5
grind name in ["a", "b"] { yap(name) }   -- items of a list
grind i, name in ["a", "b"] { ... }      -- index and item
grind key, value in player { ... }       -- map entries
grind ch in "hey" { ... }                -- characters
grind count < 10 { count++ }             -- while
grind { fr done() { dip } }              -- forever, until dip
```

### Functions

```sigma
cook add(a, b) {
    serve a + b
}
yap(add(2, 3))

-- lambdas
double: x => x * 2
pair: (a, b) => a + b
check: (n) => {
    fr n > 0 { serve "positive" }
    serve "not positive"
}

-- closures
cook multiplier(k) {
    serve x => x * k
}
triple: multiplier(3)
yap(triple(7))   -- 21
```

Functions can be cooked anywhere at the top level and called before they're
defined. A lambda gets a *copy* of the outer variables it uses. To share
changing state, keep it in a list or map.

### Lists

```sigma
nums: [5, 2, 8, 1]
nums.push(9)          nums.pop()          nums.insert(0, 7)    nums.del(0)
nums[0]               nums[-1]            nums[0]: 100         len(nums)
nums.has(8)           nums.find(8)        nums.slice(1, 3)     nums + [10]
nums.sort()           nums.sort("desc")   nums.reverse()       nums.join(", ")
nums.map(x => x * 2)  nums.filter(x => x > 2)   nums.reduce((a, x) => a + x, 0)
nums.sum()            min(nums)           max(nums)            nums.copy()

people.sort(p => p.age)            -- sort by a key
people.sort(p => p.score, "desc")
```

`sort`, `reverse`, `map`, `filter` and `slice` return new lists. `push`, `pop`,
`insert`, `del` and `clear` change the list in place.

### Maps

```sigma
player: {name: "Peters", age: 24, score: 1500}
yap(player.name, player["score"])
player.score: 2000
player.score += 50
player["level"]: 3
player.missing          -- nil

player.keys()  player.values()  player.has("age")  player.del("age")
player.get("rank", "unranked")   len(player)

dog: {name: "Rex", speak: () => "woof"}
yap(dog.speak())
```

### Errors

```sigma
cook divide(a, b) {
    fr b == 0 { yeet "can't divide by zero" }
    serve a / b
}

yolo {
    yap(divide(1, 0))
} oops e {
    yap("caught: {e}")
}

yolo { risky() }      -- no oops: ignore the error and keep going
```

Runtime errors (bad index, math on a string, division by zero, ...) are
catchable too. If nothing catches them, the program stops with the message and
the line:

```
error: index 5 is out of range (the list has 2 items)
  --> game.sgm:12
```

### Imports

```sigma
-- utils.sgm
cook shout(s) { serve s.upper() + "!" }

-- main.sgm
yoink "utils.sgm"
yap(shout("yo"))
```

Paths are relative to the importing file, and each file is only included once.

### Built-in functions

| Function | Does |
|---|---|
| `yap(a, b, ...)` | print values separated by spaces |
| `ask(prompt?)` | read a line from input (`nil` at end of input) |
| `len(x)` | length of a string, list or map |
| `type(x)` | `"num"`, `"str"`, `"bool"`, `"list"`, `"map"`, `"fn"` or `"nil"` |
| `num(x)` / `int(x)` / `str(x)` | convert (`num("abc")` is `nil`) |
| `abs` `floor` `ceil` `round` `sqrt` `pow(a, b)` | math |
| `min(...)` / `max(...)` | of several values or a list |
| `rand()` / `rand_int(a, b)` | random number in [0, 1) / whole number in [a, b] |
| `now()` / `sleep(s)` | seconds (high resolution) / pause |
| `args()` | command-line arguments as a list |
| `read_file(p)` `write_file(p, s)` `append_file(p, s)` `file_exists(p)` | files |
| `vibecheck(cond, msg?)` | assert: fail with `msg` if `cond` is falsy |
| `exit(code?)` | stop the program |

### Operators

`+ - * / % **` (`%` follows the divisor's sign, so `-7 % 3 == 2`), comparisons
`== != < > <= >=`, logic `and or not`, ranges `a..b` and `a..=b`. `+` also joins
strings (`"n=" + 5`) and lists. `==` compares lists and maps by value.

**Truthiness:** `false`, `nil`, `0`, `""`, `[]` and `{}` are falsy; everything else is truthy.

**Line rules:** each statement goes on its own line (or use `;`). Inside `( )`,
`[ ]` and `{ }` maps you can break lines anywhere. Outside them, keep a binary
operator on the same line as its left side.

## The `sig` command

```
sig <file.sgm> [args...]        compile (cached) and run
sig build <file.sgm> [-o name]  make a standalone native executable
sig check <file.sgm>            check for errors without running
sig emit <file.sgm>             print the generated C code
sig clean                       delete the compile cache
sig version | sig help
```

Environment: `SIGMA_CC` picks the C compiler (default `$CC`, then `cc`), and
`SIGMA_CACHE` sets the cache directory.

## Performance

`python3 bench/run.py` runs every benchmark in `bench/` against Node.js,
Python and plain C. "Work" is the time measured inside the program around
the benchmark (best of 20). "Total" is the whole process, startup included.

| Benchmark | Sigma | Node.js 26 | Python 3.11 | C (`-O2`) |
|---|---|---|---|---|
| Sum of 1 million numbers (work) | **1.5 ms** | 8.8 ms | 150 ms | 1.5 ms |
| Sum of 1 million numbers (total) | **15 ms** | 129 ms | 182 ms | 7 ms |
| Recursive `fib(30)` (work) | **12.5 ms** | 15.5 ms | 232 ms | |
| 200k string keys into a map + sort (work) | **27 ms** | 37 ms | 124 ms | |

<sub>Measured on an Intel Core i7-8750H laptop, macOS 15.7, Apple clang 17.
Run `bench/run.py` to get numbers for your own machine. Laptop CPUs boost
and throttle, so expect some run-to-run variance (fib ran in 5.6 ms in quieter runs).</sub>

**Why it's fast**

- **Type inference.** Sigma works out which variables, parameters and return
  values are always numbers (or always booleans) and compiles them to raw C
  `double`/`bool`. The sum loop above compiles to
  `for (...) v_total = v_total + v_i;`, which is exactly what the C version does.
- **No interpreter.** Your code becomes C, which clang or gcc optimizes with `-O2`.
- **Cheap dynamic values.** Everything else is a 16-byte tagged value, with
  inline fast paths for common cases, bump-pointer allocation, hash maps with
  cached string hashes, and in-place string appends (so `s += x` in a loop
  isn't quadratic).
- **Compile cache.** A program that hasn't changed doesn't get recompiled.

What's *not* fast yet: allocation-heavy code is limited by memory, because
there's no garbage collector (see below).

## Upgrading from Sigma 0.1

Sigma 0.2 replaces the `$` keywords and `::` with a cleaner syntax. The
compiler recognises the old forms and tells you what to write instead:

| 0.1 | 0.2 |
|---|---|
| `$if x > 1 :: { ... }` | `fr x > 1 { ... }` |
| `$el :: { ... }` | `nah { ... }` |
| `$for (i: 0, i < 10, i++) :: { ... }` | `grind i in 0..10 { ... }` |
| `$while x < 5 :: { ... }` | `grind x < 5 { ... }` |
| `fn add: (a, b) { return a + b }` | `cook add(a, b) { serve a + b }` |
| `add.run(1, 2)` | `add(1, 2)` |
| `$fixed PI: 3.14` | `lock PI: 3.14` |
| `$try :: { } catch(e) :: { }` | `yolo { } oops e { }` |
| `$in name: "prompt"` | `name: ask("prompt")` |
| `{ name:: "x" }` | `{name: "x"}` |
| `check_type` `to_int` `to_dec` `to_str` | `type` `int` `num` `str` |
| `random(digits)` `random_range(a, b)` | `rand_int(a, b)` |

Fixed in 0.2: reassigning a variable inside a loop (`total: total + i`)
used to create a new variable, so totals stayed at 0. `%` didn't parse,
`$fixed` constants failed to compile, try/catch never caught anything, and
`sig --version` didn't work.

## Limitations (honest list)

- **No garbage collector yet.** Memory is never freed while a program runs.
  That's fine for scripts, CLI tools, puzzles and benchmarks, but not for
  long-running servers.
- Strings are bytes: `len("🔥")` is 4, and indexing works on bytes.
- Lambdas capture outer variables by value (read-only).
- A function that reads a top-level variable before the top-level code has
  assigned it sees `nil` (or `0` for numbers).
- Needs a C compiler on the machine (only to *build* programs; executables
  made with `sig build` run anywhere with the same OS and CPU type).
- macOS and Linux only for now.

## Roadmap

- Garbage collection
- Standard library modules (JSON, HTTP, dates, regex)
- Structs/classes with methods
- Package manager
- Language server for editor autocompletion

## License

MIT License - feel free to use Sigma in your projects!

---

<p align="center">
  Made with HATRED by Phemi
</p>

<p align="center">
  <sub>Sigma - Write less, run faster</sub>
</p>
