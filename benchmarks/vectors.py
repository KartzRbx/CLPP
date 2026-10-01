class V:
    __slots__ = ("x", "y", "z")
    def __init__(self, x, y, z): self.x, self.y, self.z = x, y, z
    def __add__(self, o): return V(self.x + o.x, self.y + o.y, self.z + o.z)
    def __mul__(self, k): return V(self.x * k, self.y * k, self.z * k)
def step(p, v): return p + (v * 0.016)
p = V(0.0, 10.0, 0.0)
v = V(1.0, 2.0, 3.0)
for i in range(200000):
    p = step(p, v)
print(p.x)
