<< Matemática vetorial de jogo: integra 200.000 passos de física
func step(Vector3 p, Vector3 v) {
  return p + (v * 0.016);
}
Vector3 p = Vector3(0.0, 10.0, 0.0);
Vector3 v = Vector3(1.0, 2.0, 3.0);
for (let mut i = 0; i < 200000; i += 1) {
  p = step(p, v);
}
post(p.x);
