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
page.on('console', (message) => {
  if (message.type() === 'error') errors.push(message.text());
});

await page.goto('http://127.0.0.1:5178', { waitUntil: 'networkidle' });
await page.getByText('MPL Studio', { exact: true }).first().waitFor({ timeout: 15000 });
await page.getByRole('button', { name: 'Settings' }).first().click();
await page.getByText('MPL Studio preferences').waitFor();
await page.waitForTimeout(500);
await page.screenshot({ path: '.runtime/screenshots/ui-settings.png' });
await page.getByRole('button', { name: 'Dark' }).click();
await page.waitForTimeout(400);
const dark = await page.evaluate(() => document.documentElement.dataset.theme);
await page.screenshot({ path: '.runtime/screenshots/ui-dark.png' });
await page.getByRole('button', { name: 'Light' }).click();
await page.getByRole('button', { name: 'Done' }).click();
await page.getByRole('button', { name: 'File', exact: true }).click();
await page.waitForTimeout(300);
await page.screenshot({ path: '.runtime/screenshots/ui-menu.png' });
await page.keyboard.press('Escape');

const title = await page.title();
console.log(JSON.stringify({ title, dark, errors }, null, 2));
await browser.close();
