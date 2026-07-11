<template>
  <div class="login-page">
    <div class="login-card">
      <header class="login-header">
        <img src="/images/logo-apollo-45.png" height="56" alt="" />
        <h1>{{ $t('welcome.greeting') }}</h1>
      </header>

      <form class="login-form" @submit.prevent="login">
        <label for="username">{{ $t('_common.username') }}</label>
        <InputText id="username" v-model="username" autocomplete="username" required autofocus />

        <label for="password">{{ $t('_common.password') }}</label>
        <Password
          id="password"
          v-model="password"
          :feedback="false"
          toggle-mask
          autocomplete="current-password"
          :input-props="{ required: true }"
          fluid
        />

        <div class="login-save">
          <Checkbox v-model="savePassword" input-id="save" binary />
          <label for="save">{{ $t('login.save_password') }}</label>
        </div>

        <Message v-if="error" severity="error" :closable="false" icon="pi pi-times-circle">
          {{ error }}
        </Message>

        <Button
          type="submit"
          :label="$t('welcome.login')"
          icon="pi pi-sign-in"
          :loading="loading"
        />
      </form>
    </div>
  </div>
</template>

<script>
import Button from 'primevue/button'
import Checkbox from 'primevue/checkbox'
import InputText from 'primevue/inputtext'
import Message from 'primevue/message'
import Password from 'primevue/password'

export default {
  components: { Button, Checkbox, InputText, Message, Password },
  data() {
    return {
      username: '',
      password: '',
      savePassword: false,
      error: null,
      loading: false
    }
  },
  created() {
    const saved = localStorage.getItem('login')
    if (saved) {
      try {
        const { username, password } = JSON.parse(saved)
        this.username = username
        this.password = password
        this.savePassword = true
      } catch (e) {
        console.error('Reading saved password failed!', e)
      }
    }
  },
  methods: {
    login() {
      this.error = null
      this.loading = true
      if (!this.savePassword) {
        localStorage.removeItem('login')
      }
      fetch('./api/login', {
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
        body: JSON.stringify({ username: this.username, password: this.password })
      })
        .then((res) => {
          this.loading = false
          if (res.status === 200) {
            if (this.savePassword) {
              localStorage.setItem('login', JSON.stringify({ username: this.username, password: this.password }))
            }
            const url = new URL(window.location)
            const redirectUrl = url.searchParams.get('redir')
            const hash = url.hash
            if (redirectUrl && redirectUrl.startsWith('.')) {
              location.href = redirectUrl + hash
            } else {
              location.href = './' + hash
            }
          } else if (res.status === 401) {
            throw new Error(this.$t('login.bad_credentials'))
          } else {
            throw new Error(`Server returned ${res.status}`)
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
.login-page {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1rem;
  background: var(--p-content-background);
}

.login-card {
  width: 100%;
  max-width: 24rem;
  background: var(--p-content-background);
  border: 1px solid var(--p-content-border-color);
  border-radius: 14px;
  padding: 2rem;
  box-shadow: 0 10px 40px rgb(0 0 0 / 0.25);
}

.login-header {
  text-align: center;
  margin-bottom: 1.5rem;
}

.login-header h1 {
  margin: 0.5rem 0 0;
  font-size: 1.4rem;
}

.login-form {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.login-form label {
  font-weight: 600;
  margin-bottom: -0.35rem;
}

.login-save {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.login-save label {
  font-weight: 400;
  margin: 0;
}
</style>
