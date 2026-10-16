def Point {
  number x;
  number y;
}

enum class Color {
  Red,
  Green,
  Blue
}

Point p = Point { x = 3, y = 4 };
post(p.x + p.y);
post(Color::Green);
