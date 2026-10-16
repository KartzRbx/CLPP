extern number strlen(string value);
post(strlen("abc"));
def Money { number Cents; }
Money operator+(Money left, Money right) { return Money { Cents = left.Cents + right.Cents }; }
Money operator-(Money left, Money right) { return Money { Cents = left.Cents - right.Cents }; }
post((Money { Cents = 7 } + Money { Cents = 5 }).Cents);
post((Money { Cents = 7 } - Money { Cents = 5 }).Cents);
string Take() { auto text = "local"; auto taken = move(text); return taken; }
post(Take());
any First(any ... values) { return values[0]; }
post(First(7, 8));
