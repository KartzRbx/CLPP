import fs from 'node:fs';
import path from 'node:path';
const source=path.resolve(process.argv[2] || 'compiler-docs/www/dist');
const destination=path.resolve(process.argv[3] || 'www/dist');
if (!fs.existsSync(path.join(source,'index.html'))) throw Error('Luau documentation must be built first');
fs.cpSync(source,path.join(destination,'luau'),{recursive:true});
const walk=d=>fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(d,e.name)):[path.join(d,e.name)]);
let count=0;
for(const file of walk(path.join(source,'docs')).filter(p=>p.endsWith('index.html'))) {
 const rel=path.relative(source,file);
 const legacy=path.join(destination,rel);
 if(fs.existsSync(legacy))continue;
 const slug=rel.split(path.sep).join('/').replace(/index\.html$/,'');
 const url='/CLPP/luau/'+slug;
 fs.mkdirSync(path.dirname(legacy),{recursive:true});
 fs.writeFileSync(legacy,'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta http-equiv="refresh" content="0;url='+url+'"><link rel="canonical" href="'+url+'"><title>CL++ Luau documentation</title></head><body><a href="'+url+'">CL++ Luau documentation</a></body></html>');
 count++;
}
console.log('Composed compiler documentation and '+count+' legacy redirects.');
