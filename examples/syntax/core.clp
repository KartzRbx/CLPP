#pragma once

namespace Demo {
    type Id = int;
    using Name = string;

    template<typename T>
    struct Box {
        T value;
    };

    interface Named {
        string Name();
    };

    enum class Color : int {
        Red = 0x1,
        Green = 0x2,
    };

    template<typename T>
    T IdFn(T x) {
        return x;
    }
}

struct W {
    int x;
    W(int x) {}
};

W::W(int x) {}

struct Owner {
    void Method();
};

void Owner::Method() {}

class Account : Owner {
public:
    int balance;
private:
    int secret;
};

const int N = 1;

link "./PlayerData.clh" as Purse;
link "./Wallet.clp";
link @clpp.std;

[[pure]]
void marked() {}

signal<int> changed;

type Handler = function<void(int, string)>;
Foo<Bar<int>> nested;

async void fetch() {
    await ready;
}

void init() {
    int hex = 0xFF;
    float f = 1.5e-2;
    string s = "hi\n";
    string raw = R"(raw " text)";
    string t = `hello {name}`;
    post(`a`, b, `c`);
    if (hex) {
    } else if (f) {
    } else {
    }
    guard (hex) else return;
    while (hex) break;
    do {
        hex = hex;
    } while (hex);
    for (int i = 0; i < 3; i = i + 1) {}
    for (int i in 0..<10 by 2) {}
    switch (hex) {
        case 1:
            break;
        default:
            break;
    }
    match (hex) {
        ok => {},
        int n => {},
        _ => {},
    }
    try {
    } catch (string e) {
    }
    auto [a, b] = pair;
    auto { field } = obj;
    int z = a ?? b;
    int q = cond ? a : b;
    auto m = obj?.field;
    auto tried = maybe?;
    auto scoped = A::B;
    auto casted = static_cast<int>(f);
    int coins = @coins;
    n //= 2;
    comptime {
        const int folded = 1;
    }
}
