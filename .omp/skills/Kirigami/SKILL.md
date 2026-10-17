---
name: Kirigami
description: Expert KDE Kirigami / Qt Quick QML development rules for convergent desktop+mobile apps — app structure, pages, forms, responsive sizing with Kirigami.Units, theming, i18n, and idiomatic components per the KDE HIG. Use for any Kirigami, QML, or Qt Quick Controls UI work.
---

# Kirigami UI Development Rules

You are an expert KDE Kirigami/QML developer. Write clean, convergent, modern QML following the KDE HIG.


## 0. Official Documentation
Cross-reference these before guessing an API:
* **Developer Guide & Kirigami tutorials:** https://develop.kde.org/docs/getting-started/kirigami/
* **Kirigami QML API Reference:** https://api.kde.org/kirigami-index.html
* **KDE Human Interface Guidelines (HIG):** https://develop.kde.org/hig/
* **Kirigami Gallery** — working reference QML per component. **Before inventing a UI pattern, read the matching example:** `https://invent.kde.org/sdk/kirigami-gallery/-/raw/master/src/data/contents/ui/gallery/<Component>Gallery.qml` (one `*Gallery.qml` per component: Dialogs, Cards, Forms, NavigationTabBar, OverlaySheet, TextField, …)

## 0b. Local Environment (this system — Kirigami 6.31.0, Qt 6.12.0)
Docs and the Gallery target *master*; types may not exist in this installed build. Verify before use (see `AGENTS.md` § Tooling rules). Verified facts for 6.31:
* `pageStack` on `Kirigami.ApplicationWindow` is a **readonly alias to an internal `Kirigami.PageRow`** — never assign a new object to it. Set `pageStack.initialPage: ...` (or a `Kirigami.Page` id / `Qt.resolvedUrl(...)`) and use `pageStack.push()/pop()`.
* **`Kirigami.Header` / `Headline` / `HeaderLayout` do NOT exist in this build.** Custom header: `globalHeader: Kirigami.AbstractApplicationHeader { ... }` (its `contentItem` is the default property — place controls directly inside).
* `Kirigami.PlaceholderMessage` uses `text` (title) + `explanation` (body) — there is **no `heading`** property.
* Submodule types (`Dialog`, `FormLayout`, `ListItem`, `Heading`, …) resolve via the plain `import org.kde.kirigami as Kirigami`.

## 1. Core Rules
* **Standard imports** in every Kirigami file:
  ```qml
  import QtQuick
  import QtQuick.Controls
  import QtQuick.Layouts
  import org.kde.kirigami as Kirigami
  ```
* **No fixed item sizes:** never `width: 400` / `height: 600` on items. Use layouts, implicit sizing, aspect ratios, or `Kirigami.Units`. Initial `width`/`height` on `ApplicationWindow` is fine.
* **Semantic colors only:** `Kirigami.Theme.textColor`, `backgroundColor`, `highlightColor`, `disabledTextColor`, `alternativeBackgroundColor`. Never hardcoded `#...` or raw color names.
* **i18n:** every user-facing string MUST use `qsTr()`; dynamic values via `%1` placeholders — `qsTr("%1 items").arg(n)`.
* **Convergence:** UI must work on desktop and mobile — never assume fixed orientation or a minimum resolution.

## 2. Application Structure
* **Root:** `Kirigami.ApplicationWindow` — provides `pageStack`, `showPassiveNotification()`, and global actions.
* **Pages:** `Kirigami.Page` pushed via `pageStack` (`pageStack.initialPage` for the root page).
* **Actions:** `Kirigami.Action` objects in page/window `actions` — one object per command, shared by tool buttons, menu items, and shortcuts. Never duplicate action logic in button handlers.
* **Tab navigation (3–5 pages):** `Kirigami.NavigationTabBar` + `Kirigami.NavigationTabButton`.
* **Sidebar-style navigation:** `Kirigami.PageRow`.

## 3. Page Types
| Component | Use for | Avoid |
| :--- | :--- | :--- |
| `Kirigami.Page` | Container with header/footer/actions | `Item`/`Rectangle` as a page |
| `Kirigami.ScrollablePage` | Long scrollable content | Manual `Flickable` |
| `Kirigami.Heading` | Titles and section headers (`level: 1-4`) | Bold `Label` as a header |
| `Kirigami.InlineViewHeader` | Inline view header with title + actions | |

## 4. Forms
Use `Kirigami.FormLayout` for settings/data entry — it handles desktop/mobile alignment automatically.
* Labels: `Kirigami.FormData.label: "..."`
* Optional fields: `Kirigami.FormData.isOptional: true`
* Sections: `Kirigami.FormLayout.Separator`

## 5. Responsive Sizing
* **Spacing:** `Kirigami.Units.gridUnit` / `largeSpacing` / `mediumSpacing` / `smallSpacing` — never raw pixels.
* **Durations:** `Kirigami.Units.shortDuration` / `mediumDuration` / `longDuration` in animations.
* **Adapt to screen:** `Kirigami.isCompact` (bool), `Kirigami.Screen.formFactor`, `Kirigami.layoutDirection` — flip Row↔Column and hide secondary UI in compact form.

## 6. Theming
* **Fonts:** `Kirigami.Theme.defaultFont`, `smallFont`, `headerFont`, `headerMediumFont`, `headerLargeFont`.
* Follow the system theme — never force dark/light or hardcode a palette.

## 7. Idiomatic Components (prefer over raw Controls 2)
| Task | Use |
| :--- | :--- |
| Dashboard/feed items | `Kirigami.Card`; grids via `Kirigami.CardsListView` / `CardsLayout` |
| Empty view | `Kirigami.PlaceholderMessage` |
| Loading view | `Kirigami.LoadingPlaceholder` |
| Inline status + actions | `Kirigami.InlineMessage` |
| Icons | `Kirigami.Icon` (primitives module) with Breeze names; `"-symbolic"` suffix for monochrome |
| List search | `Kirigami.SearchField` |
| Passwords | `Kirigami.PasswordField` |
| Selectable text | `Kirigami.SelectableLabel` |
| Attributes/filters/counts | `Kirigami.Chip`, `Kirigami.Badge` |
| Dialogs | `Kirigami.Dialog`; `PromptDialog` / `MenuDialog` for specific cases; `standardButtons` when applicable |
| List rows / switches | `org.kde.kirigami.delegates` (`ListItem`, `Switch`, `Checkbox`) |
| Always-available global actions | `Kirigami.GlobalDrawer` |
| Small secondary tasks | `Kirigami.OverlaySheet` |

## 8. Standard Patterns
* **Tooltips:**
  ```qml
  ToolTip.text: qsTr("...")
  ToolTip.visible: hovered
  ToolTip.delay: Kirigami.Units.toolTipDelay
  ```
* **Transient feedback:** `applicationWindow().showPassiveNotification(message, "short")` — use `"long"` for errors.
* **Icon-only buttons:** `display: AbstractButton.IconOnly` + Breeze icon name + `Accessible.name`.
* **Transitions:** `Behavior on opacity { OpacityAnimator { duration: Kirigami.Units.shortDuration } }`.
* **Accessibility:** set `Accessible.name` on every control without a visible label; form labels via `FormData` already provide it.
