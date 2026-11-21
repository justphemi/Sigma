import time
def fib(n):
    return n if n < 2 else fib(n - 1) + fib(n - 2)
start = time.perf_counter()
r = fib(30)
print(f"fib(30): {r}  time: {(time.perf_counter() - start) * 1000:.3f} ms")
