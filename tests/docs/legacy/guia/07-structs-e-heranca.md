# 7. Structs e herança

Um `struct` agrupa dados (campos) e comportamento (métodos). É o bloco básico para modelar entidades de jogo: jogadores, inimigos, itens, projéteis.

## Declarar e construir

```clp
struct Player {
  string name;
  int hp;
  Vector3 position;
}

Player hero = Player("Ada", 100, Vector3(0, 0, 0));
post(hero.name);
post(hero.hp);
```

```saida
Ada
100
```

O construtor recebe os campos na ordem em que foram declarados. Também dá para nomeá-los, em qualquer ordem:

```clp
struct Item { string name; int price; int weight; }
Item sword = Item(price: 150, name: "espada", weight: 3);
post(sword.name .: " custa " .: sword.price);
```

```saida
espada custa 150
```

`new Player(...)` é aceito como sinônimo, para quem vem de C++ ou C#.

## Alterar campos

Campos de uma variável mutável podem ser alterados diretamente, inclusive campos aninhados, componentes de vetores e formas compostas.

```clp
struct Stats { int hp; int mana; }
struct Hero { string name; Stats stats; Vector3 position; }

Hero h = Hero("Ada", Stats(100, 50), Vector3(0, 0, 0));
h.name = "Bea";
h.stats.hp -= 30;
h.stats.mana += 5;
h.position.y = 4.5;
post(h.name .: " " .: h.stats.hp .: " " .: h.stats.mana .: " " .: h.position);
```

```saida
Bea 70 55 (0, 4.5, 0)
```

Uma variável declarada com `let` (sem `mut`) não pode ter campos alterados.

## Métodos

Funções declaradas dentro do struct. Dentro delas, `self` é o objeto em que o método foi chamado. `@campo` é um atalho para `self.campo` (e `@this.campo` também funciona).

```clp
struct Player {
  string name;
  int hp;

  func isAlive() -> bool { return self.hp > 0; }
  func describe() -> string { return @name .: " (" .: @hp .: " hp)"; }
}

Player p = Player("Ada", 80);
post(p.describe());
post(p.isAlive());
```

```saida
Ada (80 hp)
true
```

### Métodos que alteram o objeto

Um método que muda `self` atualiza a variável em que foi chamado.

```clp
struct Player {
  int hp;
  int maxHp;

  func damage(int amount) -> int {
    @hp = @hp - amount;
    if (@hp < 0) { @hp = 0; }
    return @hp;
  }
  func heal(int amount) {
    self.hp = self.hp + amount;
    if (self.hp > self.maxHp) { self.hp = self.maxHp; }
  }
}

Player p = Player(100, 100);
p.damage(30);
p.heal(10);
post(p.hp);
post(p.damage(500));
```

```saida
80
0
```

Structs são valores (capítulo 3): passar `p` para uma função comum entrega uma cópia. Para alterar o original, chame um método nele ou atribua o resultado de volta (`p = withBonus(p);`).

## Herança

`struct Filho : Pai` herda todos os campos e métodos do pai. O construtor do filho recebe primeiro os campos do pai e depois os próprios.

```clp
struct Entity {
  string name;
  int hp;
  func describe() -> string { return @name .: " hp=" .: @hp; }
}

struct Enemy : Entity {
  int damage;
  func attack() -> string { return @name .: " causa " .: @damage; }
}

Enemy orc = Enemy("Orc", 60, 12);
post(orc.describe());     << herdado de Entity
post(orc.attack());
post(orc.hp);
```

```saida
Orc hp=60
Orc causa 12
60
```

O editor mostra os membros herdados na lista de sugestões, com "(herdado de Entity)" ao lado.

### Polimorfismo

Um método redefinido no filho substitui o do pai, mesmo quando o objeto é visto pelo tipo do pai. A escolha do método acontece na execução (despacho dinâmico).

```clp
struct Animal {
  string name;
  func sound() -> string { return "..."; }
}
struct Dog : Animal {
  func sound() -> string { return "au"; }
}
struct Cat : Animal {
  func sound() -> string { return "miau"; }
}

Animal a = Dog("Rex");
Animal b = Cat("Mia");
post(a.name .: ": " .: a.sound());
post(b.name .: ": " .: b.sound());
```

```saida
Rex: au
Mia: miau
```

### `abstract`, `override` e `final`

| Palavra | Onde | Efeito |
| --- | --- | --- |
| `abstract struct` | struct | não pode ser construído diretamente, só herdado |
| `abstract func` | método | o filho é obrigado a implementar |
| `override func` | método | declara que substitui um método do pai (erro se o pai não tiver) |
| `final func` | método | proíbe que filhos o substituam |
| `final struct` | struct | proíbe que seja herdado |

```clp
abstract struct Shape {
  abstract func area() -> float {}
}
struct Square : Shape {
  float side;
  override func area() -> float { return self.side * self.side; }
}
Square s = Square(3);
post(s.area());
```

```saida
9
```

```clp erro
abstract struct Shape { abstract func area() -> float {} }
Shape s = Shape();
```

```saida
abstract type
```

### Herança múltipla

Um struct pode herdar de vários pais. Se dois pais herdarem do mesmo avô (o "losango"), os campos do avô aparecem uma vez só.

```clp
struct Base { int health; }
struct Armored : Base { int armor; }
struct Fast : Base { int speed; }
struct Knight : Armored, Fast { int honor; }

Knight k = Knight(100, 20, 5, 9);
post(k.health);
post(k.armor .: " " .: k.speed .: " " .: k.honor);
```

```saida
100
20 5 9
```

Dois pais com um campo de mesmo nome, vindo de origens diferentes, é erro: o compilador pede que você resolva a ambiguidade.

## Campos privados

`private` restringe o acesso aos métodos do próprio struct.

```clp erro
struct Account {
  private int pin;
  func check(int guess) -> bool { return guess == self.pin; }
}
Account acc = Account(1234);
post(acc.pin);
```

```saida
private member
```

## Structs genéricos

```clp
struct Box<T> { T item; }
Box<int> coins = Box<int>(25);
Box<string> note = Box<string>("mapa");
post(coins.item);
post(note.item);
```

```saida
25
mapa
```

## Listas de structs

Para acessar campos de elementos, declare a lista com o tipo do elemento (`array<Stats>`), assim o compilador sabe o que há dentro.

```clp
struct Stats { int hp; }
array<Stats> team = array<Stats>(Stats(10), Stats(20));
team[1].hp = 99;
post(team[0].hp .: " " .: team[1].hp);
```

```saida
10 99
```

## Mostrar um struct

`post` mostra os campos entre parênteses.

```clp
struct Point { int x; int y; }
post(Point(3, 4));
```

```saida
(3, 4)
```
