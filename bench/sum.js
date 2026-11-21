const start = process.hrtime.bigint();
let total = 0;
for (let i = 0; i < 1_000_000; i++) total += i;
const ms = Number(process.hrtime.bigint() - start) / 1e6;
console.log(`sum: ${total}  loop: ${ms.toFixed(3)} ms`);
