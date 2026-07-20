// Sonnenschein Decky plugin — "Stream via Sonnenschein" button on the
// native Steam library game page (/library/app/:appid).
//
// The route-patching approach (locating the app-details renderFunc via
// createReactTreePatcher/afterPatch, then splicing a button next to the
// existing app panel inside appDetailsClasses.InnerContainer) is adapted
// verbatim from MoonDeck (GPL-3, https://github.com/FrogTheFrog/moondeck).
//
// Button sizing/placement note: MoonDeck itself solves this with a
// MutationObserver-driven absolutely-positioned overlay anchored to the
// header's TopCapsule (see its moondecklaunchbutton/*.tsx) — too much
// machinery to copy 1:1 here. What we DO take from it: (1) Steam's own
// DialogButton CSS wins on margin/min-height/padding unless overridden with
// `!important` (MoonDeck's buttonstyle.tsx does the same for `margin`), and
// (2) it renders its button as a *sibling row item next to* the play button,
// never as an extra full-width row. We replicate #2 without the observer by
// wrapping the existing app-panel array element (found the same way as
// before) in a flex row together with our button, instead of splicing a new
// row into the array.

import {
  afterPatch,
  appDetailsClasses,
  createReactTreePatcher,
  DialogButton,
  findInReactTree,
} from "@decky/ui";
import { callable, routerHook, toaster } from "@decky/api";
import { useEffect, useState } from "react";
import { hostGameIndex, normalizeTitle, streamGame } from "./steamlib";

interface StreamButtonProps {
  appName: string;
}

interface HostStatus {
  reachable: boolean;
  busy: boolean;
}

const checkHostAvailable = callable<[string, number], HostStatus>("check_host_available");

const STREAM_BUTTON_CLASS = "sonnenschein-stream-button";
const HOST_POLL_INTERVAL_MS = 8000;

// Scoped !important overrides (MoonDeck's buttonstyle.tsx trick) — without
// these, Steam's own DialogButton CSS keeps its large default min-height/
// padding/margin and the button ends up oversized again.
const StreamButtonStyle = (
  <style>
    {`
      .${STREAM_BUTTON_CLASS} {
        margin: 0 !important;
        min-width: 0 !important;
        min-height: 0 !important;
        height: 40px !important;
        padding: 0 16px !important;
        display: inline-flex !important;
        align-items: center !important;
        gap: 8px !important;
        white-space: nowrap !important;
        font-size: 14px !important;
      }
    `}
  </style>
);

function statusDotColor(status: HostStatus): string {
  if (!status.reachable) {
    return "#c9463d";
  }
  return status.busy ? "#d29922" : "#3fb950";
}

function statusDotTitle(status: HostStatus): string {
  if (!status.reachable) {
    return "Host nicht erreichbar";
  }
  return status.busy ? "Host ist mit einem anderen Spiel beschäftigt" : "Host verfügbar";
}

function StreamButton({ appName }: StreamButtonProps) {
  // "starting" = this click's launch is in flight; "hostStatus" = periodic
  // reachability/busy probe of the host itself — kept separate so one
  // doesn't get overwritten by the other's setState.
  const [starting, setStarting] = useState(false);
  const [hostStatus, setHostStatus] = useState<HostStatus>({ reachable: false, busy: false });

  const entry = hostGameIndex.get(normalizeTitle(appName));

  // All hooks must run unconditionally (entry can be undefined on some
  // renders) — the early `return null` below happens only after this.
  useEffect(() => {
    if (!entry) {
      return;
    }
    let cancelled = false;
    const poll = async () => {
      try {
        const status = await checkHostAvailable(entry.host.address, entry.host.port);
        if (!cancelled) {
          setHostStatus(status);
        }
      } catch (e) {
        console.error("Sonnenschein: check_host_available failed", e);
        if (!cancelled) {
          setHostStatus({ reachable: false, busy: false });
        }
      }
    };
    poll();
    const interval = setInterval(poll, HOST_POLL_INTERVAL_MS);
    return () => {
      cancelled = true;
      clearInterval(interval);
    };
  }, [entry?.host.address, entry?.host.port]);

  if (!entry) {
    // Not a game we found on the host — render nothing.
    return null;
  }

  const onClick = async () => {
    setStarting(true);
    try {
      const ok = await streamGame(entry.host, entry.app.title);
      if (!ok) {
        toaster.toast({
          title: "Sonnenschein",
          body: `${appName} konnte nicht gestartet werden.`,
        });
      }
    } finally {
      setStarting(false);
    }
  };

  return (
    <>
      {StreamButtonStyle}
      <DialogButton className={STREAM_BUTTON_CLASS} disabled={starting} onClick={onClick}>
        <span
          title={statusDotTitle(hostStatus)}
          style={{
            display: "inline-block",
            width: "8px",
            height: "8px",
            borderRadius: "50%",
            backgroundColor: statusDotColor(hostStatus),
            flexShrink: 0,
          }}
        />
        {starting ? "Startet…" : "Stream"}
      </DialogButton>
    </>
  );
}

export function patchLibraryApp(route: string) {
  return routerHook.addPatch(route, (tree: any) => {
    const routeProps = findInReactTree(tree, (x: any) => x?.renderFunc);
    if (routeProps) {
      let appId: number | undefined;
      let appName: string | undefined;

      const patchHandler = createReactTreePatcher(
        [
          (tree: any) => {
            const children = findInReactTree(tree, (x: any) => x?.props?.children?.props?.overview)?.props
              ?.children;
            if (typeof children !== "object") {
              return null;
            }
            const overview = children.props?.overview;
            if (
              typeof overview !== "object" ||
              typeof overview.appid !== "number" ||
              typeof overview.display_name !== "string"
            ) {
              return null;
            }
            ({ appid: appId, display_name: appName } = overview);
            return children;
          },
        ],
        (_: any, ret?: any) => {
          const parent = findInReactTree(
            ret,
            (x: any) =>
              Array.isArray(x?.props?.children) && x?.props?.className?.includes(appDetailsClasses.InnerContainer)
          );
          if (typeof parent !== "object" || typeof appId !== "number" || typeof appName !== "string") {
            return ret;
          }

          const appPanelIndex = parent.props.children.findIndex(
            (x: any) => x?.props?.overview && x?.props?.onShowLaunchingDetails
          );

          if (appPanelIndex >= 0) {
            // Wrap the existing app-panel element (Play/Install button row)
            // together with ours in a flex row, so our button sits BESIDE it
            // instead of below it as an extra full-width row.
            const original = parent.props.children[appPanelIndex];
            parent.props.children[appPanelIndex] = (
              <div
                key={original?.key ?? "sonnenschein-panel-row"}
                style={{ display: "flex", flexDirection: "row", alignItems: "center", gap: "8px" }}
              >
                <div style={{ flex: "1 1 auto", minWidth: 0 }}>{original}</div>
                <StreamButton appName={appName} key="sonnenschein-stream-button" />
              </div>
            );
            return ret;
          }

          // Fallback (app panel not found, e.g. future Steam UI drift):
          // still show the button somewhere rather than nowhere at all.
          const hltbIndex = parent.props.children.findIndex((x: any) => x.props.id === "hltb-for-deck");
          parent.props.children.splice(
            hltbIndex < 0 ? -1 : hltbIndex,
            0,
            <StreamButton appName={appName} key="sonnenschein-stream-button" />
          );
          return ret;
        }
      );
      afterPatch(routeProps, "renderFunc", patchHandler);
    }
    return tree;
  });
}
