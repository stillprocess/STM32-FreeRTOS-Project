<template>
  <main class="dashboard-shell">
    <header class="topbar">
      <div>
        <p class="eyebrow">STM32 · FreeRTOS · OneNET</p>
        <h1>环境监测控制台</h1>
      </div>
      <div class="topbar__actions">
        <div class="connection-state" :class="{ 'connection-state--online': connected }">
          <i></i>
          <span>{{ connected ? '设备数据在线' : '设备数据离线' }}</span>
        </div>
        <button class="refresh-button" :disabled="loading" @click="fetchData">
          {{ loading ? '刷新中' : '刷新数据' }}
        </button>
      </div>
    </header>

    <section class="summary-row">
      <div>
        <span>设备数据时间</span>
        <strong>{{ formatTime(lastDataAt) }}</strong>
      </div>
      <div>
        <span>页面刷新时间</span>
        <strong>{{ formatTime(lastUpdate) }}</strong>
      </div>
      <label>
        <span>自动刷新</span>
        <select v-model.number="interval" @change="restartPolling">
          <option :value="5000">5 秒</option>
          <option :value="10000">10 秒</option>
          <option :value="30000">30 秒</option>
          <option :value="60000">60 秒</option>
        </select>
      </label>
    </section>

    <div v-if="error" class="error-banner">{{ error }}</div>

    <div v-if="alerts.length" class="alarm-banner">
      <strong>环境报警</strong>
      <span v-for="item in alerts" :key="item">{{ item }}</span>
    </div>

    <section class="metrics-grid">
      <MetricCard
        label="当前温度"
        icon="T"
        :value="temperature"
        unit="°C"
        :low="thresholds.minitemp_set"
        :high="thresholds.maxtemp_set"
      />
      <MetricCard
        label="当前湿度"
        icon="H"
        :value="humidity"
        unit="%"
        :low="thresholds.minihum_set"
        :high="thresholds.maxhum_set"
      />
    </section>

    <ThresholdPanel
      :thresholds="thresholds"
      :saving="saving"
      :message="saveMessage"
      @apply="saveThresholds"
    />

    <HistoryChart :history="history" @clear="clearHistory" />
  </main>
</template>

<script setup>
import { onMounted, onUnmounted, ref } from 'vue'
import HistoryChart from './components/HistoryChart.vue'
import MetricCard from './components/MetricCard.vue'
import ThresholdPanel from './components/ThresholdPanel.vue'
import { useDevice } from './composables/useDevice'

const {
  temperature, humidity, thresholds, history, loading, saving, error,
  lastUpdate, lastDataAt, connected, alerts,
  fetchData, applyThresholds, startPolling, stopPolling, clearHistory,
} = useDevice()

const interval = ref(10000)
const saveMessage = ref('')
let noticeTimer = null

onMounted(() => startPolling(interval.value))
onUnmounted(() => {
  stopPolling()
  if (noticeTimer) window.clearTimeout(noticeTimer)
})

function restartPolling() {
  startPolling(interval.value)
}

async function saveThresholds(values) {
  saveMessage.value = ''
  try {
    await applyThresholds(values)
    saveMessage.value = '设置指令已发送'
    noticeTimer = window.setTimeout(() => { saveMessage.value = '' }, 3000)
  } catch {
    saveMessage.value = ''
  }
}

function formatTime(value) {
  return value ? value.toLocaleString('zh-CN', { hour12: false }) : '--'
}
</script>
