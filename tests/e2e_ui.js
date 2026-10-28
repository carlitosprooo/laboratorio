// Browser end-to-end test of every control. Usage: node tests/e2e_ui.js <port> <reference.wav>
// (set PLAYWRIGHT_MODULE to the path of a playwright install if it is not resolvable).
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert = (c, m) => { if (!c) { console.log('FAIL:', m); process.exitCode = 1; } else console.log('ok  :', m); };
(async () => {
  const port = process.argv[2] || '7861';
  const ref = process.argv[3];
  const browser = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required'] });
  const ctx = await browser.newContext({ viewport: { width: 1470, height: 900 }, deviceScaleFactor: 2, acceptDownloads: true });
  const page = await ctx.newPage();
  page.on('pageerror', e => console.log('pageerror:', e.message));
  await page.goto(`http://127.0.0.1:${port}/`, { waitUntil: 'networkidle' });
  const md = id => page.locator(id).innerText();

  // dark mode forced
  assert(await page.evaluate(() => document.body.classList.contains('dark')), 'dark theme forced');
  // counter
  const ta = page.locator('#vs-text textarea');
  await ta.fill('Olá mundo. Hello galaxy');
  await page.waitForTimeout(700);
  assert((await md('#vs-counter')).includes('23 characters · 4 words'), 'counter updates: ' + await md('#vs-counter'));

  // insert tag at caret
  await ta.click();
  await ta.evaluate(el => { el.setSelectionRange(10, 10); });
  await page.locator('#vs-tag input').click();
  await page.getByRole('option', { name: '[laughter]' }).click();
  await page.locator('#vs-insert').click();
  await page.waitForTimeout(800);
  const v = await ta.inputValue();
  assert(v === 'Olá mundo. [laughter] Hello galaxy', 'insert tag at caret: ' + JSON.stringify(v));

  // mode switching
  await page.getByText('Voice Design', { exact: true }).first().click();
  await page.waitForTimeout(600);
  assert(await page.getByText('🎨 Voice Design').isVisible(), 'design panel visible in Voice Design');
  assert(!(await page.locator('#vs-ref-audio').isVisible()), 'clone panel hidden in Voice Design');
  await page.getByText('Auto Voice', { exact: true }).first().click();
  await page.waitForTimeout(600);
  assert(!(await page.getByText('🎨 Voice Design').isVisible()) && !(await page.locator('#vs-ref-audio').isVisible()), 'no voice panel in Auto Voice');

  // auto voice generation
  await page.locator('#vs-generate').click();
  await page.waitForFunction(() => document.querySelector('#vs-status').innerText.includes('Finished') || document.querySelector('#vs-status').innerText.includes('failed'), null, { timeout: 120000 });
  let st = await md('#vs-status');
  assert(st.includes('Finished') && st.includes('Internal generation calls: 1') && st.includes('not used (Auto Voice)'), 'auto voice generates');

  // voice clone: upload + transcript + prepare
  await page.getByText('Voice Clone', { exact: true }).first().click();
  await page.waitForTimeout(600);
  await page.locator('#vs-ref-audio input[type=file]').setInputFiles(ref);
  await page.waitForTimeout(2500);
  assert((await md('#vs-voice-status')).includes('Voice not cached'), 'indicator: not cached after upload');
  await page.locator('#vs-ref-text textarea').fill('A used RAV4 hybrid is supposed to be the easy decision.');
  await page.getByRole('button', { name: '🚀 Prepare / Cache Voice' }).click();
  await page.waitForFunction(() => document.querySelector('#vs-voice-status').innerText.includes('Voice cached'), null, { timeout: 60000 });
  assert(/Reference encoded in \d+\.\d\ds/.test(await md('#vs-voice-status')), 'prepare: ' + (await md('#vs-voice-status')).replace(/\n+/g, ' | '));

  await page.locator('#vs-generate').click();
  await page.waitForFunction(() => /Finished|failed|Check/.test(document.querySelector('#vs-status').innerText) && document.querySelector('#vs-status').innerText.includes('cache hit'), null, { timeout: 120000 });
  st = await md('#vs-status');
  assert(st.includes('Voice prompt: cache hit ✅'), 'generate with cached voice -> cache hit');
  console.log(st);

  // audio player: press play, time must advance
  await page.waitForTimeout(1500);
  const t0 = await page.locator('#vs-audio-out #time').first().innerText();
  await page.locator('#vs-audio-out .play-pause-button').first().click();
  await page.waitForTimeout(1600);
  const t1 = await page.locator('#vs-audio-out #time').first().innerText();
  assert(t0 === '0:00' && t1 !== '0:00', `player plays (${t0} -> ${t1})`);
  await page.locator('#vs-audio-out .play-pause-button').first().click();
  const dl = page.waitForEvent('download');
  await page.locator('#vs-file-out a').first().click();
  const d = await dl;
  assert((await d.suggestedFilename()).match(/^omnivoice_turbo_[0-9a-f]{7}\.wav$/), 'download WAV: ' + await d.suggestedFilename());
  await d.saveAs('downloaded.wav');
  await page.screenshot({ path: 'shots5/after_gen.png' });

  // voice design via dropdowns
  await page.getByText('Voice Design', { exact: true }).first().click();
  await page.waitForTimeout(500);
  await page.locator('#vs-design-gender input').click();
  await page.getByRole('option', { name: 'Female', exact: true }).click();
  await page.locator('#vs-generate').click();
  await page.waitForFunction(() => document.querySelector('#vs-status').innerText.includes('not used (Voice Design)'), null, { timeout: 120000 });
  assert(true, 'voice design generates');
  // voice design with nothing selected -> friendly error
  // long-form narration through the UI (clone mode, uses cached voice)
  await page.getByText('Voice Clone', { exact: true }).first().click();
  await page.waitForTimeout(500);
  const longText = Array.from({ length: 40 }, (_, i) => `This is sentence number ${i} of a long narration that keeps going.`).join(' ');
  await ta.fill(longText);
  await page.getByText('Quality / Advanced Settings').click();
  await page.waitForTimeout(400);
  const steps = page.locator('#vs-steps input[type=number]');
  await steps.fill('4'); await steps.press('Enter');
  const prevFile = ((await md('#vs-status')).match(/File: (\S+)/) || [])[1];
  await page.locator('#vs-generate').click();
  await page.waitForFunction((prev) => { const t = document.querySelector('#vs-status').innerText; const m = t.match(/File: (\S+)/); return m && m[1] !== prev && t.includes('Steps: 4'); }, prevFile, { timeout: 300000 });
  const calls = +(await md('#vs-status')).match(/Internal generation calls: (\d+)/)[1];
  assert(calls > 1, `long-form uses native chunking (${calls} internal calls) · ` + (await md('#vs-status')).match(/Audio: [^\n]*/)[0]);
  await page.getByText('Quality / Advanced Settings').click();

  // clear gpu
  await page.getByText('Quality / Advanced Settings').click();
  await page.getByRole('button', { name: '🧹 Clear GPU / Graph Caches' }).click();
  await page.waitForFunction(() => document.querySelector('#vs-status').innerText.includes('Caches cleared'), null, { timeout: 30000 });
  assert(true, 'clear GPU/graph caches');

  // clear voice cache
  await page.getByRole('button', { name: 'Clear Voice Cache' }).click();
  await page.waitForFunction(() => document.querySelector('#vs-voice-status').innerText.includes('not cached'), null, { timeout: 30000 });
  assert(true, 'clear voice cache resets indicator');

  // clear text/output
  await page.locator('#vs-clear').click();
  await page.waitForTimeout(1000);
  assert((await ta.inputValue()) === '' && (await md('#vs-status')).includes('Ready') && (await md('#vs-counter')).includes('0 characters'), 'clear text/output');

  // empty text -> warning
  await page.locator('#vs-generate').click();
  await page.waitForFunction(() => document.querySelector('#vs-status').innerText.includes('Check the inputs'), null, { timeout: 30000 });
  assert(true, 'empty text is rejected with a message');
  await browser.close();
})();
