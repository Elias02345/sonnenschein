// Sonnenschein Decky plugin — "Stream via Sonnenschein" button on the
// native Steam library game page (/library/app/:appid).
//
// The route-patching approach (locating the app-details renderFunc via
// createReactTreePatcher/afterPatch, then splicing a button next to the
// existing app panel inside appDetailsClasses.InnerContainer) is adapted
// verbatim from MoonDeck (GPL-3, https://github.com/FrogTheFrog/moondeck).

import {
  afterPatch,
  appDetailsClasses,
  createReactTreePatcher,
  DialogButton,
  findInReactTree,
} from "@decky/ui";
import { routerHook, toaster } from "@decky/api";
import { useState } from "react";
import { hostGameIndex, normalizeTitle, streamGame } from "./steamlib";

interface StreamButtonProps {
  appName: string;
}

function StreamButton({ appName }: StreamButtonProps) {
  const [busy, setBusy] = useState(false);

  const entry = hostGameIndex.get(normalizeTitle(appName));
  if (!entry) {
    // Not a game we found on the host — render nothing.
    return null;
  }

  const onClick = async () => {
    setBusy(true);
    try {
      const ok = await streamGame(entry.host, entry.app.title);
      if (!ok) {
        toaster.toast({
          title: "Sonnenschein",
          body: `${appName} konnte nicht gestartet werden.`,
        });
      }
    } finally {
      setBusy(false);
    }
  };

  return (
    <DialogButton style={{ marginTop: "10px" }} disabled={busy} onClick={onClick}>
      {busy ? "Startet…" : "Stream via Sonnenschein"}
    </DialogButton>
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

          const hltbIndex = parent.props.children.findIndex((x: any) => x.props.id === "hltb-for-deck");
          const appPanelIndex = parent.props.children.findIndex(
            (x: any) => x.props.overview && x.props.onShowLaunchingDetails
          );
          parent.props.children.splice(
            hltbIndex < 0 ? (appPanelIndex < 0 ? -1 : appPanelIndex - 1) : hltbIndex,
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
