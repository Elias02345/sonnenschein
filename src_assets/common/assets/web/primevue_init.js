// PrimeVue 4 foundation for the next-generation Sonnenschein WebUI.
//
// This is intentionally isolated from the existing Bootstrap-based pages
// (index/config/pin/...). New views opt in by bootstrapping through
// `initPrimeApp` instead of the legacy `initApp` in init.js, so migrating the
// UI can happen one page at a time without regressing the working UI.
import { definePreset } from '@primevue/themes'
import Aura from '@primevue/themes/aura'
import PrimeVue from 'primevue/config'
import i18n from './locale'

// Brand the Aura preset with Sonnenschein's amber accent.
const SonnenscheinPreset = definePreset(Aura, {
  semantic: {
    primary: {
      50: '{amber.50}',
      100: '{amber.100}',
      200: '{amber.200}',
      300: '{amber.300}',
      400: '{amber.400}',
      500: '{amber.500}',
      600: '{amber.600}',
      700: '{amber.700}',
      800: '{amber.800}',
      900: '{amber.900}',
      950: '{amber.950}'
    }
  }
})

// Resolve the initial color scheme: explicit choice → system preference → dark.
function initialDarkMode() {
  const stored = localStorage.getItem('theme')
  if (stored === 'light') return false
  if (stored === 'dark') return true
  return !window.matchMedia || window.matchMedia('(prefers-color-scheme: dark)').matches
}

export function initPrimeApp(app, config) {
  app.use(PrimeVue, {
    theme: {
      preset: SonnenscheinPreset,
      options: {
        // Dark mode is toggled by adding `.sonnenschein-dark` to <html>.
        darkModeSelector: '.sonnenschein-dark'
      }
    }
  })

  if (initialDarkMode()) {
    document.documentElement.classList.add('sonnenschein-dark')
  }

  i18n().then((i18n) => {
    app.use(i18n)
    app.provide('i18n', i18n.global)
    app.mount('#app')
    if (config) config(app)
  })
}
