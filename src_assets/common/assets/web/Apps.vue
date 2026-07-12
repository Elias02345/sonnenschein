<template>
  <div class="dash-page">
    <Topbar />

    <main class="apps-main">
      <!-- Listenansicht -->
      <template v-if="!showEditForm">
        <h1>{{ $t('apps.applications_title') }}</h1>
        <p class="apps-muted">{{ $t('apps.applications_desc') }}</p>
        <p class="apps-muted">{{ $t('apps.applications_reorder_desc') }}</p>
        <p class="apps-muted apps-prewrap">{{ $t('apps.applications_tips') }}</p>

        <Card>
          <template #content>
            <div class="apps-list">
              <div
                v-for="(app, i) in apps"
                :key="app.uuid || 'placeholder'"
                class="apps-row"
                :class="{ dragover: app.dragover }"
                draggable="true"
                @dragstart="onDragStart($event, i)"
                @dragenter="onDragEnter($event, app)"
                @dragover="onDragOver($event)"
                @dragleave="onDragLeave(app)"
                @dragend="onDragEnd()"
                @drop="onDrop($event, app, i)"
              >
                <i class="pi pi-bars apps-grip" />
                <span class="apps-name" @dblclick="exportLauncherFile(app)">{{ app.name || ' ' }}</span>
                <div v-if="app.uuid" class="apps-actions">
                  <Button icon="pi pi-pencil" size="small" severity="secondary" :disabled="actionDisabled"
                    :aria-label="$t('_common.save')" @click="editApp(app)" />
                  <Button icon="pi pi-trash" size="small" severity="danger" :disabled="actionDisabled"
                    aria-label="Delete" @click="showDeleteForm(app)" />
                  <Button v-if="currentApp === app.uuid" icon="pi pi-stop" size="small" severity="warn"
                    :disabled="actionDisabled" aria-label="Stop" @click="closeApp()" />
                  <a v-else
                    :href="'art://launch?host_uuid=' + hostUUID + '&host_name=' + hostName + '&app_uuid=' + app.uuid + '&app_name=' + app.name"
                    @click.prevent="launchApp($event, app)">
                    <Button icon="pi pi-play" size="small" severity="success" :disabled="actionDisabled"
                      aria-label="Launch" />
                  </a>
                </div>
              </div>
            </div>

            <div class="apps-list-footer">
              <Button :label="$t('apps.add_new')" icon="pi pi-plus" :disabled="actionDisabled" @click="newApp" />
              <Button v-if="!listReordered" :label="$t('apps.alphabetize')" icon="pi pi-sort-alpha-down"
                severity="secondary" :disabled="actionDisabled" @click="alphabetizeApps" />
              <Button v-else :label="$t('apps.save_order')" icon="pi pi-save" severity="warn"
                :disabled="actionDisabled" @click="saveOrder" />
            </div>
          </template>
        </Card>
      </template>

      <!-- Editieransicht -->
      <template v-else>
        <h1>{{ editForm.name || '< NO NAME >' }}</h1>
        <div class="apps-edit-toolbar">
          <Button :label="$t('_common.cancel')" icon="pi pi-times" severity="secondary" @click="showEditForm = false" />
          <Button :label="$t('_common.save')" icon="pi pi-save"
            :disabled="actionDisabled || !editForm.name.trim()" @click="save" />
          <Button :label="$t('apps.export_launcher_file')" icon="pi pi-upload" severity="success"
            :disabled="!editForm.uuid" @click="exportLauncherFile(editForm)" />
        </div>

        <Card class="apps-edit-card">
          <template #content>
            <div class="apps-form">
              <!-- Name + Cover-Finder -->
              <div class="apps-field">
                <label for="appName">{{ $t('apps.app_name') }}</label>
                <div class="apps-name-group">
                  <InputText id="appName" v-model="editForm.name" class="apps-grow" />
                  <Button :label="$t('apps.find_cover')" severity="secondary" icon="pi pi-image"
                    @click="showCoverFinder" />
                </div>
                <small class="apps-muted">{{ $t('apps.app_name_desc') }}</small>
              </div>

              <!-- Bild -->
              <div class="apps-field">
                <label for="appImagePath">{{ $t('apps.image') }}</label>
                <InputText id="appImagePath" v-model="editForm['image-path']" class="apps-mono" />
                <small class="apps-muted">{{ $t('apps.image_desc') }}</small>
              </div>

              <!-- Gamepad-Override -->
              <div class="apps-field">
                <label for="gamepad">{{ $t('config.gamepad') }}</label>
                <Select id="gamepad" v-model="editForm.gamepad" :options="gamepadOptions"
                  option-label="label" option-value="value" />
                <small class="apps-muted">{{ $t('config.gamepad_desc') }}</small>
              </div>

              <!-- Kommando -->
              <div class="apps-field">
                <label for="appCmd">{{ $t('apps.cmd') }}</label>
                <InputText id="appCmd" v-model="editForm.cmd" class="apps-mono" />
                <small class="apps-muted">
                  {{ $t('apps.cmd_desc') }}<br>
                  <b>{{ $t('_common.note') }}</b> {{ $t('apps.cmd_note') }}
                </small>
              </div>

              <!-- Detached-Kommandos -->
              <div class="apps-field">
                <label>{{ $t('apps.detached_cmds') }}</label>
                <div v-for="(c, i) in editForm.detached" :key="i" class="apps-cmd-row">
                  <InputText v-model="editForm.detached[i]" class="apps-mono apps-grow" />
                  <Button icon="pi pi-trash" severity="danger" text @click="editForm.detached.splice(i, 1)" />
                  <Button icon="pi pi-plus" severity="success" text @click="editForm.detached.splice(i, 0, '')" />
                </div>
                <div>
                  <Button :label="$t('apps.detached_cmds_add')" icon="pi pi-plus" severity="success" size="small"
                    @click="editForm.detached.push('')" />
                </div>
                <small class="apps-muted">
                  {{ $t('apps.detached_cmds_desc') }}<br>
                  <b>{{ $t('_common.note') }}</b> {{ $t('apps.detached_cmds_note') }}
                </small>
              </div>

              <SettingToggle id="clientCommands" label="apps.allow_client_commands"
                desc="apps.allow_client_commands_desc" v-model="editForm['allow-client-commands']" />

              <!-- Prep- und State-Kommandos -->
              <template v-for="type in ['prep', 'state']" :key="type">
                <SettingToggle :id="'excludeGlobal_' + type" :label="'apps.global_' + type + '_name'"
                  :desc="'apps.global_' + type + '_desc'" v-model="editForm['exclude-global-' + type + '-cmd']"
                  inverse-values />
                <div class="apps-field">
                  <label>{{ $t('apps.cmd_' + type + '_name') }}</label>
                  <small class="apps-muted apps-prewrap">{{ $t('apps.cmd_' + type + '_desc') }}</small>
                  <div v-if="editForm[type + '-cmd'].length > 0" class="apps-cmd-table">
                    <div class="apps-cmd-head">
                      <span><i class="pi pi-play" /> {{ $t('_common.do_cmd') }}</span>
                      <span><i class="pi pi-undo" /> {{ $t('_common.undo_cmd') }}</span>
                      <span></span>
                    </div>
                    <div v-for="(c, i) in editForm[type + '-cmd']" :key="i" class="apps-cmd-grid">
                      <InputText v-model="c.do" class="apps-mono" />
                      <InputText v-model="c.undo" class="apps-mono" />
                      <div class="apps-cmd-btns">
                        <Button icon="pi pi-trash" severity="danger" text
                          @click="editForm[type + '-cmd'].splice(i, 1)" />
                        <Button icon="pi pi-plus" severity="success" text
                          @click="addCmd(editForm[type + '-cmd'], i)" />
                      </div>
                    </div>
                  </div>
                  <div>
                    <Button :label="$t('apps.add_cmds')" icon="pi pi-plus" severity="success" size="small"
                      @click="addCmd(editForm[type + '-cmd'], -1)" />
                  </div>
                </div>
              </template>

              <!-- Arbeitsverzeichnis -->
              <div class="apps-field">
                <label for="appWorkingDir">{{ $t('apps.working_dir') }}</label>
                <InputText id="appWorkingDir" v-model="editForm['working-dir']" class="apps-mono" />
                <small class="apps-muted">{{ $t('apps.working_dir_desc') }}</small>
              </div>

              <!-- Output -->
              <div class="apps-field">
                <label for="appOutput">{{ $t('apps.output_name') }}</label>
                <InputText id="appOutput" v-model="editForm.output" class="apps-mono" />
                <small class="apps-muted">{{ $t('apps.output_desc') }}</small>
              </div>

              <SettingToggle id="autoDetach" label="apps.auto_detach" desc="apps.auto_detach_desc"
                v-model="editForm['auto-detach']" />
              <SettingToggle id="waitAll" label="apps.wait_all" desc="apps.wait_all_desc"
                v-model="editForm['wait-all']" />
              <SettingToggle id="terminateOnPause" label="apps.terminate_on_pause"
                desc="apps.terminate_on_pause_desc" v-model="editForm['terminate-on-pause']" />

              <!-- Exit-Timeout -->
              <div class="apps-field">
                <label for="exitTimeout">{{ $t('apps.exit_timeout') }}</label>
                <InputNumber id="exitTimeout" v-model="editForm['exit-timeout']" :min="0" placeholder="5"
                  show-buttons />
                <small class="apps-muted">{{ $t('apps.exit_timeout_desc') }}</small>
              </div>

              <SettingToggle id="virtualDisplay" label="apps.virtual_display" desc="apps.virtual_display_desc"
                v-model="editForm['virtual-display']" />
              <SettingToggle id="useAppIdentity" label="apps.use_app_identity" desc="apps.use_app_identity_desc"
                v-model="editForm['use-app-identity']" />
              <SettingToggle v-if="isTruthy(editForm['use-app-identity'])" id="perClientAppIdentity"
                label="apps.per_client_app_identity" desc="apps.per_client_app_identity_desc"
                v-model="editForm['per-client-app-identity']" />

              <!-- Umgebungsvariablen-Referenz -->
              <Message severity="info" :closable="false" class="apps-env">
                <h4>{{ $t('apps.env_vars_about') }}</h4>
                <p>{{ $t('apps.env_vars_desc') }}</p>
                <div class="apps-env-table">
                  <div v-for="row in envVars" :key="row.name" class="apps-env-row">
                    <code>{{ row.name }}</code>
                    <span>{{ $t(row.desc) }}</span>
                  </div>
                </div>
                <p class="apps-env-example">
                  <b>{{ $t('apps.env_xrandr_example') }}</b>
                  <code class="apps-env-pre">sh -c "xrandr --output HDMI-1 --mode \"${APOLLO_CLIENT_WIDTH}x${APOLLO_CLIENT_HEIGHT}\" --rate ${APOLLO_CLIENT_FPS}"</code>
                </p>
                <a href="https://docs.lizardbyte.dev/projects/sunshine/latest/md_docs_2app__examples.html"
                  target="_blank">{{ $t('_common.see_more') }}</a>
              </Message>

              <Message severity="info" :closable="false" icon="pi pi-info-circle">
                {{ $t('apps.env_sunshine_compatibility') }}
              </Message>

              <div class="apps-edit-toolbar">
                <Button :label="$t('_common.cancel')" icon="pi pi-times" severity="secondary"
                  @click="showEditForm = false" />
                <Button :label="$t('_common.save')" icon="pi pi-save"
                  :disabled="actionDisabled || !editForm.name.trim()" @click="save" />
              </div>
            </div>
          </template>
        </Card>
      </template>
    </main>

    <!-- Cover-Finder -->
    <Dialog v-model:visible="coverFinderVisible" :header="$t('apps.covers_found')" modal
      class="apps-cover-dialog" :style="{ width: '40rem', maxWidth: '95vw' }">
      <div class="apps-cover-grid" :class="{ busy: coverFinderBusy }">
        <div v-if="coverSearching" class="apps-cover-item">
          <div class="apps-cover-box">
            <i class="pi pi-spin pi-spinner apps-cover-spinner" />
          </div>
          <label>{{ $t('apps.loading') }}</label>
        </div>
        <div v-for="cover in coverCandidates" :key="cover.key" class="apps-cover-item result"
          @click="useCover(cover)">
          <div class="apps-cover-box">
            <img :src="cover.url" />
          </div>
          <label>{{ cover.name }}</label>
        </div>
        <p v-if="!coverSearching && !coverCandidates.length" class="apps-muted">—</p>
      </div>
    </Dialog>
  </div>
