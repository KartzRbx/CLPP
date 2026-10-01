struct Point {
  int x;
  int y;
}

enum Color {
  Red,
  Green,
  Blue
}

Point p = Point(3, 4);
post(p.x + p.y);
post(Color.Green);
