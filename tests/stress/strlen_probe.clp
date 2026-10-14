extern number strlen(string s);
const auto v = list(3, 1, 2);
const auto s = sort(v);
post(s[0]);
post(s[1]);
post(s[2]);
post(find(s, 2));
post(strlen("CL++"));
