import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const binary=process.env.CLPP_BINARY || path.join(root,'target','debug',process.platform==='win32'?'clpp.exe':'clpp');
const analyze=process.env.LUAU_ANALYZE || 'luau-analyze';
const runtime=process.env.LUAU || 'luau';
const update=process.argv.includes('--update');
const cases=path.join(root,'tests','golden','cases');
const scratch=path.join(root,'target','golden-check');fs.mkdirSync(scratch,{recursive:true});
let failed=0;
for(const name of fs.readdirSync(cases).filter(f=>f.endsWith('.clpp')).sort()) {
 const source=fs.readFileSync(path.join(cases,name),'utf8');
 for(const optimized of [false,true]) {
 const compiled=spawnSync(binary,['api','compile'],{cwd:root,input:JSON.stringify({source,fileName:'tests/golden/cases/'+name,strict:true,optimize:optimized}),encoding:'utf8'});
 let artifact;try { artifact=JSON.parse(compiled.stdout); } catch { console.error(name,compiled.stderr);failed++;continue; }
 if(!artifact.ok){console.error(name,artifact.diagnostics);failed++;continue;}
 const expected=path.join(cases,name.replace(/\.clpp$/,'.luau'));
 if(update&&!optimized)fs.writeFileSync(expected,artifact.luau);
 if(!optimized&&(!fs.existsSync(expected)||fs.readFileSync(expected,'utf8').replace(/\r\n/g,'\n')!==artifact.luau)){console.error(name,'golden mismatch');failed++;continue;}
 const preludeFile=path.join(cases,name.replace(/\.clpp$/,'.prelude.luau'));
 const prelude=fs.existsSync(preludeFile)?fs.readFileSync(preludeFile,'utf8'):'';
 const generated=path.join(scratch,(optimized?'optimized-':'')+name.replace(/\.clpp$/,'.luau'));
 // Host fixtures provide a small test implementation of Roblox globals and library modules.
 // Emitted source itself is compared exactly and never repaired.
 fs.writeFileSync(generated,artifact.luau.split('\n')[0]+'\n'+prelude+'\n'+artifact.luau.split('\n').slice(1).join('\n'));
 for(const [tool,args] of [[analyze,['--mode=strict',generated]],[runtime,[generated]]]) {
  const check=spawnSync(tool,args,{encoding:'utf8'});
  if(check.error||check.status!==0){console.error(name,tool,check.error?.message||check.stdout+check.stderr);failed++;}
 }
 }
}
if(failed)process.exit(1);
console.log('Golden output, strict analysis and runtime checks passed.');
