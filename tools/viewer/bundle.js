// The map viewer (app/www/viewer.html) as one file: its scripts put inside the page, so it can be copied anywhere and opened in a browser. No game data goes in.
// Usage (Electron as Node): bundle.js <out.html>
const fs = require('fs'), path = require('path'), www = path.join(__dirname, '..', '..', 'app', 'www');
let t = fs.readFileSync(path.join(www, 'viewer.html'), 'utf8');
t = t.replace(/<script src="([^"]+)"><\/script>/g, (m, f) => '<script>' + fs.readFileSync(path.join(www, f), 'utf8').replace(/<\/script/gi, '<\\/script') + '</script>');
if (/<script src=/.test(t)) throw new Error('a script was left out');
fs.writeFileSync(process.argv[2], t); console.log(process.argv[2], Math.round(t.length / 1024), 'KB');
