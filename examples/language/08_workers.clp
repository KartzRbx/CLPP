atomic count = 0;
auto gate = mutex();
number Increment(number cell, number gateHandle) {
  lock(gateHandle); fetch_add(cell, 1); unlock(gateHandle); return 1;
}
auto first = thread Increment(count, gate);
auto second = thread Increment(count, gate);
post(join(first) + join(second));
post(atomic_load(count));
number Double(number value) { return value + value; }
post(actor(Double, 21));
number Step() { coroutine.yield(1); return 2; }
auto sequence = coroutine.create(Step);
post(coroutine.resume(sequence));
post(coroutine.resume(sequence));
