# 7. Structs e herança

Um `struct` agrupa dados (campos) e comportamento (métodos). É o bloco básico para modelar entidades de jogo: jogadores, inimigos, itens, projéteis.

## Declarar e construir

```clp
def Player {
  string name;
  number hp;
  Vector3 position;
}

Player hero = Player { name = "Ada", hp = 100, position = Vector3(0, 0, 0) };
post(hero.name);
post(hero.hp);
```

```saida
Ada
100
```

O construtor recebe os campos na ordem em que foram declarados. Também dá para nomeá-los, em qualquer ordem:

```clp
def Item { string name; number price; number weight; }
Item sword = Item { price=150, name="espada", weight=3 };
post(sword.name .: " custa " .: sword.price);
```

```saida
espada custa 150
```

`new Player(...)` é aceito como sinônimo, para quem vem de C++ ou C#.

## Alterar campos

Campos de uma variável mutável podem ser alterados diretamente, inclusive campos aninhados, componentes de vetores e formas compostas.

```clp
def Stats { number hp; number mana; }
def Hero { string name; Stats stats; Vector3 position; }

Hero h = Hero { name = "Ada", stats = Stats { hp = 100, mana = 50 }, position = Vector3(0, 0, 0) };
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

Funções declaradas dentro de `def` usam `@this` como receptor. Acesse campos com `@this.campo`; não há receptor implícito chamado `self` nem atalho `@campo`.

```clp
def Player {
  string name;
  number hp;

  bool isAlive() { return @this.hp > 0; }
  string describe() { return @this.name .: " (" .: @this.hp .: " hp)"; }
}

Player p = Player { name = "Ada", hp = 80 };
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
def Player {
  number hp;
  number maxHp;

  number damage(number amount) {
    @this.hp = @this.hp - amount;
    if (@this.hp < 0) { @this.hp = 0; }
    return @this.hp;
  }
  void heal(number amount) {
    @this.hp = @this.hp + amount;
    if (@this.hp > @this.maxHp) { @this.hp = @this.maxHp; }
  }
}

Player p = Player { hp = 100, maxHp = 100 };
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
def Entity {
  string name;
  number hp;
  string describe() { return @this.name .: " hp=" .: @this.hp; }
}

def Enemy : Entity {
  number damage;
  string attack() { return @this.name .: " causa " .: @this.damage; }
}

Enemy orc = Enemy { name = "Orc", hp = 60, damage = 12 };
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
def Animal {
  string name;
  string sound() { return "..."; }
}
def Dog : Animal {
  string sound() { return "au"; }
}
def Cat : Animal {
  string sound() { return "miau"; }
}

Animal a = Dog { name = "Rex" };
Animal b = Cat { name = "Mia" };
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
abstract def Shape {
  abstract float area() {}
}
def Square : Shape {
  float side;
  override float area() { return @this.side * @this.side; }
}
Square s = Square { side = 3 };
post(s.area());
```

```saida
9
```

```clp erro
abstract def Shape { abstract float area() {} }
Shape s = Shape {  };
```

```saida
abstract type
```

### Herança múltipla

Uma definição pode herdar de uma base com armazenamento e de contratos abstratos sem campos. Um losango de contratos pode repetir a mesma especialização, mas duas bases com armazenamento são rejeitadas.

```clp
def Base { number health; }
def Armored : Base { number armor; }
abstract def Fast { abstract number Speed(); }
def Knight : Armored, Fast { number speed; number honor; number Speed() { return @this.speed; } }

Knight k = Knight { health = 100, armor = 20, speed = 5, honor = 9 };
post(k.health);
post(k.armor .: " " .: k.speed .: " " .: k.honor);
```

```saida
100
20 5 9
```

Os campos pertencem à única base com armazenamento ou à própria definição. Cada contrato exige uma implementação compatível de seus métodos; não adiciona campos ao objeto.

## Campos privados

`private` restringe o acesso aos métodos do próprio struct.

```clp erro
def Account {
  private number pin;
  bool check(number guess) { return guess == @this.pin; }
}
Account acc = Account { pin = 1234 };
post(acc.pin);
```

```saida
private member
```

## Structs genéricos

```clp
def Box<T> { T item; }
Box<number> coins = Box<number> { item = 25 };
Box<string> note = Box<string> { item = "mapa" };
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
def Stats { number hp; }
array<Stats> team = array<Stats>(Stats { hp = 10 }, Stats { hp = 20 });
team[1].hp = 99;
post(team[0].hp .: " " .: team[1].hp);
```

```saida
10 99
```

## Mostrar um struct

`post` mostra os campos entre parênteses.

```clp
def Point { number x; number y; }
post(Point { x = 3, y = 4 });
```

```saida
(3, 4)
```
