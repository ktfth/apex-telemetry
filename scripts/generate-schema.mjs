#!/usr/bin/env node
/**
 * Gera `packages/contracts/schemas/lap-comparison.json` a partir do OpenAPI.
 *
 * O OpenAPI é a fonte de verdade única do contrato; o JSON Schema é derivado para
 * que validadores fora do ecossistema HTTP possam usá-lo. Manter os dois à mão
 * garantiria que divergissem.
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { parse } from 'yaml';

const OPENAPI = 'packages/contracts/openapi.yaml';
const OUTPUT = 'packages/contracts/schemas/lap-comparison.json';

/** Converte `$ref` de OpenAPI (`#/components/schemas/X`) em `$ref` de JSON Schema (`#/$defs/X`). */
function rewriteRefs(node) {
  if (Array.isArray(node)) return node.map(rewriteRefs);
  if (node && typeof node === 'object') {
    return Object.fromEntries(
      Object.entries(node).map(([key, value]) =>
        key === '$ref' && typeof value === 'string' && value.startsWith('#/components/schemas/')
          ? ['$ref', `#/$defs/${value.split('/').pop()}`]
          : [key, rewriteRefs(value)]
      )
    );
  }
  return node;
}

const spec = parse(readFileSync(OPENAPI, 'utf8'));
const defs = rewriteRefs(spec.components.schemas);

const document = {
  $schema: 'https://json-schema.org/draft/2020-12/schema',
  $id: 'https://apex-telemetry.io/schemas/lap-comparison.json',
  title: 'LapComparison',
  description:
    'Comparação de duas voltas alinhadas por distância, produzida pelo motor de ' +
    'alinhamento espacial em C++23. Todo valor tem origem em uma medição real da ' +
    'OpenF1; nenhum campo é preenchido com dado sintético. Gerado a partir de ' +
    `${OPENAPI} — não editar à mão.`,
  $defs: defs,
  $ref: '#/$defs/LapComparison'
};

writeFileSync(OUTPUT, `${JSON.stringify(document, null, 2)}\n`);
console.log(`${OUTPUT} gerado a partir de ${OPENAPI} (${Object.keys(defs).length} definições).`);
