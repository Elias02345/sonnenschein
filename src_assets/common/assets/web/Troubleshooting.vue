<template>
  <div class="dash-page">
    <Topbar />

    <main class="ts-main">
      <h1>{{ $t('troubleshooting.troubleshooting') }}</h1>

      <div class="ts-grid">
        <!-- Force close app -->
        <Card>
          <template #title>
            <span class="ts-card-title"><i class="pi pi-times-circle" /> {{ $t('troubleshooting.force_close') }}</span>
          </template>
          <template #content>
            <p class="ts-muted">{{ $t('troubleshooting.force_close_desc') }}</p>
            <Message v-if="closeAppStatus === true" severity="success" :closable="false">
              {{ $t('troubleshooting.force_close_success') }}
            </Message>
            <Message v-if="closeAppStatus === false" severity="error" :closable="false">
              {{ $t('troubleshooting.force_close_error') }}
            </Message>
            <Button
              :label="$t('troubleshooting.force_close')"
              severity="warn"
              :loading="closeAppPressed"
              @click="closeApp"
            />
          </template>
        </Card>

        <!-- Restart -->
        <Card>
          <template #title>
            <span class="ts-card-title"><i class="pi pi-refresh" /> {{ $t('troubleshooting.restart_apollo') }}</span>
          </template>
          <template #content>
            <p class="ts-muted">{{ $t('troubleshooting.restart_apollo_desc') }}</p>
            <Message v-if="serverRestarting" severity="success" :closable="false">
              {{ $t('troubleshooting.restart_apollo_success') }}
            </Message>
            <Button
              :label="$t('troubleshooting.restart_apollo')"
              severity="warn"
              :disabled="serverQuitting || serverRestarting"
              @click="restart"
            />
          </template>
        </Card>

        <!-- Quit -->
        <Card>
          <template #title>
            <span class="ts-card-title"><i class="pi pi-power-off" /> {{ $t('troubleshooting.quit_apollo') }}</span>
          </template>
          <template #content>
            <p class="ts-muted">{{ $t('troubleshooting.quit_apollo_desc') }}</p>
            <Message v-if="serverQuit" severity="success" :closable="false">
              {{ $t('troubleshooting.quit_apollo_success') }}
            </Message>
            <Message v-if="serverQuitting" severity="success" :closable="false">
              {{ $t('troubleshooting.quit_apollo_success_ongoing') }}
            </Message>
            <Button
              :label="$t('troubleshooting.quit_apollo')"
              severity="danger"
              :disabled="serverQuitting || serverRestarting"
              @click="quit"
            />
          </template>
        </Card>
      </div>

      <!-- Logs -->
      <Card id="logs" class="ts-logs-card">
        <template #title>
          <div class="ts-logs-header">
            <span class="ts-card-title"><i class="pi pi-file" /> {{ $t('troubleshooting.logs') }}</span>
            <InputText v-model="logFilter" :placeholder="$t('troubleshooting.logs_find')" class="ts-log-filter" />
          </div>
        </template>
        <template #content>
          <p class="ts-muted">{{ $t('troubleshooting.logs_desc') }}</p>
          <div class="ts-logs">
            <Button
              class="ts-copy"
              icon="pi pi-copy"
              severity="secondary"
              text
              rounded
              aria-label="Copy"
              @click="copyLogs"
            />{{ actualLogs }}
          </div>
        </template>
      </Card>
    </main>
  </div>
</template>

<script>
import Button from 'primevue/button'
import Card from 'primevue/card'
import InputText from 'primevue/inputtext'
import Message from 'primevue/message'
import Topbar from './Topbar.vue'

export default {
  components: { Button, Card, InputText, Message, Topbar },
  data() {
    return {
      closeAppPressed: false,
      closeAppStatus: null,
      logs: 'Loading...',
      logFilter: null,
      logInterval: null,
      serverRestarting: false,
      serverQuitting: false,
      serverQuit: false
    }
  },
  computed: {
    actualLogs() {
      if (!this.logFilter) return this.logs
      return this.logs
        .split('\n')
        .filter((x) => x.indexOf(this.logFilter) !== -1)
        .join('\n')
    }
  },
  created() {
    this.logInterval = setInterval(() => this.refreshLogs(), 5000)
    this.refreshLogs()
  },
  beforeUnmount() {
    clearInterval(this.logInterval)
  },
  methods: {
    refreshLogs() {
      fetch('./api/logs', { credentials: 'include' })
        .then((response) => {
          const contentType = response.headers.get('Content-Type') || ''
          const charsetMatch = contentType.match(/charset=([^;]+)/i)
          const charset = charsetMatch ? charsetMatch[1].trim() : 'utf-8'
          return response.arrayBuffer().then((buffer) => new TextDecoder(charset).decode(buffer))
        })
        .then((text) => {
          this.logs = text
        })
        .catch((error) => console.error('Error fetching logs:', error))
    },
    copyLogs() {
      navigator.clipboard.writeText(this.actualLogs)
    },
    closeApp() {
      this.closeAppPressed = true
      fetch('./api/apps/close', {
        credentials: 'include',
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
      })
        .then((r) => r.json())
        .then((r) => {
          this.closeAppPressed = false
          this.closeAppStatus = r.status
          setTimeout(() => {
            this.closeAppStatus = null
          }, 5000)
        })
        .catch(() => {
          this.closeAppPressed = false
          this.closeAppStatus = false
        })
    },
    restart() {
      this.serverRestarting = true
      setTimeout(() => {
        this.serverRestarting = false
      }, 5000)
      fetch('./api/restart', {
        credentials: 'include',
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
      }).catch(() => {
        this.serverRestarting = false
        setTimeout(() => location.reload(), 1000)
      })
    },
    quit() {
      if (!window.confirm(this.$t('troubleshooting.quit_apollo_confirm'))) return
      this.serverQuitting = true
      fetch('./api/quit', {
        credentials: 'include',
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
      })
        .then(() => {
          // Server antwortet noch = Quit hat nicht gegriffen.
          this.serverQuitting = false
          this.serverQuit = false
        })
        .catch(() => {
          // Verbindung weg = Server ist tatsächlich beendet.
          this.serverQuitting = false
          this.serverQuit = true
        })
    }
  }
}
</script>

<style scoped>
.dash-page {
  min-height: 100vh;
  background: var(--p-content-background);
  color: var(--p-text-color);
}

.ts-main {
  max-width: 72rem;
  margin: 0 auto;
  padding: 1.5rem 1.25rem 3rem;
}

.ts-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(18rem, 1fr));
  gap: 1rem;
  margin: 1.25rem 0;
}

.ts-card-title {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
}

.ts-muted {
  color: var(--p-text-muted-color);
  margin-top: 0;
}

.ts-logs-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  flex-wrap: wrap;
}

.ts-log-filter {
  width: 18rem;
  max-width: 100%;
}

.ts-logs {
  position: relative;
  white-space: pre;
  font-family: monospace;
  font-size: 0.85rem;
  overflow: auto;
  max-height: 500px;
  min-height: 300px;
  border: 1px solid var(--p-content-border-color);
  border-radius: 8px;
  padding: 0.75rem;
}

.ts-copy {
  position: sticky;
  top: 0;
  float: right;
}
</style>
