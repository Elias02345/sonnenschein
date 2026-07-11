<template>
  <div class="wizard-page">
    <div class="wizard-card">
      <header class="wizard-header">
        <img src="/images/logo-apollo-45.png" height="56" alt="" />
        <h1>{{ $t('welcome.greeting') }}</h1>
        <p class="wizard-tagline">{{ $t('welcome.tagline') }}</p>
      </header>

      <!-- Step indicator -->
      <div class="wizard-steps">
        <div class="wizard-step" :class="{ active: step === 1, done: step > 1 }">
          <span class="wizard-step-dot">
            <i v-if="step > 1" class="pi pi-check" />
            <template v-else>1</template>
          </span>
          <span class="wizard-step-label">{{ $t('welcome.step_credentials') }}</span>
        </div>
        <div class="wizard-step-line" :class="{ done: step > 1 }" />
        <div class="wizard-step" :class="{ active: step === 2 }">
          <span class="wizard-step-dot">2</span>
          <span class="wizard-step-label">{{ $t('welcome.step_pairing') }}</span>
        </div>
      </div>

      <!-- Step 1: credentials -->
      <form v-if="step === 1" class="wizard-form" @submit.prevent="save">
        <p class="wizard-text">{{ $t('welcome.create_creds') }}</p>

        <label for="username">{{ $t('_common.username') }}</label>
        <InputText id="username" v-model="username" autocomplete="username" />

        <label for="password">{{ $t('_common.password') }}</label>
        <Password
          id="password"
          v-model="password"
          :feedback="true"
          toggle-mask
          autocomplete="new-password"
          :input-props="{ required: true }"
          fluid
        />

        <label for="confirm">{{ $t('welcome.confirm_password') }}</label>
        <Password
          id="confirm"
          v-model="confirmPassword"
          :feedback="false"
          toggle-mask
          autocomplete="new-password"
          :input-props="{ required: true }"
          fluid
          :invalid="!!confirmPassword && confirmPassword !== password"
        />

        <Message severity="warn" :closable="false" icon="pi pi-key">
          {{ $t('welcome.create_creds_alert') }}
        </Message>

        <Message v-if="error" severity="error" :closable="false" icon="pi pi-times-circle">
          {{ error }}
        </Message>

        <Button
          type="submit"
          :label="$t('welcome.continue')"
          icon="pi pi-arrow-right"
          icon-pos="right"
          :loading="loading"
          :disabled="!password || password !== confirmPassword"
        />
      </form>

      <!-- Step 2: success + pairing guidance -->
      <div v-else class="wizard-form">
        <Message severity="success" :closable="false" icon="pi pi-check-circle">
          {{ $t('welcome.done_title') }}
        </Message>

        <p class="wizard-text">{{ $t('welcome.done_pairing_hint') }}</p>
        <ol class="wizard-list">
          <li>{{ $t('welcome.done_step_moonlight') }}</li>
          <li>{{ $t('welcome.done_step_select_host') }}</li>
          <li>{{ $t('welcome.done_step_pin') }}</li>
        </ol>

        <Message severity="info" :closable="false" icon="pi pi-lock">
          {{ $t('welcome.done_relogin') }}
        </Message>

        <Button
          :label="$t('welcome.open_pairing')"
          icon="pi pi-link"
          @click="openPairing"
        />
      </div>
    </div>
  </div>
</template>

<script>
import Button from 'primevue/button'
import InputText from 'primevue/inputtext'
import Message from 'primevue/message'
import Password from 'primevue/password'

export default {
  components: { Button, InputText, Message, Password },
  data() {
    return {
      step: 1,
      username: 'sonnenschein',
      password: '',
      confirmPassword: '',
      error: null,
      loading: false
    }
  },
  methods: {
    save() {
      if (this.password !== this.confirmPassword) {
        this.error = this.$t('welcome.password_mismatch')
        return
      }
      this.error = null
      this.loading = true
      fetch('./api/password', {
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify({
          newUsername: this.username,
          newPassword: this.password,
          confirmNewPassword: this.confirmPassword
        })
      })
        .then((r) => (r.status === 200 ? r.json() : Promise.reject(new Error('HTTP ' + r.status))))
        .then((rj) => {
          this.loading = false
          if (rj.status === true) {
            this.step = 2
          } else {
            this.error = rj.error || this.$t('_common.error')
          }
        })
        .catch((e) => {
          this.loading = false
          this.error = String(e)
        })
    },
    openPairing() {
      // The browser will prompt for the freshly created credentials.
      document.location.href = './pin'
    }
  }
}
</script>

<style scoped>
.wizard-page {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1rem;
  background: var(--p-content-background);
}

.wizard-card {
  width: 100%;
  max-width: 30rem;
  background: var(--p-content-background);
  border: 1px solid var(--p-content-border-color);
  border-radius: 14px;
  padding: 2rem;
  box-shadow: 0 10px 40px rgb(0 0 0 / 0.25);
}

.wizard-header {
  text-align: center;
  margin-bottom: 1.5rem;
}

.wizard-header h1 {
  margin: 0.5rem 0 0.25rem;
  font-size: 1.5rem;
}

.wizard-tagline {
  margin: 0;
  color: var(--p-text-muted-color);
}

.wizard-steps {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.75rem;
  margin-bottom: 1.5rem;
}

.wizard-step {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  color: var(--p-text-muted-color);
}

.wizard-step.active {
  color: var(--p-text-color);
  font-weight: 600;
}

.wizard-step-dot {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 1.75rem;
  height: 1.75rem;
  border-radius: 50%;
  border: 2px solid var(--p-content-border-color);
  font-size: 0.85rem;
}

.wizard-step.active .wizard-step-dot,
.wizard-step.done .wizard-step-dot {
  border-color: var(--p-primary-color);
  color: var(--p-primary-color);
}

.wizard-step-line {
  width: 3rem;
  height: 2px;
  background: var(--p-content-border-color);
}

.wizard-step-line.done {
  background: var(--p-primary-color);
}

.wizard-form {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.wizard-form label {
  font-weight: 600;
  margin-bottom: -0.35rem;
}

.wizard-text {
  margin: 0;
}

.wizard-list {
  margin: 0;
  padding-left: 1.25rem;
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
}

@media (max-width: 480px) {
  .wizard-card {
    padding: 1.25rem;
  }
}
</style>
