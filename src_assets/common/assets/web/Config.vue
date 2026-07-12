<template>
  <div class="dash-page">
    <Topbar />

    <main class="cfg-main">
      <h1>{{ $t('config.configuration') }}</h1>

      <div v-if="config">
        <!-- Tab-Leiste -->
        <nav class="cfg-tabs">
          <button v-for="tab in tabs" :key="tab.id" class="cfg-tab" :class="{ active: tab.id === currentTab }"
            @click="currentTab = tab.id">
            {{ tab.name }}
          </button>
        </nav>

        <!-- Tab-Inhalte: bestehende Bootstrap-Komponenten, unverändert -->
        <div class="cfg-content">
          <general v-if="currentTab === 'general'" :config="config" :global-prep-cmd="global_prep_cmd"
            :global-state-cmd="global_state_cmd" :server-cmd="server_cmd" :platform="platform"></general>
          <inputs v-if="currentTab === 'input'" :config="config" :platform="platform"></inputs>
          <audio-video v-if="currentTab === 'av'" :config="config" :platform="platform"
            :vdisplay="vdisplayStatus"></audio-video>
          <network v-if="currentTab === 'network'" :config="config" :platform="platform"></network>
          <files v-if="currentTab === 'files'" :config="config" :platform="platform"></files>
          <advanced v-if="currentTab === 'advanced'" :config="config" :platform="platform"></advanced>
          <container-encoders :current-tab="currentTab" :config="config" :platform="platform"></container-encoders>
        </div>
      </div>

      <Message v-if="saved && !restarted" severity="success" :closable="false" class="cfg-msg">
        <b>{{ $t('_common.success') }}</b> {{ $t('config.apply_note') }}
      </Message>
      <Message v-if="restarted" severity="success" :closable="false" class="cfg-msg">
        <b>{{ $t('_common.success') }}</b> {{ $t('config.restart_note') }}
      </Message>

      <div class="cfg-buttons">
        <Button :label="$t('_common.save')" icon="pi pi-save" @click="save" />
        <Button v-if="saved && !restarted" :label="$t('_common.apply')" icon="pi pi-check" severity="success"
          @click="apply" />
      </div>
    </main>
  </div>
</template>

<script>
import { computed } from 'vue'
import Button from 'primevue/button'
import Message from 'primevue/message'
import Topbar from './Topbar.vue'
import General from './configs/tabs/General.vue'
import Inputs from './configs/tabs/Inputs.vue'
import Network from './configs/tabs/Network.vue'
import Files from './configs/tabs/Files.vue'
import Advanced from './configs/tabs/Advanced.vue'
import AudioVideo from './configs/tabs/AudioVideo.vue'
import ContainerEncoders from './configs/tabs/ContainerEncoders.vue'

let fallbackDisplayModeCache = ''

