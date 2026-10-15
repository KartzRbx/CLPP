<< Line comments use two less-than characters.
<<[ Block comments use these delimiters.
They do not nest. ]>>
pragma strict;
number exact = 9_007_199_254_740_993;
float decimal = 7.0 / 2;
bool enabled = true;
string name = 'Ada';
constexpr number LIMIT = 2 * 3;
auto inferred = 0b1010 + 0x10;
decltype(exact) saved = 4;
post(exact);
post(decimal);
post(`${name}: ${enabled}`);
post(LIMIT);
post(inferred);
post(saved);
