<template>
  <div class="pw-page">
    <div class="pw-card">
      <header class="pw-header">
        <img src="/images/logo-apollo-45.png" height="56" alt="" />
        <h1>{{ $t('password.password_change') }}</h1>
      </header>

      <form class="pw-form" @submit.prevent="save">
        <h2>{{ $t('password.current_creds') }}</h2>

        <label for="currentUsername">{{ $t('_common.username') }}</label>
        <InputText
          id="currentUsername"
          v-model="passwordData.currentUsername"
          autocomplete="username"
          required
          autofocus
        />

        <label for="currentPassword">{{ $t('_common.password') }}</label>
        <Password
          id="currentPassword"
          v-model="passwordData.currentPassword"
          :feedback="false"
          toggle-mask
          autocomplete="current-password"
          :input-props="{ required: true }"
          fluid
        />

        <h2>{{ $t('password.new_creds') }}</h2>

        <label for="newUsername">{{ $t('_common.username') }}</label>
        <InputText id="newUsername" v-model="passwordData.newUsername" autocomplete="username" />
        <small class="pw-hint">{{ $t('password.new_username_desc') }}</small>

        <label for="newPassword">{{ $t('_common.password') }}</label>
        <Password
          id="newPassword"
          v-model="passwordData.newPassword"
          :feedback="false"
          toggle-mask
          autocomplete="new-password"
          :input-props="{ required: true }"
          fluid
        />

        <label for="confirmNewPassword">{{ $t('password.confirm_password') }}</label>
        <Password
          id="confirmNewPassword"
          v-model="passwordData.confirmNewPassword"
          :feedback="false"
          toggle-mask
          autocomplete="new-password"
          :input-props="{ required: true }"
          :invalid="mismatch"
          fluid
        />
        <small v-if="mismatch" class="pw-mismatch">{{ $t('welcome.password_mismatch') }}</small>

        <Message v-if="error" severity="error" :closable="false" icon="pi pi-times-circle">
          {{ error }}
        </Message>
        <Message v-if="success" severity="success" :closable="false" icon="pi pi-check-circle">
          {{ $t('password.success_msg') }}
        </Message>

        <Button
          type="submit"
          :label="$t('_common.save')"
          icon="pi pi-save"
          :loading="loading"
          :disabled="mismatch"
        />

        <a class="pw-back" href="./">
          <i class="pi pi-arrow-left"></i> {{ $t('password.back') }}
        </a>
      </form>
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
      error: null,
      success: false,
      loading: false,
      passwordData: {
        currentUsername: '',
        currentPassword: '',
        newUsername: '',
        newPassword: '',
        confirmNewPassword: ''
      }
    }
  },
  computed: {
    mismatch() {
      return (
        this.passwordData.confirmNewPassword.length > 0 &&
        this.passwordData.newPassword !== this.passwordData.confirmNewPassword
      )
    }
  },
  methods: {
    save() {
      if (this.passwordData.newPassword !== this.passwordData.confirmNewPassword) {
        this.error = this.$t('welcome.password_mismatch')
        return
      }
      this.error = null
      this.loading = true
      fetch('./api/password', {
        credentials: 'include',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify(this.passwordData)
      })
        .then((r) => {
          this.loading = false
          if (r.status !== 200) throw new Error(`Server returned ${r.status}`)
          return r.json()
        })
        .then((rj) => {
          if (rj.status === true) {
            this.success = true
            setTimeout(() => document.location.reload(), 5000)
          } else {
            this.error = rj.error
          }
        })
        .catch((e) => {
          this.loading = false
          this.error = e.message
        })
    }
  }
}
</script>

<style scoped>
.pw-page {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1rem;
  background: var(--p-content-background);
}

.pw-card {
  width: 100%;
  max-width: 26rem;
  background: var(--p-content-background);
  border: 1px solid var(--p-content-border-color);
  border-radius: 14px;
  padding: 2rem;
  box-shadow: 0 10px 40px rgb(0 0 0 / 0.25);
}

.pw-header {
  text-align: center;
  margin-bottom: 1.5rem;
}

.pw-header h1 {
  margin: 0.5rem 0 0;
  font-size: 1.4rem;
}

.pw-form {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.pw-form h2 {
  font-size: 1rem;
  margin: 0.5rem 0 0;
  color: var(--p-text-muted-color);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.pw-form label {
  font-weight: 600;
  margin-bottom: -0.35rem;
}

.pw-hint {
  color: var(--p-text-muted-color);
  margin-top: -0.4rem;
}

.pw-mismatch {
  color: var(--p-red-400);
  margin-top: -0.4rem;
}

.pw-back {
  text-align: center;
  color: var(--p-text-muted-color);
  text-decoration: none;
  margin-top: 0.25rem;
}

.pw-back:hover {
  color: var(--p-text-color);
}
</style>