export default {
  components: {
    Button,
    Message,
    Topbar,
    General,
    Inputs,
    Network,
    Files,
    Advanced,
    AudioVideo,
    ContainerEncoders
  },
  data() {
    return {
      platform: '',
      saved: false,
      restarted: false,
      config: null,
      currentTab: 'general',
      vdisplayStatus: '1',
      global_prep_cmd: [],
      global_state_cmd: [],
      server_cmd: [],
      tabs: [
        {
          id: 'general',
          name: 'General',
          options: {
            'locale': 'en',
            'sunshine_name': '',
            'min_log_level': 2,
            'global_prep_cmd': [],
            'global_state_cmd': [],
            'server_cmd': [],
            'notify_pre_releases': 'disabled',
            'system_tray': 'enabled',
            'hide_tray_controls': 'disabled',
            'enable_pairing': 'enabled',
            'enable_discovery': 'enabled'
          }
        },
        {
          id: 'input',
          name: 'Input',
          options: {
            'controller': 'enabled',
            'gamepad': 'auto',
            'ds4_back_as_touchpad_click': 'enabled',
            'motion_as_ds4': 'enabled',
            'touchpad_as_ds4': 'enabled',
            'ds5_inputtino_randomize_mac': 'enabled',
            'back_button_timeout': -1,
            'keyboard': 'enabled',
            'key_repeat_delay': 500,
            'key_repeat_frequency': 24.9,
            'always_send_scancodes': 'enabled',
            'key_rightalt_to_key_win': 'disabled',
            'mouse': 'enabled',
            'high_resolution_scrolling': 'enabled',
            'native_pen_touch': 'enabled',
            'enable_input_only_mode': 'disabled',
            'forward_rumble': 'enabled',
            'keybindings': '[0x10,0xA0,0x11,0xA2,0x12,0xA4]'
          }
        },
        {
          id: 'av',
          name: 'Audio/Video',
          options: {
            'audio_sink': '',
            'virtual_sink': '',
            'stream_audio': 'enabled',
            'install_steam_audio_drivers': 'enabled',
            'keep_sink_default': 'enabled',
            'auto_capture_sink': 'enabled',
            'adapter_name': '',
            'output_name': '',
            'fallback_mode': '',
            'dd_configuration_option': 'disabled',
            'dd_resolution_option': 'auto',
            'dd_manual_resolution': '',
            'dd_refresh_rate_option': 'auto',
            'dd_manual_refresh_rate': '',
            'dd_hdr_option': 'auto',
            'dd_wa_hdr_toggle_delay': 0,
            'dd_config_revert_delay': 3000,
            'dd_config_revert_on_disconnect': 'disabled',
            'dd_mode_remapping': { 'mixed': [], 'resolution_only': [], 'refresh_rate_only': [] },
            'dd_wa_hdr_toggle': 'disabled',
            'headless_mode': 'disabled',
            'double_refreshrate': 'disabled',
            'max_bitrate': 0,
            'minimum_fps_target': 0,
            'isolated_virtual_display_option': 'disabled'
          }
        },
        {
          id: 'network',
          name: 'Network',
          options: {
            'upnp': 'disabled',
            'address_family': 'ipv4',
            'port': 47989,
            'origin_web_ui_allowed': 'lan',
            'external_ip': '',
            'lan_encryption_mode': 0,
            'wan_encryption_mode': 1,
            'ping_timeout': 10000
          }
        },
        {
          id: 'files',
          name: 'Config Files',
          options: {
            'file_apps': '',
            'credentials_file': '',
            'log_path': '',
            'pkey': '',
            'cert': '',
            'file_state': ''
          }
        },
        {
          id: 'advanced',
          name: 'Advanced',
          options: {
            'fec_percentage': 20,
            'qp': 28,
            'min_threads': 2,
            'limit_framerate': 'enabled',
            'envvar_compatibility_mode': 'disabled',
            'legacy_ordering': 'disabled',
            'ignore_encoder_probe_failure': 'disabled',
            'hevc_mode': 0,
            'av1_mode': 0,
            'capture': '',
            'encoder': ''
          }
        },
        {
          id: 'nv',
          name: 'NVIDIA NVENC Encoder',
          options: {
            'nvenc_preset': 1,
            'nvenc_twopass': 'quarter_res',
            'nvenc_spatial_aq': 'disabled',
            'nvenc_vbv_increase': 0,
            'nvenc_realtime_hags': 'enabled',
            'nvenc_latency_over_power': 'enabled',
            'nvenc_opengl_vulkan_on_dxgi': 'enabled',
            'nvenc_h264_cavlc': 'disabled',
            'nvenc_intra_refresh': 'disabled'
          }
        },
        {
          id: 'qsv',
          name: 'Intel QuickSync Encoder',
          options: {
            'qsv_preset': 'medium',
            'qsv_coder': 'auto',
            'qsv_slow_hevc': 'disabled'
          }
        },
        {
          id: 'amd',
          name: 'AMD AMF Encoder',
          options: {
            'amd_usage': 'ultralowlatency',
            'amd_rc': 'vbr_latency',
            'amd_enforce_hrd': 'disabled',
            'amd_quality': 'balanced',
            'amd_preanalysis': 'disabled',
            'amd_vbaq': 'enabled',
            'amd_coder': 'auto'
          }
        },
        {
          id: 'vt',
          name: 'VideoToolbox Encoder',
          options: {
            'vt_coder': 'auto',
            'vt_software': 'auto',
            'vt_realtime': 'enabled'
          }
        },
        {
          id: 'vaapi',
          name: 'VA-API Encoder',
          options: {
            'vaapi_strict_rc_buffer': 'disabled'
          }
        },
        {
          id: 'sw',
          name: 'Software Encoder',
          options: {
            'sw_preset': 'superfast',
            'sw_tune': 'zerolatency'
          }
        }
      ]
    }
  },
  provide() {
    return {
      platform: computed(() => this.platform)
    }
  },
  created() {
    fetch('./api/config', { credentials: 'include' })
      .then((r) => r.json())
      .then((r) => {
        this.config = r
        this.platform = this.config.platform

        if (this.platform === 'windows') {
          this.tabs = this.tabs.filter((el) => el.id !== 'vt' && el.id !== 'vaapi')
        }
        if (this.platform === 'linux') {
          this.tabs = this.tabs.filter((el) => el.id !== 'amd' && el.id !== 'qsv' && el.id !== 'vt')
        }
        if (this.platform === 'macos') {
          this.tabs = this.tabs.filter(
            (el) => el.id !== 'amd' && el.id !== 'nv' && el.id !== 'qsv' && el.id !== 'vaapi'
          )
        }

        // Werte entfernen, die nicht in die Config-Datei gehören
        delete this.config.platform
        delete this.config.status
        delete this.config.version

        this.vdisplayStatus = this.config.vdisplayStatus
        delete this.config.vdisplayStatus

        fallbackDisplayModeCache = this.config.fallback_mode || ''

        const specialOptions = ['dd_mode_remapping', 'global_prep_cmd', 'global_state_cmd', 'server_cmd']
        for (const optionKey of specialOptions) {
          if (typeof this.config[optionKey] === 'string') {
            this.config[optionKey] = JSON.parse(this.config[optionKey])
          } else {
            this.config[optionKey] = null
          }
        }

        this.config.dd_mode_remapping ??= { mixed: [], resolution_only: [], refresh_rate_only: [] }
        this.config.global_prep_cmd ??= []
        this.config.global_state_cmd ??= []
        this.config.server_cmd ??= []

        // Default-Werte aus den Tab-Optionen auffüllen
        this.tabs.forEach((tab) => {
          Object.keys(tab.options).forEach((optionKey) => {
            if (this.config[optionKey] === undefined) {
              this.config[optionKey] = JSON.parse(JSON.stringify(tab.options[optionKey]))
            }
          })
        })

        if (this.platform === 'windows') {
          this.global_prep_cmd = this.config.global_prep_cmd.map((i) => {
            i.elevated = !!i.elevated
            return i
          })
          this.global_state_cmd = this.config.global_state_cmd.map((i) => {
            i.elevated = !!i.elevated
            return i
          })
          this.server_cmd = this.config.server_cmd.map((i) => {
            i.elevated = !!i.elevated
            return i
          })
        } else {
          this.global_prep_cmd = this.config.global_prep_cmd
          this.global_state_cmd = this.config.global_state_cmd
          this.server_cmd = this.config.server_cmd
        }
      })
  },
  mounted() {
    const handleHash = () => {
      const hash = window.location.hash
      if (!hash) return
      const strippedHash = hash.substring(1)
      this.tabs.forEach((tab) => {
        Object.keys(tab.options).forEach((key) => {
          if (tab.id === strippedHash || key === strippedHash) {
            this.currentTab = tab.id
          }
          if (key === strippedHash) {
            setTimeout(() => {
              const element = document.getElementById(strippedHash)
              if (element) {
                window.location.hash = hash
              }
            }, 2000)
          }
        })
      })
    }

    handleHash()
    window.addEventListener('hashchange', handleHash)
  },
  methods: {
    forceUpdate() {
      this.$forceUpdate()
    },
    serialize() {
      if (this.config.fallback_mode && !this.config.fallback_mode.match(/^\d+x\d+x\d+$/)) {
        this.config.fallback_mode = fallbackDisplayModeCache
      } else {
        fallbackDisplayModeCache = this.config.fallback_mode
      }
      const config = JSON.parse(JSON.stringify(this.config))
      config.global_prep_cmd = this.global_prep_cmd.filter(
        (cmd) => (cmd.do && cmd.do.trim()) || (cmd.undo && cmd.undo.trim())
      )
      config.global_state_cmd = this.global_state_cmd.filter(
        (cmd) => (cmd.do && cmd.do.trim()) || (cmd.undo && cmd.undo.trim())
      )
      config.server_cmd = this.server_cmd.filter((cmd) => cmd.name && cmd.cmd && cmd.name.trim() && cmd.cmd.trim())
      return config
    },
    save() {
      this.saved = false
      this.restarted = false

      const config = this.serialize()

      // Default-Werte nicht in die Config-Datei schreiben
      this.tabs.forEach((tab) => {
        Object.keys(tab.options).forEach((optionKey) => {
          if (JSON.stringify(config[optionKey]) === JSON.stringify(tab.options[optionKey])) {
            delete config[optionKey]
          }
        })
      })

      return fetch('./api/config', {
        credentials: 'include',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify(config)
      }).then((r) => {
        if (r.status === 200) {
          this.saved = true
          return this.saved
        }
        return false
      })
    },
    apply() {
      this.saved = this.restarted = false
      this.save().then((result) => {
        if (result !== true) return
        this.restarted = true
        setTimeout(() => {
          this.saved = this.restarted = false
        }, 5000)
        fetch('./api/restart', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' }
        })
          .then((resp) => {
            if (resp.status !== 200) {
              location.reload()
            }
          })
          .catch((e) => {
            console.error(e)
            setTimeout(() => location.reload(), 1000)
          })
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

.cfg-main {
  max-width: 72rem;
  margin: 0 auto;
  padding: 1.5rem 1.25rem 3rem;
}

.cfg-tabs {
  display: flex;
  flex-wrap: wrap;
  gap: 0.25rem;
  border-bottom: 1px solid var(--p-content-border-color);
  margin-bottom: 0;
}

.cfg-tab {
  appearance: none;
  background: none;
  border: none;
  border-bottom: 2px solid transparent;
  padding: 0.6rem 0.9rem;
  color: var(--p-text-muted-color);
  font-size: 0.95rem;
  cursor: pointer;
  border-radius: 8px 8px 0 0;
}

.cfg-tab:hover {
  background: var(--p-content-hover-background, rgba(128, 128, 128, 0.12));
  color: var(--p-text-color);
}

.cfg-tab.active {
  color: var(--p-primary-color);
  border-bottom-color: var(--p-primary-color);
  font-weight: 600;
}

.cfg-content {
  border: 1px solid var(--p-content-border-color);
  border-top: none;
  border-radius: 0 0 8px 8px;
  padding: 0.5rem 1rem 1rem;
}

.cfg-msg {
  margin-top: 1rem;
}

.cfg-buttons {
  display: flex;
  gap: 0.5rem;
  margin-top: 1rem;
}
</style>