</template>

<script>
import Button from 'primevue/button'
import Card from 'primevue/card'
import Dialog from 'primevue/dialog'
import InputNumber from 'primevue/inputnumber'
import InputText from 'primevue/inputtext'
import Message from 'primevue/message'
import Select from 'primevue/select'
import SettingToggle from './SettingToggle.vue'
import Topbar from './Topbar.vue'

const NEW_APP = {
  'name': 'New App',
  'output': '',
  'cmd': '',
  'exclude-global-prep-cmd': false,
  'exclude-global-state-cmd': false,
  'elevated': false,
  'auto-detach': true,
  'wait-all': true,
  'exit-timeout': 5,
  'prep-cmd': [],
  'state-cmd': [],
  'detached': [],
  'image-path': '',
  'scale-factor': 100,
  'use-app-identity': false,
  'per-client-app-identity': false,
  'allow-client-commands': true,
  'virtual-display': false,
  'terminate-on-pause': false,
  'gamepad': ''
}

const ENV_VARS = [
  { name: 'APOLLO_APP_ID', desc: 'apps.env_app_id' },
  { name: 'APOLLO_APP_NAME', desc: 'apps.env_app_name' },
  { name: 'APOLLO_APP_UUID', desc: 'apps.env_app_uuid' },
  { name: 'APOLLO_APP_STATUS', desc: 'apps.env_app_status' },
  { name: 'APOLLO_CLIENT_UUID', desc: 'apps.env_client_uuid' },
  { name: 'APOLLO_CLIENT_NAME', desc: 'apps.env_client_name' },
  { name: 'APOLLO_CLIENT_WIDTH', desc: 'apps.env_client_width' },
  { name: 'APOLLO_CLIENT_HEIGHT', desc: 'apps.env_client_height' },
  { name: 'APOLLO_CLIENT_FPS', desc: 'apps.env_client_fps' },
  { name: 'APOLLO_CLIENT_HDR', desc: 'apps.env_client_hdr' },
  { name: 'APOLLO_CLIENT_GCMAP', desc: 'apps.env_client_gcmap' },
  { name: 'APOLLO_CLIENT_HOST_AUDIO', desc: 'apps.env_client_host_audio' },
  { name: 'APOLLO_CLIENT_ENABLE_SOPS', desc: 'apps.env_client_enable_sops' },
  { name: 'APOLLO_CLIENT_AUDIO_CONFIGURATION', desc: 'apps.env_client_audio_config' }
]

