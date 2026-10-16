<< Server-side plausibility example, not a client memory scanner.
<< Production inputs must come from the authoritative server, with its timestamps.
array<string> Audit(array<number> sequences, array<number> times,
                    array<number> positions, number maxSpeed) {
  if (len(sequences) != len(times) || len(times) != len(positions)) {
    report("telemetry lengths differ");
  }
  array<string> alerts = array<string>();
  number index = 1;
  while (index < len(times)) {
    if (sequences[index] <= sequences[index - 1]) {
      push(alerts, "replay:" .: index);
    }
    number elapsed = times[index] - times[index - 1];
    number distance = positions[index] - positions[index - 1];
    if (distance < 0) { distance = -distance; }
    if (elapsed <= 0) { push(alerts, "clock:" .: index); }
    else if (distance > maxSpeed * elapsed) { push(alerts, "speed:" .: index); }
    index += 1;
  }
  return alerts;
}
array<number> sequences = { 1, 2, 2, 4, 5 };
array<number> times = { 0, 1, 2, 2, 3 };
array<number> positions = { 0, 3, 30, 31, 33 };
for (string alert in Audit(sequences, times, positions, 5)) { post(alert); }
post(len(Audit({ 1, 2, 3 }, { 0, 1, 2 }, { 0, 3, 7 }, 5)));
