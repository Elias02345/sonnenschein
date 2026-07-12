<template>
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
</template>

<script>
import Button from 'primevue/button'

export default {
  components: { Button },
  data() {
    return {
      dark: document.documentElement.classList.contains('sonnenschein-dark')
    }
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
</style>
