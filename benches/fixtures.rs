// Fair-suite fixtures (RFC 0011). Compile-only stubs for Numeric / Data / Realistic.
// Runtime Studio timing is Cluaupp's job; these prove the compiler can emit the workloads.
// Note: `by` is a range keyword — do not use it as a parameter name.

/// Numeric hot loop — distance / damage style math.
pub const NUMERIC: &str = r#"
float Dist2(float ax, float ay, float az, float bx, float byy, float bz) {
    float dx = ax - bx;
    float dy = ay - byy;
    float dz = az - bz;
    return dx * dx + dy * dy + dz * dz;
}
float Damage(float base, float mult) {
    return base * mult;
}
float Hot(int n) {
    float acc = 0.0;
    for (int i = 0; i < n; i++) {
        acc = acc + Damage(Dist2(0.0, 0.0, 0.0, 1.0, 2.0, 3.0), 1.5);
    }
    return acc;
}
"#;

/// Data-oriented particle step (homogeneous floats — layoutHints should fire).
pub const DATA: &str = r#"
struct Particle {
    float px;
    float py;
    float pz;
    float vx;
    float vy;
    float vz;
    float life;
};
void Step(array<float> px, array<float> py, array<float> pz, array<float> vx, array<float> vy, array<float> vz, array<float> life, float dt, int n) {
    for (int i = 0; i < n; i++) {
        px[i] = px[i] + vx[i] * dt;
        py[i] = py[i] + vy[i] * dt;
        pz[i] = pz[i] + vz[i] * dt;
        life[i] = life[i] - dt;
    }
}
"#;

/// Realistic-ish NPC update without fake Roblox APIs beyond math.
pub const REALISTIC: &str = r#"
float PickTarget(float sx, float sy, float sz, float tx, float ty, float tz) {
    float dx = sx - tx;
    float dy = sy - ty;
    float dz = sz - tz;
    return dx * dx + dy * dy + dz * dz;
}
void UpdateNpc(int count, float sx, float sy, float sz) {
    for (int i = 0; i < count; i++) {
        float d = PickTarget(sx, sy, sz, 0.0, 0.0, 0.0);
        if (d < 100.0) {
            float hit = 10.0 * 1.25;
            post(hit);
        }
    }
}
"#;
