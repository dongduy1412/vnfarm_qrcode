import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { verifyContract, ContractError } from '../cloudflare/src/contract.ts';
const v=JSON.parse(await readFile(new URL('../docs/contract-vectors.json',import.meta.url),'utf8'));
const lookup=async did=>did===v.did?v.publicKey:null;
for(const sample of v.cases) {
  const url=new URL(v.url);
  if(sample.name==='tampered_signature') url.searchParams.set('sig',(v.sig[0]==='A'?'B':'A')+v.sig.slice(1));
  if(sample.name==='unknown_device') url.searchParams.set('did','unknown');
  if(sample.name==='missing_signature') url.searchParams.delete('sig');
  let status=302;
  try {await verifyContract(url,lookup,v.TTL,v.SKEW,sample.now);}
  catch(error) {assert(error instanceof ContractError); status=error.status;}
  assert.equal(status,sample.expectedStatus,sample.name);
}
console.log('PASS: Worker WebCrypto verifies shared contract vectors and status classifications. HTTP idempotence is tested in test_do.py.');
