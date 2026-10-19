// Exercise the shipped TextMate grammar through Oniguruma, as VS Code does.
// Shiki is already installed by the documentation build; no editor SDK needed.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createHighlighter } from '../../www/node_modules/shiki/dist/index.mjs';

const grammar = JSON.parse(fs.readFileSync(process.argv[2] || new URL('../../tools/vscode/syntaxes/clpp.tmLanguage.json', import.meta.url), 'utf8'));
grammar.name = 'clpp-grammar-qa';
const highlighter = await createHighlighter({ themes: ['dark-plus'], langs: [grammar] });
let checks = 0;
try {
  const scopes = code => highlighter.codeToTokensBase(code, {
    lang: grammar.name, theme: 'dark-plus', includeExplanation: 'scopeName',
  }).flat().flatMap(token => token.explanation || []);
  const role = (tokens, word) => {
    const entry = tokens.find(token => token.content === word);
    assert.ok(entry, `Missing highlighted token ${word}`);
    return entry.scopes.map(scope => scope.scopeName);
  };
  for (const type of ['array', 'dictionary', 'optional', 'tuple']) {
    for (const code of [`${type}<T> output = ${type}<T>();`, `${type} < T > output = ${type} < T > ();`]) {
      const tokens = scopes(code);
      assert.ok(role(tokens, type).includes('support.type.primitive.clpp'), code);
      checks++;
    }
  }
  for (const code of ['array<T> Filter(array<T> input, Predicate<T> predicate) {',
                      'array < T > Filter(array < T > input, Predicate < T > predicate) {']) {
    const tokens = scopes(code);
    assert.ok(role(tokens, 'Filter').includes('entity.name.function.clpp'), code);
    assert.ok(role(tokens, 'Predicate').includes('entity.name.type.clpp'), code);
    checks += 2;
  }
  for (const code of ['<< array < T > Filter()', '<<[ array<T> Filter() ]>>']) {
    assert.ok(scopes(code).every(token => token.scopes.some(scope => scope.scopeName.startsWith('comment.'))), code);
    checks++;
  }
  for (const [code, word] of [
    ['auto value = await Read();', 'await'],
    ['post(await pending);', 'await'],
    ['auto pending = spawn Read();', 'spawn'],
    ['return Read();', 'return'],
    ['auto worker = thread Work();', 'thread'],
    ['if (ready and Allowed()) {}', 'and'],
    ['auto value = await worker::Run();', 'await'],
    ['auto work = spawn worker::Run();', 'spawn'],
  ]) {
    const actual = role(scopes(code), word);
    assert.ok(actual.includes(word === 'thread' ? 'storage.modifier.clpp' : 'keyword.control.clpp'), `${word}: ${actual}`);
    assert.ok(!actual.includes('storage.type.clpp') && !actual.includes('entity.name.type.clpp'), code);
    checks++;
  }
  for (const code of ['number Point::Sum() { return @this.X; }',
                      'template<typename T> T Box<T>::Read() { return @this.Value; }']) {
    const tokens = scopes(code);
    const owner = code.includes('Point') ? 'Point' : 'Box';
    const method = code.includes('Point') ? 'Sum' : 'Read';
    assert.ok(role(tokens, owner).some(scope => scope.startsWith('entity.name.type.')), `${owner}: ${JSON.stringify(tokens)}`);
    assert.ok(role(tokens, method).includes('entity.name.function.member.clpp'), method);
    checks += 2;
  }
  for (const [code, word, expected] of [
    ['abstract def Named { abstract string Name(); }', 'abstract', 'storage.modifier.clpp'],
    ['final def Player : Storage, Named {}', 'final', 'storage.modifier.clpp'],
    ['optional<number> cpf;', 'optional', 'support.type.primitive.clpp'],
    ['signal<Human, string> joined;', 'signal', 'storage.modifier.clpp'],
    ['guard (person.cpf) else { return; }', 'guard', 'keyword.control.clpp'],
    ['if (value:is<Human>) {}', 'is', 'entity.name.function.member.clpp'],
    ['number Point::Sum() { return @this.X; }', 'return', 'keyword.control.clpp'],
  ]) {
    assert.ok(role(scopes(code), word).includes(expected), `${word}: ${role(scopes(code), word)}`);
    checks++;
  }
  console.log(`PASS ${checks} real TextMate/Oniguruma generic and comment classifications`);
} finally {
  highlighter.dispose();
}
