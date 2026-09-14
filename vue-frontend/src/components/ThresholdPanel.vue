<template>
  <section class="panel threshold-panel">
    <div class="section-heading">
      <div><p class="eyebrow">远程设置</p><h2>报警阈值</h2></div>
      <span class="section-note">下发后由 STM32 更新运行阈值</span>
    </div>
    <form class="threshold-form" @submit.prevent="$emit('apply', { ...form })">
      <label><span>温度下限</span><div><input v-model.number="form.minitemp_set" type="number" min="-40" max="200" step="0.1" required><b>°C</b></div></label>
      <label><span>温度上限</span><div><input v-model.number="form.maxtemp_set" type="number" min="-40" max="200" step="0.1" required><b>°C</b></div></label>
      <label><span>湿度下限</span><div><input v-model.number="form.minihum_set" type="number" min="0" max="100" step="1" required><b>%</b></div></label>
      <label><span>湿度上限</span><div><input v-model.number="form.maxhum_set" type="number" min="0" max="100" step="1" required><b>%</b></div></label>
      <div class="threshold-form__action">
        <button class="primary-button" :disabled="saving">{{ saving ? '正在下发' : '下发到设备' }}</button>
        <span v-if="message" class="success-message">{{ message }}</span>
      </div>
    </form>
  </section>
</template>

<script setup>
import { reactive, watch } from 'vue'

const props = defineProps({
  thresholds: { type: Object, required: true },
  saving: { type: Boolean, default: false },
  message: { type: String, default: '' },
})
defineEmits(['apply'])
const form = reactive({ ...props.thresholds })
watch(() => props.thresholds, (value) => Object.assign(form, value), { deep: true })
</script>
