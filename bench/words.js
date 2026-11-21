const start = process.hrtime.bigint();
const counts = new Map();
for (let i = 0; i < 200000; i++) { const key = "w" + (i % 1000); counts.set(key, (counts.get(key) || 0) + 1); }
const top = [...counts.keys()].sort((a, b) => counts.get(b) - counts.get(a)).slice(0, 3);
console.log(`keys: ${counts.size}  top: ${JSON.stringify(top)}  time: ${(Number(process.hrtime.bigint() - start) / 1e6).toFixed(3)} ms`);
