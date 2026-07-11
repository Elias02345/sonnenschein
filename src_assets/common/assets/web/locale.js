import {createI18n} from "vue-i18n";

// Import only the fallback language files
import en from './public/assets/locale/en.json'

export default async function() {
    // If the locale API is unreachable (backend down, static preview,
    // transient error) the UI must still render — fall back to the
    // browser language instead of leaving a blank page.
    let locale = "en";
    try {
        let r = await (await fetch("./api/configLocale", { credentials: 'include' })).json();
        locale = r.locale ?? "en";
    } catch (e) {
        console.error("Failed to fetch configured locale, using browser language", e);
        locale = (navigator.language || "en").split("-")[0];
    }
    document.querySelector('html').setAttribute('lang', locale);
    let messages = {
        en
    };
    try {
        if (locale !== 'en') {
            let r = await (await fetch(`./assets/locale/${locale}.json`, { credentials: 'include' })).json();
            messages[locale] = r;
        }
    } catch (e) {
        console.error("Failed to download translations", e);
        locale = 'en';
    }
    const i18n = createI18n({
        locale: locale, // set locale
        fallbackLocale: 'en', // set fallback locale
        messages: messages
    })
    return i18n;
}
