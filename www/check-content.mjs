import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..');
const docs = path.join(root, 'docs');
const sources = ['guide', 'reference', 'handbook'].flatMap(dir => fs.readdirSync(path.join(docs, dir)).filter(name => name.endsWith('.md')).map(name => path.join(docs, dir, name)));
sources.push(path.join(here, 'src/content/docs/index.mdx'), path.join(root, 'README.md'));
let blocks = 0, targets = 0, links = 0;
const errors = [];
for (const source of sources) {
  const text = fs.readFileSync(source, 'utf8');
  const label = path.relative(root, source);
  // Published explanations use English; strip examples, URLs and inline code before checking.
  const prose = text.replace(/```[\s\S]*?```/g, ' ').replace(/`[^`]*`/g, ' ').replace(/\]\([^)]*\)/g, ']');
  const portuguese = prose.match(/(?<!\p{L})(?:você|vocês|não|também|português|variáveis|tipagem|tipagens|funções|capítulos|código|compilação|implementação|explicação|referência|própria|linguagem|sintaxe)(?!\p{L})/giu);
  if (portuguese) errors.push(`${label}: published prose must be English (${[...new Set(portuguese)].join(', ')})`);

  for (const match of text.matchAll(/```([^\n`]*)\r?\n([\s\S]*?)```/g)) {
    const options = match[1].trim().split(/\s+/);
    if (options[0] === 'clpp') errors.push(`${label}: use the site's clp fence and explicit target marker`);
    if (options[0] !== 'clp') continue;
    blocks++;
    if (options.includes('target')) targets++;
    const line = text.slice(0, match.index).split('\n').length;
    const code = match[2].replace(/"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`/g, ' ').replace(/<<\[[\s\S]*?\]>>|<<[^\r\n]*/g, ' ');
    if (/\.clh\b/.test(match[2])) errors.push(`${label}:${line}: header paths are forbidden in public examples`);
    if (/(?:[{,]\s*)\.[A-Za-z_]\w*\s*=|\blog\s*\(|\b(?:let|mut|struct|int|double)\b|\bfunc\s+[A-Za-z_]\w*\s*(?:<[^>]+>)?\s*\(|\.clh\b/.test(code)) errors.push(`${label}:${line}: obsolete public syntax`);
  }
  for (const match of text.matchAll(/\]\(([^)\s]+)(?:\s+"[^"]*")?\)/g)) {
    const href = match[1].split('#')[0];
    if (!href || /^(?:[a-z]+:|\/)/i.test(href)) continue;
    links++;
    if (!fs.existsSync(path.resolve(path.dirname(source), decodeURIComponent(href)))) errors.push(`${label}: missing Markdown link ${href}`);
  }
}
if (process.argv.includes('--built')) {
  const dist = path.join(here, 'dist');
  function walk(dir) { return fs.readdirSync(dir,{withFileTypes:true}).flatMap(entry => entry.isDirectory()?walk(path.join(dir,entry.name)):[path.join(dir,entry.name)]); }
  for (const file of walk(dist).filter(name => name.endsWith('.html'))) {
    const html = fs.readFileSync(file,'utf8');
    const visible = html.replace(/<(script|style|pre)\b[^>]*>[\s\S]*?<\/\1>/gi, ' ').replace(/<[^>]*>/g, ' ');
    const untranslated = visible.match(/(?<!\p{L})(?:você|vocês|não|também|português|variáveis|tipagem|tipagens|funções|capítulos|código|compilação|implementação|explicação|referência|própria|linguagem|sintaxe)(?!\p{L})/giu);
    if (untranslated) errors.push(`${path.relative(dist,file)}: generated HTML contains untranslated prose (${[...new Set(untranslated)].join(', ')})`);

    for (const match of html.matchAll(/(?:href|src)="(\/CLPP\/[^"#?]*)(?:[?#][^"]*)?"/g)) {
      const relative = decodeURIComponent(match[1].slice('/CLPP/'.length));
      const target = path.join(dist, relative);
      if (!fs.existsSync(target) && !fs.existsSync(path.join(target,'index.html'))) errors.push(`${path.relative(dist,file)}: missing built route/asset ${match[1]}`);
    }
    if (html.includes('This example specifies the author')) errors.push(`${file}: removed automatic example notice must not be published`);
  }
}
if (errors.length) { for(const error of errors) console.error(error); process.exit(1); }
console.log(`Content policy passed: ${sources.length} sources, ${blocks} CL++ blocks (${targets} target specifications), ${links} local Markdown links.`);
if (process.argv.includes('--built')) console.log('All /CLPP/ links and assets in generated HTML exist.');
