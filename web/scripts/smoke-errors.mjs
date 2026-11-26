import { chromium } from 'playwright-core';
import fs from 'node:fs';
fs.mkdirSync('.runtime/screenshots', { recursive: true });

const browser = await chromium.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
await page.goto('http://127.0.0.1:5178', { waitUntil: 'networkidle' });
await page.getByText('MPL Studio', { exact: true }).first().waitFor();
await page.locator('.cm-content').click();
await page.keyboard.press('Control+A');
await page.keyboard.type('project Broken {');
await page.waitForTimeout(2200);
const body = await page.locator('body').innerText();
const problems = await page.locator('button').filter({ hasText: 'Problems' }).count();
await page.screenshot({ path: '.runtime/screenshots/studio-errors.png' });
console.log(JSON.stringify({
  hasDiagnostic: /parse-|index-|type-|sema-/i.test(body),
  hasIssues: body.includes('Issues') || body.includes('PROBLEMS') || body.includes('Problems'),
  problems,
  errors,
}, null, 2));
await browser.close();
