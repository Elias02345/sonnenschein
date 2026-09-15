// Small sun marker for native Steam games that are also available from the
// paired Sonnenschein host. Steam exposes no stable capsule extension API, so
// this uses the same DOM-observer pattern as established Decky badge plugins.

import { isHostStreamableAppId, subscribeHostGameIndex } from "./steamlib";

const BADGE_CLASS = "sonnenschein-capsule-badge";

function bigPictureWindow(): Window | null {
  try {
    const trees = (window as any).DFL?.getGamepadNavigationTrees?.() ?? [];
    for (const tree of trees) {
      const candidate = tree?.m_window as Window | undefined;
      if (candidate?.document?.body) {
        return candidate;
      }
    }
  } catch (e) {
    console.error("Sonnenschein: Big Picture window lookup failed", e);
  }
  return null;
}

function appIdFromCapsule(capsule: Element): string | null {
  const dataId = capsule.getAttribute("data-id");
  if (dataId && /^\d+$/.test(dataId)) {
    return dataId;
  }

  const href = capsule.querySelector("a")?.getAttribute("href") ?? "";
  const hrefMatch = href.match(/(?:\/app\/|\/details\/|run\/)(\d+)/i);
  if (hrefMatch) {
    return hrefMatch[1];
  }

  const src = capsule.querySelector("img")?.getAttribute("src") ?? "";
  const imageMatch = src.match(/(?:apps|rungameid)[\/_](\d+)/i);
  if (imageMatch) {
    return imageMatch[1];
  }

  // Virtualized Steam lists sometimes expose the id only through React.
  for (const element of [capsule, ...Array.from(capsule.children)]) {
    const fiberKey = Object.keys(element).find(
      (key) => key.startsWith("__reactFiber$") || key.startsWith("__reactInternalInstance$")
    );
    let fiber = fiberKey ? (element as any)[fiberKey] : null;
    for (let depth = 0; fiber && depth < 5; depth++, fiber = fiber.return) {
      const props = fiber.memoizedProps ?? fiber.return?.memoizedProps;
      const id = props?.appid ?? props?.appId ?? props?.overview?.appid ?? props?.appOverview?.appid;
      if (id) {
        return String(id);
      }
    }
  }
  return null;
}

function scan(changedPositions: Map<HTMLElement, string>): void {
  const targetWindow = bigPictureWindow();
  if (!targetWindow) {
    return;
  }
  const capsules = targetWindow.document.querySelectorAll(
    'div[role="tabpanel"] div[role="gridcell"], .ReactVirtualized__Grid__innerScrollContainer div[role="listitem"]'
  );
  capsules.forEach((capsule) => {
    const existing = capsule.querySelector(`.${BADGE_CLASS}`);
    const appId = appIdFromCapsule(capsule);
    if (!appId || !isHostStreamableAppId(appId)) {
      existing?.remove();
      return;
    }
    if (existing) {
      return;
    }
    const badge = targetWindow.document.createElement("span");
    badge.className = BADGE_CLASS;
    badge.textContent = "☀";
    badge.title = "Per Sonnenschein streambar";
    Object.assign(badge.style, {
      position: "absolute",
      right: "7px",
      top: "7px",
      zIndex: "20",
      width: "23px",
      height: "23px",
      borderRadius: "50%",
      display: "flex",
      alignItems: "center",
      justifyContent: "center",
      color: "#17120a",
      background: "#f6c453",
      boxShadow: "0 1px 5px rgba(0, 0, 0, 0.55)",
      fontSize: "16px",
      lineHeight: "1",
      pointerEvents: "none",
    });
    const target = (capsule.querySelector('div[role="link"]') ?? capsule) as HTMLElement;
    if (targetWindow.getComputedStyle(target).position === "static") {
      // Restored on plugin unload; do not leave Steam's DOM modified.
      changedPositions.set(target, target.style.position);
      target.style.position = "relative";
    }
    target.appendChild(badge);
  });
}

export function startCapsuleBadges(): () => void {
  let observer: MutationObserver | null = null;
  let observedDocument: Document | null = null;
  let scanPending = false;
  let scanTimeout: number | null = null;
  let stopped = false;
  const changedPositions = new Map<HTMLElement, string>();

  const scheduleScan = () => {
    if (stopped || scanPending) return;
    scanPending = true;
    scanTimeout = window.setTimeout(() => {
      scanTimeout = null;
      scanPending = false;
      if (!stopped) scan(changedPositions);
    }, 50);
  };

  const attach = () => {
    const targetWindow = bigPictureWindow();
    if (stopped) return;
    if (!targetWindow || observedDocument === targetWindow.document) {
      scheduleScan();
      return;
    }
    observer?.disconnect();
    observedDocument = targetWindow.document;
    const nextObserver = new MutationObserver(scheduleScan);
    nextObserver.observe(targetWindow.document.body, { childList: true, subtree: true });
    observer = nextObserver;
    scheduleScan();
  };

  const interval = window.setInterval(attach, 2000);
  const unsubscribe = subscribeHostGameIndex(scheduleScan);
  attach();

  return () => {
    stopped = true;
    window.clearInterval(interval);
    if (scanTimeout !== null) window.clearTimeout(scanTimeout);
    unsubscribe();
    observer?.disconnect();
    observedDocument?.querySelectorAll(`.${BADGE_CLASS}`).forEach((badge) => badge.remove());
    for (const [element, position] of changedPositions) {
      element.style.position = position;
    }
    changedPositions.clear();
  };
}
