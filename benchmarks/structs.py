class Animal:
    def __init__(self, legs): self.legs = legs
    def speed(self): return self.legs
class Dog(Animal):
    def __init__(self, legs, bones): super().__init__(legs); self.bones = bones
    def speed(self): return self.legs + self.bones
a = Dog(4, 2)
total = 0
for i in range(300000):
    total += a.speed()
print(total)
