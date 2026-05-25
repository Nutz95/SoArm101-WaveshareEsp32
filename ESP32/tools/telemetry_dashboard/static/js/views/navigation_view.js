export function initNavigationView(defaultView = "overview") {
  const tabs = Array.from(document.querySelectorAll("[data-view-tab]"));
  const panels = Array.from(document.querySelectorAll("[data-view-panel]"));

  function applyView(viewName) {
    tabs.forEach((tab) => {
      const isActive = tab.dataset.viewTab === viewName;
      tab.classList.toggle("is-active", isActive);
    });

    panels.forEach((panel) => {
      const panelView = panel.dataset.viewPanel;
      const visible = panelView === viewName;
      panel.classList.toggle("is-visible", visible);
    });
  }

  tabs.forEach((tab) => {
    tab.addEventListener("click", () => {
      applyView(String(tab.dataset.viewTab || defaultView));
    });
  });

  applyView(defaultView);
}
