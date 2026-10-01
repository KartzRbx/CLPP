# Documentação do CL++

## Guia da linguagem

Cada exemplo destes capítulos é executado pela suíte de testes: se a linguagem mudar e um exemplo deixar de produzir a saída mostrada, o build falha.

1. [Começando](guia/01-comecando.md): instalação, primeiro programa, linha de comando
2. [Léxico](guia/02-lexico.md): comentários, nomes, palavras-chave, literais, templates
3. [Variáveis e tipos](guia/03-variaveis-e-tipos.md): `let`, `let mut`, tipos, constantes, semântica de valor, `move`
4. [Operadores](guia/04-operadores.md): aritmética, lógica, bits, `.:`, ternário, fatias, vetores, precedência
5. [Controle de fluxo](guia/05-controle-de-fluxo.md): `if`, `while`, `for`, `for … in`, `break`, `continue`, `switch`, `match`
6. [Funções](guia/06-funcoes.md): parâmetros padrão e nomeados, variádicas, recursão, genéricos, namespaces, lambdas, `extern`
7. [Structs e herança](guia/07-structs-e-heranca.md): campos, métodos, `self`/`@`, herança simples e múltipla, polimorfismo, `abstract`/`override`/`final`, `private`
8. [Enums, variants, Option e Result](guia/08-enums-variants-option-result.md): enums com dados, `Option`, `Result`, uniões
9. [Coleções e textos](guia/09-colecoes-e-textos.md): listas, `array<T>`, dicionários, vetores, buffers, biblioteca de texto
10. [Módulos](guia/10-modulos.md): `link` sem cabeçalhos, constantes, biblioteca padrão
11. [Erros](guia/11-erros.md): `try`/`catch`, `report`, `warn`, `pcall`, mensagens
12. [Concorrência](guia/12-concorrencia.md): `async`/`await`, `parallel`, threads, `atomic`, `mutex`, corrotinas, `actor`
13. [Eventos](guia/13-eventos.md): `signal` e `observable`
14. [Biblioteca padrão](guia/14-biblioteca-padrao.md): Axiom (matemática para jogos), Fs, Os, Http
15. [Ferramentas e editor](guia/15-ferramentas-e-editor.md): CLI, extensão do VS Code, build, testes, embutir em um jogo
16. [Desempenho](guia/16-desempenho.md): benchmarks, o que foi otimizado e por quê, segurança

## O que mudou nesta revisão

[CHANGES.md](CHANGES.md): correções, recursos novos, editor, desempenho e limitações conhecidas.

## Referência técnica

- [Palavras-chave e operadores](keywords.md)
- [Lexer](lexer.md)
- [Gramática (EBNF)](ebnf.md)
- [Arquitetura do compilador](architecture.md)
- [CLIR v1](clir-v1.md) e [opcodes](opcodes.md)
- [Backends](backend-capabilities.md)
- [Toolchain](toolchain.md)
