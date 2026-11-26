import { chromium } from 'playwright-core';
import fs from 'node:fs';

const executablePath = process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const browser = await chromium.launch({ executablePath, headless: true });
fs.mkdirSync('.runtime/screenshots', { recursive: true });
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 }, deviceScaleFactor: 1 });
const errors = [];
page.on('pageerror', (error) => errors.push(`pageerror: ${error.message}`));
page.on('console', (message) => {
  if (message.type() === 'error') errors.push(`console: ${message.text()}`);
});

await page.goto('http://127.0.0.1:5178', { waitUntil: 'networkidle' });
await page.getByText('MPL Studio', { exact: true }).first().waitFor({ timeout: 15000 });
await page.screenshot({ path: '.runtime/screenshots/studio-idle.png' });

await page.getByRole('button', { name: /Build & Play/i }).click();
await page.getByText('TRUSTED RENDER', { exact: false }).waitFor({ timeout: 120000 });
await page.locator('canvas').waitFor({ timeout: 15000 });
await page.screenshot({ path: '.runtime/screenshots/studio-built.png' });

const title = await page.title();
const body = await page.locator('body').innerText();
const audio = await page.locator('audio').getAttribute('src');
const canvasCount = await page.locator('canvas').count();
console.log(JSON.stringify({
  title,
  hasTrustedRender: body.toLowerCase().includes('trusted render'),
  hasArtifacts: body.includes('WAV 48k') || /\.wav/.test(body),
  audio,
  canvasCount,
  errors,
}, null, 2));

await browser.close();
