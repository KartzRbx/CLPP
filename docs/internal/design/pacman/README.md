# Pacman: referência de design enviada pelo autor

Os seis arquivos nesta pasta preservam os exemplos originais e as expectativas de diagnósticos e de emissão. São material de design, não exemplos executáveis do compilador atual nem testes aprovados.

## Decisão atual: módulos sem headers

CL++ não terá headers. Cada arquivo `.clp` é um módulo. O `Entities.clh` do anexo é mantido apenas para rastrear a proposta original; sua organização como header e o export automático por extensão foram descartados. Uma evolução dos tipos e de sua visibilidade deverá usar os próprios módulos, sem exigir um arquivo separado de declarações.

## O que os arquivos descrevem

| Arquivo original | Intenção |
| --- | --- |
| [Entities.clh](Entities.clh) | Dados, aliases, callbacks, genéricos e estado do jogo; reorganizar em módulo `.clp` ao implementar |
| [Maze.clp](Maze.clp) | Movimento, colisões, pontos e eventos |
| [Game.client.clpp](Game.client.clpp) | Integração com o cliente Roblox na proposta original |
| [Save.server.clpp](Save.server.clpp) | Persistência e autoridade de servidor na proposta original |
| [Diagnostics.expected.clpp](Diagnostics.expected.clpp) | Erros, warnings, hints e correções esperados; códigos propostos não são garantias atuais |
| [Entities.expected.luau](Entities.expected.luau) | Contrato sugerido para o antigo backend Luau; não é saída do backend atual |

O repositório atual usa C++20, bytecode e VM própria. A presença destes arquivos não adiciona `def`, backend Luau, contextos Roblox, novos códigos de diagnóstico ou comandos de migração. Os marcadores `[NOVO]` e `[VERIFICAR]` continuam sendo propostas a avaliar contra o código e os testes.
