import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {ESLint} from 'eslint';
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

const eslint = new ESLint({
    cwd: projectRoot
});

const results = [];
for (const file of files.sort()) {
    const source = fs.readFileSync(file, 'utf8');
    const transformed = featureBlocksPlugin.transform(source, file);
    const code = transformed?.code ?? source;
    const fileResults = await eslint.lintText(code, {filePath: file, warnIgnored: false});
    results.push(...fileResults);
}

const formatter = await eslint.loadFormatter('stylish');
const output = formatter.format(results);
if (output) {
    process.stdout.write(output.endsWith('\n') ? output : `${output}\n`);
}

const errorCount = results.reduce((sum, result) => sum + result.errorCount + result.fatalErrorCount, 0);
const warningCount = results.reduce((sum, result) => sum + result.warningCount, 0);

if (errorCount > 0) {
    process.exit(1);
}

console.log(`Lint check passed for ${files.length} files with ${warningCount} warning${warningCount === 1 ? '' : 's'}.`);