export default {
  components: { Button, Card, Dialog, InputNumber, InputText, Message, Select, SettingToggle, Topbar },
  data() {
    return {
      apps: [],
      showEditForm: false,
      actionDisabled: false,
      editForm: null,
      coverSearching: false,
      coverFinderBusy: false,
      coverFinderVisible: false,
      coverCandidates: [],
      currentApp: '',
      draggingApp: -1,
      hostName: '',
      hostUUID: '',
      listReordered: false,
      envVars: ENV_VARS
    }
  },
  computed: {
    gamepadOptions() {
      return [
        { value: '', label: this.$t('_common.default_global') },
        { value: 'disabled', label: this.$t('_common.disabled') },
        { value: 'auto', label: this.$t('_common.auto') },
        { value: 'ds5', label: this.$t('config.gamepad_ds5') },
        { value: 'switch', label: this.$t('config.gamepad_switch') },
        { value: 'xone', label: this.$t('config.gamepad_xone') }
      ]
    }
  },
  created() {
    this.loadApps()
  },
  methods: {
    isTruthy(v) {
      return v === true || v === 1 || ['true', '1', 'enabled', 'enable', 'yes', 'on'].includes(`${v}`.toLowerCase().trim())
    },
    onDragStart(e, idx) {
      if (this.showEditForm) {
        e.preventDefault()
        return
      }
      this.draggingApp = idx
      this.apps.push({})
    },
    onDragEnter(e, app) {
      if (this.draggingApp < 0) return
      e.preventDefault()
      e.dataTransfer.dropEffect = 'move'
      app.dragover = true
    },
    onDragOver(e) {
      if (this.draggingApp < 0) return
      e.preventDefault()
      e.dataTransfer.dropEffect = 'move'
    },
    onDragLeave(app) {
      app.dragover = false
    },
    onDragEnd() {
      this.draggingApp = -1
      this.apps.pop()
    },
    onDrop(e, app, idx) {
      app.dragover = false
      if (this.draggingApp < 0) return
      e.preventDefault()
      if (idx === this.draggingApp || idx - 1 === this.draggingApp) return
      const draggedApp = this.apps[this.draggingApp]
      this.apps.splice(this.draggingApp, 1)
      if (idx > this.draggingApp) idx -= 1
      this.apps.splice(idx, 0, draggedApp)
      this.listReordered = true
    },
    alphabetizeApps() {
      let orderStat = 0
      this.apps.sort((a, b) => {
        const result = a.name.localeCompare(b.name)
        orderStat += result
        return result
      })
      this.listReordered = orderStat !== this.apps.length - 1
      if (!this.listReordered) {
        alert(this.$t('apps.already_ordered'))
      }
    },
    saveOrder() {
      this.actionDisabled = true
      const reorderedUUIDs = this.apps.filter((i) => i.uuid).map((i) => i.uuid)
      fetch('./api/apps/reorder', {
        credentials: 'include',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify({ order: reorderedUUIDs })
      })
        .then((r) => r.json())
        .then((r) => {
          if (!r.status) {
            alert(this.$t('apps.reorder_failed') + r.error)
          }
        })
        .finally(() => this.loadApps())
        .finally(() => {
          this.actionDisabled = false
        })
    },
    loadApps() {
      return fetch('./api/apps', { credentials: 'include' })
        .then((r) => r.json())
        .then((r) => {
          this.apps = r.apps.filter((i) => i.uuid).map((i) => ({ ...i, launching: false, dragover: false }))
          this.currentApp = r.current_app
          this.hostName = r.host_name
          this.hostUUID = r.host_uuid
          this.listReordered = false
        })
    },
    newApp() {
      this.editForm = Object.assign({}, NEW_APP)
      this.showEditForm = true
    },
    launchApp(event, app) {
      const isLocalHost = ['localhost', '127.0.0.1', '[::1]'].indexOf(location.hostname) >= 0
      if (!isLocalHost && confirm(this.$t('apps.launch_local_client'))) {
        const link = document.createElement('a')
        link.href = event.currentTarget?.href || event.target.closest('a').href
        link.click()
        return
      }
      if (confirm(this.$t('apps.launch_warning'))) {
        this.actionDisabled = true
        fetch('./api/apps/launch', {
          credentials: 'include',
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
          body: JSON.stringify({ uuid: app.uuid })
        })
          .then((r) => r.json())
          .then((r) => {
            if (!r.status) {
              alert(this.$t('apps.launch_failed') + r.error)
            }
          })
          .finally(() => {
            this.actionDisabled = false
            this.loadApps()
          })
      }
    },
    closeApp() {
      if (confirm(this.$t('apps.close_warning'))) {
        this.actionDisabled = true
        fetch('./api/apps/close', {
          credentials: 'include',
          headers: { 'Content-Type': 'application/json' },
          method: 'POST'
        })
          .then((r) => r.json())
          .then((r) => {
            if (!r.status) {
              alert(this.$t('apps.close_failed'))
            }
          })
          .finally(() => {
            this.actionDisabled = false
            this.loadApps()
          })
      }
    },
    editApp(app) {
      this.editForm = Object.assign({}, NEW_APP, JSON.parse(JSON.stringify(app)))
      this.showEditForm = true
    },
    exportLauncherFile(app) {
      const link = document.createElement('a')
      const fileContent = `# Artemis app entry
# Exported by Sonnenschein
# https://github.com/Elias02345/sonnenschein

[host_uuid] ${this.hostUUID}
[host_name] ${this.hostName}
[app_uuid] ${app.uuid}
[app_name] ${app.name}
`
      link.href = `data:text/plain;charset=utf-8,${encodeURIComponent(fileContent)}`
      link.download = `${app.name}.art`
      link.click()
    },
    showDeleteForm(app) {
      if (!confirm('Are you sure to delete ' + app.name + '?')) return
      this.actionDisabled = true
      fetch('./api/apps/delete', {
        credentials: 'include',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify({ uuid: app.uuid })
      })
        .then((r) => r.json())
        .then((r) => {
          if (!r.status) {
            alert('Delete failed! ' + r.error)
          }
        })
        .finally(() => {
          this.actionDisabled = false
          this.loadApps()
        })
    },
    addCmd(cmdArr, idx) {
      const template = { do: '', undo: '' }
      if (idx < 0) {
        cmdArr.push(template)
      } else {
        cmdArr.splice(idx, 0, template)
      }
    },
    showCoverFinder() {
      this.coverCandidates = []
      this.coverSearching = true
      this.coverFinderVisible = true

      function getSearchBucket(name) {
        const bucket = name.substring(0, Math.min(name.length, 2)).toLowerCase().replaceAll(/[^a-z\d]/g, '')
        return bucket || '@'
      }

      function searchCovers(name) {
        if (!name) return Promise.resolve([])
        const searchName = name.replaceAll(/\s+/g, '.').toLowerCase()
        const dbUrl = 'https://raw.githubusercontent.com/LizardByte/GameDB/gh-pages'
        const bucket = getSearchBucket(name)
        return fetch(`${dbUrl}/buckets/${bucket}.json`)
          .then((r) => {
            if (!r.ok) throw new Error('Failed to search covers')
            return r.json()
          })
          .then((maps) =>
            Promise.all(
              Object.keys(maps)
                .map((id) => {
                  const item = maps[id]
                  if (item.name.replaceAll(/\s+/g, '.').toLowerCase().startsWith(searchName)) {
                    return fetch(`${dbUrl}/games/${id}.json`).then((r) => r.json()).catch(() => null)
                  }
                  return null
                })
                .filter((item) => item)
            )
          )
          .then((results) =>
            results
              .filter((item) => item && item.cover && item.cover.url)
              .map((game) => {
                const thumb = game.cover.url
                const dotIndex = thumb.lastIndexOf('.')
                const slashIndex = thumb.lastIndexOf('/')
                if (dotIndex < 0 || slashIndex < 0) return null
                const slug = thumb.substring(slashIndex + 1, dotIndex)
                return {
                  name: game.name,
                  key: `igdb_${game.id}`,
                  url: `https://images.igdb.com/igdb/image/upload/t_cover_big/${slug}.jpg`,
                  saveUrl: `https://images.igdb.com/igdb/image/upload/t_cover_big_2x/${slug}.png`
                }
              })
              .filter((item) => item)
          )
      }

      searchCovers(this.editForm.name.toString().trim())
        .then((list) => (this.coverCandidates = list))
        .finally(() => (this.coverSearching = false))
    },
    useCover(cover) {
      this.coverFinderBusy = true
      fetch('./api/covers/upload', {
        credentials: 'include',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify({ key: cover.key, url: cover.saveUrl })
      })
        .then((r) => {
          if (!r.ok) throw new Error('Failed to download covers')
          return r.json()
        })
        .then((body) => (this.editForm['image-path'] = body.path))
        .then(() => (this.coverFinderVisible = false))
        .finally(() => (this.coverFinderBusy = false))
    },
    save() {
      this.editForm.name = this.editForm.name.trim()
      if (!this.editForm.name) return
      this.editForm['exit-timeout'] = parseInt(this.editForm['exit-timeout']) || 5
      this.editForm['scale-factor'] = parseInt(this.editForm['scale-factor']) || 100
      this.editForm['image-path'] = this.editForm['image-path'].toString().trim().replace(/"/g, '')
      delete this.editForm.id
      delete this.editForm.launching
      delete this.editForm.dragover
      fetch('./api/apps', {
        credentials: 'include',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify(this.editForm)
      })
        .then((r) => r.json())
        .then((r) => {
          if (!r.status) {
            alert(this.$t('apps.save_failed') + r.error)
            throw new Error(`App save failed: ${r.error}`)
          }
        })
        .then(() => {
          this.showEditForm = false
          this.loadApps()
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

.apps-main {
  max-width: 60rem;
  margin: 0 auto;
  padding: 1.5rem 1.25rem 3rem;
}

.apps-muted {
  color: var(--p-text-muted-color);
  margin: 0.25rem 0;
}

.apps-prewrap {
  white-space: pre-wrap;
}

.apps-list {
  display: flex;
  flex-direction: column;
}

.apps-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.5rem 0.25rem;
  border-bottom: 1px solid var(--p-content-border-color);
  min-height: 3rem;
}

.apps-row.dragover {
  border-top: 2px solid var(--p-primary-color);
}

.apps-grip {
  color: var(--p-text-muted-color);
  cursor: grab;
}

.apps-name {
  margin-right: auto;
  font-weight: 600;
}

.apps-actions {
  display: flex;
  gap: 0.35rem;
}

.apps-actions a {
  text-decoration: none;
}

.apps-list-footer {
  display: flex;
  justify-content: space-between;
  gap: 0.5rem;
  margin-top: 1rem;
  flex-wrap: wrap;
}

.apps-edit-toolbar {
  display: flex;
  gap: 0.5rem;
  margin: 0.75rem 0;
  flex-wrap: wrap;
}

.apps-edit-card {
  margin-top: 0.5rem;
}

.apps-form {
  display: flex;
  flex-direction: column;
  gap: 1.1rem;
}

.apps-field {
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
}

.apps-field > label {
  font-weight: 600;
}

.apps-name-group {
  display: flex;
  gap: 0.5rem;
}

.apps-grow {
  flex: 1;
}

.apps-mono :deep(input),
.apps-mono {
  font-family: monospace;
}

.apps-cmd-row {
  display: flex;
  gap: 0.35rem;
  margin-bottom: 0.35rem;
}

.apps-cmd-head,
.apps-cmd-grid {
  display: grid;
  grid-template-columns: 1fr 1fr auto;
  gap: 0.35rem;
  align-items: center;
  margin-bottom: 0.35rem;
}

.apps-cmd-head {
  font-weight: 600;
  color: var(--p-text-muted-color);
}

.apps-cmd-btns {
  display: flex;
}

.apps-env h4 {
  margin: 0 0 0.25rem;
}

.apps-env-table {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
  margin: 0.5rem 0;
}

.apps-env-row {
  display: grid;
  grid-template-columns: minmax(16rem, auto) 1fr;
  gap: 0.75rem;
  border-bottom: 1px solid var(--p-content-border-color);
  padding-bottom: 0.25rem;
}

.apps-env-pre {
  display: block;
  white-space: pre-wrap;
  margin-top: 0.25rem;
}

.apps-cover-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(9rem, 1fr));
  gap: 0.75rem;
  max-height: 24rem;
  overflow-y: auto;
}

.apps-cover-grid.busy * {
  cursor: wait !important;
  pointer-events: none;
}

.apps-cover-item.result {
  cursor: pointer;
}

.apps-cover-box {
  position: relative;
  padding-top: 133.33%;
  border-radius: 8px;
  overflow: hidden;
  background: var(--p-content-border-color);
}

.apps-cover-box img {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.apps-cover-spinner {
  position: absolute;
  inset: 0;
  margin: auto;
  width: 2rem;
  height: 2rem;
  font-size: 2rem;
}

.apps-cover-item label {
  display: block;
  text-align: center;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  margin-top: 0.25rem;
}
</style>
