#!/usr/bin/env node
/**
 * Verificação de ponta a ponta da pilha ApexTelemetry contra dados reais.
 *
 * Não usa mocks: sobe contra um gateway em execução, exercita cada endpoint que a
 * interface consome e confere as invariantes que tornam a análise confiável —
 * entre elas o fechamento do delta acumulado contra o cronômetro oficial, que é a
 * verificação independente do alinhamento espacial.
 *
 *   node scripts/verify-stack.mjs [--gateway http://localhost:8080/api/v1] [--session 9468]
 */

const args = process.argv.slice(2);
const option = (name, fallback) => {
  const index = args.indexOf(`--${name}`);
  return index >= 0 && args[index + 1] ? args[index + 1] : fallback;
};

const GATEWAY = option('gateway', process.env.APEX_GATEWAY_URL ?? 'http://localhost:8080/api/v1');
const YEAR = Number(option('year', '2024'));

let passed = 0;
let failed = 0;

function check(condition, label, detail = '') {
  if (condition) {
    passed += 1;
    console.log(`  \x1b[32m✓\x1b[0m ${label}${detail ? ` \x1b[90m${detail}\x1b[0m` : ''}`);
  } else {
    failed += 1;
    console.log(`  \x1b[31m✗\x1b[0m ${label}${detail ? ` \x1b[90m${detail}\x1b[0m` : ''}`);
  }
}

async function get(path, { expectStatus = 200 } = {}) {
  const response = await fetch(`${GATEWAY}${path}`, { signal: AbortSignal.timeout(120_000) });
  const source = response.headers.get('X-Apex-Data-Source');
  const text = await response.text();
  let body;
  try {
    body = JSON.parse(text);
  } catch {
    body = text;
  }
  if (response.status !== expectStatus) {
    throw new Error(
      `${path} devolveu ${response.status}, esperado ${expectStatus}: ${JSON.stringify(body).slice(0, 200)}`
    );
  }
  return { body, source, status: response.status };
}

async function main() {
  console.log(`\n\x1b[1mApexTelemetry — verificação da pilha\x1b[0m`);
  console.log(`gateway: ${GATEWAY}\n`);

  // 1. Saúde e dependências declaradas.
  console.log('\x1b[1m1. Gateway\x1b[0m');
  const { body: health } = await get('/health');
  check(health.status === 'healthy', 'gateway responde saudável', `v${health.version}`);
  check(health.cpp_standard === 202302, 'motor numérico compilado em C++23');
  check(
    typeof health.database?.configured === 'boolean',
    'estado do armazém declarado',
    health.database.healthy ? 'postgresql conectado' : 'sem armazém: upstream ao vivo'
  );
  check(
    typeof health.strategy_engine?.configured === 'boolean',
    'estado do motor Haskell declarado',
    health.strategy_engine.healthy ? 'strategy-hs ativo' : 'strategy-hs ausente'
  );

  // 2. Catálogo real.
  console.log('\n\x1b[1m2. Catálogo\x1b[0m');
  const { body: sessions, source: sessionSource } = await get(`/sessions?year=${YEAR}`);
  check(Array.isArray(sessions) && sessions.length > 0, `sessões de ${YEAR}`, `${sessions.length} · ${sessionSource}`);

  const qualifying =
    sessions.find((s) => s.session_type === 'Qualifying') ?? sessions[sessions.length - 1];
  const sessionKey = Number(option('session', String(qualifying.session_key)));
  const session = sessions.find((s) => s.session_key === sessionKey) ?? qualifying;
  console.log(`  \x1b[90msessão escolhida: ${sessionKey} — ${session.circuit_name} ${session.session_name}\x1b[0m`);

  const { body: drivers } = await get(`/sessions/${sessionKey}/drivers`);
  check(drivers.length >= 2, 'lista de inscritos', `${drivers.length} pilotos`);
  check(
    drivers.every((d) => /^#[0-9a-fA-F]{6}$/.test(d.team_colour)),
    'cores de equipe em hexadecimal válido'
  );

  const { body: allLaps } = await get(`/sessions/${sessionKey}/laps`);
  const timed = allLaps.filter((lap) => lap.lap_time_s !== null && lap.lap_time_s > 0);
  check(timed.length > 0, 'voltas cronometradas', `${timed.length} de ${allLaps.length}`);
  check(
    allLaps.some((lap) => lap.lap_time_s === null),
    'voltas não cronometradas expostas como null, não como zero'
  );

  // As duas voltas mais rápidas de pilotos distintos: o caso de uso real.
  const byDriver = new Map();
  for (const lap of timed) {
    const best = byDriver.get(lap.driver_number);
    if (!best || lap.lap_time_s < best.lap_time_s) byDriver.set(lap.driver_number, lap);
  }
  const ranked = [...byDriver.values()].sort((a, b) => a.lap_time_s - b.lap_time_s);
  if (ranked.length < 2) throw new Error('a sessão não tem duas voltas cronometradas comparáveis');
  const [reference, comparison] = ranked;
  console.log(
    `  \x1b[90mreferência: #${reference.driver_number} L${reference.lap_number} ${reference.lap_time_s}s · ` +
      `comparação: #${comparison.driver_number} L${comparison.lap_number} ${comparison.lap_time_s}s\x1b[0m`
  );

  // 3. Traçado reconstruído do transponder.
  console.log('\n\x1b[1m3. Traçado do circuito\x1b[0m');
  const { body: circuit, source: circuitSource } = await get(`/sessions/${sessionKey}/circuit`);
  check(circuit.path.length > 100, 'polilinha do traçado', `${circuit.path.length} pontos · ${circuitSource}`);
  check(
    circuit.path.every(([d, x, y]) => Number.isFinite(d) && Number.isFinite(x) && Number.isFinite(y)),
    'todos os pontos do traçado são finitos'
  );
  const distances = circuit.path.map(([d]) => d);
  check(
    distances.every((d, i) => i === 0 || d >= distances[i - 1]),
    'a distância ao longo do traçado é monotônica'
  );
  check(circuit.corners.length > 0, 'curvas detectadas no traçado', `${circuit.corners.length}`);
  check(
    circuit.corners.every((c) => /^C\d+$/.test(c.label)),
    'curvas rotuladas como detectadas (C1..Cn), não como numeração oficial'
  );

  // 4. Comparação espacial e suas invariantes.
  console.log('\n\x1b[1m4. Comparação alinhada por distância\x1b[0m');
  const query =
    `session_key=${sessionKey}&ref_driver=${reference.driver_number}&ref_lap=${reference.lap_number}` +
    `&comp_driver=${comparison.driver_number}&comp_lap=${comparison.lap_number}&step_m=5`;
  const { body: analysis, source: analysisSource } = await get(`/analysis/compare?${query}`);

  check(analysis.schema_version === '1.2.0', 'versão do esquema', analysis.schema_version);
  check(analysis.channels.length > 100, 'canais alinhados', `${analysis.channels.length} nós · ${analysisSource}`);
  check(analysis.data_source !== 'unknown', 'origem dos dados declarada', analysis.data_source);

  const step = analysis.grid_step_m;
  const spacingOk = analysis.channels.every(
    (point, index) => index === 0 || Math.abs(point.distance_m - analysis.channels[index - 1].distance_m - step) < 1e-6
  );
  check(spacingOk, `grade espacial regular de ${step} m`);

  const officialGap = analysis.comparison_lap.lap_time_s - analysis.reference_lap.lap_time_s;
  const accumulated = analysis.channels[analysis.channels.length - 1].delta_time_s;
  const closure = Math.abs(accumulated - officialGap);
  check(
    closure <= 0.02,
    'o delta acumulado fecha contra o cronômetro oficial',
    `|${accumulated.toFixed(4)} − ${officialGap.toFixed(4)}| = ${closure.toFixed(4)} s`
  );
  check(
    Math.abs(analysis.quality_audit.delta_closure_error_s - closure) < 1e-3,
    'a auditoria reporta o mesmo desvio que o cliente calcula'
  );

  check(
    analysis.quality_audit.coverage_pct > 0 && analysis.quality_audit.coverage_pct <= 100,
    'cobertura da grade dentro do domínio',
    `${analysis.quality_audit.coverage_pct.toFixed(1)}%`
  );
  check(
    analysis.quality_audit.raw_samples_ref > 0 && analysis.quality_audit.raw_samples_comp > 0,
    'contagem de amostras brutas reportada',
    `${analysis.quality_audit.raw_samples_ref}/${analysis.quality_audit.raw_samples_comp}`
  );
  check(
    analysis.channels.every(
      (p) =>
        p.ref.throttle_pct >= 0 && p.ref.throttle_pct <= 100 &&
        p.ref.brake_pct >= 0 && p.ref.brake_pct <= 100 &&
        p.comp.throttle_pct >= 0 && p.comp.throttle_pct <= 100 &&
        p.comp.brake_pct >= 0 && p.comp.brake_pct <= 100
    ),
    'pedais dentro de 0–100% em todos os nós'
  );
  check(
    analysis.microsectors.length > 0 &&
      Math.abs(analysis.microsectors.reduce((sum, m) => sum + m.delta_s, 0) - accumulated) < 0.05,
    'a soma dos microsetores reproduz o delta total',
    `${analysis.microsectors.length} microsetores`
  );

  const official = analysis.speed_traps.filter((t) => t.origin === 'openf1_marshalling_loop');
  check(analysis.speed_traps.length > 0, 'pontos de medição de velocidade', `${analysis.speed_traps.length} (${official.length} oficiais)`);

  // 5. Explicabilidade.
  console.log('\n\x1b[1m5. Explicabilidade\x1b[0m');
  check(Array.isArray(analysis.insights), 'insights presentes', `${analysis.insights.length}`);
  for (const insight of analysis.insights) {
    check(
      insight.summary.includes(insight.time_loss_s.toFixed(3)),
      `insight ${insight.id} cita a perda medida no texto`
    );
    check(
      insight.confidence >= 0 && insight.confidence <= 1,
      `insight ${insight.id} com confiança no intervalo [0,1]`,
      `${insight.confidence}`
    );
    check(
      insight.distance_start_m < insight.distance_end_m,
      `insight ${insight.id} com trecho bem formado`
    );
  }

  // 6. Exportação.
  console.log('\n\x1b[1m6. Exportação\x1b[0m');
  const csvResponse = await fetch(`${GATEWAY}/analysis/export?${query}&format=motec_csv`, {
    signal: AbortSignal.timeout(120_000)
  });
  const csv = await csvResponse.text();
  const dataRows = csv.split('\n').filter((line) => /^\d/.test(line));
  check(csv.includes('"Format","MoTeC CSV Telemetry Export"'), 'cabeçalho MoTeC presente');
  check(csv.includes(`"Venue","${analysis.circuit_name}"`), 'circuito real no cabeçalho', analysis.circuit_name);
  check(
    dataRows.length === analysis.channels.length,
    'uma linha de dados por nó da grade',
    `${dataRows.length} linhas`
  );
  check(
    (csvResponse.headers.get('Content-Disposition') ?? '').includes(analysis.reference_lap.driver_code),
    'nome do arquivo derivado dos pilotos reais'
  );

  // 7. Erros honestos.
  console.log('\n\x1b[1m7. Erros estruturados\x1b[0m');
  const cases = [
    [`/analysis/compare?session_key=${sessionKey}&ref_driver=${reference.driver_number}&ref_lap=99999&comp_driver=${comparison.driver_number}&comp_lap=${comparison.lap_number}`, 404, 'LAP_NOT_FOUND'],
    [`/analysis/compare?session_key=${sessionKey}&ref_driver=${reference.driver_number}&ref_lap=${reference.lap_number}&comp_driver=${reference.driver_number}&comp_lap=${reference.lap_number}`, 422, 'IDENTICAL_LAPS'],
    [`/analysis/compare?session_key=${sessionKey}&ref_driver=1&ref_lap=1&comp_driver=2&comp_lap=1&step_m=999`, 422, 'INVALID_GRID_STEP'],
    [`/sessions?year=1`, 422, 'INVALID_YEAR'],
    [`/analysis/compare?session_key=${sessionKey}`, 400, 'MISSING_PARAMETER']
  ];
  for (const [path, status, code] of cases) {
    const { body } = await get(path, { expectStatus: status });
    check(body.code === code, `${code} (HTTP ${status})`, body.error?.slice(0, 60));
    check(
      !Object.prototype.hasOwnProperty.call(body, 'channels'),
      `${code} não devolve telemetria inventada junto do erro`
    );
  }

  // 8. Replay em tempo de pista.
  console.log('\n\x1b[1m8. Replay (SSE)\x1b[0m');
  const streamResponse = await fetch(`${GATEWAY}/sessions/${sessionKey}/live?${query}&speed=50`, {
    signal: AbortSignal.timeout(60_000)
  });
  const reader = streamResponse.body.getReader();
  const decoder = new TextDecoder();
  let buffer = '';
  let initFrame = null;
  // Contamos sequências distintas: reprocessar o buffer a cada leitura contaria o
  // mesmo quadro várias vezes e inflaria o número.
  const seenSequences = new Set();
  let ended = false;
  const deadline = Date.now() + 25_000;
  while (Date.now() < deadline && !ended) {
    const { done, value } = await reader.read();
    if (done) break;
    buffer += decoder.decode(value, { stream: true });

    // Processa apenas blocos completos e consome o que já foi lido.
    let boundary;
    while ((boundary = buffer.indexOf('\n\n')) !== -1) {
      const frame = buffer.slice(0, boundary);
      buffer = buffer.slice(boundary + 2);
      const eventMatch = frame.match(/event: (\w+)/);
      const dataMatch = frame.match(/data: (.+)/);
      if (!eventMatch) continue;
      if (eventMatch[1] === 'init' && dataMatch) initFrame = JSON.parse(dataMatch[1]);
      if (eventMatch[1] === 'telemetry_tick' && dataMatch) {
        seenSequences.add(JSON.parse(dataMatch[1]).seq);
      }
      if (eventMatch[1] === 'end') ended = true;
    }
  }
  const ticks = seenSequences.size;
  await reader.cancel().catch(() => {});
  check(initFrame !== null, 'quadro inicial recebido', initFrame ? `${initFrame.points} pontos` : '');
  check(ticks > 10, 'quadros de telemetria distintos transmitidos', `${ticks} de ${initFrame?.points ?? '?'}`);
  check(
    initFrame === null || ticks <= initFrame.points,
    'nenhum quadro além dos nós realmente alinhados'
  );
  check(initFrame?.mode === 'replay', 'modo declarado como replay de dados medidos');

  console.log(
    `\n\x1b[1m${failed === 0 ? '\x1b[32mPilha verificada' : '\x1b[31mFalhas detectadas'}\x1b[0m: ` +
      `${passed} verificações passaram, ${failed} falharam.\n`
  );
  process.exit(failed === 0 ? 0 : 1);
}

main().catch((error) => {
  console.error(`\n\x1b[31mFalha na verificação:\x1b[0m ${error.message}\n`);
  process.exit(1);
});
