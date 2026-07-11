<template>
  <div class="dash-page">
    <!-- Top bar -->
    <header class="dash-topbar">
      <a class="dash-brand" href="./" title="Sonnenschein">
        <img src="/images/logo-apollo-45.png" height="36" alt="" />
        <span>Sonnenschein</span>
      </a>
      <nav class="dash-nav">
        <a href="./pin"><i class="pi pi-link" /> {{ $t('navbar.pin') }}</a>
        <a href="./apps"><i class="pi pi-play" /> {{ $t('navbar.applications') }}</a>
        <a href="./config"><i class="pi pi-cog" /> {{ $t('navbar.configuration') }}</a>
        <a href="./password"><i class="pi pi-shield" /> {{ $t('navbar.password') }}</a>
        <a href="./troubleshooting"><i class="pi pi-wrench" /> {{ $t('navbar.troubleshoot') }}</a>
        <a href="./diagnostics"><i class="pi pi-heart" /> {{ $t('dashboard.diagnostics') }}</a>
      </nav>
      <Button
        class="dash-theme-toggle"
        :icon="dark ? 'pi pi-sun' : 'pi pi-moon'"
        severity="secondary"
        text
        rounded
        :aria-label="$t('dashboard.toggle_theme')"
        @click="toggleTheme"
      />
    </header>

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
              <div v-if="stableBuildAvailable && githubVersion?.release" class="dash-update">
                <p>{{ githubVersion.release.name }}</p>
                <a :href="githubVersion.release.html_url" target="_blank">
                  <Button :label="$t('index.download')" icon="pi pi-download" size="small" />
                </a>
              </div>
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
import Tag from 'primevue/tag'
import ApolloVersion from './apollo_version'

export default {
  components: { Button, Card, Message, Tag },
  data() {
    return {
      version: null,
      githubVersion: null,
      loading: true,
      logs: null,
      clients: [],
      dark: document.documentElement.classList.contains('sonnenschein-dark')
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
    this.loading = false
  },
  methods: {
    toggleTheme() {
      this.dark = !this.dark
      document.documentElement.classList.toggle('sonnenschein-dark', this.dark)
      localStorage.setItem('theme', this.dark ? 'dark' : 'light')
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

.dash-topbar {
  display: flex;
  align-items: center;
  gap: 1.25rem;
  padding: 0.6rem 1.25rem;
  border-bottom: 1px solid var(--p-content-border-color);
  flex-wrap: wrap;
}

.dash-brand {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  font-weight: 700;
  font-size: 1.1rem;
  color: var(--p-text-color);
  text-decoration: none;
}

.dash-nav {
  display: flex;
  gap: 0.35rem;
  flex-wrap: wrap;
  margin-right: auto;
}

.dash-nav a {
  display: inline-flex;
  align-items: center;
  gap: 0.4rem;
  padding: 0.45rem 0.7rem;
  border-radius: 8px;
  color: var(--p-text-muted-color);
  text-decoration: none;
  font-size: 0.95rem;
}

.dash-nav a:hover {
  background: var(--p-content-hover-background, rgba(128, 128, 128, 0.12));
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
