import { createHmac } from 'node:crypto'
import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'

const ONENET_BASE_URL = 'https://iot-api.heclouds.com'
const propertyKeys = new Set([
  'maxtemp_set',
  'minitemp_set',
  'maxhum_set',
  'minihum_set',
])

function createAuthorization(env) {
  const version = '2022-05-01'
  const method = 'sha1'
  const expires = Math.floor(Date.now() / 1000) + 3600
  const resource = `userid/${env.ONENET_USER_ID}`
  const source = `${expires}\n${method}\n${resource}\n${version}`
  const key = Buffer.from(env.ONENET_ACCESS_KEY, 'base64')
  const sign = createHmac('sha1', key).update(source).digest('base64')

  return new URLSearchParams({
    version,
    res: resource,
    et: String(expires),
    method,
    sign,
  }).toString()
}

function validateEnvironment(env) {
  const names = ['ONENET_USER_ID', 'ONENET_ACCESS_KEY', 'ONENET_PRODUCT_ID', 'ONENET_DEVICE_NAME']
  const missing = names.filter((name) => !env[name])
  if (missing.length) throw new Error(`请先在 .env 中填写：${missing.join(', ')}`)
}

function readJsonBody(request) {
  return new Promise((resolve, reject) => {
    let body = ''
    request.on('data', (chunk) => {
      body += chunk
      if (body.length > 65536) reject(new Error('请求内容过大'))
    })
    request.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {})
      } catch {
        reject(new Error('请求格式错误'))
      }
    })
    request.on('error', reject)
  })
}

function sendJson(response, status, payload) {
  response.statusCode = status
  response.setHeader('Content-Type', 'application/json; charset=utf-8')
  response.end(JSON.stringify(payload))
}

async function callOneNet(env, path, options = {}) {
  validateEnvironment(env)
  const response = await fetch(`${ONENET_BASE_URL}${path}`, {
    ...options,
    headers: {
      authorization: createAuthorization(env),
      'Content-Type': 'application/json',
      ...(options.headers || {}),
    },
  })
  const result = await response.json().catch(() => ({}))
  if (!response.ok || result.code !== 0) {
    throw new Error(result.msg || `OneNET 返回错误 ${response.status}`)
  }
  return result
}

function createApiMiddleware(env) {
  return async (request, response, next) => {
    try {
      if (request.method === 'GET' && request.url?.startsWith('/api/device')) {
        const query = new URLSearchParams({
          product_id: env.ONENET_PRODUCT_ID || '',
          device_name: env.ONENET_DEVICE_NAME || '',
        })
        const result = await callOneNet(env, `/thingmodel/query-device-property?${query}`)
        const properties = {}
        for (const item of result.data || []) {
          if (item.identifier) {
            properties[item.identifier] = { value: item.value, time: item.time }
          }
        }
        return sendJson(response, 200, { ok: true, properties })
      }

      if (request.method === 'POST' && request.url?.startsWith('/api/thresholds')) {
        const input = await readJsonBody(request)
        const params = {}
        for (const [key, raw] of Object.entries(input)) {
          if (!propertyKeys.has(key)) continue
          const value = Number(raw)
          if (!Number.isFinite(value)) throw new Error(`${key} 不是有效数值`)
          params[key] = value
        }
        if (Object.keys(params).length !== 4) throw new Error('必须同时提交四个阈值')

        await callOneNet(env, '/thingmodel/set-device-property', {
          method: 'POST',
          body: JSON.stringify({
            product_id: env.ONENET_PRODUCT_ID,
            device_name: env.ONENET_DEVICE_NAME,
            params,
          }),
        })
        return sendJson(response, 200, { ok: true })
      }

      next()
    } catch (error) {
      sendJson(response, 500, { ok: false, message: error.message || '服务请求失败' })
    }
  }
}

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  const api = createApiMiddleware(env)
  return {
    plugins: [
      vue(),
      {
        name: 'onenet-local-api',
        configureServer(server) {
          server.middlewares.use(api)
        },
        configurePreviewServer(server) {
          server.middlewares.use(api)
        },
      },
    ],
    server: { host: '127.0.0.1', port: 5173 },
    preview: { host: '127.0.0.1', port: 4173 },
  }
})
