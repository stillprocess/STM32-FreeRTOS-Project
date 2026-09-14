async function request(path, options = {}) {
  const response = await fetch(path, {
    ...options,
    headers: {
      'Content-Type': 'application/json',
      ...(options.headers || {}),
    },
  })

  const data = await response.json().catch(() => ({}))
  if (!response.ok || data.ok === false) {
    throw new Error(data.message || 'OneNET 请求失败')
  }
  return data
}

export function queryDeviceProperties() {
  return request('/api/device')
}

export function setDeviceThresholds(params) {
  return request('/api/thresholds', {
    method: 'POST',
    body: JSON.stringify(params),
  })
}
