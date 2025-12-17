# Sigma Programming Language

<p align="center">
  <img src="https://i.ibb.co/wNcYc2gs/svgviewer-png-output.png" width="200" alt="Sigma Logo">
</p>

<p align="center">
  <strong>A blazingly fast compiled programming language with a clean, unique syntax</strong>
</p>

<p align="center">
  <a href="#installation">Installation</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#syntax">Syntax</a> •
  <a href="#examples">Examples</a>
</p>

---

## What is Sigma?

Sigma is a compiled programming language that combines Python's simplicity with C's performance. It compiles directly to optimized C code, making it extremely fast even on low-end devices.

**Why Sigma?**
-  **Fast** - Compiles to optimized C, runs at native speed
-  **Simple** - Clean syntax, easy to learn
-  **Dynamic** - Python-like dynamic typing
-  **Minimal** - One binary, no dependencies
-  **Beautiful** - Unique syntax that makes code readable

## Installation

### macOS/Linux

```bash
git clone https://github.com/justphemi/Sigma.git
cd sigma-lang
chmod +x build.sh
./build.sh
```

### Verify Installation

```bash
sig --version
```

### VS Code Extension (Optional but Recommended)

Get syntax highlighting and file icons:

```bash
code --install-extension sigma-icons-0.0.1.vsix
```

Or search "Sigma" in VS Code Extensions marketplace.

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

### Variables (Dynamic Typing)

```sigma
-- Numbers
age: 45
weight: 61.835

-- Strings
name: "Christopher Jenkins"

-- Booleans
isAdult: true
```

### Functions

```sigma
fn add_numbers: (a, b) {
    return a + b
}

result: add_numbers.run(5, 3)
yap(result)  -- Prints: 8.00
```

### Conditionals

```sigma
age: 20

$if age > 18 :: yap("Adult")
$el :: yap("Minor")

-- Multi-line
$if age > 65 :: {
    yap("Senior citizen")
    yap("Eligible for benefits")
}
$el :: {
    yap("Working age")
}
```

### Loops

**For Loop:**
```sigma
$for (i: 0, i < 10, i++) :: {
    yap(i)
}
```

**While Loop:**
```sigma
counter: 0
$while counter < 5 :: {
    yap(counter)
    counter++
}
```

### Arrays

```sigma
numbers: [1, 2, 3, 4, 5]
yap(numbers[0])  -- Prints: 1.00

-- Update values
numbers[0]: 10
```

### Objects

```sigma
player: {
    name:: "Peters",
    age:: 24,
    score:: 1500
}

yap(player.name)   -- Prints: Peters
yap(player.score)  -- Prints: 1500.00

-- Update properties
player.score: 2000
```

### Comments

```sigma
-- Single line comment

---
Multi-line
comment
block
---
```

### Built-in Functions

```sigma
yap("Hello")           -- Print to console
$time_start()          -- Start timer (coming soon)
$time_end()            -- End timer (coming soon)
```

---

## Examples

### Fibonacci

```sigma
fn fibonacci: (n) {
    $if n < 2 :: return n
    return fibonacci.run(n - 1) + fibonacci.run(n - 2)
}

result: fibonacci.run(10)
yap("Fibonacci(10) = " + result)
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

### Calculator

```sigma
fn add: (a, b) { return a + b }
fn subtract: (a, b) { return a - b }
fn multiply: (a, b) { return a * b }
fn divide: (a, b) { return a / b }

x: 10
y: 5

yap("Add: " + add.run(x, y))
yap("Subtract: " + subtract.run(x, y))
yap("Multiply: " + multiply.run(x, y))
yap("Divide: " + divide.run(x, y))
```

---

## Current Features

✅ Variables with dynamic typing  
✅ Functions with parameters  
✅ Conditionals (`$if`, `$el`)  
✅ Loops (`$for`, `$while`)  
✅ Arrays (basic)  
✅ Objects (basic)  
✅ String concatenation  
✅ Arithmetic operations  
✅ Comments (single & multi-line)  
✅ Print function (`yap`)  

## Roadmap

🔜 Array methods (`.sort()`, `.push()`, `.pop()`)  
🔜 String methods (`.length()`, `.upper()`, `.lower()`)  
🔜 File I/O operations  
🔜 Timing functions (`$time_start`, `$time_end`)  
🔜 Standard library  
🔜 Package manager  
🔜 Error handling (`try`/`catch`)  

---

## Performance

Sigma compiles to C with `-O3` optimization, making it extremely fast:

```
Benchmark: Sum 1 million numbers
Sigma:  0.003s
Python: 0.089s
Node:   0.021s
```

Sigma is **30x faster than Python** and **7x faster than Node.js** for computational tasks.

---

## Language Design

**Reserved Keywords:**

All control flow keywords start with `$`:
- `$if`, `$el` - Conditionals
- `$for`, `$while` - Loops
- `$time_start`, `$time_end` - Timing
- `$set_timeout`, `$set_interval` - Async (planned)

**Built-in Functions:**

Use snake_case with `$` prefix:
- `$to_uppercase()`, `$to_lowercase()` (planned)
- `$parse_int()`, `$parse_float()` (planned)

**User Functions:**

Use any naming style:
```sigma
fn my_function: (param) { }
fn myFunction: (param) { }
fn MyFunction: (param) { }
```

---

## Contributing

Contributions welcome! Here's how:

1. Fork the repo
2. Create a feature branch: `git checkout -b my-feature`
3. Commit changes: `git commit -am 'Add feature'`
4. Push: `git push origin my-feature`
5. Open a Pull Request

---

## Building from Source

**Requirements:**
- C++ compiler (g++ or clang)
- CMake 3.10+
- GCC (for compiling generated C code)

**Build:**
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
├── compiler/          # Lexer, parser, code generator
│   ├── lexer.cpp
│   ├── parser.cpp
│   ├── codegen.cpp
│   └── main.cpp
├── include/           # Header files
├── examples/          # Example programs
├── vscode-extension/  # VS Code extension for icons
└── build.sh           # Build script
```

---

## How It Works

```
.sgm file → Lexer → Parser → AST → Code Generator → C code → GCC → Binary
```

1. **Lexer** breaks code into tokens
2. **Parser** builds Abstract Syntax Tree (AST)
3. **Code Generator** converts AST to C code
4. **GCC** compiles C to native binary with `-O3`

---

## FAQ

**Q: Why create another programming language?**  
A: To combine Python's simplicity with C's performance. Most languages force you to choose between ease-of-use and speed. Sigma gives you both.

**Q: Is Sigma production-ready?**  
A: Not yet. This is an early release (v0.1). Use for learning, prototyping, and experimentation.

**Q: Can I use Sigma for web development?**  
A: Not currently. Focus is on systems programming and CLI tools. Web support planned for v2.0.

**Q: How does Sigma compare to Go/Rust/Zig?**  
A: Sigma is simpler. It's designed for people who find Go too verbose, Rust too complex, and just want to write fast code quickly.

---

## License

MIT License - feel free to use Sigma in your projects!

---

## Community

- 💬 [Discord](https://discord.gg/sigmalang) (coming soon)
- 🐦 [Twitter](https://twitter.com/sigmalang) (coming soon)
- 📧 Email: hello@sigmalang.dev

---

<p align="center">
  Made with 💜 by developers who believe programming should be simple AND fast
</p>

<p align="center">
  <sub>Sigma - Write less, run faster</sub>
</p>
