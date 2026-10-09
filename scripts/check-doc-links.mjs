import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const dist=path.join(root,'www','dist');
const siteBase='/CLPP/luau/';
const walk=d=>fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(d,e.name)):[path.join(d,e.name)]);
const missing=new Map();
for(const file of walk(dist).filter(f=>f.endsWith('.html'))) {
 const rel=path.relative(dist,file).split(path.sep).join('/');
 if(rel==='404.html')continue;
 const base=new URL(siteBase+rel.replace(/index\.html$/,''),'https://kartzrbx.github.io');
 const html=fs.readFileSync(file,'utf8');
 for(const [,href] of html.matchAll(/href="([^"]+)"/g)) {
  if(/^(?:#|mailto:|tel:|javascript:)/.test(href))continue;
  const url=new URL(href.replaceAll('&amp;','&'),base);
  if(url.origin!==base.origin||!url.pathname.startsWith(siteBase))continue;
  const target=path.join(dist,decodeURIComponent(url.pathname.slice(siteBase.length)));
  if(fs.existsSync(target)||fs.existsSync(path.join(target,'index.html')))continue;
  const key=url.pathname;const from=missing.get(key)||new Set();from.add(rel);missing.set(key,from);
 }
}
for(const [url,from] of missing)console.error(url,'<-', [...from].slice(0,3).join(', '));
if(missing.size)throw Error(missing.size+' broken documentation paths');
console.log('All local documentation links resolve.');
