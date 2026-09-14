<template>
  <section class="panel history-panel">
    <div class="section-heading">
      <div><p class="eyebrow">最近 {{ history.length }} 条</p><h2>温湿度趋势</h2></div>
      <div class="section-actions">
        <button class="quiet-button" :disabled="history.length === 0" @click="exportCsv">导出 CSV</button>
        <button class="quiet-button" :disabled="history.length === 0" @click="$emit('clear')">清空</button>
      </div>
    </div>
    <div class="chart-wrap">
      <canvas v-show="history.length" ref="canvas"></canvas>
      <p v-if="history.length === 0" class="empty-state">获取到设备数据后，这里会生成趋势曲线。</p>
    </div>
  </section>
</template>

<script setup>
import { nextTick, onUnmounted, ref, watch } from 'vue'
import Chart from 'chart.js/auto'

const props = defineProps({ history: { type: Array, required: true } })
defineEmits(['clear'])
const canvas = ref(null)
let chart = null

function render() {
  nextTick(() => {
    if (!canvas.value || !props.history.length) return
    const labels = props.history.map((item) => item.time.toLocaleTimeString('zh-CN'))
    const temperatures = props.history.map((item) => item.temperature)
    const humidities = props.history.map((item) => item.humidity)
    if (chart) {
      chart.data.labels = labels
      chart.data.datasets[0].data = temperatures
      chart.data.datasets[1].data = humidities
      chart.update('none')
      return
    }
    chart = new Chart(canvas.value, {
      type: 'line',
      data: {
        labels,
        datasets: [
          { label: '温度 °C', data: temperatures, borderColor: '#ff8a4c', backgroundColor: 'rgba(255,138,76,.12)', tension: .34, fill: true, pointRadius: 2, yAxisID: 'temperature' },
          { label: '湿度 %', data: humidities, borderColor: '#37c9d0', backgroundColor: 'rgba(55,201,208,.08)', tension: .34, fill: true, pointRadius: 2, yAxisID: 'humidity' },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: { mode: 'index', intersect: false },
        plugins: { legend: { labels: { color: '#c7d5df', usePointStyle: true } } },
        scales: {
          x: { ticks: { color: '#778d9d', maxTicksLimit: 8 }, grid: { color: 'rgba(127,153,168,.1)' } },
          temperature: { position: 'left', ticks: { color: '#ff8a4c' }, grid: { color: 'rgba(127,153,168,.1)' } },
          humidity: { position: 'right', min: 0, max: 100, ticks: { color: '#37c9d0' }, grid: { drawOnChartArea: false } },
        },
      },
    })
  })
}
watch(() => props.history.length, render, { immediate: true })
onUnmounted(() => chart?.destroy())

function exportCsv() {
  const rows = props.history.map((item) => [item.time.toLocaleString('zh-CN'), item.temperature, item.humidity].join(','))
  const csv = '\uFEFF时间,温度(°C),湿度(%)\n' + rows.join('\n')
  const url = URL.createObjectURL(new Blob([csv], { type: 'text/csv;charset=utf-8' }))
  const link = document.createElement('a')
  link.href = url
  link.download = `sensor-history-${Date.now()}.csv`
  link.click()
  URL.revokeObjectURL(url)
}
</script>
