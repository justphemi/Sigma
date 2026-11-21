import time
start = time.perf_counter()
counts = {}
for i in range(200_000):
    key = "w" + str(i % 1000)
    counts[key] = counts.get(key, 0) + 1
top = sorted(counts, key=lambda k: counts[k], reverse=True)[:3]
print(f"keys: {len(counts)}  top: {top}  time: {(time.perf_counter() - start) * 1000:.3f} ms")
