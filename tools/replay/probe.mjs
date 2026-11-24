import fs from 'fs';
const wasm = await WebAssembly.compile(fs.readFileSync(new URL('./assets/v3_0_0.wasm', import.meta.url)));
console.log(WebAssembly.Module.imports(wasm).map(i=>i.module+'.'+i.name+':'+i.kind).join(' '));
console.log(WebAssembly.Module.exports(wasm).map(i=>i.name).join(' '));
