import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const root = join(here, '..')
const manifestPath = join(root, 'manifest.json')
const pagePath = join(root, 'src', 'pages', 'index', 'index.ux')

const manifest = JSON.parse(await readFile(manifestPath, 'utf8'))
const ux = await readFile(pagePath, 'utf8')

assert.equal(manifest.package, 'com.openvela.contest2026.team133.auraspace')
assert.equal(manifest.router.entry, 'pages/index')
assert.equal(manifest.config.designWidth, 466)
assert.ok(manifest.features.some((item) => item.name === 'system.velaclaw'))
assert.match(ux, /<template>[\s\S]*?<div class="app-shell">/)
assert.match(ux, /<script>[\s\S]*?import velaclaw from '@system\.velaclaw'/)
assert.match(ux, /<style>[\s\S]*?\.app-shell/)
assert.doesNotMatch(ux, /秦嘉一/)

const script = ux.match(/<script>([\s\S]*?)<\/script>/)?.[1]
assert.ok(script, 'index.ux must contain a script block')

const syntaxOnly = script
  .replace(/^import\s+.*$/gm, '')
  .replace('export default', 'const component =')

new Function(syntaxOnly)

console.log('Aura-Space quick app source checks passed.')
