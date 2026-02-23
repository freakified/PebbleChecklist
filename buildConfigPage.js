const fs = require('fs');
const path = require('path');

const htmlPath = path.join(__dirname, 'config-page', 'config.html');
const cssPath = path.join(__dirname, 'config-page', 'config.css');
const jsPath = path.join(__dirname, 'config-page', 'config.js');

const html = fs.readFileSync(htmlPath, 'utf8');
const css = fs.readFileSync(cssPath, 'utf8');
const js = fs.readFileSync(jsPath, 'utf8');

let output = html
  .replace('<link rel="stylesheet" href="config.css">', `<style>${css}</style>`)
  .replace('<script src="config.js"></script>', `<script>${js}</script>`);

const dataUri = `data:text/html;charset=utf-8,${encodeURIComponent(output)}`;
const outputPath = path.join(__dirname, 'src', 'pkjs', 'configDataUri.js');

fs.writeFileSync(outputPath, `module.exports = "${dataUri}";\n`);
console.log('Built config data URI');
