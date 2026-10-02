# 13. Eventos: `signal` e `observable`

Eventos desacoplam quem avisa de quem reage. O sistema de vida não precisa conhecer a interface, o som e o sistema de conquistas: ele só dispara "o jogador morreu", e cada interessado reage.

## `signal`

Um `signal` é um evento com nome. `~>` conecta uma função a ele. Chamar o signal como função dispara todas as conectadas, na ordem em que foram conectadas.

```clp
signal playerDied;

func showGameOver(int score) { post("game over: " .: score); }
func saveScore(int score) { post("salvando " .: score); }

playerDied ~> showGameOver;
playerDied ~> saveScore;

playerDied(1200);
```

```saida
game over: 1200
salvando 1200
```

Um signal pode não levar valores:

```clp
signal levelStart;
func playMusic() { post("música"); }
func spawnEnemies() { post("inimigos"); }
levelStart ~> playMusic;
levelStart ~> spawnEnemies;
levelStart();
```

```saida
música
inimigos
```

As conexões são resolvidas na compilação: disparar um signal custa o mesmo que chamar as funções diretamente.

## `observable`

Uma variável `observable` avisa quando muda de valor. `.OnChange(função)` registra quem quer saber; a função recebe o novo valor. Funciona com funções anônimas (`func (int v) { ... }`) e com funções nomeadas.

```clp
observable int coins = 10;
coins.OnChange(func (int value) {
  post("HUD: " .: value .: " moedas");
});

coins = 25;
coins = coins + 5;
post(coins);
```

```saida
HUD: 25 moedas
HUD: 30 moedas
30
```

O aviso acontece a cada atribuição, logo depois de o valor ser gravado.

## Padrão: eventos entre módulos

Declare as funções que reagem em módulos separados e conecte no arquivo principal. Assim cada módulo fica independente:

```clp arquivo=hud.clp
func onDamage(int amount) { post("HUD: -" .: amount); }
```

```clp arquivo=audio.clp
func onDamage(int amount) { post("som de impacto"); }
```

```clp
link "./hud.clp" as Hud;
link "./audio.clp" as Audio;

signal damaged;
damaged ~> Hud.onDamage;
damaged ~> Audio.onDamage;

damaged(15);
```

```saida
HUD: -15
som de impacto
```
