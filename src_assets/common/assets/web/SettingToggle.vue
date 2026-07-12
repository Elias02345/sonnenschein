<template>
  <div class="st-row">
    <ToggleSwitch :model-value="checked" :input-id="id" @update:model-value="set" />
    <div class="st-text">
      <label :for="id">{{ $t(label) }}</label>
      <small v-if="desc" class="st-desc">{{ $t(desc) }}</small>
    </div>
  </div>
</template>

<script>
import ToggleSwitch from 'primevue/toggleswitch'

// Config-Werte kommen historisch als bool ODER String ("true"/"enabled"/...).
// Beim Zurückschreiben bleibt die vorhandene Repräsentation erhalten.
const STRING_PAIRS = [
  ['true', 'false'],
  ['1', '0'],
  ['enabled', 'disabled'],
  ['enable', 'disable'],
  ['yes', 'no'],
  ['on', 'off']
]

function boolPair(value) {
  if (value === true || value === false) return [true, false]
  if (value === 1 || value === 0) return [1, 0]
  const v = `${value}`.toLowerCase().trim()
  for (const p of STRING_PAIRS) {
    if (v === p[0] || v === p[1]) return p
  }
  return [true, false]
}

function isRawTruthy(value) {
  if (value === true || value === 1) return true
  const v = `${value}`.toLowerCase().trim()
  return STRING_PAIRS.some((p) => v === p[0])
}

export default {
  components: { ToggleSwitch },
  props: {
    id: { type: String, required: true },
    label: { type: String, required: true },
    desc: { type: String, default: null },
    modelValue: { required: true },
    inverseValues: { type: Boolean, default: false }
  },
  emits: ['update:modelValue'],
  computed: {
    checked() {
      const truthy = isRawTruthy(this.modelValue)
      return this.inverseValues ? !truthy : truthy
    }
  },
  methods: {
    set(newChecked) {
      const wantTruthy = this.inverseValues ? !newChecked : newChecked
      const pair = boolPair(this.modelValue)
      this.$emit('update:modelValue', wantTruthy ? pair[0] : pair[1])
    }
  }
}
</script>

<style scoped>
.st-row {
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
}

.st-text {
  display: flex;
  flex-direction: column;
}

.st-text label {
  font-weight: 600;
  cursor: pointer;
}

.st-desc {
  color: var(--p-text-muted-color);
}
</style>
