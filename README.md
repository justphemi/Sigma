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
- 🚀 **Fast** - Compiles to optimized C, runs at native speed
- ✨ **Simple** - Clean syntax, easy to learn
- 🎯 **Dynamic** - Python-like dynamic typing
- 📦 **Minimal** - One binary, no dependencies
- 💎 **Beautiful** - Unique syntax that makes code readable

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

### Constants (Immutable)

```sigma
$fixed PI: 3.14159
$fixed APP_NAME: "Sigma"
$fixed MAX_USERS: 1000

-- PI: 3.14  -- ERROR: Cannot reassign constant variable
```

### Functions

```sigma
fn add_numbers: (a, b) {
    return a + b
}

result: add_numbers.run(5, 3)
yap(result)  -- Prints: 8
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
-- Create arrays
numbers: [1, 2, 3, 4, 5]
names: ["Alice", "Bob", "Charlie"]

-- Access elements
yap(numbers[0])  -- Prints: 1

-- Update elements
numbers[0]: 10
yap(numbers[0])  -- Prints: 10

-- Sort arrays
sorted_asc: numbers.sort("asc")   -- Ascending
sorted_desc: numbers.sort("desc")  -- Descending
yap(sorted_asc)
```

### Objects

```sigma
-- Create objects
player: {
    name:: "Peters",
    age:: 24,
    score:: 1500
}

-- Access properties
yap(player.name)   -- Prints: Peters
yap(player.score)  -- Prints: 1500

-- Update properties
player.score: 2000
player.age: 25
yap(player.score)  -- Prints: 2000
```

### Error Handling

```sigma
$try :: {
    result: 100
    yap("Operation successful")
    yap(result)
} catch(e) :: {
    yap("An error occurred!")
}
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
```

---

## Examples

### Array Manipulation

```sigma
-- Create and manipulate arrays
numbers: [5, 2, 8, 1, 9, 3]
yap(numbers)

-- Sort ascending
sorted: numbers.sort("asc")
yap(sorted)  -- [1, 2, 3, 5, 8, 9]

-- Sort descending
reversed: numbers.sort("desc")
yap(reversed)  -- [9, 8, 5, 3, 2, 1]

-- Access and modify
yap(numbers[0])
numbers[0]: 100
yap(numbers[0])
```

### Object Management

```sigma
-- Create a user profile
user: {
    username:: "john_doe",
    email:: "john@example.com",
    age:: 28,
    verified:: true
}

-- Display user info
yap(user.username)
yap(user.email)

-- Update user data
user.age: 29
user.verified: false
yap(user.age)
```

### Constants Protection

```sigma
-- Define constants
$fixed MAX_SPEED: 120
$fixed APP_VERSION: "1.0.0"
$fixed COLORS: ["red", "green", "blue"]

yap(MAX_SPEED)
yap(APP_VERSION)

-- This would cause an error:
-- MAX_SPEED: 150  -- ERROR: Cannot reassign constant
```

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
    yap("Result: " + result)
} catch(e) :: {
    yap("Calculation failed")
}
```

---

## Current Features

✅ Variables with dynamic typing  
✅ **Constants with `$fixed` keyword**  
✅ Functions with parameters  
✅ Conditionals (`$if`, `$el`)  
✅ Loops (`$for`, `$while`)  
✅ **Arrays with indexing and updates**  
✅ **Array sorting (`.sort("asc")`, `.sort("desc")`)**  
✅ **Objects with property access**  
✅ **Object property updates**  
✅ **Try-catch error handling**  
✅ String concatenation  
✅ Arithmetic operations  
✅ Comparison operators  
✅ Comments (single & multi-line)  
✅ Print function (`yap`)  
✅ Recursion support  

## Roadmap

🔜 More array methods (`.push()`, `.pop()`, `.length()`, `.map()`, `.filter()`)  
🔜 String methods (`.length()`, `.upper()`, `.lower()`, `.split()`)  
🔜 File I/O operations  
🔜 Timing functions (`$time_start`, `$time_end`)  
🔜 Import/module system  
🔜 Standard library  
🔜 Package manager  
🔜 Better error messages with line numbers  

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
- `$fixed` - Constants
- `$try` - Error handling
- `$time_start`, `$time_end` - Timing (planned)
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
│   ├── token.h
│   └── ast.h
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
3. **Code Generator** converts AST to C code with runtime support
4. **GCC** compiles C to native binary with `-O3`

---

## FAQ

**Q: Why create another programming language?**  
A: To combine Python's simplicity with C's performance. Most languages force you to choose between ease-of-use and speed. Sigma gives you both.

**Q: Is Sigma production-ready?**  
A: Not yet. This is an early release (v0.2). Use for learning, prototyping, and experimentation.

**Q: Can I use Sigma for web development?**  
A: Not currently. Focus is on systems programming and CLI tools. Web support planned for v2.0.

**Q: How does Sigma compare to Go/Rust/Zig?**  
A: Sigma is simpler. It's designed for people who find Go too verbose, Rust too complex, and just want to write fast code quickly.

**Q: What makes Sigma different?**  
A: Clean syntax with `$` prefixed keywords, `.run()` function calls, `::` for object properties, and compiles to optimized C for maximum performance.

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
  Made with HATRED by Phemi
</p>

<p align="center">
  <sub>Sigma - Write less, run faster</sub>
</p>