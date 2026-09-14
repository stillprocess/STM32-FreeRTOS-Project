<template>
  <article class="metric-card" :class="{ 'metric-card--alarm': alarm }">
    <div class="metric-card__head">
      <span class="metric-card__icon" aria-hidden="true">{{ icon }}</span>
      <span class="metric-card__state">{{ alarm ? '超出范围' : '范围正常' }}</span>
    </div>
    <p class="metric-card__label">{{ label }}</p>
    <p class="metric-card__value">{{ value === null ? '--' : value }}<span>{{ unit }}</span></p>
    <div class="metric-card__range">
      <span>{{ low }}{{ unit }}</span>
      <div class="metric-card__track"><i :style="{ width: gaugeWidth }"></i></div>
      <span>{{ high }}{{ unit }}</span>
    </div>
  </article>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  label: { type: String, required: true },
  icon: { type: String, required: true },
  value: { type: Number, default: null },
  unit: { type: String, required: true },
  low: { type: Number, required: true },
  high: { type: Number, required: true },
})

const alarm = computed(() => props.value !== null && (props.value >= props.high || props.value <= props.low))
const gaugeWidth = computed(() => {
  if (props.value === null || props.high === props.low) return '0%'
  const ratio = (props.value - props.low) / (props.high - props.low)
  return `${Math.min(100, Math.max(0, ratio * 100))}%`
})
</script>
