import time
start = time.perf_counter()
total = 0
for i in range(1_000_000):
    total += i
print(f"sum: {total}  loop: {(time.perf_counter() - start) * 1000:.3f} ms")
