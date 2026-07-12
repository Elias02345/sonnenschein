<template>
  <div class="dash-page">
    <Topbar />

    <main class="dash-main">
      <h1>{{ $t('index.welcome') }}</h1>
      <p class="dash-sub">{{ $t('index.description') }}</p>

      <!-- Startup errors -->
      <Message v-if="fatalLogs.length" severity="error" :closable="false" icon="pi pi-exclamation-triangle">
        <span v-html="$t('index.startup_errors')" />
        <ul class="dash-fatal-list">
          <li v-for="(line, i) in fatalLogs" :key="i">{{ line }}</li>
        </ul>
        <div class="dash-fatal-actions">
          <a href="./troubleshooting#logs">
            <Button :label="$t('dashboard.view_logs')" severity="danger" size="small" />
          </a>
          <a :href="crashReportUrl" target="_blank">
            <Button :label="$t('dashboard.report_issue')" icon="pi pi-github" severity="secondary" size="small" />
          </a>
        </div>
      </Message>

      <div class="dash-grid">
        <!-- Version / update card -->
        <Card>
          <template #title>
            <span class="dash-card-title"><i class="pi pi-box" /> Version</span>
          </template>
          <template #content>
            <div v-if="version" class="dash-version">
              <div class="dash-version-row">
                <span class="dash-version-number">{{ version.version }}</span>
                <Tag v-if="loading" :value="$t('index.loading_latest')" severity="secondary" />
                <Tag v-else-if="buildVersionIsDirty" value="dev build" severity="info" />
                <Tag v-else-if="stableBuildAvailable" :value="$t('index.new_stable')" severity="warn" />
                <Tag v-else :value="$t('index.version_latest')" severity="success" />
              </div>
              <!-- Git-based update state (works for dev builds too) -->
              <div v-if="gitUpdateAvailable" class="dash-update">
                <p>{{ $t('dashboard.commits_behind', { n: updateState.available.commits_behind, branch: updateState.available.branch }) }}</p>
              </div>
              <p v-else-if="updateState && !gitUpdateAvailable" class="dash-muted dash-uptodate">
                <i class="pi pi-check-circle" /> {{ $t('dashboard.up_to_date') }}
              </p>

              <div v-if="stableBuildAvailable && githubVersion?.release" class="dash-update">
                <p>{{ githubVersion.release.name }}</p>
                <a :href="githubVersion.release.html_url" target="_blank">
                  <Button :label="$t('index.download')" icon="pi pi-external-link" severity="secondary" size="small" />
                </a>
              </div>

              <!-- Update controls: branch selector + trigger -->
              <div class="dash-update-controls">
                <label class="dash-branch-label" for="branch">{{ $t('dashboard.branch') }}</label>
                <Select id="branch" v-model="branch" :options="branchOptions" option-label="label"
                  option-value="value" size="small" class="dash-branch-select" />
                <Button :label="$t('dashboard.update_now')" icon="pi pi-refresh" size="small"
                  :loading="updating" @click="runUpdate" />
              </div>

              <Message v-if="updateMsg" :severity="updateError ? 'error' : 'success'" :closable="false"
                class="dash-update-msg">
                {{ updateMsg }}
              </Message>
            </div>
            <div v-else>
              <Tag :value="$t('index.loading_latest')" severity="secondary" />
            </div>
          </template>
        </Card>

        <!-- Paired devices card -->
        <Card>
          <template #title>
            <span class="dash-card-title"><i class="pi pi-mobile" /> {{ $t('dashboard.paired_devices') }}</span>
          </template>
          <template #content>
            <div v-if="clients.length" class="dash-clients">
              <div v-for="client in clients" :key="client.uuid" class="dash-client-row">
                <i class="pi pi-tablet" />
                <span class="dash-client-name">{{ client.name || $t('pin.unpair_single_unknown') }}</span>
                <Tag
                  :value="client.connected ? $t('dashboard.connected') : $t('dashboard.paired')"
                  :severity="client.connected ? 'success' : 'secondary'"
                />
              </div>
            </div>
            <p v-else class="dash-muted">{{ $t('dashboard.no_devices') }}</p>
            <div class="dash-card-actions">
              <a href="./pin">
                <Button :label="$t('dashboard.pair_new')" icon="pi pi-plus" size="small" />
              </a>
              <a v-if="clients.length" href="./pin#unpair">
                <Button :label="$t('dashboard.manage_devices')" icon="pi pi-pencil" severity="secondary" size="small" />
              </a>
            </div>
          </template>
        </Card>

        <!-- Moonlight clients card -->
        <Card>
          <template #title>
            <span class="dash-card-title"><i class="pi pi-download" /> {{ $t('client_card.clients') }}</span>
          </template>
          <template #content>
            <p class="dash-muted">{{ $t('client_card.clients_desc') }}</p>
            <div class="dash-card-actions">
              <a href="https://moonlight-stream.org" target="_blank">
                <Button label="Moonlight" icon="pi pi-external-link" severity="secondary" size="small" />
              </a>
              <a href="https://github.com/ClassicOldSong/moonlight-android" target="_blank">
                <Button label="Artemis (Android)" icon="pi pi-external-link" severity="secondary" size="small" />
              </a>
            </div>
          </template>
        </Card>
      </div>
    </main>
  </div>
</template>

