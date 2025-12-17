````markdown
# Sigma Programming Language

![Sigma Logo](https://i.ibb.co/wNcYc2gs/svgviewer-png-output.png)

A blazingly fast compiled programming language with a clean, unique syntax

[Installation](#installation) • [Quick Start](#quick-start) • [Syntax](#syntax) • [Examples](#examples)

---

## What is Sigma?

Sigma is a compiled programming language that combines Python's simplicity with C's performance. It compiles directly to optimized C code, making it extremely fast even on low-end devices.

**Why Sigma?**

- Fast - Compiles to optimized C, runs at native speed  
- Simple - Clean syntax, easy to learn  
- Dynamic - Python-like dynamic typing  
- Minimal - One binary, no dependencies  
- Beautiful - Unique syntax that makes code readable  

---

## Installation

### macOS/Linux

```bash
git clone https://github.com/justphemi/Sigma.git
cd Sigma
chmod +x build.sh
./build.sh
````

### Windows (via Git Bash or WSL)

```bash
git clone https://github.com/justphemi/Sigma.git
cd Sigma
bash build.sh
```

Or, using PowerShell with MinGW installed:

```powershell
git clone https://github.com/justphemi/Sigma.git
cd Sigma
.\build.ps1
```

### Verify Installation

```bash
sig --version
```

### VS Code Extension (Optional)

```bash
code --install-extension sigma-icons-0.0.1.vsix
```

Or search for "Sigma" in the VS Code Extensions marketplace.

---

## Quick Start

Create `hello.sgm`:

```sigma
name: "Sigma"
yap("Hello, " + name + "!")
```

Run it:

```bash
sig hello.sgm
```

Output:

```
Hello, Sigma!
```

---

## Syntax

### Variables

```sigma
age: 45
weight: 61.835
name: "Christopher Jenkins"
isAdult: true
```

### Constants

```sigma
$fixed PI: 3.14159
$fixed APP_NAME: "Sigma"
$fixed MAX_USERS: 1000
```

### Functions

```sigma
fn add_numbers: (a, b) {
    return a + b
}

result: add_numbers.run(5, 3)
yap(result)
```

### Conditionals

```sigma
age: 20
$if age > 18 :: yap("Adult")
$el :: yap("Minor")
```

### Loops

```sigma
$for (i: 0, i < 10, i++) :: {
    yap(i)
}

counter: 0
$while counter < 5 :: {
    yap(counter)
    counter++
}
```

### Arrays

```sigma
numbers: [1, 2, 3, 4, 5]
yap(numbers[0])
numbers[0]: 10
sorted_asc: numbers.sort("asc")
sorted_desc: numbers.sort("desc")
```

### Objects

```sigma
player: {
    name:: "Peters",
    age:: 24,
    score:: 1500
}

yap(player.name)
player.score: 2000
```

### Error Handling

```sigma
$try :: {
    result: 100
    yap(result)
} catch(e) :: {
    yap("Error occurred")
}
```

### Comments

```sigma
-- Single line
---
Multi-line
comment
---
```

### Built-in Functions

```sigma
yap("Hello")
```

---

## Examples

### Array Manipulation

```sigma
numbers: [5, 2, 8, 1, 9, 3]
sorted: numbers.sort("asc")
reversed: numbers.sort("desc")
numbers[0]: 100
```

### Object Management

```sigma
user: {username:: "john_doe", email:: "john@example.com", age:: 28, verified:: true}
user.age: 29
user.verified: false
```

### Constants Protection

```sigma
$fixed MAX_SPEED: 120
$fixed APP_VERSION: "1.0.0"
```

### Fibonacci

```sigma
fn fibonacci: (n) {
    $if n < 2 :: return n
    return fibonacci.run(n - 1) + fibonacci.run(n - 2)
}

result: fibonacci.run(10)
yap(result)
```

### FizzBuzz

```sigma
$for (i: 1, i < 101, i++) :: {
    $if i % 15 == 0 :: yap("FizzBuzz")
    $el :: {
        $if i % 3 == 0 :: yap("Fizz")
        $el :: {
            $if i % 5 == 0 :: yap("Buzz")
            $el :: yap(i)
        }
    }
}
```

### Calculator with Error Handling

```sigma
fn divide: (a, b) {
    $if b == 0 :: {
        yap("Error: Division by zero")
        return 0
    }
    return a / b
}

$try :: {
    result: divide.run(10, 2)
    yap(result)
} catch(e) :: {
    yap("Calculation failed")
}
```

---

## Current Features

* Variables with dynamic typing
* Constants with `$fixed`
* Functions with parameters
* Conditionals (`$if`, `$el`)
* Loops (`$for`, `$while`)
* Arrays with indexing and updates
* Array sorting (`.sort("asc")`, `.sort("desc")`)
* Objects with property access
* Try-catch error handling
* Print function (`yap`)
* Recursion support

---

## Roadmap

* More array methods (`.push()`, `.pop()`, `.length()`, `.map()`, `.filter()`)
* String methods (`.length()`, `.upper()`, `.lower()`, `.split()`)
* File I/O operations
* Timing functions
* Import/module system
* Standard library
* Package manager
* Better error messages with line numbers

---

## Performance

Sigma compiles to C with `-O3` optimization:

```
Benchmark: Sum 1 million numbers
Sigma:  0.003s
Python: 0.089s
Node:   0.021s
```

---

## Language Design

Reserved keywords: `$if`, `$el`, `$for`, `$while`, `$fixed`, `$try`
Built-in functions: snake_case with `$` prefix
User functions: flexible naming

---

## Contributing

1. Fork the repo
2. Create a feature branch
3. Commit changes
4. Push branch
5. Open Pull Request

---

## Building from Source

Requirements: C++ compiler, CMake 3.10+, GCC

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

---

## Project Structure

```
sigma/
├── compiler/
├── include/
├── examples/
├── vscode-extension/
└── build.sh
```

---

## How It Works

```
.sgm → Lexer → Parser → AST → Code Generator → C → GCC → Binary
```

---

## FAQ

**Why Sigma?** Python simplicity + C performance
**Production-ready?** Early release v0.2
**Web development?** Not yet
**Difference from Go/Rust/Zig?** Simpler, faster to write

---

## License

MIT

---

## Community

* Discord (coming soon)
* Twitter (coming soon)
* Email: [hello@sigmalang.dev](mailto:hello@sigmalang.dev)

---

Made with HATRED by Phemi
Sigma - Write less, run faster

```
```
