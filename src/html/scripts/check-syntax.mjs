import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {transformSync} from '@babel/core';
import {loadEnv} from 'vite';
import {htmlFeatureBlocksPlugin} from '../feature-blocks-plugin.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const projectRoot = path.resolve(__dirname, '..');
const env = {
    ...loadEnv('production', projectRoot, ''),
    ...process.env
};
const featureBlocksPlugin = htmlFeatureBlocksPlugin(env);

const INCLUDE_EXTENSIONS = new Set(['.js', '.mjs']);
const EXCLUDE_DIRS = new Set(['dist', 'headers', 'node_modules', '.vite', '.idea']);
const rootsToCheck = [
    path.join(projectRoot, 'src'),
    path.join(projectRoot, 'dev-mock-plugin.js'),
    path.join(projectRoot, 'vite.config.js')
];

function collectFiles(targetPath, out) {
    const stat = fs.statSync(targetPath);
    if (stat.isDirectory()) {
        for (const entry of fs.readdirSync(targetPath, {withFileTypes: true})) {
            if (entry.isDirectory() && EXCLUDE_DIRS.has(entry.name)) {
                continue;
            }
            collectFiles(path.join(targetPath, entry.name), out);
        }
        return;
    }

    if (INCLUDE_EXTENSIONS.has(path.extname(targetPath))) {
        out.push(targetPath);
    }
}

const files = [];
for (const root of rootsToCheck) {
    if (fs.existsSync(root)) {
        collectFiles(root, files);
    }
}

const failures = [];
for (const file of files.sort()) {
    try {
        const source = fs.readFileSync(file, 'utf8');
        const transformed = featureBlocksPlugin.transform(source, file);
        transformSync(transformed?.code ?? source, {
            filename: file,
            babelrc: false,
            configFile: false,
            sourceType: 'module',
            ast: false,
            code: false,
            plugins: [['@babel/plugin-proposal-decorators', {version: '2023-11'}]]
        });
    } catch (error) {
        failures.push({file, error});
    }
}

if (failures.length > 0) {
    for (const {file, error} of failures) {
        const relativePath = path.relative(projectRoot, file);
        console.error(`Syntax check failed: ${relativePath}`);
        console.error(error.message);
    }
    process.exit(1);
}

console.log(`Syntax check passed for ${files.length} files.`);
