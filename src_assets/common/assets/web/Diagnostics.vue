<template>
  <div class="diag-wrap">
    <header class="diag-header">
      <h1>Sonnenschein — Diagnostics</h1>
      <Tag :value="overallTier" :severity="overallSeverity" />
    </header>
    <p class="diag-sub">
      Compositor, GPU and driver readiness for virtual-display streaming.
      This is the PrimeVue 4 foundation view; live probes are wired to the
      backend incrementally.
    </p>

    <div class="diag-grid">
      <Card v-for="item in checks" :key="item.key">
        <template #title>
          <span class="diag-card-title">
            <i :class="item.icon" />
            {{ item.title }}
          </span>
        </template>
        <template #content>
          <div class="diag-row">
            <span class="diag-value">{{ item.value }}</span>
            <Tag :value="item.status" :severity="item.severity" />
          </div>
          <small class="diag-hint">{{ item.hint }}</small>
        </template>
      </Card>
    </div>

    <div class="diag-actions">
      <Button label="Re-run probes" icon="pi pi-refresh" @click="refresh" :loading="loading" />
    </div>
  </div>
</template>

<script>
import Button from 'primevue/button'
import Card from 'primevue/card'
import Tag from 'primevue/tag'

export default {
  components: { Button, Card, Tag },
  data() {
    return {
      loading: false,
      config: null,
      checks: []
    }
  },
  computed: {
    overallTier() {
      if (!this.checks.length) return 'Unknown'
      if (this.checks.some((c) => c.severity === 'danger')) return 'Not ready'
      if (this.checks.some((c) => c.severity === 'warn')) return 'Partial'
      return 'Ready'
    },
    overallSeverity() {
      switch (this.overallTier) {
        case 'Ready':
          return 'success'
        case 'Partial':
          return 'warn'
        case 'Not ready':
          return 'danger'
        default:
          return 'secondary'
      }
    }
  },
  async created() {
    await this.refresh()
  },
  methods: {
    async refresh() {
      this.loading = true
      try {
        this.config = await fetch('./api/config').then((r) => r.json())
      } catch (e) {
        console.error('Failed to load config', e)
        this.config = null
      }
      this.checks = this.buildChecks()
      this.loading = false
    },
    buildChecks() {
      const platform = this.config?.platform ?? 'unknown'
      const isWayland = platform === 'linux'
      return [
        {
          key: 'platform',
          title: 'Platform',
          icon: 'pi pi-server',
          value: platform,
          status: isWayland ? 'linux' : platform,
          severity: isWayland ? 'success' : 'warn',
          hint: 'Sonnenschein targets Linux/Wayland first.'
        },
        {
          key: 'compositor',
          title: 'Compositor',
          icon: 'pi pi-desktop',
          value: 'Probe pending',
          status: 'TODO',
          severity: 'secondary',
          hint: 'KWin / Mutter / wlroots detection wired in a follow-up.'
        },
        {
          key: 'gpu',
          title: 'GPU / Encoder',
          icon: 'pi pi-bolt',
          value: 'Probe pending',
          status: 'TODO',
          severity: 'secondary',
          hint: 'NVENC / VAAPI / QSV capability probe wired in a follow-up.'
        },
        {
          key: 'version',
          title: 'Version',
          icon: 'pi pi-tag',
          value: this.config?.version ?? 'unknown',
          severity: this.config ? 'success' : 'danger',
          status: this.config ? 'online' : 'offline',
          hint: 'Reported by the running Sonnenschein server.'
        }
      ]
    }
  }
}
</script>

<style>
.diag-wrap {
  max-width: 960px;
  margin: 0 auto;
  padding: 2rem 1rem;
}

.diag-header {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.diag-sub {
  color: var(--p-text-muted-color);
  margin-bottom: 1.5rem;
}

.diag-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  gap: 1rem;
}

.diag-card-title {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.diag-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
}

.diag-value {
  font-weight: 600;
}

.diag-hint {
  display: block;
  margin-top: 0.5rem;
  color: var(--p-text-muted-color);
}

.diag-actions {
  margin-top: 1.5rem;
}
</style>
