import { computed, ref } from 'vue'
import { queryDeviceProperties, setDeviceThresholds } from '../api/onenet'

const temperature = ref(null)
const humidity = ref(null)
const thresholds = ref({
  maxtemp_set: 30,
  minitemp_set: 20,
  maxhum_set: 70,
  minihum_set: 40,
})
const history = ref([])
const loading = ref(false)
const saving = ref(false)
const error = ref('')
const lastUpdate = ref(null)
const lastDataAt = ref(null)
let timer = null

const connected = computed(() => {
  if (!lastDataAt.value) return false
  return Date.now() - lastDataAt.value.getTime() < 180000
})

const alerts = computed(() => {
  const result = []
  if (temperature.value !== null) {
    if (temperature.value >= thresholds.value.maxtemp_set) result.push(`温度达到上限：${temperature.value} °C`)
    else if (temperature.value <= thresholds.value.minitemp_set) result.push(`温度达到下限：${temperature.value} °C`)
  }
  if (humidity.value !== null) {
    if (humidity.value >= thresholds.value.maxhum_set) result.push(`湿度达到上限：${humidity.value} %`)
    else if (humidity.value <= thresholds.value.minihum_set) result.push(`湿度达到下限：${humidity.value} %`)
  }
  return result
})

function readNumber(properties, identifier) {
  const value = Number(properties?.[identifier]?.value)
  return Number.isFinite(value) ? value : null
}

async function fetchData() {
  if (loading.value) return
  loading.value = true
  error.value = ''
  try {
    const { properties } = await queryDeviceProperties()
    const nextTemp = readNumber(properties, 'temp_value')
    const nextHumi = readNumber(properties, 'humidity_value')
    temperature.value = nextTemp
    humidity.value = nextHumi

    for (const key of Object.keys(thresholds.value)) {
      const value = readNumber(properties, key)
      if (value !== null) thresholds.value[key] = value
    }

    const times = ['temp_value', 'humidity_value']
      .map((key) => Number(properties?.[key]?.time))
      .filter(Number.isFinite)
    if (times.length) lastDataAt.value = new Date(Math.max(...times))
    lastUpdate.value = new Date()

    if (nextTemp !== null && nextHumi !== null) {
      history.value.push({ time: new Date(), temperature: nextTemp, humidity: nextHumi })
      if (history.value.length > 200) history.value = history.value.slice(-200)
    }
  } catch (cause) {
    error.value = cause.message
  } finally {
    loading.value = false
  }
}

async function applyThresholds(next) {
  if (next.minitemp_set >= next.maxtemp_set) throw new Error('温度下限必须小于温度上限')
  if (next.minihum_set >= next.maxhum_set) throw new Error('湿度下限必须小于湿度上限')

  saving.value = true
  error.value = ''
  try {
    await setDeviceThresholds(next)
    thresholds.value = { ...next }
  } catch (cause) {
    error.value = cause.message
    throw cause
  } finally {
    saving.value = false
  }
}

function startPolling(interval = 10000) {
  stopPolling()
  fetchData()
  timer = window.setInterval(fetchData, interval)
}

function stopPolling() {
  if (timer !== null) {
    window.clearInterval(timer)
    timer = null
  }
}

function clearHistory() {
  history.value = []
}

export function useDevice() {
  return {
    temperature, humidity, thresholds, history, loading, saving, error,
    lastUpdate, lastDataAt, connected, alerts,
    fetchData, applyThresholds, startPolling, stopPolling, clearHistory,
  }
}
