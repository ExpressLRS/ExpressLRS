import babelParser from '@babel/eslint-parser';
import litPlugin from 'eslint-plugin-lit';

export default [
    {
        ignores: ['dist/**', 'headers/**', 'node_modules/**', '.vite/**']
    },
    {
        files: ['src/**/*.js', 'dev-mock-plugin.js', 'vite.config.js'],
        languageOptions: {
            parser: babelParser,
            parserOptions: {
                requireConfigFile: false,
                babelOptions: {
                    plugins: [['@babel/plugin-proposal-decorators', {version: '2023-11'}]]
                }
            },
            sourceType: 'module',
            ecmaVersion: 'latest'
        },
        plugins: {
            lit: litPlugin
        },
        rules: {
            'lit/no-invalid-html': 'error',
            'lit/no-useless-template-literals': 'warn',
            'lit/attribute-value-entities': 'warn',
            'lit/binding-positions': 'error',
            'lit/no-property-change-update': 'warn'
        }
    }
];
