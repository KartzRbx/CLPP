# 13. Eventos: `signal` e `observable`

Eventos desacoplam quem avisa de quem reage. O sistema de vida não precisa conhecer a interface, o som e o sistema de conquistas: ele só dispara "o jogador morreu", e cada interessado reage.

## `signal`

Um `signal` é um evento com nome. `~>Connect(callback)` registra um handler no zelador interno. `::Fire(...)` executa os handlers síncronos em ordem de inscrição e para no primeiro erro. `::Clear()` limpa as inscrições gerenciadas.

```clp
signal<number> playerDied;

void showGameOver(number score) { post("game over: " .: score); }
void saveScore(number score) { post("salvando " .: score); }

playerDied~>Connect(showGameOver);
playerDied~>Connect(saveScore);

playerDied::Fire(1200);
```

```saida
game over: 1200
salvando 1200
```

Um signal pode não levar valores:

```clp
signal<> levelStart;
void playMusic() { post("música"); }
void spawnEnemies() { post("inimigos"); }
levelStart~>Connect(playMusic);
levelStart~>Connect(spawnEnemies);
levelStart::Fire();
```

```saida
música
inimigos
```

As inscrições têm vida em execução; `::Connect` retorna uma conexão com `::Disconnect`, e `~>Connect` usa a limpeza interna do evento.

## `observable`

Uma variável `observable` avisa quando muda de valor. `.OnChange(função)` registra quem quer saber; a função recebe o novo valor. Funciona com funções anônimas (`func (number v) -> void { ... }`) e com funções nomeadas.

```clp
observable number coins = 10;
coins.OnChange(func (number value) -> void {
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
export void onDamage(number amount) { post("HUD: -" .: amount); }
```

```clp arquivo=audio.clp
export void onDamage(number amount) { post("som de impacto"); }
```

```clp
link "./hud.clp" as Hud;
link "./audio.clp" as Audio;

signal<number> damaged;
damaged~>Connect(Hud.onDamage);
damaged~>Connect(Audio.onDamage);

damaged::Fire(15);
```

```saida
HUD: -15
som de impacto
```
