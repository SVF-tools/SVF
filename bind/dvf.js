#!/usr/bin/env node
const { spawn } = require('child_process');
const { resolve } = require('path');

let cmd = spawn(resolve(__dirname, '../Release-build/bin/dvf'), process.argv.slice(2), {
	cwd: resolve('.')
});
cmd.stderr.pipe(process.stderr);
cmd.stdout.pipe(process.stdout);
process.stdin.pipe(cmd.stdin);