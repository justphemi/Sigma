function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
const start = process.hrtime.bigint();
const r = fib(30);
console.log(`fib(30): ${r}  time: ${(Number(process.hrtime.bigint() - start) / 1e6).toFixed(3)} ms`);