<script>
import Button from 'primevue/button'
import Card from 'primevue/card'
import Message from 'primevue/message'
import Select from 'primevue/select'
import Tag from 'primevue/tag'
import ApolloVersion from './apollo_version'
import Topbar from './Topbar.vue'

export default {
  components: { Button, Card, Message, Select, Tag, Topbar },
  data() {
    return {
      version: null,
      githubVersion: null,
      loading: true,
      logs: null,
      clients: [],
      updating: false,
      updateMsg: null,
      updateError: false,
      updateState: null,
      branch: 'main',
      branchOptions: [
        { value: 'main', label: 'main (stable)' },
        { value: 'dev', label: 'dev (preview)' }
      ]
    }
  },
  computed: {
    stableBuildAvailable() {
      if (!this.githubVersion || !this.version) return false
      return this.githubVersion.isGreater(this.version)
    },
    buildVersionIsDirty() {
      return this.version?.version?.split('.').length === 5 && this.version.version.indexOf('dirty') !== -1
    },
    fatalLogs() {
      if (!this.logs) return []
      const regex = /(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}]):\s/g
      const raw = this.logs.split(regex).splice(1)
      const fatal = []
      for (let i = 0; i < raw.length; i += 2) {
        const line = raw[i + 1] || ''
        if (line.split(':')[0] === 'Fatal') fatal.push(line)
      }
      return fatal
    },
    // Crash reporter v1: pre-filled GitHub issue (crash.yml template).
    // Only what the user can see on this page is sent — no telemetry.
    crashReportUrl() {
      const params = new URLSearchParams({
        template: 'crash.yml',
        version: this.version?.version || 'unknown',
        stack: this.fatalLogs.join('\n')
      })
      return `https://github.com/Elias02345/sonnenschein/issues/new?${params.toString()}`
    },
    gitUpdateAvailable() {
      return this.updateState?.available?.available === true
    }
  },
  async created() {
    try {
      const config = await fetch('./api/config').then((r) => r.json())
      this.version = new ApolloVersion(null, config.version)
    } catch (e) {
      console.error(e)
    }
    try {
      this.githubVersion = new ApolloVersion(
        await fetch('https://api.github.com/repos/Elias02345/sonnenschein/releases/latest').then((r) => r.json()),
        null
      )
    } catch (e) {
      console.error(e)
    }
    try {
      this.logs = await fetch('./api/logs').then((r) => r.text())
    } catch (e) {
      console.error(e)
    }
    try {
      const resp = await fetch('./api/clients/list', { credentials: 'include' }).then((r) => r.json())
      if (resp.status && resp.named_certs) {
        this.clients = resp.named_certs
      }
    } catch (e) {
      console.error(e)
    }
    try {
      this.updateState = await fetch('./api/update-state', { credentials: 'include' }).then((r) => r.json())
      if (this.updateState?.available?.branch) {
        this.branch = this.updateState.available.branch
      }
    } catch (e) {
      console.error(e)
    }
    this.loading = false
  },
  methods: {
    runUpdate() {
      this.updating = true
      this.updateMsg = null
      this.updateError = false
      fetch('./api/update', {
        credentials: 'include',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify({ branch: this.branch })
      })
        .then((r) => r.json())
        .then((r) => {
          if (r.status) {
            this.updateMsg = this.$t('dashboard.update_started')
          } else {
            this.updateError = true
            this.updateMsg = r.error
          }
        })
        .catch((e) => {
          this.updateError = true
          this.updateMsg = e.message
        })
        .finally(() => {
          this.updating = false
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

.dash-main {
  max-width: 72rem;
  margin: 0 auto;
  padding: 1.5rem 1.25rem 3rem;
}

.dash-main h1 {
  margin: 0.5rem 0 0.25rem;
}

.dash-sub {
  color: var(--p-text-muted-color);
  margin-top: 0;
}

.dash-fatal-list {
  margin: 0.5rem 0;
  padding-left: 1.25rem;
}

.dash-fatal-actions {
  display: flex;
  gap: 0.5rem;
  flex-wrap: wrap;
}

.dash-fatal-actions a {
  text-decoration: none;
}

.dash-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(18rem, 1fr));
  gap: 1rem;
  margin-top: 1.25rem;
}

.dash-card-title {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
}

.dash-version-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  flex-wrap: wrap;
}

.dash-version-number {
  font-size: 1.4rem;
  font-weight: 700;
  font-variant-numeric: tabular-nums;
}

.dash-update {
  margin-top: 0.75rem;
}

.dash-update-msg {
  margin-top: 0.75rem;
}

.dash-update-controls {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  margin-top: 0.85rem;
  flex-wrap: wrap;
}

.dash-branch-label {
  font-size: 0.85rem;
  color: var(--p-text-muted-color);
}

.dash-branch-select {
  min-width: 9rem;
}

.dash-uptodate {
  display: inline-flex;
  align-items: center;
  gap: 0.4rem;
  margin: 0.6rem 0 0;
}

.dash-clients {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.dash-client-row {
  display: flex;
  align-items: center;
  gap: 0.6rem;
}

.dash-client-name {
  margin-right: auto;
}

.dash-muted {
  color: var(--p-text-muted-color);
  margin-top: 0;
}

.dash-card-actions {
  display: flex;
  gap: 0.5rem;
  margin-top: 1rem;
  flex-wrap: wrap;
}

.dash-card-actions a {
  text-decoration: none;
}
</style>
